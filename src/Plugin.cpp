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

  mGuiManager->addPluginTabToSideBarFromHTML(
      "Measurement Tools", "multiline_chart", "../share/resources/gui/measurement-tools-tab.html");

  mGuiManager->addScriptToSideBarFromJS("../share/resources/gui/js/measurement-tools-tab.js");

  mGuiManager->getSideBar()->callJavascript(
      "add_measurement_tool", "Location Flag", "edit_location");
  mGuiManager->getSideBar()->callJavascript(
      "add_measurement_tool", "Landing Ellipse", "location_searching");
  mGuiManager->getSideBar()->callJavascript("add_measurement_tool", "Path", "timeline");
  mGuiManager->getSideBar()->callJavascript("add_measurement_tool", "Dip & Strike", "clear_all");
  mGuiManager->getSideBar()->callJavascript("add_measurement_tool", "Polygon", "crop_landscape");

  mGuiManager->getSideBar()->registerCallback<std::string>(
      "set_measurement_tool", [this](std::string const& name) { mNextTool = name; });

  mInputManager->pButtons[0].onChange().connect([this](bool pressed) {
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
              mGuiManager, mTimeControl, body->getCenterName(), body->getFrameName());
          tool->pLngLat = cs::utils::convert::toLngLatHeight(
              mInputManager->pHoveredObject.get().mPosition, radii[0], radii[0])
                              .xy();
          mGuiManager->registerTool(tool);
        } else if (mNextTool == "Landing Ellipse") {
          auto tool = std::make_shared<EllipseTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mGuiManager, mTimeControl, body->getCenterName(), body->getFrameName());
          tool->getCenterHandle().pLngLat = cs::utils::convert::toLngLatHeight(
              mInputManager->pHoveredObject.get().mPosition, radii[0], radii[0])
                                                .xy();
          tool->setNumSamples(mPluginSettings.mEllipse.mNumSamples);
          mGuiManager->registerTool(tool);
        } else if (mNextTool == "Path") {
          auto tool = std::make_shared<PathTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mGuiManager, mTimeControl, body->getCenterName(), body->getFrameName());
          tool->setNumSamples(mPluginSettings.mPath.mNumSamples);
          mGuiManager->registerTool(tool);
        } else if (mNextTool == "Dip & Strike") {
          auto tool = std::make_shared<DipStrikeTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mGuiManager, mTimeControl, body->getCenterName(), body->getFrameName());
          mGuiManager->registerTool(tool);
        } else if (mNextTool == "Polygon") {
          auto tool = std::make_shared<PolygonTool>(mInputManager, mSolarSystem, mGraphicsEngine,
              mGuiManager, mTimeControl, body->getCenterName(), body->getFrameName());
          tool->setHeightDiff(mPluginSettings.mPolygon.mHeightDiff);
          tool->setMaxAttempt(mPluginSettings.mPolygon.mMaxAttempt);
          tool->setMaxPoints(mPluginSettings.mPolygon.mMaxPoints);
          tool->setSleekness(mPluginSettings.mPolygon.mSleekness);
          mGuiManager->registerTool(tool);
        } else if (mNextTool != "none") {
          std::cout << mNextTool << " is not implemented yet." << std::endl;
        }
        mNextTool = "none";
        mGuiManager->getSideBar()->callJavascript("deselect_measurement_tool");
      }
    }
  });

  mInputManager->sOnDoubleClick.connect([this]() {
    mNextTool = "none";
    mGuiManager->getSideBar()->callJavascript("deselect_measurement_tool");
  });

  mInputManager->pButtons[1].onChange().connect([this](bool pressed) {
    if (pressed) {
      mNextTool = "none";
      mGuiManager->getSideBar()->callJavascript("deselect_measurement_tool");
    }
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  mGuiManager->getSideBar()->unregisterCallback("set_measurement_tool");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::measurementtools
