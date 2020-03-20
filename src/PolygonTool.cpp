////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PolygonTool.hpp"

#include "../../../src/cs-core/GraphicsEngine.hpp"
#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/InputManager.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-core/TimeControl.hpp"
#include "../../../src/cs-core/tools/DeletableMark.hpp"
#include "../../../src/cs-scene/CelestialAnchorNode.hpp"
#include "../../../src/cs-utils/convert.hpp"
#include "../../../src/cs-utils/utils.hpp"

#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>
#include <spdlog/spdlog.h>

namespace csp::measurementtools {

////////////////////////////////////////////////////////////////////////////////////////////////////

const int PolygonTool::NUM_SAMPLES = 256;

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string PolygonTool::SHADER_VERT = R"(
#version 330

layout(location=0) in vec3 iPosition;

out vec4 vPosition;

uniform mat4 uMatModelView;
uniform mat4 uMatProjection;

void main()
{
    vPosition   = uMatModelView * vec4(iPosition, 1.0);
    gl_Position = uMatProjection * vPosition;
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string PolygonTool::SHADER_FRAG = R"(
#version 330

in vec4 vPosition;
//in vec2 vTexcoord;

uniform float uOpacity;
uniform float uFarClip;
uniform vec4 uColor;

layout(location = 0) out vec4 oColor;

void main()
{
    oColor = uColor;

    gl_FragDepth = length(vPosition.xyz) / uFarClip;
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

PolygonTool::PolygonTool(std::shared_ptr<cs::core::InputManager> const& pInputManager,
    std::shared_ptr<cs::core::SolarSystem> const&                       pSolarSystem,
    std::shared_ptr<cs::core::GraphicsEngine> const&                    graphicsEngine,
    std::shared_ptr<cs::core::TimeControl> const& pTimeControl, std::string const& sCenter,
    std::string const& sFrame)
    : MultiPointTool(pInputManager, pSolarSystem, graphicsEngine, pTimeControl, sCenter, sFrame)
    , mGuiArea(std::make_unique<cs::gui::WorldSpaceGuiArea>(600, 300))
    , mGuiItem(std::make_unique<cs::gui::GuiItem>("file://../share/resources/gui/polygon.html")) {

  // Create the shader
  mShader.InitVertexShaderFromString(SHADER_VERT);
  mShader.InitFragmentShaderFromString(SHADER_FRAG);
  mShader.Link();

  // Attach this as OpenGLNode to scenegraph's root (all line vertices
  // will be draw relative to the observer, therfore we do not want
  // any transformation)
  auto pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
  mParent.reset(pSG->NewOpenGLNode(pSG->GetRoot(), this));

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mParent.get(), static_cast<int>(cs::utils::DrawOrder::eOpaqueItems));

  // Create a a VistaCelestialAnchorNode for the user interface
  // it will be moved to the center of all points when a point is moved
  // and rotated in such a way, that it always faces the observer
  mGuiAnchor = std::make_shared<cs::scene::CelestialAnchorNode>(
      pSG->GetRoot(), pSG->GetNodeBridge(), "", sCenter, sFrame);
  mGuiAnchor->setAnchorScale(mSolarSystem->getObserver().getAnchorScale());
  mSolarSystem->registerAnchor(mGuiAnchor);

  // Create the user interface
  mGuiTransform.reset(pSG->NewTransformNode(mGuiAnchor.get()));
  mGuiTransform->Translate(0.0f, 0.9f, 0.0f);
  mGuiTransform->Scale(0.001f * mGuiArea->getWidth(), 0.001f * mGuiArea->getHeight(), 1.f);
  mGuiTransform->Rotate(VistaAxisAndAngle(VistaVector3D(0.0, 1.0, 0.0), -glm::pi<float>() / 2.f));
  mGuiArea->addItem(mGuiItem.get());
  mGuiArea->setUseLinearDepthBuffer(true);
  mGuiNode.reset(pSG->NewOpenGLNode(mGuiTransform.get(), mGuiArea.get()));

  mInputManager->registerSelectable(mGuiNode.get());

  mGuiItem->setCanScroll(false);
  mGuiItem->waitForFinishedLoading();

  mGuiItem->registerCallback("deleteMe", "Call this to delete the tool.",
      std::function([this]() { pShouldDelete = true; }));

  mGuiItem->registerCallback("setAddPointMode", "Call this to enable creation of new points.",
      std::function([this](bool enable) {
        addPoint();
        pAddPointMode = enable;
      }));

  mGuiItem->registerCallback("showMesh", "Enables or disables the rendering of the surface grid.",
      std::function([this]() { mShowMesh = !mShowMesh; }));

  mGuiItem->setCursorChangeCallback([](cs::gui::Cursor c) { cs::core::GuiManager::setCursor(c); });

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mGuiNode.get(), static_cast<int>(cs::utils::DrawOrder::eTransparentItems));

  // Whenever the height scale changes our vertex positions need to be updated
  mScaleConnection = mGraphicsEngine->pHeightScale.connectAndTouch([this](float const& h) {
    updateLineVertices();
    updateCalculation();
  });

  // Add one point initially
  addPoint();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

PolygonTool::~PolygonTool() {
  mGraphicsEngine->pHeightScale.disconnect(mScaleConnection);
  mGuiItem->unregisterCallback("deleteMe");
  mGuiItem->unregisterCallback("setAddPointMode");
  mGuiItem->unregisterCallback("showMesh");

  mInputManager->unregisterSelectable(mGuiNode.get());
  mSolarSystem->unregisterAnchor(mGuiAnchor);

  auto pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
  pSG->GetRoot()->DisconnectChild(mGuiAnchor.get());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::setHeightDiff(float const& hDiff) {
  mHeightDiff = hDiff;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::setMaxAttempt(int const& att) {
  mMaxAttempt = att;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::setMaxPoints(int const& points) {
  mMaxPoints = points;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::setSleekness(int const& degree) {
  mSleekness = degree;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

glm::dvec4 PolygonTool::getInterpolatedPosBetweenTwoMarks(cs::core::tools::DeletableMark const& l0,
    cs::core::tools::DeletableMark const& l1, double value) {
  double     h_scale = mGraphicsEngine->pHeightScale.get();
  glm::dvec3 radii   = mSolarSystem->getRadii(mGuiAnchor->getCenterName());

  // Calculates the position for the new segment anchor
  double h0 = mSolarSystem->pActiveBody.get()->getHeight(l0.pLngLat.get()) * h_scale;
  double h1 = mSolarSystem->pActiveBody.get()->getHeight(l1.pLngLat.get()) * h_scale;

  // Gets cartesian coordinates for interpolation
  glm::dvec3 p0 = cs::utils::convert::toCartesian(l0.pLngLat.get(), radii[0], radii[0], h0);
  glm::dvec3 p1 = cs::utils::convert::toCartesian(l1.pLngLat.get(), radii[0], radii[0], h1);
  glm::dvec3 interpolatedPos = p0 + (value * (p1 - p0));

  // Calculates final position
  glm::dvec2 ll     = cs::utils::convert::toLngLatHeight(interpolatedPos, radii[0], radii[0]).xy();
  double     height = mSolarSystem->pActiveBody.get()->getHeight(ll) * h_scale;
  glm::dvec3 pos    = cs::utils::convert::toCartesian(ll, radii[0], radii[0], height);
  return glm::dvec4(pos, height);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Based on
// https://stackoverflow.com/questions/8721406/how-to-determine-if-a-point-is-inside-a-2d-convex-polygon
bool PolygonTool::checkPoint(glm::dvec2 const& point) {
  int  i, j;
  bool result = 0;

  // Positive x (other directions could be compared, but it works reliable with only one direction)
  for (i = 0, j = mCorners.size() - 1; i < mCorners.size(); j = i++) {
    if ((mCorners[i].mY > point.y) != (mCorners[j].mY > point.y) &&
        (point.x < (mCorners[j].mX - mCorners[i].mX) * (point.y - mCorners[i].mY) /
                           (mCorners[j].mY - mCorners[i].mY) +
                       mCorners[i].mX)) {
      result = !result;
      // Checks surroundings to avoid numerical errors
    } else if ((mCorners[i].mY > point.y) != (mCorners[j].mY > point.y) &&
               std::abs(point.x - ((mCorners[j].mX - mCorners[i].mX) * (point.y - mCorners[i].mY) /
                                          (mCorners[j].mY - mCorners[i].mY) +
                                      mCorners[i].mX)) < 0.001) {
      result = !result;
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool PolygonTool::findIntersection(Site const& s1, Site const& s2, Site const& s3, Site const& s4,
    double& intersectionX, double& intersectionY) {
  // Avoids division with 0
  if ((s1.mX == 0) || (s2.mX == 0) || (s3.mX == 0) || (s4.mX == 0) || (s1.mY == 0) ||
      (s2.mY == 0) || (s3.mY == 0) || (s4.mY == 0))
    return 0;

  // Based on
  // http://www.softwareandfinance.com/Visual_CPP/VCPP_Intersection_Two_lines_EndPoints.html

  // Safety band - to avoid point duplications - set to 1%
  double safety = 0.01;

  double m1, m2;
  double c1, c2;
  double minX, maxX, minY, maxY;

  // Line 1 (y = m1 * x + c1)
  m1 = (s2.mY - s1.mY) / (s2.mX - s1.mX);
  c1 = s1.mY - m1 * s1.mX;

  // Line 2 (y = m2 * x + c2)
  m2 = (s4.mY - s3.mY) / (s4.mX - s3.mX);
  c2 = s3.mY - m2 * s3.mX;

  // Edges are not exactly parallel
  if (m1 != m2) {
    // Intersection of lines
    intersectionX = (c2 - c1) / (m1 - m2);
    intersectionY = m1 * (intersectionX) + c1;

    // Checks if intersection point is on the edges or not (between a bounding box)
    if (((s1.mX > intersectionX) != (s2.mX > intersectionX)) &&
        ((s3.mX > intersectionX) != (s4.mX > intersectionX)) &&
        ((s1.mY > intersectionY) != (s2.mY > intersectionY)) &&
        ((s3.mY > intersectionY) != (s4.mY > intersectionY))) {
      // Checks all 4 points: do not return with an intersection point within the safety band
      if (((std::abs((s1.mX - intersectionX) / s1.mX) > safety) ||
              (std::abs((s1.mY - intersectionY) / s1.mY) > safety)) &&
          ((std::abs((s2.mX - intersectionX) / s2.mX) > safety) ||
              (std::abs((s2.mY - intersectionY) / s2.mY) > safety)) &&
          ((std::abs((s3.mX - intersectionX) / s3.mX) > safety) ||
              (std::abs((s3.mY - intersectionY) / s3.mY) > safety)) &&
          ((std::abs((s4.mX - intersectionX) / s4.mX) > safety) ||
              (std::abs((s4.mY - intersectionY) / s4.mY) > safety))) {
        return 1;
      }
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::createMesh(std::vector<Triangle>& triangles) {
  bool edgesOK = 0;
  int  it      = 0;

  // Does the triangulaiton of the original polygon
  // Checks and refines the triangulation until all original edges of the polygon are in the
  // triangulation Quits after 5 iteration to avoid performance issues and displays error message
  while (!edgesOK && it < 5) {
    it++;

    // Performs the Delaunay triangulation
    VoronoiGenerator voronoi;
    voronoi.parse(mCorners);

    // Number of the original edges of the polygon
    int countEdges = mCorners.size();

    // Vector of the original edges of the polygon from Delaunay triangulation
    std::vector<Edge2> voronoiEdges;

    for (auto const& s : voronoi.getTriangulation()) {
      // Finds original edges based on their addresses
      if (((std::abs(s.second.mAddr - s.first.mAddr) == 1 ||
               (std::abs(s.second.mAddr - s.first.mAddr) == mCorners.size() - 1)) &&
              s.first.mAddr < mCorners.size() && s.second.mAddr < mCorners.size())) {
        // Counts found edges
        countEdges--;

        Site site1(0, 0, 0);
        Site site2(0, 0, 0);

        // Orders addresses of the found edge
        if (((s.first.mAddr == mCorners.size() - 1) && (s.second.mAddr == 0)) ||
            ((s.second.mAddr > s.first.mAddr) &&
                !((s.first.mAddr == 0) && (s.second.mAddr == mCorners.size() - 1)))) {
          site1 = s.first;
          site2 = s.second;
        } else {
          site1 = s.second;
          site2 = s.first;
        }
        // Saves edges of the triangulation
        voronoiEdges.push_back(std::make_pair(site1, site2));
      }
    }

    // If some of the polygon edges did not match with a voronoi edge
    // This means, that some edges are missing, and need to be recovered
    // Intersection points of the missing edges and voronoi edges need to determined
    // These points are added to mCorners, and the triangulation hopefully
    // solves the problem in the next cycle (works for most of the cases)
    if (countEdges != 0) {
      // Vector of corners on missing edges - to be added to mCorners
      std::vector<Site> addCorners;

      // Finds the missing edges: search for every original edge in voronoiEdges
      // (the original polygon edges have neighbor addresses -> searches for corners)
      for (int i = 0; i < mCorners.size(); i++) {
        bool       found = 0;
        glm::ivec2 missingAddr;

        // In case of the last line of the polygon
        if (i == (mCorners.size() - 1)) {
          for (auto const& v : voronoiEdges)
            if ((v.first.mAddr == i) && (v.second.mAddr == 0))
              found = 1;

          if (!found)
            missingAddr = glm::ivec2(i, 0);
        }
        // Every other line
        else {
          for (auto const& v : voronoiEdges)
            if ((v.first.mAddr == i) && (v.second.mAddr == i + 1))
              found = 1;
          if (!found)
            missingAddr = glm::ivec2(i, i + 1);
        }

        // If this edge is missing
        if (!found) {
          // Points of the missing edge
          Site              site1(0, 0, 0);
          Site              site2(0, 0, 0);
          std::vector<Site> sites;

          // Pairs the known addresses of the missing edge with sites
          for (auto const& s : voronoi.getTriangulation()) {
            if (s.first.mAddr == missingAddr.x)
              site1 = s.first;
            if (s.second.mAddr == missingAddr.x)
              site1 = s.second;

            if (s.first.mAddr == missingAddr.y)
              site2 = s.first;
            if (s.second.mAddr == missingAddr.y)
              site2 = s.second;
          }

          // Finds intersecting edges (if a triangulation edge intersects the original polygon edge,
          // it is wrong)
          for (auto const& s : voronoi.getTriangulation()) {
            double intersectionX, intersectionY;

            if (findIntersection(site1, site2, s.first, s.second, intersectionX, intersectionY)) {
              int addrNew =
                  site1.mAddr + 1; // = site2.mAddr (except for the last edge, where site2.mAddr=0!
              Site oldCorner(0, 0, 0);
              bool done = 0;

              // Cycles through addCorners vector and saves current intersection into the right
              // place
              for (int i = 0; i < addCorners.size(); i++) {
                // If current intersection is not saved yet
                if (!done) {
                  int addr2 = addCorners[i].mAddr;

                  // Compares addresses of the two intersection
                  if (addCorners[i].mAddr < addrNew) {
                    // Skips first elements of vector
                  } else if (addCorners[i].mAddr == addrNew) {
                    // If edges points to positive x
                    if (intersectionX > site1.mX) {
                      // Saves intersection only when it is in front of the
                      // existing intersection; otherwise it will be handled
                      // in the next cycle
                      if (addCorners[i].mX > intersectionX) {
                        // Saves existing intersection to oldCorner
                        // will be placed back to vector in the next cycle
                        oldCorner     = addCorners[i];
                        addCorners[i] = Site(intersectionX, intersectionY, addrNew);
                        done          = 1;
                      }
                    }
                    // If edges points to negative x
                    else if (intersectionX < site1.mX) {
                      if (addCorners[i].mX < intersectionX) {
                        oldCorner     = addCorners[i];
                        addCorners[i] = Site(intersectionX, intersectionY, addrNew);
                        done          = 1;
                      }
                    }
                    // Handles the very rare case of a vertical edge
                    // Does the same, as before, just now with y coordinates
                    else if (intersectionX == site1.mX) {
                      if (intersectionY > site1.mY) {
                        if (addCorners[i].mY > intersectionY) {
                          oldCorner     = addCorners[i];
                          addCorners[i] = Site(intersectionX, intersectionY, addrNew);
                          done          = 1;
                        }
                      } else if (intersectionY < site1.mY) {
                        if (addCorners[i].mY < intersectionY) {
                          oldCorner     = addCorners[i];
                          addCorners[i] = Site(intersectionX, intersectionY, addrNew);
                          done          = 1;
                        }
                      }
                    } // if (intersectionX==s.first.mX)
                  }   // if (addCorners[i].mAddr == addrNew)
                  // if (addCorners[i].mAddr > addrNew)
                  else {
                    oldCorner     = addCorners[i];
                    addCorners[i] = Site(intersectionX, intersectionY, addrNew);
                    done          = 1;
                  }
                } // if (!done)
                else {
                  // After the current intersecting point is added to the middle of the vector
                  // shifts back every other element by one position
                  Site newCorner = addCorners[i];
                  addCorners[i]  = oldCorner;
                  oldCorner      = newCorner;
                }
              } // for (int i = 0; i < addCorners.size(); i++)

              // If the current intersection was placed in the vector
              if (done) {
                // Emplaces the last element to the end of the vector
                addCorners.emplace_back(oldCorner);
              } else {
                // Emplaces back current intersection to the end of the vector
                addCorners.emplace_back(Site(intersectionX, intersectionY, addrNew));
              }

              // Corners needed to be added -> run the cycle again
              edgesOK = 0;
            } // if (findIntersection(...))
          }   // for (auto const& s : triangulation)
        }     // if (!found)
      }       // for (int i = 0; i < mCorners.size(); i++)

      // Counts the intersection corners already added to mCorners
      int cornerCount = 0;

      // Goes through the intersection corners and adds them to the right place of mCorners
      for (auto const& c : addCorners) {
        // Address of the intersection if it is the only one in the vector
        int addr3 = c.mAddr;
        addr3 += cornerCount;

        // If intersection is not between last and first Site
        if (addr3 < mCorners.size()) {
          // Saves intersection corner to mCorners and element of mCorners to oldSite
          Site oldSite    = mCorners[addr3];
          mCorners[addr3] = Site(c.mX, c.mY, addr3);

          addr3++;

          // Shifts every other corner of mCorners behind by one
          for (addr3; addr3 < mCorners.size(); addr3++) {
            Site newSite    = mCorners[addr3];
            mCorners[addr3] = Site(oldSite.mX, oldSite.mY, addr3);
            oldSite         = newSite;
          }

          // Emplaces back the last corner
          mCorners.emplace_back(Site(oldSite.mX, oldSite.mY, addr3));
        } else {
          // Emplaces back the intersection corner
          mCorners.emplace_back(Site(c.mX, c.mY, addr3));
        }

        cornerCount++;
      }
    } // if (countEdges != 0)
    else {
      // All of the original edges are in voronoiEdges -> no need for an other cycle
      edgesOK = 1;
    }
    // Saves the original triangles
    triangles = voronoi.getTriangles();
  } // while (!edgesOK && it < 5)

  // If the voronoi edges are still wrong after 5 cycles of refinement, display the problem
  if (!edgesOK) {
    spdlog::warn("Area calculation can be false: Concave or self-intersecting polygon! Check "
                 "triangulation mesh.");
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool PolygonTool::checkSleekness(int count) {
  // Voronoi inside the original triangles - to check triangle angles
  VoronoiGenerator voronoiCheck;
  voronoiCheck.parse(mCornersFine[count]);

  // Vector to save addresses of added points - to avoid adding the same point twice
  std::vector<std::pair<int, int>> addedPoints;

  // Checks triangle sleekness and add middle points to vector if they are too sleek
  // Could be done in multiple iterations for a more precise result
  for (auto const& t2 : voronoiCheck.getTriangles()) {
    // Minimun angle criteria (for 2 simple cases, approximately correct in general)
    float minAngle = mSleekness * glm::pi<float>() / 180;
    // Ratio of two edges in triangle
    float sleekness1 = 1 / std::sin(minAngle);
    // Ration between the sum of 2 smaller edges and the long edge in triangle
    float sleekness2 = 1 / std::cos(minAngle);

    Site si1(0, 0, 0);
    Site si2(0, 0, 0);
    Site si3(0, 0, 0);
    std::tie(si1, si2, si3) = t2;

    // Length of the edges
    double length1 = glm::length(glm::dvec2(si1.mX, si1.mY) - glm::dvec2(si2.mX, si2.mY));
    double length2 = glm::length(glm::dvec2(si1.mX, si1.mY) - glm::dvec2(si3.mX, si3.mY));
    double length3 = glm::length(glm::dvec2(si2.mX, si2.mY) - glm::dvec2(si3.mX, si3.mY));

    // Edge 1 is too long compared to the others
    if ((length2 * sleekness1 < length1) || (length3 * sleekness1 < length1) ||
        (length2 + length3 < length1 * sleekness2)) {
      bool addPoint = 1;
      // Checks previously added points if they are the same
      for (auto const& addr : addedPoints) {
        if (((addr.first == si1.mAddr) && (addr.second == si2.mAddr)) ||
            ((addr.first == si2.mAddr) && (addr.second == si1.mAddr)))
          addPoint = 0;
      }
      // If not, adds this point to vector
      if (addPoint) {
        mCornersFine[count].emplace_back(
            (si1.mX + si2.mX) / 2, (si1.mY + si2.mY) / 2, mCornersFine[count].size());
        addedPoints.emplace_back(si1.mAddr, si2.mAddr);
      }
    }

    // Edge 2 is too long compared to the others
    if ((length1 * sleekness1 < length2) || (length3 * sleekness1 < length2) ||
        (length1 + length3 < length2 * sleekness2)) {
      bool addPoint = 1;
      for (auto const& addr : addedPoints) {
        if (((addr.first == si1.mAddr) && (addr.second == si3.mAddr)) ||
            ((addr.first == si3.mAddr) && (addr.second == si1.mAddr)))
          addPoint = 0;
      }
      if (addPoint) {
        mCornersFine[count].emplace_back(
            (si1.mX + si3.mX) / 2, (si1.mY + si3.mY) / 2, mCornersFine[count].size());
        addedPoints.emplace_back(si1.mAddr, si3.mAddr);
      }
    }

    // Edge 3 is too long compared to the others
    if ((length1 * sleekness1 < length3) || (length2 * sleekness1 < length3) ||
        (length1 + length2 < length3 * sleekness2)) {
      bool addPoint = 1;
      for (auto const& addr : addedPoints) {
        if (((addr.first == si2.mAddr) && (addr.second == si3.mAddr)) ||
            ((addr.first == si3.mAddr) && (addr.second == si2.mAddr)))
          addPoint = 0;
      }
      if (addPoint) {
        mCornersFine[count].emplace_back(
            (si2.mX + si3.mX) / 2, (si2.mY + si3.mY) / 2, mCornersFine[count].size());
        addedPoints.emplace_back(si2.mAddr, si3.mAddr);
      }
    }
  }

  if (addedPoints.size() > 1.5 * (mCornersFine[count].size() - addedPoints.size()))
    return 1;

  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::displayMesh(Edge2 const& edge, double mdist, glm::dvec3 const& e,
    glm::dvec3 const& n, glm::dvec3 const& r, double scale, double& h1, double& h2) {
  // Cartesian coordinates without height
  glm::dvec3 p1 =
      glm::normalize(mMiddlePoint + mdist * edge.first.mX * e + mdist * edge.first.mY * n) * r[0];
  glm::dvec3 p2 =
      glm::normalize(mMiddlePoint + mdist * edge.second.mX * e + mdist * edge.second.mY * n) * r[0];

  // LongLat coordinates
  glm::dvec3 l1 = cs::utils::convert::toLngLatHeight(p1, r[0], r[0]);
  glm::dvec3 l2 = cs::utils::convert::toLngLatHeight(p2, r[0], r[0]);

  // Heights of the points
  h1 = mSolarSystem->pActiveBody.get()->getHeight(l1.xy());
  h2 = mSolarSystem->pActiveBody.get()->getHeight(l2.xy());

  // Cartesian coordinates with height
  glm::dvec3 r1 = cs::utils::convert::toCartesian(l1, r[0], r[0], h1 * scale);
  glm::dvec3 r2 = cs::utils::convert::toCartesian(l2, r[0], r[0], h2 * scale);

  // Emplaces back points in Cartesian (on planet surface) for display
  mTriangulation.emplace_back(r1);
  mTriangulation.emplace_back(r2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::refineMesh(Edge2 const& edge, double mdist, glm::dvec3 const& e,
    glm::dvec3 const& n, glm::dvec3 const& r, int count, double h1, double h2, bool& fine) {
  // Length of the edge in meters (on the voronoi plane!)
  double length = std::sqrt(std::pow(mdist * edge.first.mX - mdist * edge.second.mX, 2) +
                            std::pow(mdist * edge.first.mY - mdist * edge.second.mY, 2));

  // Middle point of the edge on voronoi plane
  glm::dvec2 avgPoint2 =
      glm::dvec2((edge.first.mX + edge.second.mX) / 2, (edge.first.mY + edge.second.mY) / 2);
  // Middle point on planet´s surface
  glm::dvec3 pAvg =
      glm::normalize(mMiddlePoint + mdist * avgPoint2.x * e + mdist * avgPoint2.y * n) * r[0];

  // Heights of the points over see level
  double hAvg = mSolarSystem->pActiveBody.get()->getHeight(
      cs::utils::convert::toLngLatHeight(pAvg, r[0], r[0]).xy());

  // Checks height of the middle point
  if ((hAvg / ((h1 + h2) / 2) > mHeightDiff) || (((h1 + h2) / 2) / hAvg > mHeightDiff)) {
    mCornersFine[count].emplace_back(avgPoint2.x, avgPoint2.y, mCornersFine[count].size());
    fine = 0;
  }
  // Checks height of other points between the two Sites
  else {
    // Trisecting points, etc.
    for (int j = 3; j < 6; j++) {
      // Checks "level" only if no points were emplaced back form the previous cycle
      if (fine) {
        for (int i = 1; i < j; i++) {
          // Point
          glm::dvec2 avgPoint3 = glm::dvec2((i * edge.first.mX + (j - i) * edge.second.mX) / j,
              (i * edge.first.mY + (j - i) * edge.second.mY) / j);
          // Cartesian coordinate of the point
          glm::dvec3 cAvg3 =
              glm::normalize(mMiddlePoint + mdist * avgPoint3.x * e + mdist * avgPoint3.y * n) *
              r[0];
          // Height of the point
          double heAvg3 = mSolarSystem->pActiveBody.get()->getHeight(
              cs::utils::convert::toLngLatHeight(cAvg3, r[0], r[0]).xy());

          if ((heAvg3 / ((i * h1 + (j - i) * h2) / j) > mHeightDiff) ||
              (((i * h1 + (j - i) * h2) / j) / heAvg3 > mHeightDiff)) {
            mCornersFine[count].emplace_back(avgPoint3.x, avgPoint3.y, mCornersFine[count].size());
            fine = 0;
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::calculateAreaAndVolume(std::vector<Triangle> const& triangles, double mdist,
    glm::dvec3 const& e, glm::dvec3 const& n, glm::dvec3 const& r, double& area, double& pvol,
    double& nvol) {
  // Counts area and volume in every triangle
  for (int it = 0; it < triangles.size(); it++) {
    // ------------------------------------------ AREA ------------------------------------------
    Site si1(0, 0, 0);
    Site si2(0, 0, 0);
    Site si3(0, 0, 0);
    std::tie(si1, si2, si3) = triangles[it];

    // Cartesian coordinates without height
    glm::dvec3 p1 = glm::normalize(mMiddlePoint + mdist * si1.mX * e + mdist * si1.mY * n) * r[0];
    glm::dvec3 p2 = glm::normalize(mMiddlePoint + mdist * si2.mX * e + mdist * si2.mY * n) * r[0];
    glm::dvec3 p3 = glm::normalize(mMiddlePoint + mdist * si3.mX * e + mdist * si3.mY * n) * r[0];

    // LongLat coordinates
    glm::dvec3 l1 = cs::utils::convert::toLngLatHeight(p1, r[0], r[0]);
    glm::dvec3 l2 = cs::utils::convert::toLngLatHeight(p2, r[0], r[0]);
    glm::dvec3 l3 = cs::utils::convert::toLngLatHeight(p3, r[0], r[0]);

    // Heights of the points
    double h1 = mSolarSystem->pActiveBody.get()->getHeight(l1.xy());
    double h2 = mSolarSystem->pActiveBody.get()->getHeight(l2.xy());
    double h3 = mSolarSystem->pActiveBody.get()->getHeight(l3.xy());

    // Cartesian coordinates with height
    glm::dvec3 r1 = cs::utils::convert::toCartesian(l1, r[0], r[0], h1);
    glm::dvec3 r2 = cs::utils::convert::toCartesian(l2, r[0], r[0], h2);
    glm::dvec3 r3 = cs::utils::convert::toCartesian(l3, r[0], r[0], h3);

    // Area is the half of the cross product of two edges in triangle
    area += glm::length(glm::cross(r2 - r1, r3 - r1)) / 2;

    // ----------------------------------------- Volume -----------------------------------------
    // Points on the least squares plane (from DipAndStrikeTool)
    glm::dvec3 pl1 = glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, p1) * p1;
    glm::dvec3 pl2 = glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, p2) * p2;
    glm::dvec3 pl3 = glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, p3) * p3;

    // Heights over the least squares plane
    double hl1 = h1 - (glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, p1) - 1) *
                          glm::length(mMiddlePoint2);
    double hl2 = h2 - (glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, p2) - 1) *
                          glm::length(mMiddlePoint2);
    double hl3 = h3 - (glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, p3) - 1) *
                          glm::length(mMiddlePoint2);

    double baseArea1 = 0;
    double baseArea2 = 0;
    double volume    = 0;

    // If all of the triangle's corners' are on the same size of the least square plane
    if (((hl1 > 0) && (hl2 > 0) && (hl3 > 0)) || ((hl1 < 0) && (hl2 < 0) && (hl3 < 0))) {
      // Base area: planet surface without heights / least square plane
      baseArea1 = glm::length(glm::cross(p2 - p1, p3 - p1)) / 2;
      // Volume is the multiplication of surface and average height over the plane
      volume = baseArea1 * ((hl1 + hl2 + hl3) / 3);

      // Counts positive and negative volumes separately
      if (volume > 0) {
        pvol += volume;
      } else {
        nvol += volume;
      }
    }
    // If not: find intersection points with the least square plane
    // If 2 intersection points are found:
    // Split the triangle into a smaller triangle and a quadrilateral
    else {
      glm::dvec3 pM1    = glm::dvec3(0.0);
      glm::dvec3 pM2    = glm::dvec3(0.0);
      glm::dvec3 pM3    = glm::dvec3(0.0);
      glm::dvec3 pM     = glm::dvec3(0.0);
      glm::dvec3 pMOld  = glm::dvec3(0.0);
      glm::dvec3 lM     = glm::dvec3(0.0);
      double     hM     = 0;
      double     hlM    = 0;
      double     hlMOld = 0;
      bool       b1     = 0;
      bool       b2     = 0;
      bool       b3     = 0;

      // Resolution of edge sampling
      int    res  = 32;
      double frac = 0;

      // If the two points are on the other side of the plane
      if ((hl1 > 0) != (hl2 > 0)) {
        // Samples of edge to find the intersection point between edge and plane
        // (Does not consider multiple intersection points (f.eg.: mountains in triangle)
        // They have been mostly eliminated with triangulation
        for (int i = 0; i < res; i++) {
          frac = (double)i / res;
          // Point coordinate without height
          pM = glm::normalize((1 - frac) * p1 + frac * p2) * r[0];

          // LongLat
          lM = cs::utils::convert::toLngLatHeight(pM, r[0], r[0]);
          // Height
          hM = mSolarSystem->pActiveBody.get()->getHeight(lM.xy());
          // Height over least square plane
          hlM = hM - (glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, pM) - 1) *
                         glm::length(mMiddlePoint2);
          // If intersection is between this and previous sample point
          // Interpolate between this and previous point and end loop
          if ((hl1 > 0) != (hlM > 0)) {
            pM1 = pMOld - (pM - pMOld) * hlMOld / (hlM - hlMOld);
            // To quit loop
            i  = res;
            b1 = 1;
          } else {
            // Save values for the next cycle
            pMOld  = pM;
            hlMOld = hlM;
          }
        }
      }

      if ((hl1 > 0) != (hl3 > 0)) {
        for (int i = 0; i < res; i++) {
          frac = (double)i / res;
          pM   = glm::normalize((1 - frac) * p1 + frac * p3) * r[0];
          lM   = cs::utils::convert::toLngLatHeight(pM, r[0], r[0]);
          hM   = mSolarSystem->pActiveBody.get()->getHeight(lM.xy());
          hlM  = hM - (glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, pM) - 1) *
                         glm::length(mMiddlePoint2);
          if ((hl1 > 0) != (hlM > 0)) {
            pM2 = pMOld - (pM - pMOld) * hlMOld / (hlM - hlMOld);
            i   = res;
            b2  = 1;
          } else {
            pMOld  = pM;
            hlMOld = hlM;
          }
        }
      }

      if ((hl2 > 0) != (hl3 > 0)) {
        for (int i = 0; i < res; i++) {
          frac = (double)i / res;
          pM   = glm::normalize((1 - frac) * p2 + frac * p3) * r[0];
          lM   = cs::utils::convert::toLngLatHeight(pM, r[0], r[0]);
          hM   = mSolarSystem->pActiveBody.get()->getHeight(lM.xy());
          hlM  = hM - (glm::dot(mNormal2, mMiddlePoint2) / glm::dot(mNormal2, pM) - 1) *
                         glm::length(mMiddlePoint2);
          if ((hl2 > 0) != (hlM > 0)) {
            pM3 = pMOld - (pM - pMOld) * hlMOld / (hlM - hlMOld);
            i   = res;
            b3  = 1;
          } else {
            pMOld  = pM;
            hlMOld = hlM;
          }
        }
      }

      // If the first two edges have an intersection point with the plane
      if ((b1 == 1) && (b2 == 1) && (b3 == 0)) {
        // Area of the smaller triangle
        baseArea1 = glm::length(glm::cross(pM1 - p1, pM2 - p1)) / 2;
        // Area of the quadrilateral
        baseArea2 = glm::length(glm::cross(pM1 - p3, pM2 - p3)) / 2 +
                    glm::length(glm::cross(pM1 - p2, p3 - p2)) / 2;

        // Decide the sign of the volume based on the
        // height of the corner in the small triangle
        // (Heights of intersections are considered to be 0)
        if (hl1 > 0) {
          // Add volumes
          pvol += baseArea1 * hl1 / 3;
          nvol += baseArea2 * ((hl2 + hl3) / 4);
        } else {
          nvol += baseArea1 * hl1 / 3;
          pvol += baseArea2 * ((hl2 + hl3) / 4);
        }
      } else if ((b1 == 1) && (b2 == 0) && (b3 == 1)) {
        baseArea1 = glm::length(glm::cross(pM1 - p2, pM3 - p2)) / 2;
        baseArea2 = glm::length(glm::cross(pM1 - p1, pM3 - p1)) / 2 +
                    glm::length(glm::cross(pM3 - p3, p1 - p3)) / 2;

        if (hl2 > 0) {
          pvol += baseArea1 * hl2 / 3;
          nvol += baseArea2 * ((hl1 + hl3) / 4);
        } else {
          nvol += baseArea1 * hl2 / 3;
          pvol += baseArea2 * ((hl1 + hl3) / 4);
        }
      } else if ((b1 == 0) && (b2 == 1) && (b3 == 1)) {
        baseArea1 = glm::length(glm::cross(pM3 - p3, pM2 - p3)) / 2;
        baseArea2 = glm::length(glm::cross(pM2 - p2, pM3 - p2)) / 2 +
                    glm::length(glm::cross(pM2 - p1, p2 - p1)) / 2;

        if (hl3 > 0) {
          pvol += baseArea1 * hl3 / 3;
          nvol += baseArea2 * ((hl1 + hl2) / 4);
        } else {
          nvol += baseArea1 * hl3 / 3;
          pvol += baseArea2 * ((hl1 + hl2) / 4);
        }
      }
      // If more or fewer as 2 intersection points are found
      // Calculate volume without spliting the triangle (as in the first case)
      else {
        baseArea1 = glm::length(glm::cross(p2 - p1, p3 - p1)) / 2;
        volume    = baseArea1 * ((hl1 + hl2 + hl3) / 3);

        if (volume > 0) {
          pvol += volume;
        } else {
          nvol += volume;
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::onPointMoved() {
  // Return if point is not on planet
  for (auto const& mark : mPoints) {
    glm::dvec3 vec = mark->getAnchor()->getAnchorPosition();
    if ((glm::length(vec) == 0) || std::isnan(vec.x) || std::isnan(vec.y) || std::isnan(vec.z))
      return;
  }

  updateLineVertices();
  updateCalculation();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::onPointAdded() {
  // Return if point is not on planet
  for (auto const& mark : mPoints) {
    glm::dvec3 vec = mark->getAnchor()->getAnchorPosition();
    if ((glm::length(vec) == 0) || std::isnan(vec.x) || std::isnan(vec.y) || std::isnan(vec.z))
      return;
  }

  updateLineVertices();
  updateCalculation();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::onPointRemoved(int index) {
  // Don't allow to become only one line
  if (mPoints.size() == 2)
    pAddPointMode = true;

  updateLineVertices();
  updateCalculation();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::updateLineVertices() {
  if (mPoints.size() == 0)
    return;

  // Fills the vertex buffer with sampled data
  mSampledPositions.clear();

  // Middle point of cs::core::tools::DeletableMarks
  glm::dvec3 averagePosition(0.0);
  for (auto const& mark : mPoints)
    averagePosition += mark->getAnchor()->getAnchorPosition() / (double)mPoints.size();

  auto radii = mSolarSystem->getRadii(mGuiAnchor->getCenterName());

  auto   lngLatHeight = cs::utils::convert::toLngLatHeight(averagePosition, radii[0], radii[0]);
  double height = mSolarSystem->getBody(mGuiAnchor->getCenterName())->getHeight(lngLatHeight.xy());
  height *= mGraphicsEngine->pHeightScale.get();
  auto center = cs::utils::convert::toCartesian(lngLatHeight.xy(), radii[0], radii[0], height);
  mGuiAnchor->setAnchorPosition(center);

  // This seems to be the first time the tool is moved, so we have to store the distance to the
  // observer so that we can scale the tool later based on the observer's position.
  if (mOriginalDistance < 0) {
    double simulationTime(mTimeControl->pSimulationTime.get());
    mOriginalDistance = mSolarSystem->getObserver().getAnchorScale() *
                        glm::length(mSolarSystem->getObserver().getRelativePosition(
                            mTimeControl->pSimulationTime.get(), *mGuiAnchor));
  }

  auto lastMark = mPoints.begin();
  auto currMark = ++mPoints.begin();

  // minLng,maxLng,minLat,maxLat
  glm::dvec4 boundingBox = glm::dvec4(0.0);

  while (currMark != mPoints.end()) {
    // Generates X points for each line segment
    for (int vertex_id = 0; vertex_id < NUM_SAMPLES; vertex_id++) {
      glm::dvec4 pos = getInterpolatedPosBetweenTwoMarks(
          **lastMark, **currMark, (vertex_id / (double)NUM_SAMPLES));
      mSampledPositions.push_back(pos.xyz());
    }

    // Saves the point coordinates to vector (normalized by the radius)
    glm::dvec2 lngLat0 = (*lastMark)->pLngLat.get();
    glm::dvec2 lngLat1 = (*currMark)->pLngLat.get();

    // Creates BoundingBox
    if (boundingBox == glm::dvec4(0.0)) {
      boundingBox.x = std::min(lngLat0.x, lngLat1.x);
      boundingBox.y = std::max(lngLat0.x, lngLat1.x);
      boundingBox.z = std::min(lngLat0.y, lngLat1.y);
      boundingBox.w = std::max(lngLat0.y, lngLat1.y);
    } else {
      boundingBox.x = std::min(std::min(lngLat0.x, lngLat1.x), boundingBox.x);
      boundingBox.y = std::max(std::max(lngLat0.x, lngLat1.x), boundingBox.y);
      boundingBox.z = std::min(std::min(lngLat0.y, lngLat1.y), boundingBox.z);
      boundingBox.w = std::max(std::max(lngLat0.y, lngLat1.y), boundingBox.w);
    }

    lastMark = currMark;
    ++currMark;
  }

  mBoundingBox = boundingBox;

  // Last line to draw a polygon instead of a path
  currMark = mPoints.begin();
  for (int vertex_id = 0; vertex_id < NUM_SAMPLES; vertex_id++) {
    glm::dvec4 pos = getInterpolatedPosBetweenTwoMarks(
        **lastMark, **currMark, (vertex_id / (double)NUM_SAMPLES));
    mSampledPositions.push_back(pos.xyz());
  }

  // Variables for display on tool
  double minLng = cs::utils::convert::toDegrees(mBoundingBox.x);
  double maxLng = cs::utils::convert::toDegrees(mBoundingBox.y);
  double minLat = cs::utils::convert::toDegrees(mBoundingBox.z);
  double maxLat = cs::utils::convert::toDegrees(mBoundingBox.w);

  mGuiItem->callJavascript("setBoundaryPosition", minLng, minLat, maxLng, maxLat);

  mIndexCount = mSampledPositions.size();

  // Upload new data
  mVBO.Bind(GL_ARRAY_BUFFER);
  mVBO.BufferData(mSampledPositions.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
  mVBO.Release();

  mVAO.EnableAttributeArray(0);
  mVAO.SpecifyAttributeArrayFloat(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0, &mVBO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Creates a new plane normal to the middle of the polygon and projects the polygon points to
// this plane and generates a Delaunay-mesh on this plane and calculates the area and volume
// of the original polygon using this mesh
void PolygonTool::updateCalculation() {
  // Returns if no triangle can be created
  if (mPoints.size() < 3)
    return;

  mCorners.clear();
  mCornersFine.clear();
  mTriangulation.clear();

  double h_scale = mGraphicsEngine->pHeightScale.get();
  auto   radii   = mSolarSystem->getRadii(mGuiAnchor->getCenterName());

  // Middle point of cs::core::tools::DeletableMarks
  glm::dvec3 averagePosition(0.0);
  for (auto const& mark : mPoints)
    averagePosition += mark->getAnchor()->getAnchorPosition() / (double)mPoints.size();

  // Corrected average position (works for every height scale)
  glm::dvec3 averagePositionNorm(0.0);
  for (auto const& mark : mPoints) {
    glm::dvec3 pos = glm::normalize(mark->getAnchor()->getAnchorPosition()) * radii[0];
    // LongLat coordinate
    glm::dvec3 l = cs::utils::convert::toLngLatHeight(pos, radii[0], radii[0]);
    // Height of the point
    double h = mSolarSystem->pActiveBody.get()->getHeight(l.xy());
    // Cartesian coordinate with height
    glm::dvec3 posNorm = cs::utils::convert::toCartesian(l, radii[0], radii[0], h);

    averagePositionNorm += posNorm / (double)mPoints.size();
  }

  // Longest distance to average position
  double maxDist = 0;
  for (auto const& mark : mPoints) {
    double dist = glm::length(averagePosition - mark->getAnchor()->getAnchorPosition());
    if (dist > maxDist)
      maxDist = dist;
  }

  // If polygon is to big (disable area calculation and mesh generation)
  // Voronoi implementation is designed for a maximal area of one hemisphere
  if (maxDist > radii[0]) {
    mGuiItem->callJavascript("setArea", 0);
    mGuiItem->callJavascript("setVolume", 0, 0);
    mShowMesh = 0;
    return;
  }
  // Converts maxDist to Voronoi plane (approx.)
  // 1.2 is for safety -> makes sure, that the voronoi coordinates are under 1
  maxDist = 1.2 * maxDist * radii[0] / (std::sqrt(std::pow(radii[0], 2) - std::pow(maxDist, 2)));

  // Planes normal is perpendicular to the average position
  mNormal      = glm::normalize(averagePosition);
  mMiddlePoint = mNormal * radii[0];
  // Coordinate system of the plane
  glm::dvec3 east, north = glm::dvec3(0.0);

  if (mNormal.y != 0) {
    // Normal and north is perpendicular -> dot product is 0
    double yNorth = (std::pow(mNormal.x, 2) + std::pow(mNormal.z, 2)) / mNormal.y;
    north         = glm::normalize(glm::dvec3(-mNormal.x, yNorth, -mNormal.z));
    // Changes south to north on the southern hemisphere
    if (yNorth < 0)
      north = glm::normalize(glm::dvec3(mNormal.x, -yNorth, mNormal.z));
  } else {
    // If plane normal is perpendicular to y axes, north is y
    north = glm::dvec3(0, 1, 0);
  }

  east = -glm::cross(mNormal, north);

  // Calculates plane for volume calculation
  // From DipStrikeTool
  // Based on http://stackoverflow.com/questions/1400213/3d-least-squares-plane
  glm::dmat3 mat(0);
  glm::dvec3 vec(0);

  mNormal2 = glm::normalize(averagePositionNorm);
  mOffset  = 0.f;

  for (auto const& p : mPoints) {
    glm::dvec3 pos     = glm::normalize(p->getAnchor()->getAnchorPosition()) * radii[0];
    glm::dvec3 l       = cs::utils::convert::toLngLatHeight(pos, radii[0], radii[0]);
    double     h       = mSolarSystem->pActiveBody.get()->getHeight(l.xy());
    glm::dvec3 posNorm = cs::utils::convert::toCartesian(l, radii[0], radii[0], h);

    glm::dvec3 realtivePosition = posNorm - averagePositionNorm;

    mat[0][0] += realtivePosition.x * realtivePosition.x;
    mat[1][0] += realtivePosition.x * realtivePosition.y;
    mat[2][0] += realtivePosition.x;
    mat[0][1] += realtivePosition.x * realtivePosition.y;
    mat[1][1] += realtivePosition.y * realtivePosition.y;
    mat[2][1] += realtivePosition.y;
    mat[0][2] += realtivePosition.x;
    mat[1][2] += realtivePosition.y;
    mat[2][2] += 1;

    vec[0] += realtivePosition.x * realtivePosition.z;
    vec[1] += realtivePosition.y * realtivePosition.z;
    vec[2] += realtivePosition.z;
  }

  glm::dvec3 solution = glm::inverse(mat) * vec;
  mNormal2            = glm::normalize(glm::dvec3(-solution.x, -solution.y, 1.f));

  if (glm::dot(mNormal, mNormal2) < 0)
    mNormal2 = -mNormal2;

  mOffset       = solution.z;
  mMiddlePoint2 = averagePositionNorm + mNormal2 * radii[0] * mOffset;

  // Projects points to Voronoi plane and calculates their position in the new coordinate system
  int        addr = 0;
  glm::dvec3 lastPosition;

  for (auto const& mark : mPoints) {
    glm::dvec3 currentPosition = mark->getAnchor()->getAnchorPosition();

    // Filters out double points
    if (currentPosition != lastPosition) {
      // Corrects distance from origin (average point is inside of the sphere)
      double     k   = glm::dot(mNormal, mMiddlePoint) / glm::dot(mNormal, currentPosition);
      glm::dvec3 pos = k * currentPosition;

      // Coordinates on the plane
      double x = glm::dot(east, pos - mMiddlePoint);
      double y = glm::dot(north, pos - mMiddlePoint);

      // Avoids crashing when moving to the other side of the planet
      if ((std::isnan(x / maxDist)) || (std::isnan(y / maxDist)))
        return;

      // Saves coordinates normalized with maxDist
      mCorners.emplace_back(x / maxDist, y / maxDist, addr);

      lastPosition = currentPosition;
      addr++;
    }
  }

  // Vector to save triangles from voronoi generator
  std::vector<Triangle> triangles;

  // Creates Delaunay-mesh of the original polygon
  createMesh(triangles);

  bool   fine          = 0;
  int    attempt       = 0;
  double area          = 0;
  double negVolume     = 0;
  double posVolume     = 0;
  int    triangleCount = 0;
  int    pointCount    = 0;

  // Counts points of the original Delaunay-mesh
  for (auto const& vect : mCornersFine) {
    pointCount += vect.size();
  }

  // Refines triangulation until it is necessary or mMaxAttempt or mMaxPoints
  while ((!fine) && (attempt < mMaxAttempt) && (pointCount < mMaxPoints)) {
    attempt++;
    fine = 1;

    area          = 0;
    negVolume     = 0;
    posVolume     = 0;
    triangleCount = 0;
    pointCount    = 0;

    mTriangulation.clear();

    // Goes through every triangle of original Delaunay-mesh separately
    for (auto const& t : triangles) {

      Site s1(0, 0, 0);
      Site s2(0, 0, 0);
      Site s3(0, 0, 0);
      std::tie(s1, s2, s3) = t;

      // Middle point of the triangle
      glm::dvec2 avgPoint = glm::dvec2((s1.mX + s2.mX + s3.mX) / 3, (s1.mY + s2.mY + s3.mY) / 3);

      // Checks, if middle point is is the polygon
      if (checkPoint(avgPoint)) {
        if (attempt == 1) {
          // Emplaces back the 3 corners of the triangle
          std::vector<Site> corners;
          corners.emplace_back(s1.mX, s1.mY, 0);
          corners.emplace_back(s2.mX, s2.mY, 1);
          corners.emplace_back(s3.mX, s3.mY, 2);

          mCornersFine.emplace_back(corners);
        }

        // Checks sleekness of triangles in Delaunay-mesh and refines them, if necessary
        bool refine = checkSleekness(triangleCount);

        // Voronoi inside the original triangles - to refine triangle angles
        VoronoiGenerator voronoiRefine;
        voronoiRefine.parse(mCornersFine[triangleCount]);

        // No need for checkPoint, all of the edges are inside the triangle and the polygon
        for (auto const& s : voronoiRefine.getTriangulation()) {
          double h1, h2;

          // Calculates mesh coordinates on planet's surface and saves these coordinates for display
          displayMesh(s, maxDist, east, north, radii, h_scale, h1, h2);

          // If not too many points are addded in checkSleekness and it is not the the last attempt
          // than refines the mesh based on edge length and height differences
          if ((!refine) && (pointCount < mMaxPoints) && (attempt < mMaxAttempt))
            refineMesh(s, maxDist, east, north, radii, triangleCount, h1, h2, fine);
        }

        std::vector<Triangle> trianglesRefined = voronoiRefine.getTriangles();

        // Calculates area and volume
        calculateAreaAndVolume(
            trianglesRefined, maxDist, east, north, radii, area, posVolume, negVolume);

        pointCount += mCornersFine[triangleCount].size();
        triangleCount++;
      } // if (checkPoint(avgPoint))
    }   // for (auto const& t : triangles)

    // Displays values
    if (!std::isnan(area)) {
      mGuiItem->callJavascript("setArea", area);
    } else {
      mGuiItem->callJavascript("setArea", 0);
    }

    if ((!std::isnan(posVolume)) && (!std::isnan(negVolume))) {
      mGuiItem->callJavascript("setVolume", posVolume, negVolume);
    } else if (!std::isnan(negVolume)) {
      mGuiItem->callJavascript("setVolume", 0, negVolume);
    } else if (!std::isnan(posVolume)) {
      mGuiItem->callJavascript("setVolume", posVolume, 0);
    } else {
      mGuiItem->callJavascript("setVolume", 0, 0);
    }
  } // while ((!fine) && (attempt < mMaxAttempt) && (pointCount < mMaxPoints))

  mIndexCount2 = mTriangulation.size();

  // Uploads new data
  mVBO2.Bind(GL_ARRAY_BUFFER);
  mVBO2.BufferData(mTriangulation.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
  mVBO2.Release();

  mVAO2.EnableAttributeArray(0);
  mVAO2.SpecifyAttributeArrayFloat(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0, &mVBO2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PolygonTool::update() {
  MultiPointTool::update();

  double simulationTime(mTimeControl->pSimulationTime.get());

  cs::core::SolarSystem::scaleRelativeToObserver(*mGuiAnchor, mSolarSystem->getObserver(),
      simulationTime, mOriginalDistance, mGraphicsEngine->pWidgetScale.get());
  cs::core::SolarSystem::turnToObserver(
      *mGuiAnchor, mSolarSystem->getObserver(), simulationTime, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool PolygonTool::Do() {
  // Transforms all high precision sample points to observer centric low precision coordinates
  std::vector<glm::vec3> vRelativePositions(mIndexCount);
  // For Delaunay
  std::vector<glm::vec3> vRelativePositions2(mIndexCount2);

  auto        time     = mTimeControl->pSimulationTime.get();
  auto const& observer = mSolarSystem->getObserver();

  cs::scene::CelestialAnchor centerAnchor(mGuiAnchor->getCenterName(), mGuiAnchor->getFrameName());
  auto                       mat = observer.getRelativeTransform(time, centerAnchor);

  for (int i(0); i < mIndexCount; ++i) {
    vRelativePositions[i] = (mat * glm::dvec4(mSampledPositions[i], 1.0)).xyz();
  }
  // For Delaunay
  for (int i(0); i < mIndexCount2; ++i) {
    vRelativePositions2[i] = (mat * glm::dvec4(mTriangulation[i], 1.0)).xyz();
  }

  // Uploads the new points to the GPU
  mVBO.Bind(GL_ARRAY_BUFFER);
  mVBO.BufferSubData(0, vRelativePositions.size() * sizeof(glm::vec3), vRelativePositions.data());
  mVBO.Release();

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT);

  // Enables alpha blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Enables and configures line rendering
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glLineWidth(5);

  GLfloat glMatMV[16], glMatP[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, &glMatMV[0]);
  glGetFloatv(GL_PROJECTION_MATRIX, &glMatP[0]);

  mShader.Bind();
  mVAO.Bind();
  glUniformMatrix4fv(mShader.GetUniformLocation("uMatModelView"), 1, GL_FALSE, glMatMV);
  glUniformMatrix4fv(mShader.GetUniformLocation("uMatProjection"), 1, GL_FALSE, glMatP);

  mShader.SetUniform(
      mShader.GetUniformLocation("uFarClip"), cs::utils::getCurrentFarClipDistance());

  mShader.SetUniform(mShader.GetUniformLocation("uColor"), 1.f, 1.f, 1.f, 1.f);

  // Draws the linestrip
  glDrawArrays(GL_LINE_STRIP, 0, mIndexCount);
  mVAO.Release();

  // For Delaunay
  if (mShowMesh) {
    mVBO2.Bind(GL_ARRAY_BUFFER);
    mVBO2.BufferSubData(
        0, vRelativePositions2.size() * sizeof(glm::vec3), vRelativePositions2.data());
    mVBO2.Release();

    glLineWidth(2);

    mVAO2.Bind();

    mShader.SetUniform(mShader.GetUniformLocation("uColor"), 0.5f, 0.5f, 1.f, 0.8f);

    glDisable(GL_DEPTH_TEST);

    // Draws the linestrip (Delaunay)
    glDrawArrays(GL_LINES, 0, mIndexCount2);
    mVAO2.Release();

    glEnable(GL_DEPTH_TEST);
  }

  mShader.Release();

  glPopAttrib();
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool PolygonTool::GetBoundingBox(VistaBoundingBox& bb) {
  float fMin[3] = {-0.1f, -0.1f, -0.1f};
  float fMax[3] = {0.1f, 0.1f, 0.1f};

  bb.SetBounds(fMin, fMax);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::measurementtools
