#pragma once

#include "geometry/Plane.hpp"

#include <string>
#include <vector>

namespace mtb::geometry {

struct WindingValidation {
  bool valid = false;
  double area = 0.0;
  Vec3 normal;
  std::vector<std::string> diagnostics;
};

struct ConvexVolumeValidation {
  bool valid = false;
  double volume = 0.0;
  std::vector<Vec3> vertices;
  std::vector<std::string> diagnostics;
};

Vec3 polygon_area_vector(const std::vector<Vec3>& points);

double polygon_area(const std::vector<Vec3>& points);

WindingValidation validate_winding(const std::vector<Vec3>& points,
                                   const Plane& plane,
                                   double distance_epsilon = 0.01,
                                   double min_area = 0.01);

ConvexVolumeValidation validate_convex_volume(const std::vector<Plane>& planes,
                                              double epsilon = 0.01,
                                              double min_volume = 0.01);

}  // namespace mtb::geometry
