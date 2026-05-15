#pragma once

#include "geometry/Plane.hpp"

#include <vector>

namespace mtb::geometry {

struct PlaneFit {
  Plane plane;
  Vec3 centroid;
  double rms_distance = 0.0;
  double max_abs_distance = 0.0;
  std::size_t point_count = 0;
  bool valid = false;
};

struct PlanarityClassification {
  PlaneFit fit;
  bool planar = false;
};

PlaneFit fit_plane(const std::vector<Vec3>& points);

PlanarityClassification classify_planarity(const std::vector<Vec3>& points,
                                           double max_distance);

}  // namespace mtb::geometry
