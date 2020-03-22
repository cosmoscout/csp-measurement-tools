////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PathTool.hpp"

#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/InputManager.hpp"
#include "../../../src/cs-core/Settings.hpp"
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

namespace csp::measurementtools {

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string PathTool::SHADER_VERT = R"(
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

const std::string PathTool::SHADER_FRAG = R"(
#version 330

in vec4 vPosition;

uniform float uFarClip;

layout(location = 0) out vec4 oColor;

void main()
{
    oColor = vec4(1.0);
    gl_FragDepth = length(vPosition.xyz) / uFarClip;
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

PathTool::PathTool(std::shared_ptr<cs::core::InputManager> const& pInputManager,
    std::shared_ptr<cs::core::SolarSystem> const&                 pSolarSystem,
    std::shared_ptr<cs::core::Settings> const&                    settings,
    std::shared_ptr<cs::core::TimeControl> const& pTimeControl, std::string const& sCenter,
    std::string const& sFrame)
    : MultiPointTool(pInputManager, pSolarSystem, settings, pTimeControl, sCenter, sFrame)
    , mGuiArea(std::make_unique<cs::gui::WorldSpaceGuiArea>(760, 475))
    , mGuiItem(std::make_unique<cs::gui::GuiItem>("file://../share/resources/gui/path.html")) {

  // create the shader
  mShader.InitVertexShaderFromString(SHADER_VERT);
  mShader.InitFragmentShaderFromString(SHADER_FRAG);
  mShader.Link();

  // attach this as OpenGLNode to scenegraph's root (all line vertices
  // will be draw relative to the observer, therfore we do not want
  // any transformation)
  auto pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
  mPathOpenGLNode.reset(pSG->NewOpenGLNode(pSG->GetRoot(), this));

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mPathOpenGLNode.get(), static_cast<int>(cs::utils::DrawOrder::eOpaqueItems));

  // create a a CelestialAnchorNode for the user interface
  // it will be moved to the center of all points when a point is moved
  // and rotated in such a way, that it always faces the observer
  mGuiAnchor = std::make_shared<cs::scene::CelestialAnchorNode>(
      pSG->GetRoot(), pSG->GetNodeBridge(), "", sCenter, sFrame);
  mGuiAnchor->setAnchorScale(mSolarSystem->getObserver().getAnchorScale());
  mSolarSystem->registerAnchor(mGuiAnchor);

  // create the user interface
  mGuiTransform.reset(pSG->NewTransformNode(mGuiAnchor.get()));
  mGuiTransform->Translate(0.f, 0.9f, 0.f);
  mGuiTransform->Scale(0.001f * mGuiArea->getWidth(), 0.001f * mGuiArea->getHeight(), 1.f);
  mGuiTransform->Rotate(VistaAxisAndAngle(VistaVector3D(0.0, 1.0, 0.0), -glm::pi<float>() / 2.f));
  mGuiArea->addItem(mGuiItem.get());
  mGuiArea->setUseLinearDepthBuffer(true);
  mGuiOpenGLNode.reset(pSG->NewOpenGLNode(mGuiTransform.get(), mGuiArea.get()));

  mInputManager->registerSelectable(mGuiOpenGLNode.get());

  mGuiItem->setCanScroll(false);
  mGuiItem->waitForFinishedLoading();

  mGuiItem->registerCallback("deleteMe", "Call this to delete the tool.",
      std::function([this]() { pShouldDelete = true; }));

  mGuiItem->registerCallback("setAddPointMode", "Call this to enable creation of new points.",
      std::function([this](bool enable) {
        addPoint();
        pAddPointMode = enable;
      }));

  mGuiItem->setCursorChangeCallback([](cs::gui::Cursor c) { cs::core::GuiManager::setCursor(c); });

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mGuiAnchor.get(), static_cast<int>(cs::utils::DrawOrder::eTransparentItems));

  // whenever the height scale changes our vertex positions need to be updated
  mScaleConnection = mSettings->mGraphics.pHeightScale.connectAndTouch(
      [this](float const& h) { updateLineVertices(); });

