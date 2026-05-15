#include "geometry/PlaneFit.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace mtb::geometry {
namespace {

using Matrix3 = std::array<std::array<double, 3>, 3>;

Vec3 fallback_normal(const std::vector<Vec3>& points) {
  double best_area = 0.0;
  Vec3 best_normal;

  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      for (std::size_t k = j + 1; k < points.size(); ++k) {
        const Vec3 candidate = cross(points[j] - points[i], points[k] - points[i]);
        const double area = length_squared(candidate);
        if (area > best_area) {
          best_area = area;
          best_normal = candidate;
        }
      }
    }
  }

  return normalized(best_normal);
}

Vec3 smallest_eigenvector(Matrix3 matrix) {
  Matrix3 vectors{{
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
      {0.0, 0.0, 1.0},
  }};

  for (int iteration = 0; iteration < 50; ++iteration) {
    int p = 0;
    int q = 1;
    double largest = std::abs(matrix[0][1]);

    for (int row = 0; row < 3; ++row) {
      for (int column = row + 1; column < 3; ++column) {
        const double value = std::abs(matrix[row][column]);
        if (value > largest) {
          largest = value;
          p = row;
          q = column;
        }
      }
    }

    if (largest < 1e-12) {
      break;
    }

    const double angle =
        0.5 * std::atan2(2.0 * matrix[p][q], matrix[q][q] - matrix[p][p]);
    const double c = std::cos(angle);
    const double s = std::sin(angle);

    const double app = c * c * matrix[p][p] - 2.0 * s * c * matrix[p][q] +
                       s * s * matrix[q][q];
    const double aqq = s * s * matrix[p][p] + 2.0 * s * c * matrix[p][q] +
                       c * c * matrix[q][q];
    matrix[p][p] = app;
    matrix[q][q] = aqq;
    matrix[p][q] = 0.0;
    matrix[q][p] = 0.0;

    for (int index = 0; index < 3; ++index) {
      if (index == p || index == q) {
        continue;
      }

      const double aip = matrix[index][p];
      const double aiq = matrix[index][q];
      matrix[index][p] = c * aip - s * aiq;
      matrix[p][index] = matrix[index][p];
      matrix[index][q] = s * aip + c * aiq;
      matrix[q][index] = matrix[index][q];
    }

    for (int row = 0; row < 3; ++row) {
      const double vip = vectors[row][p];
      const double viq = vectors[row][q];
      vectors[row][p] = c * vip - s * viq;
      vectors[row][q] = s * vip + c * viq;
    }
  }

  int smallest = 0;
  if (matrix[1][1] < matrix[smallest][smallest]) {
    smallest = 1;
  }
  if (matrix[2][2] < matrix[smallest][smallest]) {
    smallest = 2;
  }

  return normalized({vectors[0][smallest], vectors[1][smallest],
                     vectors[2][smallest]});
}

}  // namespace

PlaneFit fit_plane(const std::vector<Vec3>& points) {
  PlaneFit fit;
  fit.point_count = points.size();

  if (points.size() < 3) {
    return fit;
  }

  for (const Vec3& point : points) {
    if (!is_finite(point)) {
      return fit;
    }
    fit.centroid = fit.centroid + point;
  }
  fit.centroid = fit.centroid / static_cast<double>(points.size());

  Matrix3 covariance{{
      {0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0},
  }};

  for (const Vec3& point : points) {
    const Vec3 delta = point - fit.centroid;
    covariance[0][0] += delta.x * delta.x;
    covariance[0][1] += delta.x * delta.y;
    covariance[0][2] += delta.x * delta.z;
    covariance[1][0] += delta.y * delta.x;
    covariance[1][1] += delta.y * delta.y;
    covariance[1][2] += delta.y * delta.z;
    covariance[2][0] += delta.z * delta.x;
    covariance[2][1] += delta.z * delta.y;
    covariance[2][2] += delta.z * delta.z;
  }

  Vec3 normal = smallest_eigenvector(covariance);
  if (length_squared(normal) == 0.0) {
    normal = fallback_normal(points);
  }
  if (length_squared(normal) == 0.0) {
    return fit;
  }

  fit.plane = plane_from_point_normal(fit.centroid, normal);
  double squared_distance_sum = 0.0;
  for (const Vec3& point : points) {
    const double distance_to_plane = std::abs(signed_distance(fit.plane, point));
    fit.max_abs_distance = std::max(fit.max_abs_distance, distance_to_plane);
    squared_distance_sum += distance_to_plane * distance_to_plane;
  }

  fit.rms_distance =
      std::sqrt(squared_distance_sum / static_cast<double>(points.size()));
  fit.valid = true;
  return fit;
}

PlanarityClassification classify_planarity(const std::vector<Vec3>& points,
                                           double max_distance) {
  PlanarityClassification classification;
  classification.fit = fit_plane(points);
  classification.planar = classification.fit.valid &&
                          classification.fit.max_abs_distance <= max_distance;
  return classification;
}

}  // namespace mtb::geometry
