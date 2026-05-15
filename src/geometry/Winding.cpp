#include "geometry/Winding.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace mtb::geometry {
namespace {

std::optional<Vec3> intersect_planes(const Plane& a, const Plane& b,
                                     const Plane& c) {
  const Vec3 bc = cross(b.normal, c.normal);
  const double denominator = dot(a.normal, bc);
  if (std::abs(denominator) < 1e-9) {
    return std::nullopt;
  }

  return (bc * a.distance + cross(c.normal, a.normal) * b.distance +
          cross(a.normal, b.normal) * c.distance) /
         denominator;
}

bool contains_vertex(const std::vector<Vec3>& vertices, const Vec3& candidate,
                     double epsilon) {
  for (const Vec3& vertex : vertices) {
    if (distance(vertex, candidate) <= epsilon) {
      return true;
    }
  }

  return false;
}

Vec3 perpendicular_axis(const Vec3& normal) {
  const Vec3 reference =
      std::abs(normal.z) < 0.9 ? Vec3{0.0, 0.0, 1.0} : Vec3{0.0, 1.0, 0.0};
  return normalized(cross(reference, normal));
}

std::vector<Vec3> face_vertices_for_plane(const Plane& plane,
                                          const std::vector<Vec3>& vertices,
                                          double epsilon) {
  std::vector<Vec3> face;
  for (const Vec3& vertex : vertices) {
    if (std::abs(signed_distance(plane, vertex)) <= epsilon) {
      face.push_back(vertex);
    }
  }

  if (face.size() < 3) {
    return {};
  }

  Vec3 center;
  for (const Vec3& vertex : face) {
    center = center + vertex;
  }
  center = center / static_cast<double>(face.size());

  const Vec3 axis_u = perpendicular_axis(plane.normal);
  const Vec3 axis_v = cross(plane.normal, axis_u);
  std::sort(face.begin(), face.end(), [&](const Vec3& lhs, const Vec3& rhs) {
    const Vec3 lhs_delta = lhs - center;
    const Vec3 rhs_delta = rhs - center;
    const double lhs_angle = std::atan2(dot(lhs_delta, axis_v), dot(lhs_delta, axis_u));
    const double rhs_angle = std::atan2(dot(rhs_delta, axis_v), dot(rhs_delta, axis_u));
    return lhs_angle < rhs_angle;
  });

  if (dot(polygon_area_vector(face), plane.normal) < 0.0) {
    std::reverse(face.begin(), face.end());
  }

  return face;
}

}  // namespace

Vec3 polygon_area_vector(const std::vector<Vec3>& points) {
  Vec3 area;
  if (points.size() < 3) {
    return area;
  }

  for (std::size_t index = 0; index < points.size(); ++index) {
    const Vec3& current = points[index];
    const Vec3& next = points[(index + 1) % points.size()];
    area = area + cross(current, next);
  }

  return area * 0.5;
}

double polygon_area(const std::vector<Vec3>& points) {
  return length(polygon_area_vector(points));
}

WindingValidation validate_winding(const std::vector<Vec3>& points,
                                   const Plane& input_plane,
                                   double distance_epsilon,
                                   double min_area) {
  WindingValidation validation;
  const Plane plane = normalized_plane(input_plane);

  if (points.size() < 3) {
    validation.diagnostics.push_back("Winding must contain at least three points.");
    return validation;
  }

  for (const Vec3& point : points) {
    if (!is_finite(point)) {
      validation.diagnostics.push_back("Winding contains a non-finite point.");
      return validation;
    }

    if (std::abs(signed_distance(plane, point)) > distance_epsilon) {
      validation.diagnostics.push_back("Winding point is not on the face plane.");
      return validation;
    }
  }

  const Vec3 area_vector = polygon_area_vector(points);
  validation.area = length(area_vector);
  if (validation.area < min_area) {
    validation.diagnostics.push_back("Winding area is too small.");
    return validation;
  }

  validation.normal = normalized(area_vector);
  if (dot(validation.normal, plane.normal) <= 0.0) {
    validation.diagnostics.push_back("Winding orientation does not match the face plane.");
    return validation;
  }

  validation.valid = true;
  return validation;
}

ConvexVolumeValidation validate_convex_volume(const std::vector<Plane>& input_planes,
                                              double epsilon,
                                              double min_volume) {
  ConvexVolumeValidation validation;

  if (input_planes.size() < 4) {
    validation.diagnostics.push_back("A convex volume needs at least four planes.");
    return validation;
  }

  std::vector<Plane> planes;
  planes.reserve(input_planes.size());
  for (const Plane& input_plane : input_planes) {
    const Plane plane = normalized_plane(input_plane);
    if (length_squared(plane.normal) == 0.0 || !std::isfinite(plane.distance)) {
      validation.diagnostics.push_back("Volume contains an invalid plane.");
      return validation;
    }
    planes.push_back(plane);
  }

  for (std::size_t i = 0; i < planes.size(); ++i) {
    for (std::size_t j = i + 1; j < planes.size(); ++j) {
      for (std::size_t k = j + 1; k < planes.size(); ++k) {
        const std::optional<Vec3> vertex =
            intersect_planes(planes[i], planes[j], planes[k]);
        if (!vertex.has_value() || !is_finite(*vertex)) {
          continue;
        }

        bool inside = true;
        for (const Plane& plane : planes) {
          if (signed_distance(plane, *vertex) > epsilon) {
            inside = false;
            break;
          }
        }

        if (inside && !contains_vertex(validation.vertices, *vertex, epsilon)) {
          validation.vertices.push_back(*vertex);
        }
      }
    }
  }

  if (validation.vertices.size() < 4) {
    validation.diagnostics.push_back("Volume does not enclose enough vertices.");
    return validation;
  }

  double volume = 0.0;
  for (const Plane& plane : planes) {
    const std::vector<Vec3> face =
        face_vertices_for_plane(plane, validation.vertices, epsilon);
    if (face.size() < 3) {
      validation.diagnostics.push_back("A volume plane does not form a face.");
      return validation;
    }

    const double area = polygon_area(face);
    if (area <= epsilon) {
      validation.diagnostics.push_back("A volume face has near-zero area.");
      return validation;
    }
    volume += plane.distance * area / 3.0;
  }

  validation.volume = std::abs(volume);
  if (validation.volume < min_volume) {
    validation.diagnostics.push_back("Volume is too small.");
    return validation;
  }

  validation.valid = true;
  return validation;
}

}  // namespace mtb::geometry
