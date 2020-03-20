////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_MEASUREMENT_TOOLS_PATH_HPP
#define CSP_MEASUREMENT_TOOLS_PATH_HPP

#include "../../../src/cs-core/tools/MultiPointTool.hpp"

#include <VistaKernel/GraphicsManager/VistaOpenGLDraw.h>

#include <glm/glm.hpp>
#include <vector>

namespace cs::scene {
class CelestialAnchorNode;
}

namespace cs::gui {
class GuiItem;
class WorldSpaceGuiArea;
} // namespace cs::gui

class VistaBufferObject;
class VistaGLSLShader;
class VistaOpenGLNode;
class VistaVertexArrayObject;
class VistaTransformNode;

namespace csp::measurementtools {

/// The path tool is used to measure the distance and height along a path of lines.
class PathTool : public IVistaOpenGLDraw, public cs::core::tools::MultiPointTool {
 public:
  PathTool(std::shared_ptr<cs::core::InputManager> const& pInputManager,
      std::shared_ptr<cs::core::SolarSystem> const&       pSolarSystem,
      std::shared_ptr<cs::core::GraphicsEngine> const&    graphicsEngine,
      std::shared_ptr<cs::core::TimeControl> const& pTimeControl, std::string const& sCenter,
      std::string const& sFrame);
  ~PathTool() override;

  /// Called from Tools class.
  void update() override;

  /// Inherited from IVistaOpenGLDraw.
  bool Do() override;
  bool GetBoundingBox(VistaBoundingBox& bb) override;

  void setNumSamples(int const& numSamples);

 private:
  void updateLineVertices();

  /// Returns the interpolated position in cartesian coordinates. The fourth component is height
  /// above the surface.
  glm::dvec4 getInterpolatedPosBetweenTwoMarks(cs::core::tools::DeletableMark const& pMark1,
      cs::core::tools::DeletableMark const& pMark2, double value, double const& scale);

 private:
  /// These are called by the base class MultiPointTool.
  void onPointMoved() override;
  void onPointAdded() override;
  void onPointRemoved(int index) override;

  std::shared_ptr<cs::scene::CelestialAnchorNode> mGuiAnchor;

  std::unique_ptr<VistaTransformNode>         mGuiTransform;
  std::unique_ptr<VistaOpenGLNode>            mGuiOpenGLNode;
  std::unique_ptr<VistaOpenGLNode>            mPathOpenGLNode;
  std::unique_ptr<cs::gui::WorldSpaceGuiArea> mGuiArea;
  std::unique_ptr<cs::gui::GuiItem>           mGuiItem;

  VistaVertexArrayObject mVAO;
  VistaBufferObject      mVBO;
  VistaGLSLShader        mShader;

  double mOriginalDistance = -1.0;

  std::vector<glm::dvec3> mSampledPositions;
  size_t                  mIndexCount = 0;

  int mScaleConnection = -1;

  int mNumSamples = 256;

  static const std::string SHADER_VERT;
  static const std::string SHADER_FRAG;
};

} // namespace csp::measurementtools

#endif // CSP_MEASUREMENT_TOOLS_PATH_HPP
