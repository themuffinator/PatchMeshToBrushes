#pragma once

#include "geometry/Vec.hpp"

namespace mtb::geometry {

struct Plane {
  Vec3 normal;
  double distance = 0.0;
};

inline Plane plane_from_point_normal(const Vec3& point, const Vec3& normal) {
  const Vec3 unit_normal = normalized(normal);
  return {unit_normal, dot(unit_normal, point)};
}

inline Plane normalized_plane(const Plane& plane) {
  const double normal_length = length(plane.normal);
  if (normal_length == 0.0) {
    return {};
  }

  return {plane.normal / normal_length, plane.distance / normal_length};
}

inline double signed_distance(const Plane& plane, const Vec3& point) {
  return dot(plane.normal, point) - plane.distance;
}

}  // namespace mtb::geometry
