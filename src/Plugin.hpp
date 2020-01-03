////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_MEASUREMENTTOOLS_PLUGIN_HPP
#define CSP_MEASUREMENTTOOLS_PLUGIN_HPP

#include "../../../src/cs-core/PluginBase.hpp"

#include <list>
#include <string>

namespace cs::core::tools {
class Tool;
}

namespace csp::measurementtools {

class Tools;

/// This plugin enables the user to measure different things on the surface of planets and moons.
/// See README.md for details.
class Plugin : public cs::core::PluginBase {
 public:
  struct Settings {
    struct Polygon {
      float mHeightDiff;
      int   mMaxAttempt;
      int   mMaxPoints;
      int   mSleekness;
    };

    Polygon mPolygon;

    struct Ellipse {
      int mNumSamples;
    };

    Ellipse mEllipse;

    struct Path {
      int mNumSamples;
    };

    Path mPath;
  };

  void init() override;
  void deInit() override;
  void update() override;

 private:
  Settings    mPluginSettings;
  std::string mNextTool = "none";

  std::list<std::shared_ptr<cs::core::tools::Tool>> mTools;

  int mOnClickConnection       = -1;
  int mOnDoubleClickConnection = -1;
};

} // namespace csp::measurementtools

#endif // CSP_MEASUREMENTTOOLS_PLUGIN_HPP
