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

}  // namespace mtb::geometry
