#include "conversion/BrushPlanner.hpp"

#include "geometry/BezierPatch.hpp"
#include "geometry/PlaneFit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>
#include <optional>

namespace mtb::conversion {
namespace {

constexpr const char* kCaulkMaterial = "common/caulk";

struct SurfaceCell {
  std::array<geometry::SurfaceSample, 4> corners;
};

struct PrismCorner {
  geometry::SurfaceSample sample;
  geometry::Vec3 back;
};

geometry::Vec3 polygon_area_vector(const std::vector<geometry::Vec3>& vertices) {
  geometry::Vec3 area;
  if (vertices.size() < 3) {
    return area;
  }

  for (std::size_t index = 0; index < vertices.size(); ++index) {
    const geometry::Vec3& current = vertices[index];
    const geometry::Vec3& next = vertices[(index + 1) % vertices.size()];
    area = area + geometry::cross(current, next);
  }
  return area * 0.5;
}

std::optional<geometry::Plane> plane_from_face(
    const std::vector<geometry::Vec3>& vertices) {
  if (vertices.size() < 3) {
    return std::nullopt;
  }

  const geometry::Vec3 area = polygon_area_vector(vertices);
  if (geometry::length_squared(area) == 0.0) {
    return std::nullopt;
  }
  return geometry::plane_from_point_normal(vertices.front(), area);
}

double polygon_area(const std::vector<geometry::Vec3>& vertices) {
  return geometry::length(polygon_area_vector(vertices));
}

geometry::Vec3 average_point(const std::vector<geometry::Vec3>& points) {
  geometry::Vec3 sum;
  for (const geometry::Vec3& point : points) {
    sum = sum + point;
  }
  return points.empty() ? sum : sum / static_cast<double>(points.size());
}

std::vector<geometry::Vec3> unique_vertices(const PlannedBrush& brush,
                                            double epsilon) {
  std::vector<geometry::Vec3> vertices;
  for (const PlannedFace& face : brush.faces) {
    for (const geometry::Vec3& point : face.vertices) {
      bool exists = false;
      for (const geometry::Vec3& vertex : vertices) {
        if (geometry::distance(vertex, point) <= epsilon) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        vertices.push_back(point);
      }
    }
  }
  return vertices;
}

double brush_volume(const PlannedBrush& brush, double epsilon) {
  const std::vector<geometry::Vec3> vertices = unique_vertices(brush, epsilon);
  const geometry::Vec3 center = average_point(vertices);

  double volume = 0.0;
  for (const PlannedFace& face : brush.faces) {
    if (face.vertices.size() < 3) {
      continue;
    }
    for (std::size_t index = 1; index + 1 < face.vertices.size(); ++index) {
      const geometry::Vec3 a = face.vertices[0] - center;
      const geometry::Vec3 b = face.vertices[index] - center;
      const geometry::Vec3 c = face.vertices[index + 1] - center;
      volume += std::abs(geometry::dot(a, geometry::cross(b, c))) / 6.0;
    }
  }
  return volume;
}

void validate_brush(PlannedBrush& brush, const ConversionSettings& settings) {
  if (brush.faces.size() < 4) {
    brush.diagnostics.push_back("Brush has fewer than four faces.");
  }

  for (PlannedFace& face : brush.faces) {
    const std::optional<geometry::Plane> plane = plane_from_face(face.vertices);
    if (!plane.has_value()) {
      brush.diagnostics.push_back("Brush contains a degenerate face.");
      continue;
    }
    face.plane = *plane;
    if (polygon_area(face.vertices) < settings.min_face_area) {
      brush.diagnostics.push_back("Brush contains a face below the minimum area.");
    }
  }

  brush.volume = brush_volume(brush, settings.vertex_epsilon);
  if (brush.volume <= settings.min_face_area) {
    brush.diagnostics.push_back("Brush volume is too small.");
  }

  brush.valid = brush.diagnostics.empty();
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

geometry::BezierPatchGrid bezier_grid_from_patch(
    const mtb::map::PatchSummary& patch) {
  std::vector<std::vector<geometry::BezierControlPoint>> controls;
  controls.reserve(patch.control_points.size());
  for (const std::vector<mtb::map::PatchControlPoint>& row :
       patch.control_points) {
    std::vector<geometry::BezierControlPoint> control_row;
    control_row.reserve(row.size());
    for (const mtb::map::PatchControlPoint& point : row) {
      control_row.push_back({point.position, point.uv});
    }
    controls.push_back(std::move(control_row));
  }
  return geometry::BezierPatchGrid(std::move(controls));
}

std::vector<SurfaceCell> sampled_cells(const mtb::map::PatchSummary& patch,
                                       const ConversionSettings& settings) {
  std::vector<SurfaceCell> cells;
  const geometry::BezierPatchGrid grid = bezier_grid_from_patch(patch);
  if (!grid.is_valid_quake_grid()) {
    return cells;
  }

  const geometry::SampledSurface sampled = geometry::sample_surface_by_chord_error(
      grid, settings.brush_plan_chord_error, settings.brush_plan_max_depth);
  cells.reserve(sampled.quads.size());
  for (const geometry::SurfaceQuad& quad : sampled.quads) {
    cells.push_back({{
        sampled.samples[quad.a],
        sampled.samples[quad.b],
        sampled.samples[quad.c],
        sampled.samples[quad.d],
    }});
  }
  return cells;
}

std::vector<SurfaceCell> planar_patch_cells(
    const mtb::map::PatchSummary& patch) {
  std::vector<SurfaceCell> cells;
  if (!patch.has_expected_grid_size()) {
    return cells;
  }

  const std::size_t tile_rows = (patch.dimensions.rows - 1) / 2;
  const std::size_t tile_columns = (patch.dimensions.columns - 1) / 2;
  cells.reserve(tile_rows * tile_columns);

  for (std::size_t tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (std::size_t tile_column = 0; tile_column < tile_columns;
         ++tile_column) {
      const std::size_t row = tile_row * 2;
      const std::size_t column = tile_column * 2;
      const double u0 = static_cast<double>(tile_column) /
                        static_cast<double>(tile_columns);
      const double u1 = static_cast<double>(tile_column + 1) /
                        static_cast<double>(tile_columns);
      const double v0 =
          static_cast<double>(tile_row) / static_cast<double>(tile_rows);
      const double v1 = static_cast<double>(tile_row + 1) /
                        static_cast<double>(tile_rows);

      const mtb::map::PatchControlPoint& top_left =
          patch.control_points[row][column];
      const mtb::map::PatchControlPoint& top_right =
          patch.control_points[row][column + 2];
      const mtb::map::PatchControlPoint& bottom_right =
          patch.control_points[row + 2][column + 2];
      const mtb::map::PatchControlPoint& bottom_left =
          patch.control_points[row + 2][column];

      cells.push_back(SurfaceCell{std::array<geometry::SurfaceSample, 4>{{
          {top_left.position, top_left.uv, u0, v0},
          {top_right.position, top_right.uv, u1, v0},
          {bottom_right.position, bottom_right.uv, u1, v1},
          {bottom_left.position, bottom_left.uv, u0, v1},
      }}});
    }
  }

  return cells;
}

geometry::Vec3 cell_normal(const SurfaceCell& cell) {
  std::vector<geometry::Vec3> positions;
  positions.reserve(cell.corners.size());
  for (const geometry::SurfaceSample& corner : cell.corners) {
    positions.push_back(corner.position);
  }

  const geometry::Vec3 normal = geometry::normalized(polygon_area_vector(positions));
  if (geometry::length_squared(normal) != 0.0) {
    return normal;
  }

  return geometry::normalized(geometry::cross(
      cell.corners[2].position - cell.corners[0].position,
      cell.corners[3].position - cell.corners[1].position));
}

PlannedFace make_face(std::vector<geometry::Vec3> vertices, PlannedFaceRole role,
                      std::string material, std::size_t source_patch_index,
                      bool has_source_patch) {
  PlannedFace face;
  face.vertices = std::move(vertices);
  face.role = role;
  face.material = std::move(material);
  face.source_patch_index = source_patch_index;
  face.has_source_patch = has_source_patch;
  return face;
}

std::vector<geometry::UvSample> source_uv_samples(
    const std::vector<PrismCorner>& corners) {
  std::vector<geometry::UvSample> samples;
  samples.reserve(corners.size());
  for (const PrismCorner& corner : corners) {
    samples.push_back({corner.sample.position, corner.sample.uv});
  }
  return samples;
}

std::vector<PrismCorner> prism_corners(
    const SurfaceCell& cell, const std::array<geometry::Vec3, 4>& back,
    double epsilon) {
  std::vector<PrismCorner> corners;
  corners.reserve(cell.corners.size());

  for (std::size_t index = 0; index < cell.corners.size(); ++index) {
    if (!corners.empty() &&
        geometry::distance(corners.back().sample.position,
                           cell.corners[index].position) <= epsilon) {
      continue;
    }
    corners.push_back({cell.corners[index], back[index]});
  }

  if (corners.size() > 1 &&
      geometry::distance(corners.front().sample.position,
                         corners.back().sample.position) <= epsilon) {
    corners.pop_back();
  }

  return corners;
}

PlannedBrush make_prism_brush(
    const SurfaceCell& cell, const std::array<geometry::Vec3, 4>& back,
    const mtb::map::PatchSummary& patch, std::size_t patch_index,
    BrushPlanningStrategy strategy, const ConversionSettings& settings) {
  PlannedBrush brush;
  brush.strategy = strategy;
  brush.source_patch_indices.push_back(patch_index);
  const std::vector<PrismCorner> corners =
      prism_corners(cell, back, settings.vertex_epsilon);
  if (corners.size() < 3) {
    brush.diagnostics.push_back(
        "Brush source cell has fewer than three unique vertices.");
    return brush;
  }

  std::vector<geometry::Vec3> front;
  std::vector<geometry::Vec3> back_vertices;
  front.reserve(corners.size());
  back_vertices.reserve(corners.size());
  for (const PrismCorner& corner : corners) {
    front.push_back(corner.sample.position);
    back_vertices.push_back(corner.back);
  }

  PlannedFace source_face =
      make_face(front, PlannedFaceRole::SourceSurface, patch.material,
                patch_index, true);
  source_face.uv_samples = source_uv_samples(corners);
  source_face.uv_projection =
      geometry::fit_planar_uv_projection(source_face.uv_samples);
  source_face.has_uv_projection = source_face.uv_projection.valid;
  brush.faces.push_back(std::move(source_face));

  std::vector<geometry::Vec3> back_face = back_vertices;
  std::reverse(back_face.begin(), back_face.end());
  brush.faces.push_back(make_face(std::move(back_face),
                                  PlannedFaceRole::Support, kCaulkMaterial,
                                  patch_index, false));

  for (std::size_t index = 0; index < front.size(); ++index) {
    const std::size_t next = (index + 1) % front.size();
    brush.faces.push_back(make_face(
        {front[index], back_vertices[index], back_vertices[next], front[next]},
        PlannedFaceRole::Support, kCaulkMaterial, patch_index, false));
  }

  validate_brush(brush, settings);
  return brush;
}

PlannedBrush make_backface_brush(const SurfaceCell& cell,
                                 const mtb::map::PatchSummary& patch,
                                 std::size_t patch_index,
                                 BrushPlanningStrategy strategy,
                                 const ConversionSettings& settings) {
  geometry::Vec3 normal = cell_normal(cell);
  if (geometry::length_squared(normal) == 0.0) {
    normal = {0.0, 0.0, 1.0};
  }

  std::array<geometry::Vec3, 4> back;
  for (std::size_t index = 0; index < back.size(); ++index) {
    back[index] =
        cell.corners[index].position - normal * settings.min_brush_thickness;
  }
  return make_prism_brush(cell, back, patch, patch_index, strategy, settings);
}

std::optional<geometry::Vec3> sphere_center(
    const std::vector<geometry::Vec3>& points) {
  if (points.size() < 4) {
    return std::nullopt;
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

  for (std::size_t column = 0; column < rhs.size(); ++column) {
    std::size_t pivot = column;
    for (std::size_t row = column + 1; row < rhs.size(); ++row) {
      if (std::abs(normal[row][column]) > std::abs(normal[pivot][column])) {
        pivot = row;
      }
    }
    if (std::abs(normal[pivot][column]) < 1e-12) {
      return std::nullopt;
    }
    if (pivot != column) {
      std::swap(normal[pivot], normal[column]);
      std::swap(rhs[pivot], rhs[column]);
    }
    const double divisor = normal[column][column];
    for (std::size_t item = column; item < rhs.size(); ++item) {
      normal[column][item] /= divisor;
    }
    rhs[column] /= divisor;
    for (std::size_t row = 0; row < rhs.size(); ++row) {
      if (row == column) {
        continue;
      }
      const double factor = normal[row][column];
      for (std::size_t item = column; item < rhs.size(); ++item) {
        normal[row][item] -= factor * normal[column][item];
      }
      rhs[row] -= factor * rhs[column];
    }
  }

  return geometry::Vec3{-rhs[0] * 0.5, -rhs[1] * 0.5, -rhs[2] * 0.5};
}

PlannedBrush make_sphere_sector_brush(const SurfaceCell& cell,
                                      const mtb::map::PatchSummary& patch,
                                      std::size_t patch_index,
                                      const geometry::Vec3& center,
                                      const ConversionSettings& settings) {
  std::array<geometry::Vec3, 4> back;
  const geometry::Vec3 fallback = cell_normal(cell);
  for (std::size_t index = 0; index < back.size(); ++index) {
    geometry::Vec3 radial =
        geometry::normalized(cell.corners[index].position - center);
    if (geometry::length_squared(radial) == 0.0) {
      radial = fallback;
    }
    back[index] =
        cell.corners[index].position - radial * settings.min_brush_thickness;
  }
  return make_prism_brush(cell, back, patch, patch_index,
                          BrushPlanningStrategy::SphereSector, settings);
}

bool patch_is_planar(const mtb::map::PatchSummary& patch,
                     const ConversionSettings& settings) {
  return geometry::classify_planarity(patch_positions(patch),
                                      settings.coplanar_epsilon)
      .planar;
}

BrushPlanningStrategy primary_strategy_for_assembly(
    const PatchAssembly& assembly,
    const std::vector<mtb::map::PatchSummary>& patches,
    const ConversionSettings& settings) {
  switch (assembly.kind) {
    case AssemblyKind::Planar:
      return BrushPlanningStrategy::PlanarExtrusion;
    case AssemblyKind::Cylindrical:
      return BrushPlanningStrategy::CylinderSector;
    case AssemblyKind::Spherical:
      return BrushPlanningStrategy::SphereSector;
    case AssemblyKind::Capped:
      return BrushPlanningStrategy::CylinderSector;
    case AssemblyKind::Arbitrary:
      if (assembly.patch_indices.size() == 1 &&
          !patch_is_planar(patches[assembly.patch_indices.front()], settings)) {
        return BrushPlanningStrategy::CurvedBackfaceWedge;
      }
      return BrushPlanningStrategy::ArbitraryFallback;
  }
  return BrushPlanningStrategy::ArbitraryFallback;
}

std::vector<geometry::Vec3> assembly_positions(
    const std::vector<mtb::map::PatchSummary>& patches,
    const PatchAssembly& assembly) {
  std::vector<geometry::Vec3> points;
  for (std::size_t patch_index : assembly.patch_indices) {
    const std::vector<geometry::Vec3> positions =
        patch_positions(patches[patch_index]);
    points.insert(points.end(), positions.begin(), positions.end());
  }
  return points;
}

std::size_t count_shared_boundary_vertices(
    const PatchAssembly& assembly, const PatchTopology& topology) {
  std::size_t count = 0;
  for (std::size_t connection_index : assembly.connection_indices) {
    count += topology.connections[connection_index].shared_vertex_count;
  }
  return count;
}

std::size_t count_overlapping_spans(const PatchAssembly& assembly,
                                    const PatchTopology& topology) {
  std::size_t count = 0;
  for (std::size_t connection_index : assembly.connection_indices) {
    count += topology.connections[connection_index].overlapping_span_count;
  }
  return count;
}

void append_patch_brushes(AssemblyBrushPlan& plan,
                          const mtb::map::PatchSummary& patch,
                          std::size_t patch_index,
                          BrushPlanningStrategy strategy,
                          const ConversionSettings& settings,
                          const std::optional<geometry::Vec3>& sphere_center) {
  if (!patch.has_expected_grid_size()) {
    plan.diagnostics.push_back("Skipped patch with incomplete control grid.");
    return;
  }

  if (strategy == BrushPlanningStrategy::PlanarExtrusion ||
      strategy == BrushPlanningStrategy::CapExtrusion) {
    const std::vector<SurfaceCell> cells = planar_patch_cells(patch);
    plan.lattice.source_quad_count += cells.size();
    for (const SurfaceCell& cell : cells) {
      PlannedBrush brush =
          make_backface_brush(cell, patch, patch_index, strategy, settings);
      if (brush.valid) {
        plan.brushes.push_back(std::move(brush));
        ++plan.lattice.planar_merge_count;
      } else {
        ++plan.lattice.skipped_degenerate_brush_count;
      }
    }
    return;
  }

  const std::vector<SurfaceCell> cells = sampled_cells(patch, settings);
  plan.lattice.source_quad_count += cells.size();
  for (const SurfaceCell& cell : cells) {
    PlannedBrush brush;
    if (strategy == BrushPlanningStrategy::SphereSector &&
        sphere_center.has_value()) {
      brush = make_sphere_sector_brush(cell, patch, patch_index, *sphere_center,
                                       settings);
    } else {
      brush = make_backface_brush(cell, patch, patch_index, strategy, settings);
    }

    if (brush.valid) {
      plan.brushes.push_back(std::move(brush));
    } else {
      ++plan.lattice.skipped_degenerate_brush_count;
    }
  }
}

void add_texture_projection_metrics(AssemblyBrushPlan& plan) {
  for (const PlannedBrush& brush : plan.brushes) {
    for (const PlannedFace& face : brush.faces) {
      if (face.role != PlannedFaceRole::SourceSurface) {
        continue;
      }

      ++plan.lattice.source_face_count;
      if (face.has_uv_projection) {
        ++plan.lattice.texture_projection_count;
        plan.lattice.max_texture_projection_error =
            std::max(plan.lattice.max_texture_projection_error,
                     face.uv_projection.max_error);
      } else {
        ++plan.lattice.invalid_texture_projection_count;
      }
    }
  }
}

}  // namespace

BrushPlanner::BrushPlanner(ConversionSettings settings)
    : settings_(settings) {}

BrushPlanningResult BrushPlanner::plan(
    const std::vector<mtb::map::PatchSummary>& patches,
    const PatchTopology& topology) const {
  BrushPlanningResult result;

  for (const PatchAssembly& assembly : topology.assemblies) {
    AssemblyBrushPlan assembly_plan;
    assembly_plan.assembly_index = assembly.assembly_index;
    assembly_plan.assembly_kind = assembly.kind;
    assembly_plan.patch_indices = assembly.patch_indices;
    assembly_plan.primary_strategy =
        primary_strategy_for_assembly(assembly, patches, settings_);
    assembly_plan.lattice.patch_count = assembly.patch_indices.size();
    assembly_plan.lattice.shared_boundary_vertex_count =
        count_shared_boundary_vertices(assembly, topology);
    assembly_plan.lattice.overlapping_boundary_span_count =
        count_overlapping_spans(assembly, topology);

    const std::optional<geometry::Vec3> fitted_sphere_center =
        assembly.kind == AssemblyKind::Spherical
            ? sphere_center(assembly_positions(patches, assembly))
            : std::nullopt;

    for (std::size_t patch_index : assembly.patch_indices) {
      const mtb::map::PatchSummary& patch = patches[patch_index];
      BrushPlanningStrategy strategy = assembly_plan.primary_strategy;
      if (assembly.kind == AssemblyKind::Capped) {
        strategy = patch_is_planar(patch, settings_)
                       ? BrushPlanningStrategy::CapExtrusion
                       : BrushPlanningStrategy::CylinderSector;
      } else if (assembly.kind == AssemblyKind::Arbitrary &&
                 patch_is_planar(patch, settings_)) {
        strategy = BrushPlanningStrategy::PlanarExtrusion;
      }

      append_patch_brushes(assembly_plan, patch, patch_index, strategy,
                           settings_, fitted_sphere_center);
    }

    assembly_plan.lattice.planned_brush_count = assembly_plan.brushes.size();
    add_texture_projection_metrics(assembly_plan);
    for (const PlannedBrush& brush : assembly_plan.brushes) {
      ++result.total_brush_count;
      if (!brush.valid) {
        ++result.invalid_brush_count;
      }
    }
    result.assemblies.push_back(std::move(assembly_plan));
  }

  return result;
}

std::size_t AssemblyBrushPlan::invalid_brush_count() const {
  std::size_t count = 0;
  for (const PlannedBrush& brush : brushes) {
    if (!brush.valid) {
      ++count;
    }
  }
  return count;
}

std::string brush_planning_strategy_name(BrushPlanningStrategy strategy) {
  switch (strategy) {
    case BrushPlanningStrategy::PlanarExtrusion:
      return "planar extrusion";
    case BrushPlanningStrategy::CurvedBackfaceWedge:
      return "curved backface wedge";
    case BrushPlanningStrategy::CylinderSector:
      return "cylinder sector";
    case BrushPlanningStrategy::CapExtrusion:
      return "cap extrusion";
    case BrushPlanningStrategy::SphereSector:
      return "sphere sector";
    case BrushPlanningStrategy::ArbitraryFallback:
      return "arbitrary fallback";
  }
  return "unknown";
}

std::string planned_face_role_name(PlannedFaceRole role) {
  switch (role) {
    case PlannedFaceRole::SourceSurface:
      return "source surface";
    case PlannedFaceRole::Support:
      return "support";
  }
  return "unknown";
}

}  // namespace mtb::conversion
