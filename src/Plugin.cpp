////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Plugin.hpp"

#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/InputManager.hpp"
#include "../../../src/cs-scene/CelestialBody.hpp"
#include "../../../src/cs-utils/convert.hpp"

#include "DipStrikeTool.hpp"
#include "EllipseTool.hpp"
#include "PathTool.hpp"
#include "PolygonTool.hpp"

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase* create() {
  return new csp::measurementtools::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase* pluginBase) {
  delete pluginBase;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::measurementtools {

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json& j, Plugin::Settings::Polygon& o) {
  o.mHeightDiff = cs::core::parseProperty<float>("heightDiff", j);
  o.mMaxAttempt = cs::core::parseProperty<int>("maxAttempt", j);
  o.mMaxPoints  = cs::core::parseProperty<int>("maxPoints", j);
  o.mSleekness  = cs::core::parseProperty<int>("sleekness", j);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json& j, Plugin::Settings::Ellipse& o) {
  o.mNumSamples = cs::core::parseProperty<int>("numSamples", j);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json& j, Plugin::Settings::Path& o) {
  o.mNumSamples = cs::core::parseProperty<int>("numSamples", j);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json& j, Plugin::Settings& o) {
  cs::core::parseSection("csp-measurement-tools", [&] {
    o.mPolygon = cs::core::parseSection<Plugin::Settings::Polygon>("polygon", j);
    o.mEllipse = cs::core::parseSection<Plugin::Settings::Ellipse>("ellipse", j);
    o.mPath    = cs::core::parseSection<Plugin::Settings::Path>("path", j);
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {
  std::cout << "Loading: CosmoScout VR Plugin Measurement-Tools" << std::endl;

  mPluginSettings = mAllSettings->mPlugins.at("csp-measurement-tools");

  mGuiManager->addHtmlToGui(
      "measurement-tool", "../share/resources/gui/measurement-tool-template.html");

  mGuiManager->addPluginTabToSideBarFromHTML(
      "Measurement Tools", "multiline_chart", "../share/resources/gui/measurement-tools-tab.html");

  mGuiManager->addScriptToGuiFromJS("../share/resources/gui/js/csp-measurement-tools.js");
  mGuiManager->addCssToGui("css/csp-measurement-tools-sidebar.css");

  mGuiManager->getGui()->callJavascript(
      "CosmoScout.measurementTools.add", "Location Flag", "edit_location");
  mGuiManager->getGui()->callJavascript(
      "CosmoScout.measurementTools.add", "Landing Ellipse", "location_searching");
  mGuiManager->getGui()->callJavascript("CosmoScout.measurementTools.add", "Path", "timeline");
  mGuiManager->getGui()->callJavascript(
      "CosmoScout.measurementTools.add", "Dip & Strike", "clear_all");
  mGuiManager->getGui()->callJavascript(
      "CosmoScout.measurementTools.add", "Polygon", "crop_landscape");

  mGuiManager->getGui()->registerCallback<std::string>(
      "set_measurement_tool", [this](std::string const& name) { mNextTool = name; });

  mOnClickConnection = mInputManager->pButtons[0].onChange().connect([this](bool pressed) {
    if (!pressed && !mInputManager->pHoveredGuiNode.get()) {
      auto intersection = mInputManager->pHoveredObject.get().mObject;

      if (!intersection) {
        return;
      }

      auto body = std::dynamic_pointer_cast<cs::scene::CelestialBody>(intersection);

      if (!body) {
        return;
      }

      auto radii = body->getRadii();
      if (body) {
        if (mNextTool == "Location Flag") {
          auto tool     = std::make_shared<FlagTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->pLngLat = cs::utils::convert::toLngLatHeight(
              mInputManager->pHoveredObject.get().mPosition, radii[0], radii[0])
                              .xy();
          mTools.push_back(tool);
        } else if (mNextTool == "Landing Ellipse") {
          auto tool = std::make_shared<EllipseTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->getCenterHandle().pLngLat = cs::utils::convert::toLngLatHeight(
              mInputManager->pHoveredObject.get().mPosition, radii[0], radii[0])
                                                .xy();
          tool->setNumSamples(mPluginSettings.mEllipse.mNumSamples);
          mTools.push_back(tool);
        } else if (mNextTool == "Path") {
          auto tool = std::make_shared<PathTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->setNumSamples(mPluginSettings.mPath.mNumSamples);
          mTools.push_back(tool);
        } else if (mNextTool == "Dip & Strike") {
          auto tool = std::make_shared<DipStrikeTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mTimeControl, body->getCenterName(), body->getFrameName());
          mTools.push_back(tool);
        } else if (mNextTool == "Polygon") {
          auto tool = std::make_shared<PolygonTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->setHeightDiff(mPluginSettings.mPolygon.mHeightDiff);
          tool->setMaxAttempt(mPluginSettings.mPolygon.mMaxAttempt);
          tool->setMaxPoints(mPluginSettings.mPolygon.mMaxPoints);
          tool->setSleekness(mPluginSettings.mPolygon.mSleekness);
          mTools.push_back(tool);
        } else if (mNextTool != "none") {
          std::cout << mNextTool << " is not implemented yet." << std::endl;
        }
        mNextTool = "none";
        mGuiManager->getGui()->callJavascript("CosmoScout.measurementTools.deselect");
      }
    }
  });

  mOnDoubleClickConnection = mInputManager->sOnDoubleClick.connect([this]() {
    mNextTool = "none";
    mGuiManager->getGui()->callJavascript("CosmoScout.measurementTools.deselect");
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  mGuiManager->getGui()->unregisterCallback("set_measurement_tool");
  mGuiManager->getGui()->callJavascript("CosmoScout.unregisterHtml", "measurement-tool");
  mGuiManager->getGui()->callJavascript(
      "CosmoScout.unregisterCss", "css/csp-measurement-tools-sidebar.css");

  mInputManager->pButtons[0].onChange().disconnect(mOnClickConnection);
  mInputManager->sOnDoubleClick.disconnect(mOnDoubleClickConnection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::update() {
  // Update all registered tools. If the pShouldDelete property is set, the Tool is removed from the
  // list.
  for (auto it = mTools.begin(); it != mTools.end();) {
    if ((*it)->pShouldDelete.get()) {
      it = mTools.erase(it);
    } else {
      (*it)->update();
      ++it;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::measurementtools
