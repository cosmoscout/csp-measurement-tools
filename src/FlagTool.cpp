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
    std::shared_ptr<cs::core::TimeControl> const& pTimeControl, std::string const& sCenter,
    std::string const& sFrame)
    : Mark(pInputManager, pSolarSystem, graphicsEngine, pTimeControl, sCenter, sFrame)
    , mGuiArea(new cs::gui::WorldSpaceGuiArea(420, 400))
    , mGuiItem(new cs::gui::GuiItem("file://../share/resources/gui/flag.html")) {
  auto pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();

  mGuiTransform.reset(pSG->NewTransformNode(mAnchor.get()));
  mGuiTransform->Translate(0.5f - 7.5f / 500.f, 0.5f, 0.f);
  mGuiTransform->Scale(0.001f * mGuiArea->getWidth(), 0.001f * mGuiArea->getHeight(), 1.f);
  mGuiTransform->Rotate(VistaAxisAndAngle(VistaVector3D(0.0, 1.0, 0.0), -glm::pi<float>() / 2.f));
  mGuiArea->addItem(mGuiItem.get());
  mGuiArea->setUseLinearDepthBuffer(true);

  mGuiNode.reset(pSG->NewOpenGLNode(mGuiTransform.get(), mGuiArea.get()));
  mInputManager->registerSelectable(mGuiNode.get());

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mGuiNode.get(), static_cast<int>(cs::utils::DrawOrder::eTransparentItems));

  mGuiItem->setCanScroll(false);
  mGuiItem->waitForFinishedLoading();
  mGuiItem->registerCallback("deleteMe", "Call this to delete the tool.",
      std::function([this]() { pShouldDelete = true; }));
  mGuiItem->setCursorChangeCallback([](cs::gui::Cursor c) { cs::core::GuiManager::setCursor(c); });

  // update text -------------------------------------------------------------
  mTextConnection = pText.connectAndTouch(
      [this](std::string const& value) { mGuiItem->callJavascript("setText", value); });

  mGuiItem->registerCallback("onSetText",
      "This is called whenever the text input of the tool's name changes.",
      std::function(
          [this](std::string&& value) { pText.setWithEmitForAllButOne(value, mTextConnection); }));

  // update position ---------------------------------------------------------
  pLngLat.connect([this](glm::dvec2 const& lngLat) {
    auto body = mSolarSystem->getBody(mAnchor->getCenterName());
    if (body) {
      double h = body->getHeight(lngLat);
      mGuiItem->callJavascript("setPosition", cs::utils::convert::toDegrees(lngLat.x),
          cs::utils::convert::toDegrees(lngLat.y), h);
    }
  });

  // update minimized state --------------------------------------------------
  mDoubleClickConnection = mInputManager->sOnDoubleClick.connect([this]() {
    if (pHovered.get()) {
      pMinimized = !pMinimized.get();
    }
  });

  pMinimized.connect([this](bool val) { mGuiItem->callJavascript("setMinimized", val); });

  mGuiItem->registerCallback("minimizeMe", "Call this to minimize the flag.",
      std::function([this]() { pMinimized = true; }));
  mGuiItem->callJavascript("setActivePlanet", sCenter, sFrame);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FlagTool::~FlagTool() {
  auto pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
  pSG->GetRoot()->DisconnectChild(mGuiTransform.get());

  mInputManager->sOnDoubleClick.disconnect(mDoubleClickConnection);
  mInputManager->unregisterSelectable(mGuiNode.get());
  mGuiItem->unregisterCallback("minimizeMe");
  mGuiItem->unregisterCallback("deleteMe");
  mGuiItem->unregisterCallback("onSetText");
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