  // add one point initially
  addPoint();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

PathTool::~PathTool() {
  mSettings->mGraphics.pHeightScale.disconnect(mScaleConnection);
  mGuiItem->unregisterCallback("deleteMe");
  mGuiItem->unregisterCallback("setAddPointMode");

  mInputManager->unregisterSelectable(mGuiOpenGLNode.get());
  mSolarSystem->unregisterAnchor(mGuiAnchor);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PathTool::setNumSamples(int const& numSamples) {
  mNumSamples = numSamples;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

glm::dvec4 PathTool::getInterpolatedPosBetweenTwoMarks(cs::core::tools::DeletableMark const& l0,
    cs::core::tools::DeletableMark const& l1, double value, double const& scale) {

  glm::dvec3 radii = cs::core::SolarSystem::getRadii(getCenterName());

  auto body = mSolarSystem->getBody(getCenterName());

  if (!body) {
    return glm::dvec4(0.0);
  }

  // Calculate the position for the new segment anchor
  double h0 = body->getHeight(l0.pLngLat.get()) * scale;
  double h1 = body->getHeight(l1.pLngLat.get()) * scale;

  // Get cartesian coordinates for interpolation
  glm::dvec3 p0 = cs::utils::convert::toCartesian(l0.pLngLat.get(), radii[0], radii[0], h0);
  glm::dvec3 p1 = cs::utils::convert::toCartesian(l1.pLngLat.get(), radii[0], radii[0], h1);
  glm::dvec3 interpolatedPos = p0 + (value * (p1 - p0));

  // Calc final position
  glm::dvec2 ll     = cs::utils::convert::toLngLatHeight(interpolatedPos, radii[0], radii[0]).xy();
  double     height = body->getHeight(ll) * scale;
  glm::dvec3 pos    = cs::utils::convert::toCartesian(ll, radii[0], radii[0], height);

  return glm::dvec4(pos, height);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PathTool::onPointMoved() {
  updateLineVertices();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PathTool::onPointAdded() {
  updateLineVertices();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PathTool::onPointRemoved(int index) {
  updateLineVertices();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PathTool::updateLineVertices() {
  if (mPoints.empty()) {
    return;
  }

  // Fill the vertex buffer with sampled data
  mSampledPositions.clear();

  auto body = mSolarSystem->getBody(getCenterName());

  glm::dvec3 averagePosition(0.0);
  for (auto const& mark : mPoints)
    averagePosition += mark->getAnchor()->getAnchorPosition() / (double)mPoints.size();

  double h_scale      = mSettings->mGraphics.pHeightScale.get();
  auto   radii        = cs::core::SolarSystem::getRadii(getCenterName());
  auto   lngLatHeight = cs::utils::convert::toLngLatHeight(averagePosition, radii[0], radii[0]);
  double height       = body ? body->getHeight(lngLatHeight.xy()) * h_scale : 0.0;
  auto   center = cs::utils::convert::toCartesian(lngLatHeight.xy(), radii[0], radii[0], height);

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

  std::stringstream json;
  std::string       jsonSeperator;
  double            distance = -1;
  glm::dvec3        lastPos(0.0);

  while (currMark != mPoints.end()) {
    // generate X points for each line segment
    for (int vertex_id = 0; vertex_id < mNumSamples; vertex_id++) {
      glm::dvec4 pos = getInterpolatedPosBetweenTwoMarks(
          **lastMark, **currMark, (vertex_id / (double)mNumSamples), h_scale);
      mSampledPositions.push_back(pos.xyz());

      // coordinate normalized by height scale; to count distance correctly
      glm::dvec4 posNorm = pos;
      if (h_scale != 1)
        posNorm = getInterpolatedPosBetweenTwoMarks(
            **lastMark, **currMark, (vertex_id / (double)mNumSamples), 1);

      if (distance < 0) {
        distance = 0;
      } else {
        distance += glm::length(posNorm.xyz() - lastPos);
      }

      json << jsonSeperator << "[" << distance << "," << pos.w / h_scale << "]";
      jsonSeperator = ",";

      lastPos = posNorm.xyz();
    }

    lastMark = currMark;
    ++currMark;
  }

  mGuiItem->callJavascript("setData", "[" + json.str() + "]");

  mIndexCount = mSampledPositions.size();

  // Upload new data
  mVBO.Bind(GL_ARRAY_BUFFER);
  mVBO.BufferData(mSampledPositions.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
  mVBO.Release();

  mVAO.EnableAttributeArray(0);
  mVAO.SpecifyAttributeArrayFloat(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0, &mVBO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void PathTool::update() {
  MultiPointTool::update();

  double simulationTime(mTimeControl->pSimulationTime.get());

  cs::core::SolarSystem::scaleRelativeToObserver(*mGuiAnchor, mSolarSystem->getObserver(),
      simulationTime, mOriginalDistance, mSettings->mGraphics.pWidgetScale.get());
  cs::core::SolarSystem::turnToObserver(
      *mGuiAnchor, mSolarSystem->getObserver(), simulationTime, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool PathTool::Do() {
  // transform all high precision sample points to observer centric
  // low precision coordinates
  std::vector<glm::vec3> vRelativePositions(mIndexCount);

  auto        time     = mTimeControl->pSimulationTime.get();
  auto const& observer = mSolarSystem->getObserver();

  cs::scene::CelestialAnchor centerAnchor(getCenterName(), getFrameName());
  auto                       mat = observer.getRelativeTransform(time, centerAnchor);

  for (int i(0); i < mIndexCount; ++i) {
    vRelativePositions[i] = (mat * glm::dvec4(mSampledPositions[i], 1.0)).xyz();
  }

  // upload the new points to the GPU
  mVBO.Bind(GL_ARRAY_BUFFER);
  mVBO.BufferSubData(0, vRelativePositions.size() * sizeof(glm::vec3), vRelativePositions.data());
  mVBO.Release();

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT);

  // enable alpha blending for smooth line
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // enable and configure line rendering
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

  // draw the linestrip
  glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)mIndexCount);
  mVAO.Release();
  mShader.Release();

  glPopAttrib();
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool PathTool::GetBoundingBox(VistaBoundingBox& bb) {
  float fMin[3] = {-0.1f, -0.1f, -0.1f};
  float fMax[3] = {0.1f, 0.1f, 0.1f};

  bb.SetBounds(fMin, fMax);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::measurementtools
