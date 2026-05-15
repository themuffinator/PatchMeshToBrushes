#include "conversion/PatchTopology.hpp"

#include "geometry/PlaneFit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>

namespace mtb::conversion {
namespace {

struct SphereFit {
  geometry::Vec3 center;
  double radius = 0.0;
  double max_error = 0.0;
  bool valid = false;
};

struct CylinderFit {
  geometry::Vec3 axis;
  geometry::Vec3 center;
  double radius = 0.0;
  double max_error = 0.0;
  double axis_extent = 0.0;
  bool valid = false;
};

struct BoundaryPairAnalysis {
  std::size_t shared_vertices = 0;
  std::size_t overlapping_spans = 0;
};

std::vector<geometry::Vec3> patch_positions(
    const std::vector<mtb::map::PatchSummary>& patches,
    const std::vector<std::size_t>& patch_indices) {
  std::vector<geometry::Vec3> positions;
  for (std::size_t patch_index : patch_indices) {
    const mtb::map::PatchSummary& patch = patches[patch_index];
    positions.reserve(positions.size() + patch.control_point_count());
    for (const std::vector<mtb::map::PatchControlPoint>& row :
         patch.control_points) {
      for (const mtb::map::PatchControlPoint& point : row) {
        positions.push_back(point.position);
      }
    }
  }
  return positions;
}

std::vector<geometry::Vec3> patch_positions(
    const mtb::map::PatchSummary& patch) {
  std::vector<geometry::Vec3> positions;
  positions.reserve(patch.control_point_count());
  for (const std::vector<mtb::map::PatchControlPoint>& row :
       patch.control_points) {
    for (const mtb::map::PatchControlPoint& point : row) {
      positions.push_back(point.position);
    }
  }
  return positions;
}

BoundarySample make_boundary_sample(const mtb::map::PatchControlPoint& point,
                                    std::size_t control_index,
                                    double epsilon) {
  return {point.position, quantize_position(point.position, epsilon),
          control_index};
}

BoundarySpan make_span(const mtb::map::PatchSummary& patch, BoundarySide side,
                       double epsilon) {
  BoundarySpan span;
  span.side = side;

  const std::size_t rows = patch.control_points.size();
  const std::size_t columns = rows == 0 ? 0 : patch.control_points.front().size();

  switch (side) {
    case BoundarySide::Top:
      for (std::size_t column = 0; column < columns; ++column) {
        span.samples.push_back(
            make_boundary_sample(patch.control_points.front()[column], column,
                                 epsilon));
      }
      break;
    case BoundarySide::Right:
      for (std::size_t row = 0; row < rows; ++row) {
        span.samples.push_back(make_boundary_sample(
            patch.control_points[row].back(), row, epsilon));
      }
      break;
    case BoundarySide::Bottom:
      for (std::size_t column = 0; column < columns; ++column) {
        const std::size_t reversed_column = columns - 1 - column;
        span.samples.push_back(make_boundary_sample(
            patch.control_points.back()[reversed_column], reversed_column,
            epsilon));
      }
      break;
    case BoundarySide::Left:
      for (std::size_t row = 0; row < rows; ++row) {
        const std::size_t reversed_row = rows - 1 - row;
        span.samples.push_back(make_boundary_sample(
            patch.control_points[reversed_row].front(), reversed_row, epsilon));
      }
      break;
  }

  return span;
}

std::set<QuantizedVertex> boundary_vertices(const PatchBoundary& boundary) {
  std::set<QuantizedVertex> vertices;
  for (const BoundarySpan& span : boundary.spans) {
    for (const BoundarySample& sample : span.samples) {
      vertices.insert(sample.quantized);
    }
  }
  return vertices;
}

bool segments_overlap(const geometry::Vec3& a0, const geometry::Vec3& a1,
                      const geometry::Vec3& b0, const geometry::Vec3& b1,
                      double epsilon) {
  const geometry::Vec3 a_delta = a1 - a0;
  const geometry::Vec3 b_delta = b1 - b0;
  const double a_length = geometry::length(a_delta);
  const double b_length = geometry::length(b_delta);
  if (a_length <= epsilon || b_length <= epsilon) {
    return false;
  }

  const geometry::Vec3 a_axis = a_delta / a_length;
  const geometry::Vec3 b_axis = b_delta / b_length;
  if (geometry::length(geometry::cross(a_axis, b_axis)) > 0.01) {
    return false;
  }

  const double b0_distance =
      geometry::length(geometry::cross(b0 - a0, a_axis));
  const double b1_distance =
      geometry::length(geometry::cross(b1 - a0, a_axis));
  if (b0_distance > epsilon || b1_distance > epsilon) {
    return false;
  }

  const double b0_projected = geometry::dot(b0 - a0, a_axis);
  const double b1_projected = geometry::dot(b1 - a0, a_axis);
  const double overlap_min = std::max(0.0, std::min(b0_projected, b1_projected));
  const double overlap_max =
      std::min(a_length, std::max(b0_projected, b1_projected));
  return overlap_max - overlap_min > epsilon;
}

std::size_t overlapping_segment_count(const BoundarySpan& lhs,
                                      const BoundarySpan& rhs,
                                      double epsilon) {
  if (lhs.samples.size() < 2 || rhs.samples.size() < 2) {
    return 0;
  }

  std::size_t count = 0;
  for (std::size_t i = 0; i + 1 < lhs.samples.size(); ++i) {
    for (std::size_t j = 0; j + 1 < rhs.samples.size(); ++j) {
      if (segments_overlap(lhs.samples[i].position, lhs.samples[i + 1].position,
                           rhs.samples[j].position,
                           rhs.samples[j + 1].position, epsilon)) {
        ++count;
      }
    }
  }
  return count;
}

BoundaryPairAnalysis analyze_boundary_pair(const PatchBoundary& lhs,
                                           const PatchBoundary& rhs,
                                           double epsilon) {
  BoundaryPairAnalysis analysis;
  const std::set<QuantizedVertex> lhs_vertices = boundary_vertices(lhs);
  const std::set<QuantizedVertex> rhs_vertices = boundary_vertices(rhs);

  for (const QuantizedVertex& vertex : lhs_vertices) {
    if (rhs_vertices.contains(vertex)) {
      ++analysis.shared_vertices;
    }
  }

  for (const BoundarySpan& lhs_span : lhs.spans) {
    for (const BoundarySpan& rhs_span : rhs.spans) {
      analysis.overlapping_spans +=
          overlapping_segment_count(lhs_span, rhs_span, epsilon);
    }
  }

  return analysis;
}

bool solve_linear_system(std::vector<std::vector<double>> matrix,
                         std::vector<double> rhs,
                         std::vector<double>& solution) {
  const std::size_t size = rhs.size();
  for (std::size_t column = 0; column < size; ++column) {
    std::size_t pivot = column;
    for (std::size_t row = column + 1; row < size; ++row) {
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
    for (std::size_t item = column; item < size; ++item) {
      matrix[column][item] /= divisor;
    }
    rhs[column] /= divisor;

    for (std::size_t row = 0; row < size; ++row) {
      if (row == column) {
        continue;
      }

      const double factor = matrix[row][column];
      for (std::size_t item = column; item < size; ++item) {
        matrix[row][item] -= factor * matrix[column][item];
      }
      rhs[row] -= factor * rhs[column];
    }
  }

  solution = std::move(rhs);
  return true;
}

SphereFit fit_sphere(const std::vector<geometry::Vec3>& points) {
  SphereFit fit;
  if (points.size() < 4) {
    return fit;
  }

  std::vector<std::vector<double>> normal(4, std::vector<double>(4, 0.0));
  std::vector<double> rhs(4, 0.0);
  for (const geometry::Vec3& point : points) {
    const std::array<double, 4> row{point.x, point.y, point.z, 1.0};
    const double target =
        -(point.x * point.x + point.y * point.y + point.z * point.z);
    for (std::size_t i = 0; i < row.size(); ++i) {
      rhs[i] += row[i] * target;
      for (std::size_t j = 0; j < row.size(); ++j) {
        normal[i][j] += row[i] * row[j];
      }
    }
  }

  std::vector<double> solution;
  if (!solve_linear_system(normal, rhs, solution)) {
    return fit;
  }

  fit.center = {-solution[0] * 0.5, -solution[1] * 0.5,
                -solution[2] * 0.5};
  const double radius_squared =
      geometry::length_squared(fit.center) - solution[3];
  if (radius_squared <= 0.0) {
    return fit;
  }

  fit.radius = std::sqrt(radius_squared);
  for (const geometry::Vec3& point : points) {
    fit.max_error =
        std::max(fit.max_error, std::abs(geometry::distance(point, fit.center) -
                                         fit.radius));
  }

  fit.valid = fit.radius > 0.0;
  return fit;
}

geometry::Vec3 centroid(const std::vector<geometry::Vec3>& points) {
  geometry::Vec3 result;
  for (const geometry::Vec3& point : points) {
    result = result + point;
  }
  return points.empty() ? result : result / static_cast<double>(points.size());
}

geometry::Vec3 principal_axis(const std::vector<geometry::Vec3>& points,
                              const geometry::Vec3& center) {
  std::array<std::array<double, 3>, 3> covariance{{
      {0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0},
      {0.0, 0.0, 0.0},
  }};
  for (const geometry::Vec3& point : points) {
    const geometry::Vec3 delta = point - center;
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

  geometry::Vec3 axis{1.0, 0.0, 0.0};
  for (int iteration = 0; iteration < 32; ++iteration) {
    const geometry::Vec3 next{
        covariance[0][0] * axis.x + covariance[0][1] * axis.y +
            covariance[0][2] * axis.z,
        covariance[1][0] * axis.x + covariance[1][1] * axis.y +
            covariance[1][2] * axis.z,
        covariance[2][0] * axis.x + covariance[2][1] * axis.y +
            covariance[2][2] * axis.z,
    };
    if (geometry::length_squared(next) == 0.0) {
      return axis;
    }
    axis = geometry::normalized(next);
  }
  return axis;
}

geometry::Vec3 perpendicular_axis(const geometry::Vec3& axis) {
  const geometry::Vec3 reference =
      std::abs(axis.z) < 0.9 ? geometry::Vec3{0.0, 0.0, 1.0}
                             : geometry::Vec3{0.0, 1.0, 0.0};
  return geometry::normalized(geometry::cross(reference, axis));
}

CylinderFit fit_cylinder_to_axis(const std::vector<geometry::Vec3>& points,
                                 geometry::Vec3 axis) {
  CylinderFit fit;
  axis = geometry::normalized(axis);
  if (points.size() < 4 || geometry::length_squared(axis) == 0.0) {
    return fit;
  }

  fit.axis = axis;
  const geometry::Vec3 origin = centroid(points);
  const geometry::Vec3 axis_u = perpendicular_axis(axis);
  const geometry::Vec3 axis_v = geometry::normalized(geometry::cross(axis, axis_u));
  if (geometry::length_squared(axis_u) == 0.0 ||
      geometry::length_squared(axis_v) == 0.0) {
    return fit;
  }

  double min_axis = 0.0;
  double max_axis = 0.0;
  std::vector<std::array<double, 2>> projected;
  projected.reserve(points.size());
  for (const geometry::Vec3& point : points) {
    const geometry::Vec3 delta = point - origin;
    const double along_axis = geometry::dot(delta, axis);
    min_axis = std::min(min_axis, along_axis);
    max_axis = std::max(max_axis, along_axis);
    projected.push_back({geometry::dot(delta, axis_u),
                         geometry::dot(delta, axis_v)});
  }

  std::vector<std::vector<double>> normal(3, std::vector<double>(3, 0.0));
  std::vector<double> rhs(3, 0.0);
  for (const std::array<double, 2>& point : projected) {
    const std::array<double, 3> row{point[0], point[1], 1.0};
    const double target = -(point[0] * point[0] + point[1] * point[1]);
    for (std::size_t i = 0; i < row.size(); ++i) {
      rhs[i] += row[i] * target;
      for (std::size_t j = 0; j < row.size(); ++j) {
        normal[i][j] += row[i] * row[j];
      }
    }
  }

  std::vector<double> solution;
  if (!solve_linear_system(normal, rhs, solution)) {
    return fit;
  }

  const double center_s = -solution[0] * 0.5;
  const double center_t = -solution[1] * 0.5;
  const double radius_squared =
      center_s * center_s + center_t * center_t - solution[2];
  if (radius_squared <= 0.0) {
    return fit;
  }

  fit.axis_extent = max_axis - min_axis;
  fit.radius = std::sqrt(radius_squared);
  fit.center = origin + axis_u * center_s + axis_v * center_t;
  for (const std::array<double, 2>& point : projected) {
    const double radius =
        std::sqrt((point[0] - center_s) * (point[0] - center_s) +
                  (point[1] - center_t) * (point[1] - center_t));
    fit.max_error = std::max(fit.max_error, std::abs(radius - fit.radius));
  }

  fit.valid = fit.radius > 0.0 && fit.axis_extent > 0.0;
  return fit;
}

CylinderFit fit_cylinder(const std::vector<geometry::Vec3>& points) {
  const geometry::Vec3 center = centroid(points);
  const std::array<geometry::Vec3, 4> axes{
      principal_axis(points, center),
      geometry::Vec3{1.0, 0.0, 0.0},
      geometry::Vec3{0.0, 1.0, 0.0},
      geometry::Vec3{0.0, 0.0, 1.0},
  };

  CylinderFit best;
  double best_score = std::numeric_limits<double>::infinity();
  for (const geometry::Vec3& axis : axes) {
    const CylinderFit fit = fit_cylinder_to_axis(points, axis);
    if (!fit.valid) {
      continue;
    }

    const double score = fit.radius == 0.0
                             ? std::numeric_limits<double>::infinity()
                             : fit.max_error / fit.radius;
    if (score < best_score) {
      best_score = score;
      best = fit;
    }
  }
  return best;
}

AssemblyKind classify_assembly(
    const std::vector<mtb::map::PatchSummary>& patches,
    const std::vector<std::size_t>& patch_indices,
    const ConversionSettings& settings) {
  bool has_planar_patch = false;
  bool has_nonplanar_patch = false;

  for (std::size_t patch_index : patch_indices) {
    const std::vector<geometry::Vec3> positions =
        patch_positions(patches[patch_index]);
    const geometry::PlanarityClassification planarity =
        geometry::classify_planarity(positions, settings.coplanar_epsilon);
    if (planarity.planar) {
      has_planar_patch = true;
    } else {
      has_nonplanar_patch = true;
    }
  }

  const std::vector<geometry::Vec3> positions =
      patch_positions(patches, patch_indices);
  const geometry::PlanarityClassification assembly_planarity =
      geometry::classify_planarity(positions, settings.coplanar_epsilon);
  if (assembly_planarity.planar) {
    return AssemblyKind::Planar;
  }

  if (has_planar_patch && has_nonplanar_patch) {
    return AssemblyKind::Capped;
  }

  const SphereFit sphere = fit_sphere(positions);
  if (sphere.valid) {
    const double allowed_error =
        std::max(settings.vertex_epsilon * 4.0, sphere.radius * 0.08);
    if (sphere.max_error <= allowed_error) {
      return AssemblyKind::Spherical;
    }
  }

  const CylinderFit cylinder = fit_cylinder(positions);
  if (cylinder.valid) {
    const double allowed_error =
        std::max(settings.vertex_epsilon * 4.0, cylinder.radius * 0.08);
    if (cylinder.max_error <= allowed_error &&
        cylinder.axis_extent > settings.vertex_epsilon * 4.0) {
      return AssemblyKind::Cylindrical;
    }
  }

  return AssemblyKind::Arbitrary;
}

PatchBoundary build_boundary(const mtb::map::PatchSummary& patch,
                             std::size_t patch_index,
                             double epsilon) {
  PatchBoundary boundary;
  boundary.patch_index = patch_index;
  boundary.entity_index = patch.entity_index;
  boundary.patch_index_in_entity = patch.patch_index;
  boundary.spans.push_back(make_span(patch, BoundarySide::Top, epsilon));
  boundary.spans.push_back(make_span(patch, BoundarySide::Right, epsilon));
  boundary.spans.push_back(make_span(patch, BoundarySide::Bottom, epsilon));
  boundary.spans.push_back(make_span(patch, BoundarySide::Left, epsilon));
  return boundary;
}

std::size_t unmatched_boundary_span_count(
    const PatchAssembly& assembly, const std::vector<PatchBoundary>& boundaries,
    const std::vector<PatchConnection>& connections) {
  std::map<std::size_t, std::size_t> connected_span_counts;
  for (std::size_t connection_index : assembly.connection_indices) {
    const PatchConnection& connection = connections[connection_index];
    connected_span_counts[connection.patch_a] +=
        std::max<std::size_t>(1, connection.overlapping_span_count);
    connected_span_counts[connection.patch_b] +=
        std::max<std::size_t>(1, connection.overlapping_span_count);
  }

  std::size_t unmatched = 0;
  for (std::size_t patch_index : assembly.patch_indices) {
    const auto boundary = std::find_if(
        boundaries.begin(), boundaries.end(),
        [&](const PatchBoundary& item) { return item.patch_index == patch_index; });
    if (boundary == boundaries.end()) {
      continue;
    }

    const std::size_t connected =
        std::min(boundary->spans.size(), connected_span_counts[patch_index]);
    unmatched += boundary->spans.size() - connected;
  }
  return unmatched;
}

}  // namespace

PatchTopologyBuilder::PatchTopologyBuilder(ConversionSettings settings)
    : settings_(settings) {}

PatchTopology PatchTopologyBuilder::build(
    const std::vector<mtb::map::PatchSummary>& patches) const {
  PatchTopology topology;

  for (std::size_t patch_index = 0; patch_index < patches.size(); ++patch_index) {
    const mtb::map::PatchSummary& patch = patches[patch_index];
    if (patch.malformed || !patch.has_expected_grid_size()) {
      topology.skipped_patch_indices.push_back(patch_index);
      continue;
    }
    topology.boundaries.push_back(
        build_boundary(patch, patch_index, settings_.vertex_epsilon));
  }

  for (std::size_t i = 0; i < topology.boundaries.size(); ++i) {
    for (std::size_t j = i + 1; j < topology.boundaries.size(); ++j) {
      const BoundaryPairAnalysis analysis =
          analyze_boundary_pair(topology.boundaries[i], topology.boundaries[j],
                                settings_.vertex_epsilon);
      if (analysis.shared_vertices == 0 && analysis.overlapping_spans == 0) {
        continue;
      }

      PatchConnection connection;
      connection.patch_a = topology.boundaries[i].patch_index;
      connection.patch_b = topology.boundaries[j].patch_index;
      connection.shared_vertex_count = analysis.shared_vertices;
      connection.overlapping_span_count = analysis.overlapping_spans;
      connection.kind = analysis.overlapping_spans > 0
                            ? PatchConnectionKind::OverlappingBoundarySpan
                            : PatchConnectionKind::SharedBoundaryVertex;
      topology.connections.push_back(connection);
    }
  }

  std::map<std::size_t, std::vector<std::size_t>> adjacency;
  for (std::size_t connection_index = 0;
       connection_index < topology.connections.size(); ++connection_index) {
    const PatchConnection& connection = topology.connections[connection_index];
    adjacency[connection.patch_a].push_back(connection_index);
    adjacency[connection.patch_b].push_back(connection_index);
  }

  std::set<std::size_t> unvisited;
  for (const PatchBoundary& boundary : topology.boundaries) {
    unvisited.insert(boundary.patch_index);
  }

  while (!unvisited.empty()) {
    PatchAssembly assembly;
    assembly.assembly_index = topology.assemblies.size();

    std::queue<std::size_t> frontier;
    frontier.push(*unvisited.begin());
    unvisited.erase(unvisited.begin());

    std::set<std::size_t> assembly_connections;
    while (!frontier.empty()) {
      const std::size_t patch_index = frontier.front();
      frontier.pop();
      assembly.patch_indices.push_back(patch_index);

      for (std::size_t connection_index : adjacency[patch_index]) {
        assembly_connections.insert(connection_index);
        const PatchConnection& connection = topology.connections[connection_index];
        const std::size_t other =
            connection.patch_a == patch_index ? connection.patch_b
                                              : connection.patch_a;
        if (unvisited.erase(other) > 0) {
          frontier.push(other);
        }
      }
    }

    assembly.connection_indices.assign(assembly_connections.begin(),
                                       assembly_connections.end());
    std::sort(assembly.patch_indices.begin(), assembly.patch_indices.end());
    assembly.kind = classify_assembly(patches, assembly.patch_indices, settings_);
    assembly.unmatched_boundary_spans = unmatched_boundary_span_count(
        assembly, topology.boundaries, topology.connections);
    topology.assemblies.push_back(std::move(assembly));
  }

  return topology;
}

bool operator<(const QuantizedVertex& lhs, const QuantizedVertex& rhs) {
  if (lhs.x != rhs.x) {
    return lhs.x < rhs.x;
  }
  if (lhs.y != rhs.y) {
    return lhs.y < rhs.y;
  }
  return lhs.z < rhs.z;
}

bool operator==(const QuantizedVertex& lhs, const QuantizedVertex& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

QuantizedVertex quantize_position(const geometry::Vec3& position,
                                  double epsilon) {
  const double safe_epsilon = epsilon > 0.0 ? epsilon : 0.125;
  return {
      static_cast<std::int64_t>(std::llround(position.x / safe_epsilon)),
      static_cast<std::int64_t>(std::llround(position.y / safe_epsilon)),
      static_cast<std::int64_t>(std::llround(position.z / safe_epsilon)),
  };
}

std::string boundary_side_name(BoundarySide side) {
  switch (side) {
    case BoundarySide::Top:
      return "top";
    case BoundarySide::Right:
      return "right";
    case BoundarySide::Bottom:
      return "bottom";
    case BoundarySide::Left:
      return "left";
  }
  return "unknown";
}

std::string patch_connection_kind_name(PatchConnectionKind kind) {
  switch (kind) {
    case PatchConnectionKind::SharedBoundaryVertex:
      return "shared boundary vertex";
    case PatchConnectionKind::OverlappingBoundarySpan:
      return "overlapping boundary span";
  }
  return "unknown";
}

std::string assembly_kind_name(AssemblyKind kind) {
  switch (kind) {
    case AssemblyKind::Planar:
      return "planar";
    case AssemblyKind::Cylindrical:
      return "cylindrical";
    case AssemblyKind::Spherical:
      return "spherical";
    case AssemblyKind::Capped:
      return "capped";
    case AssemblyKind::Arbitrary:
      return "arbitrary";
  }
  return "unknown";
}

}  // namespace mtb::conversion
