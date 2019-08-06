////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_MEASUREMENT_TOOLS_VORONOI_GENERATOR_HPP
#define CSP_MEASUREMENT_TOOLS_VORONOI_GENERATOR_HPP

#include "Beachline.hpp"
#include "Circle.hpp"
#include "Site.hpp"
#include "Vector2f.hpp"

#include <map>
#include <queue>
#include <vector>

namespace csp::measurementtools {

typedef std::pair<Vector2f, Vector2f> Edge;
typedef std::pair<Site, Site>         Edge2;
typedef std::tuple<Site, Site, Site>  Triangle;

class VoronoiGenerator {
 public:
  VoronoiGenerator();

  void parse(std::vector<Site> const& sites);

  double sweepLine() const;

  double maxY() const;
  double minY() const;

  std::vector<Site> const&                           getSites() const;
  std::vector<Edge> const&                           getEdges() const;
  std::vector<Edge2> const&                          getTriangulation() const;
  std::vector<Triangle> const&                       getTriangles() const;
  std::map<unsigned short, std::vector<Site>> const& getNeighbors() const;

  void addTriangulationEdge(Site const& site1, Site const& site2);
  void removeTriangulationEdge(Site const& site1, Site const& site2);

 private:
  void process(Site const& event);
  void process(Circle* event);
  void addCircleEvent(Arc* arc);
  void finishEdges();

  Beachline mBeachline;
  double    mSweepline;
  double    mMaxY, mMinY;

  std::priority_queue<Site, std::vector<Site>, SitePosComp>        mSiteEvents;
  std::priority_queue<Circle*, std::vector<Circle*>, CirclePtrCmp> mCircleEvents;

  std::vector<Site>                           mSites;
  std::vector<Edge>                           mVoronoiEdges;
  std::vector<Edge2>                          mTriangulationEdges;
  std::vector<Triangle>                       mTriangles;
  std::map<unsigned short, std::vector<Site>> mNeighbors;
};
} // namespace csp::measurementtools
#endif // CSP_MEASUREMENT_TOOLS_VORONOI_GENERATOR_HPP
