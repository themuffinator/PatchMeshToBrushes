#include "geometry/UvProjection.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace mtb::geometry {
namespace {

struct LocalSample {
  double s = 0.0;
  double t = 0.0;
  Vec2 uv;
};

Vec3 projection_axis_s(const std::vector<UvSample>& samples, const Plane& plane,
                       const Vec3& origin) {
  Vec3 best_axis;
  double best_length = 0.0;

  for (const UvSample& sample : samples) {
    const Vec3 delta = sample.position - origin;
    const Vec3 projected = delta - plane.normal * dot(delta, plane.normal);
    const double candidate_length = length_squared(projected);
    if (candidate_length > best_length) {
      best_length = candidate_length;
      best_axis = projected;
    }
  }

  if (best_length > 0.0) {
    return normalized(best_axis);
  }

  const Vec3 reference =
      std::abs(plane.normal.z) < 0.9 ? Vec3{0.0, 0.0, 1.0} : Vec3{0.0, 1.0, 0.0};
  return normalized(cross(reference, plane.normal));
}

bool solve_3x3(std::array<std::array<double, 3>, 3> matrix,
               std::array<double, 3> rhs, std::array<double, 3>& solution) {
  for (int column = 0; column < 3; ++column) {
    int pivot = column;
    for (int row = column + 1; row < 3; ++row) {
      if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) {
        pivot = row;
      }
    }

    if (std::abs(matrix[pivot][column]) < 1e-12) {
      return false;
    }

    if (pivot != column) {
      std::swap(matrix[pivot], matrix[column]);
      std::swap(rhs[pivot], rhs[column]);
    }

    const double divisor = matrix[column][column];
    for (int item = column; item < 3; ++item) {
      matrix[column][item] /= divisor;
    }
    rhs[column] /= divisor;

    for (int row = 0; row < 3; ++row) {
      if (row == column) {
        continue;
      }

      const double factor = matrix[row][column];
      for (int item = column; item < 3; ++item) {
        matrix[row][item] -= factor * matrix[column][item];
      }
      rhs[row] -= factor * rhs[column];
    }
  }

  solution = rhs;
  return true;
}

bool solve_affine_uv(const std::vector<LocalSample>& samples, bool solve_u,
                     std::array<double, 3>& coefficients) {
  std::array<std::array<double, 3>, 3> normal{{
      {0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0},
  }};
  std::array<double, 3> rhs{0.0, 0.0, 0.0};

  for (const LocalSample& sample : samples) {
    const std::array<double, 3> row{sample.s, sample.t, 1.0};
    const double target = solve_u ? sample.uv.u : sample.uv.v;
    for (int i = 0; i < 3; ++i) {
      rhs[i] += row[i] * target;
      for (int j = 0; j < 3; ++j) {
        normal[i][j] += row[i] * row[j];
      }
    }
  }

  return solve_3x3(normal, rhs, coefficients);
}

}  // namespace

PlanarUvProjection fit_planar_uv_projection(const std::vector<UvSample>& samples) {
  PlanarUvProjection projection;
  if (samples.size() < 3) {
    return projection;
  }

  std::vector<Vec3> positions;
  positions.reserve(samples.size());
  for (const UvSample& sample : samples) {
    if (!is_finite(sample.position) || !is_finite(sample.uv)) {
      return projection;
    }
    positions.push_back(sample.position);
  }

  const PlaneFit fit = fit_plane(positions);
  if (!fit.valid) {
    return projection;
  }

  projection.plane = fit.plane;
  projection.origin = fit.centroid;
  projection.axis_s = projection_axis_s(samples, projection.plane, projection.origin);
  projection.axis_t = normalized(cross(projection.plane.normal, projection.axis_s));
  if (length_squared(projection.axis_s) == 0.0 ||
      length_squared(projection.axis_t) == 0.0) {
    return projection;
  }

  std::vector<LocalSample> local_samples;
  local_samples.reserve(samples.size());
  for (const UvSample& sample : samples) {
    const Vec3 delta = sample.position - projection.origin;
    local_samples.push_back({dot(delta, projection.axis_s),
                             dot(delta, projection.axis_t), sample.uv});
  }

  std::array<double, 3> u_coefficients{0.0, 0.0, 0.0};
  std::array<double, 3> v_coefficients{0.0, 0.0, 0.0};
  if (!solve_affine_uv(local_samples, true, u_coefficients) ||
      !solve_affine_uv(local_samples, false, v_coefficients)) {
    return projection;
  }

  projection.u_from_s = u_coefficients[0];
  projection.u_from_t = u_coefficients[1];
  projection.u_offset = u_coefficients[2];
  projection.v_from_s = v_coefficients[0];
  projection.v_from_t = v_coefficients[1];
  projection.v_offset = v_coefficients[2];

  double squared_error_sum = 0.0;
  for (const UvSample& sample : samples) {
    const Vec2 fitted = evaluate_uv_projection(projection, sample.position);
    const double error = length(fitted - sample.uv);
    projection.max_error = std::max(projection.max_error, error);
    squared_error_sum += error * error;
  }
  projection.rms_error =
      std::sqrt(squared_error_sum / static_cast<double>(samples.size()));
  projection.valid = true;
  return projection;
}

Vec2 evaluate_uv_projection(const PlanarUvProjection& projection,
                            const Vec3& position) {
  const Vec3 delta = position - projection.origin;
  const double s = dot(delta, projection.axis_s);
  const double t = dot(delta, projection.axis_t);
  return {
      projection.u_from_s * s + projection.u_from_t * t + projection.u_offset,
      projection.v_from_s * s + projection.v_from_t * t + projection.v_offset,
  };
}

}  // namespace mtb::geometry
