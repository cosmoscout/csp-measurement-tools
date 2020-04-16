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
#include "../../../src/cs-utils/logger.hpp"
#include "logger.hpp"

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
  delete pluginBase; // NOLINT(cppcoreguidelines-owning-memory)
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::measurementtools {

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(nlohmann::json const& j, Plugin::Settings::Polygon& o) {
  cs::core::Settings::deserialize(j, "heightDiff", o.mHeightDiff);
  cs::core::Settings::deserialize(j, "maxAttempt", o.mMaxAttempt);
  cs::core::Settings::deserialize(j, "maxPoints", o.mMaxPoints);
  cs::core::Settings::deserialize(j, "sleekness", o.mSleekness);
}

void to_json(nlohmann::json& j, Plugin::Settings::Polygon const& o) {
  cs::core::Settings::serialize(j, "heightDiff", o.mHeightDiff);
  cs::core::Settings::serialize(j, "maxAttempt", o.mMaxAttempt);
  cs::core::Settings::serialize(j, "maxPoints", o.mMaxPoints);
  cs::core::Settings::serialize(j, "sleekness", o.mSleekness);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(nlohmann::json const& j, Plugin::Settings::Ellipse& o) {
  cs::core::Settings::deserialize(j, "numSamples", o.mNumSamples);
}

void to_json(nlohmann::json& j, Plugin::Settings::Ellipse const& o) {
  cs::core::Settings::serialize(j, "numSamples", o.mNumSamples);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(nlohmann::json const& j, Plugin::Settings::Path& o) {
  cs::core::Settings::deserialize(j, "numSamples", o.mNumSamples);
}

void to_json(nlohmann::json& j, Plugin::Settings::Path const& o) {
  cs::core::Settings::serialize(j, "numSamples", o.mNumSamples);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(nlohmann::json const& j, Plugin::Settings& o) {
  cs::core::Settings::deserialize(j, "polygon", o.mPolygon);
  cs::core::Settings::deserialize(j, "ellipse", o.mEllipse);
  cs::core::Settings::deserialize(j, "path", o.mPath);
}

void to_json(nlohmann::json& j, Plugin::Settings const& o) {
  cs::core::Settings::serialize(j, "polygon", o.mPolygon);
  cs::core::Settings::serialize(j, "ellipse", o.mEllipse);
  cs::core::Settings::serialize(j, "path", o.mPath);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {

  logger().info("Loading plugin...");

  mPluginSettings = mAllSettings->mPlugins.at("csp-measurement-tools");

  mGuiManager->addHtmlToGui(
      "measurement-tools", "../share/resources/gui/measurement-tool-template.html");

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

  mGuiManager->getGui()->registerCallback("measurementTools.setNext",
      "Selects which tool will be created next. The given string should be either 'Location Flag', "
      "'Landing Ellipse, 'Path', 'Dip & Strike' or 'Polygon'.",
      std::function([this](std::string&& name) { mNextTool = name; }));

  mOnClickConnection = mInputManager->pButtons[0].connect([this](bool pressed) {
    if (!pressed && !mInputManager->pHoveredGuiItem.get()) {
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
          auto tool     = std::make_shared<FlagTool>(mInputManager, mSolarSystem, mAllSettings,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->pLngLat = cs::utils::convert::toLngLatHeight(
              mInputManager->pHoveredObject.get().mPosition, radii[0], radii[0])
                              .xy();
          mTools.push_back(tool);
        } else if (mNextTool == "Landing Ellipse") {
          auto tool = std::make_shared<EllipseTool>(mInputManager, mSolarSystem, mAllSettings,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->getCenterHandle().pLngLat = cs::utils::convert::toLngLatHeight(
              mInputManager->pHoveredObject.get().mPosition, radii[0], radii[0])
                                                .xy();
          tool->setNumSamples(mPluginSettings.mEllipse.mNumSamples);
          mTools.push_back(tool);
        } else if (mNextTool == "Path") {
          auto tool = std::make_shared<PathTool>(mInputManager, mSolarSystem, mAllSettings,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->setNumSamples(mPluginSettings.mPath.mNumSamples);
          mTools.push_back(tool);
        } else if (mNextTool == "Dip & Strike") {
          auto tool = std::make_shared<DipStrikeTool>(mInputManager, mSolarSystem, mAllSettings,
              mTimeControl, body->getCenterName(), body->getFrameName());
          mTools.push_back(tool);
        } else if (mNextTool == "Polygon") {
          auto tool = std::make_shared<PolygonTool>(mInputManager, mSolarSystem, mAllSettings,
              mTimeControl, body->getCenterName(), body->getFrameName());
          tool->setHeightDiff(mPluginSettings.mPolygon.mHeightDiff);
          tool->setMaxAttempt(mPluginSettings.mPolygon.mMaxAttempt);
          tool->setMaxPoints(mPluginSettings.mPolygon.mMaxPoints);
          tool->setSleekness(mPluginSettings.mPolygon.mSleekness);
          mTools.push_back(tool);
        } else if (mNextTool != "none") {
          logger().warn("Failed to create tool '{}': This is an unknown tool type!", mNextTool);
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

  logger().info("Loading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  logger().info("Unloading plugin...");

  mGuiManager->removePluginTab("Measurement Tools");

  mGuiManager->getGui()->unregisterCallback("measurementTools.setNext");
  mGuiManager->getGui()->callJavascript("CosmoScout.gui.unregisterHtml", "measurement-tools");
  mGuiManager->getGui()->callJavascript(
      "CosmoScout.gui.unregisterCss", "css/csp-measurement-tools-sidebar.css");

  mInputManager->pButtons[0].disconnect(mOnClickConnection);
  mInputManager->sOnDoubleClick.disconnect(mOnDoubleClickConnection);

  logger().info("Unloading done.");
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
