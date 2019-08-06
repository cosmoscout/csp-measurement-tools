////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Arc.hpp"
#include "Breakpoint.hpp"

namespace csp::measurementtools {

Arc::Arc(Site const& s)
    : mSite(s)
    , mLeftBreak(NULL)
    , mRightBreak(NULL)
    , mEvent(NULL) {
}

void Arc::invalidateEvent() {

  if (mEvent) {
    if (mEvent->mIsValid) {
      mEvent->mIsValid = false;
    }

    mEvent = NULL;
  }
}
} // namespace csp::measurementtools
