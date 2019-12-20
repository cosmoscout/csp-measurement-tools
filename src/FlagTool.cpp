////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FlagTool.hpp"

#include "../../../src/cs-core/GraphicsEngine.hpp"
#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/InputManager.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-core/TimeControl.hpp"
#include "../../../src/cs-scene/CelestialAnchorNode.hpp"
#include "../../../src/cs-utils/convert.hpp"

#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>

namespace csp::measurementtools {

////////////////////////////////////////////////////////////////////////////////////////////////////

FlagTool::FlagTool(std::shared_ptr<cs::core::InputManager> const& pInputManager,
    std::shared_ptr<cs::core::SolarSystem> const&                 pSolarSystem,
    std::shared_ptr<cs::core::GraphicsEngine> const&              graphicsEngine,
    std::shared_ptr<cs::core::GuiManager> const&                  pGuiManager,
    std::shared_ptr<cs::core::TimeControl> const& pTimeControl, std::string const& sCenter,
    std::string const& sFrame)
    : Mark(pInputManager, pSolarSystem, graphicsEngine, pGuiManager, pTimeControl, sCenter, sFrame)
    , mGuiArea(new cs::gui::WorldSpaceGuiArea(420, 400))
    , mGuiItem(new cs::gui::GuiItem("file://../share/resources/gui/flag.html")) {
  auto pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();

  mGuiTransform = pSG->NewTransformNode(mAnchor.get());
  mGuiTransform->Translate(0.5f - 7.5f / 500.f, 0.5f, 0.f);
  mGuiTransform->Scale(0.001f * mGuiArea->getWidth(), 0.001f * mGuiArea->getHeight(), 1.f);
  mGuiTransform->Rotate(VistaAxisAndAngle(VistaVector3D(0.0, 1.0, 0.0), -glm::pi<float>() / 2.f));
  mGuiArea->addItem(mGuiItem.get());
  mGuiArea->setUseLinearDepthBuffer(true);

  mGuiNode = pSG->NewOpenGLNode(mGuiTransform, mGuiArea.get());
  mInputManager->registerSelectable(mGuiNode);

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mGuiNode, static_cast<int>(cs::utils::DrawOrder::eTransparentItems));

  mGuiItem->registerCallback("delete_me", [this]() { pShouldDelete = true; });
  mGuiItem->setCursorChangeCallback(
      [pGuiManager](cs::gui::Cursor c) { pGuiManager->setCursor(c); });

  // update text -------------------------------------------------------------
  mTextConnection = pText.onChange().connect(
      [this](std::string const& value) { mGuiItem->callJavascript("set_text", value); });

  mGuiItem->registerCallback<std::string>("on_set_text",
      [this](std::string const& value) { pText.setWithEmitForAllButOne(value, mTextConnection); });

  // update position ---------------------------------------------------------
  pLngLat.onChange().connect([this](glm::dvec2 const& lngLat) {
    auto body = mSolarSystem->getBody(mAnchor->getCenterName());
    if (body) {
      double h = body->getHeight(lngLat);
      mGuiItem->callJavascript("set_position", cs::utils::convert::toDegrees(lngLat.x),
          cs::utils::convert::toDegrees(lngLat.y), h);
    }
  });

  // update minimized state --------------------------------------------------
  mDoubleClickConnection = mInputManager->sOnDoubleClick.connect([this]() {
    if (pHovered.get()) {
      pMinimized = !pMinimized.get();
    }
  });

  pMinimized.onChange().connect(
      [this](bool val) { mGuiItem->callJavascript("set_minimized", val); });

  mGuiItem->registerCallback("minimize_me", [this]() { pMinimized = true; });
  mGuiItem->waitForFinishedLoading();
  mGuiItem->callJavascript("set_active_planet", sCenter, sFrame);

  pText.touch();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FlagTool::~FlagTool() {
  mInputManager->sOnDoubleClick.disconnect(mDoubleClickConnection);
  mInputManager->unregisterSelectable(mGuiNode);
  mGuiArea->removeItem(mGuiItem.get());

  delete mGuiNode;
  delete mGuiTransform;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FlagTool::update() {
  double simulationTime(mTimeControl->pSimulationTime.get());

  cs::core::SolarSystem::scaleRelativeToObserver(*mAnchor, mSolarSystem->getObserver(),
      simulationTime, mOriginalDistance, mGraphicsEngine->pWidgetScale.get());
  cs::core::SolarSystem::turnToObserver(
      *mAnchor, mSolarSystem->getObserver(), simulationTime, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::measurementtools
