////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Beachline.hpp"

#include "Arc.hpp"
#include "VoronoiGenerator.hpp"

namespace csp::measurementtools {

Beachline::Beachline(VoronoiGenerator* parent)
    : mBreakPoints()
    , mParent(parent)
    , mRoot(NULL) {
}

Arc* Beachline::insertArcFor(Site const& site) {
  // if site creates the very first Arc of the Beachline
  if (mRoot == NULL) {
    mRoot = new Arc(site);
    return mRoot;
  }

  Arc* newArc = new Arc(site);

  Arc* brokenArcLeft = mBreakPoints.empty() ? mRoot : mBreakPoints.getArcAt(site.mX);
  brokenArcLeft->invalidateEvent();

  // site inserted at exactly the same height as brokenArcLeft
  if (site.mY == brokenArcLeft->mSite.mY) {
    if (site.mX < brokenArcLeft->mSite.mX) {
      newArc->mRightBreak = new Breakpoint(newArc, brokenArcLeft, mParent);
      mParent->addTriangulationEdge(brokenArcLeft->mSite, newArc->mSite);
      brokenArcLeft->mLeftBreak = newArc->mRightBreak;
      mBreakPoints.insert(newArc->mRightBreak);
    }
    // new one is right of brokenArcLeft
    else {
      newArc->mLeftBreak = new Breakpoint(brokenArcLeft, newArc, mParent);
      mParent->addTriangulationEdge(brokenArcLeft->mSite, newArc->mSite);
      brokenArcLeft->mRightBreak = newArc->mLeftBreak;
      mBreakPoints.insert(newArc->mLeftBreak);
    }
  } else {
    Arc* brokenArcRight = new Arc(brokenArcLeft->mSite);

    newArc->mLeftBreak  = new Breakpoint(brokenArcLeft, newArc, mParent);
    newArc->mRightBreak = new Breakpoint(newArc, brokenArcRight, mParent);

    mParent->addTriangulationEdge(brokenArcLeft->mSite, newArc->mSite);

    brokenArcRight->mRightBreak = brokenArcLeft->mRightBreak;
    if (brokenArcRight->mRightBreak) {
      brokenArcRight->mRightBreak->mRightArc->mLeftBreak->mLeftArc = brokenArcRight;
    }
    brokenArcRight->mLeftBreak = newArc->mRightBreak;
    brokenArcLeft->mRightBreak = newArc->mLeftBreak;

    mBreakPoints.insert(newArc->mLeftBreak);
    mBreakPoints.insert(newArc->mRightBreak);
  }

  return newArc;
}

void Beachline::removeArc(Arc* arc) {
  Arc* leftArc  = arc->mLeftBreak ? arc->mLeftBreak->mLeftArc : NULL;
  Arc* rightArc = arc->mRightBreak ? arc->mRightBreak->mRightArc : NULL;

  arc->invalidateEvent();
  if (leftArc)
    leftArc->invalidateEvent();
  if (rightArc)
    rightArc->invalidateEvent();

  if (leftArc && rightArc) {
    Breakpoint* merged = new Breakpoint(leftArc, rightArc, mParent);

    mParent->addTriangulationEdge(leftArc->mSite, rightArc->mSite);

    leftArc->mRightBreak = merged;
    rightArc->mLeftBreak = merged;

    mBreakPoints.remove(arc->mRightBreak);
    mBreakPoints.remove(arc->mLeftBreak);

    mBreakPoints.insert(merged);

    delete arc->mLeftBreak;
    delete arc->mRightBreak;
  } else if (leftArc) {
    mBreakPoints.remove(arc->mLeftBreak);
    leftArc->mRightBreak = NULL;
    delete arc->mLeftBreak;
  } else if (rightArc) {
    mBreakPoints.remove(arc->mRightBreak);
    rightArc->mLeftBreak = NULL;
    delete arc->mRightBreak;
  }

  delete arc;
}

void Beachline::finish(std::vector<Edge>& edges) {
  mBreakPoints.finishAll(edges);
}
} // namespace csp::measurementtools
