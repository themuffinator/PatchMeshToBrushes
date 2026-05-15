#pragma once

#include "geometry/PlaneFit.hpp"

#include <vector>

namespace mtb::geometry {

struct UvSample {
  Vec3 position;
  Vec2 uv;
};

struct PlanarUvProjection {
  Plane plane;
  Vec3 origin;
  Vec3 axis_s;
  Vec3 axis_t;
  double u_from_s = 0.0;
  double u_from_t = 0.0;
  double u_offset = 0.0;
  double v_from_s = 0.0;
  double v_from_t = 0.0;
  double v_offset = 0.0;
  double rms_error = 0.0;
  double max_error = 0.0;
  bool valid = false;
};

PlanarUvProjection fit_planar_uv_projection(const std::vector<UvSample>& samples);

Vec2 evaluate_uv_projection(const PlanarUvProjection& projection,
                            const Vec3& position);

}  // namespace mtb::geometry
