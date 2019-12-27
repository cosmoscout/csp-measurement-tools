////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_MEASUREMENT_TOOLS_DIP_STRIKE_HPP
#define CSP_MEASUREMENT_TOOLS_DIP_STRIKE_HPP

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

/// The dip and strike tool is used to measure the steepness and orientation of slopes. It uses a
/// set of points on the surface to generate a plane that has the lowest sum of squared distances to
/// all points.
/// The dip (steepness) is given in degrees from 0° to 90° and the strike (orientation) is also
/// given in degrees, where at 0° the peak is in the east and at 90° the peak is in the north.
class DipStrikeTool : public IVistaOpenGLDraw, public cs::core::tools::MultiPointTool {
 public:
  DipStrikeTool(std::shared_ptr<cs::core::InputManager> const& pInputManager,
      std::shared_ptr<cs::core::SolarSystem> const&            pSolarSystem,
      std::shared_ptr<cs::core::GraphicsEngine> const&         graphicsEngine,
      std::shared_ptr<cs::core::TimeControl> const& pTimeControl, std::string const& sCenter,
      std::string const& sFrame);
  ~DipStrikeTool() override;

  /// Called from Tools class.
  void update() override;

  /// Inherited from IVistaOpenGLDraw.
  bool Do() override;
  bool GetBoundingBox(VistaBoundingBox& bb) override;

 private:
  void calculateDipAndStrike();

  /// Returns the interpolated position in cartesian coordinates the fourth component is height
  /// above the surface.
  glm::dvec4 getInterpolatedPosBetweenTwoMarks(cs::core::tools::DeletableMark const& pMark1,
      cs::core::tools::DeletableMark const& pMark2, double value);

 private:
  /// These are called by the base class MultiPointTool.
  void onPointMoved() override;
  void onPointAdded() override;
  void onPointRemoved(int index) override;

  VistaOpenGLNode*                                mGuiNode      = nullptr;
  VistaTransformNode*                             mGuiTransform = nullptr;
  std::shared_ptr<cs::scene::CelestialAnchorNode> mGuiAnchor    = nullptr;
  std::shared_ptr<cs::scene::CelestialAnchorNode> mPlaneAnchor  = nullptr;
  VistaOpenGLNode*                                mParent       = nullptr;

  std::unique_ptr<cs::gui::WorldSpaceGuiArea> mGuiArea;
  std::unique_ptr<cs::gui::GuiItem>           mGuiItem;
  std::unique_ptr<VistaVertexArrayObject>     mVAO;
  std::unique_ptr<VistaBufferObject>          mVBO;
  std::unique_ptr<VistaGLSLShader>            mShader;

  double    mOriginalDistance = -1.0, mSize;
  glm::vec3 mNormal = glm::vec3(0.0), mMip = glm::vec3(0.0);
  float     mOffset, mSizeFactor = 1.5f, mOpacity = 0.5;

  int mScaleConnection = -1;

  static const int         RESOLUTION;
  static const std::string SHADER_VERT;
  static const std::string SHADER_FRAG;
};

} // namespace csp::measurementtools

#endif // CSP_MEASUREMENT_TOOLS_DIP_STRIKE_HPP
