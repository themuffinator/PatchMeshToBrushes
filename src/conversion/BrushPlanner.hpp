#pragma once

#include "conversion/ConversionSettings.hpp"
#include "conversion/PatchTopology.hpp"
#include "geometry/Plane.hpp"
#include "geometry/UvProjection.hpp"
#include "map/MapDocument.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace mtb::conversion {

enum class BrushPlanningStrategy {
  PlanarExtrusion,
  CurvedBackfaceWedge,
  CylinderSector,
  CapExtrusion,
  SphereSector,
  ArbitraryFallback,
};

enum class PlannedFaceRole {
  SourceSurface,
  Support,
};

struct PlannedFace {
  std::vector<geometry::Vec3> vertices;
  std::vector<geometry::UvSample> uv_samples;
  geometry::Plane plane;
  geometry::PlanarUvProjection uv_projection;
  PlannedFaceRole role = PlannedFaceRole::Support;
  std::string material = "textures/common/caulk";
  std::size_t source_patch_index = 0;
  bool has_source_patch = false;
  bool has_uv_projection = false;
};

struct PlannedBrush {
  BrushPlanningStrategy strategy = BrushPlanningStrategy::ArbitraryFallback;
  std::vector<std::size_t> source_patch_indices;
  std::vector<PlannedFace> faces;
  double volume = 0.0;
  bool valid = false;
  std::vector<std::string> diagnostics;
};

struct SegmentationLattice {
  std::size_t patch_count = 0;
  std::size_t source_quad_count = 0;
  std::size_t planned_brush_count = 0;
  std::size_t shared_boundary_vertex_count = 0;
  std::size_t overlapping_boundary_span_count = 0;
  std::size_t planar_merge_count = 0;
  std::size_t skipped_degenerate_brush_count = 0;
  std::size_t source_face_count = 0;
  std::size_t texture_projection_count = 0;
  std::size_t invalid_texture_projection_count = 0;
  double max_texture_projection_error = 0.0;
};

struct AssemblyBrushPlan {
  std::size_t assembly_index = 0;
  AssemblyKind assembly_kind = AssemblyKind::Arbitrary;
  BrushPlanningStrategy primary_strategy = BrushPlanningStrategy::ArbitraryFallback;
  std::vector<std::size_t> patch_indices;
  SegmentationLattice lattice;
  std::vector<PlannedBrush> brushes;
  std::vector<std::string> diagnostics;

  [[nodiscard]] std::size_t invalid_brush_count() const;
};

struct BrushPlanningResult {
  std::vector<AssemblyBrushPlan> assemblies;
  std::size_t total_brush_count = 0;
  std::size_t invalid_brush_count = 0;
};

class BrushPlanner {
 public:
  explicit BrushPlanner(ConversionSettings settings);

  [[nodiscard]] BrushPlanningResult plan(
      const std::vector<mtb::map::PatchSummary>& patches,
      const PatchTopology& topology) const;

 private:
  ConversionSettings settings_;
};

std::string brush_planning_strategy_name(BrushPlanningStrategy strategy);
std::string planned_face_role_name(PlannedFaceRole role);

}  // namespace mtb::conversion
