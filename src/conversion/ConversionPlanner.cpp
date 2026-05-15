#include "conversion/ConversionPlanner.hpp"

#include "geometry/BezierPatch.hpp"
#include "geometry/PlaneFit.hpp"
#include "geometry/UvProjection.hpp"

#include <exception>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace mtb::conversion {
namespace {

std::string format_double(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(4) << value;
  return out.str();
}

std::vector<mtb::geometry::Vec3> patch_positions(
    const mtb::map::PatchSummary& patch) {
  std::vector<mtb::geometry::Vec3> positions;
  positions.reserve(patch.control_point_count());
  for (const std::vector<mtb::map::PatchControlPoint>& row :
       patch.control_points) {
    for (const mtb::map::PatchControlPoint& point : row) {
      positions.push_back(point.position);
    }
  }
  return positions;
}

std::vector<mtb::geometry::UvSample> patch_uv_samples(
    const mtb::map::PatchSummary& patch) {
  std::vector<mtb::geometry::UvSample> samples;
  samples.reserve(patch.control_point_count());
  for (const std::vector<mtb::map::PatchControlPoint>& row :
       patch.control_points) {
    for (const mtb::map::PatchControlPoint& point : row) {
      samples.push_back({point.position, point.uv});
    }
  }
  return samples;
}

mtb::geometry::BezierPatchGrid patch_bezier_grid(
    const mtb::map::PatchSummary& patch) {
  std::vector<std::vector<mtb::geometry::BezierControlPoint>> controls;
  controls.reserve(patch.control_points.size());
  for (const std::vector<mtb::map::PatchControlPoint>& row :
       patch.control_points) {
    std::vector<mtb::geometry::BezierControlPoint> control_row;
    control_row.reserve(row.size());
    for (const mtb::map::PatchControlPoint& point : row) {
      control_row.push_back({point.position, point.uv});
    }
    controls.push_back(std::move(control_row));
  }
  return mtb::geometry::BezierPatchGrid(std::move(controls));
}

void add_geometry_notes(PatchPlan& patch_plan,
                        const ConversionSettings& settings) {
  const mtb::map::PatchSummary& patch = patch_plan.patch;
  if (!patch.has_expected_grid_size()) {
    patch_plan.notes.push_back(
        "Geometry analysis skipped because the control grid is incomplete.");
    return;
  }

  const std::vector<mtb::geometry::Vec3> positions = patch_positions(patch);
  const mtb::geometry::PlanarityClassification planarity =
      mtb::geometry::classify_planarity(positions, settings.coplanar_epsilon);
  patch_plan.notes.push_back(
      std::string("Control grid planarity: ") +
      (planarity.planar ? "planar" : "non-planar") +
      ", max plane distance " + format_double(planarity.fit.max_abs_distance) +
      " units.");

  if (planarity.planar) {
    const mtb::geometry::PlanarUvProjection uv_projection =
        mtb::geometry::fit_planar_uv_projection(patch_uv_samples(patch));
    if (uv_projection.valid) {
      patch_plan.notes.push_back(
          "Planar UV projection fit max error " +
          format_double(uv_projection.max_error) + ".");
    }
  }

  try {
    const mtb::geometry::BezierPatchGrid grid = patch_bezier_grid(patch);
    if (grid.is_valid_quake_grid()) {
      const mtb::geometry::SampledSurface sampled =
          mtb::geometry::sample_surface_by_chord_error(grid, 1.0, 5);
      patch_plan.notes.push_back(
          "Chord-error sampling at 1 unit produced " +
          std::to_string(sampled.samples.size()) + " samples and " +
          std::to_string(sampled.quads.size()) + " quads.");
    }
  } catch (const std::exception& error) {
    patch_plan.notes.push_back(std::string("Bezier sampling skipped: ") +
                               error.what());
  }
}

}  // namespace

ConversionPlanner::ConversionPlanner(ConversionSettings settings)
    : settings_(settings) {}

ConversionPlan ConversionPlanner::plan(const mtb::map::MapDocument& document) const {
  ConversionPlan plan;
  plan.entity_count = document.entities().size();
  plan.brush_count = document.brushes().size();
  plan.parser_diagnostic_count = document.diagnostics().size();

  for (const mtb::map::ParseDiagnostic& diagnostic : document.diagnostics()) {
    plan.diagnostics.push_back(
        std::string(mtb::map::diagnostic_severity_name(diagnostic.severity)) +
        " at offset " + std::to_string(diagnostic.span.start) + ": " +
        diagnostic.message);
  }

  if (document.patches().empty()) {
    plan.diagnostics.push_back("No patchDef blocks were discovered.");
    return plan;
  }

  for (const mtb::map::PatchSummary& patch : document.patches()) {
    PatchPlan patch_plan;
    patch_plan.patch = patch;
    patch_plan.strategy = patch.malformed
                              ? PlannedStrategy::NeedsParserRepair
                              : PlannedStrategy::PendingGeometryAnalysis;
    if (!patch.material.empty()) {
      patch_plan.notes.push_back("Material: " + patch.material + ".");
    }

    if (patch.dimensions.parsed) {
      patch_plan.notes.push_back(
          "Parsed control grid: " + std::to_string(patch.dimensions.rows) +
          " rows x " + std::to_string(patch.dimensions.columns) +
          " columns, " + std::to_string(patch.control_point_count()) +
          " control points.");
    }

    if (!patch.malformed) {
      add_geometry_notes(patch_plan, settings_);
    }

    patch_plan.notes.push_back(
        "Geometry strategy will be selected after topology and curvature "
        "analysis.");
    patch_plan.notes.push_back(
        "Minimum brush thickness setting is " +
        std::to_string(settings_.min_brush_thickness) + " units.");
    plan.patches.push_back(std::move(patch_plan));
  }

  plan.diagnostics.push_back(
      "Patch discovery complete. Geometry conversion is scaffolded but not yet "
      "implemented.");
  return plan;
}

std::string planned_strategy_name(PlannedStrategy strategy) {
  switch (strategy) {
    case PlannedStrategy::PendingGeometryAnalysis:
      return "pending geometry analysis";
    case PlannedStrategy::NeedsParserRepair:
      return "needs parser repair";
  }

  return "unknown";
}

std::string render_markdown_report(const ConversionPlan& plan,
                                   const ConversionSettings& settings) {
  std::ostringstream out;
  out << "# MeshToBrushes Conversion Report\n\n";
  out << "## Settings\n\n";
  out << "- Minimum brush thickness: " << settings.min_brush_thickness << "\n";
  out << "- Vertex epsilon: " << settings.vertex_epsilon << "\n";
  out << "- Coplanar epsilon: " << settings.coplanar_epsilon << "\n";
  out << "- Preserve patches: " << (settings.preserve_patches ? "yes" : "no")
      << "\n";
  out << "- Replace patches: " << (settings.replace_patches ? "yes" : "no")
      << "\n\n";

  out << "## Summary\n\n";
  out << "- Parsed entities: " << plan.entity_count << "\n";
  out << "- Parsed brushes: " << plan.brush_count << "\n";
  out << "- Discovered patches: " << plan.patches.size() << "\n";
  out << "- Parser diagnostics: " << plan.parser_diagnostic_count << "\n\n";

  if (!plan.diagnostics.empty()) {
    out << "## Diagnostics\n\n";
    for (const std::string& diagnostic : plan.diagnostics) {
      out << "- " << diagnostic << "\n";
    }
    out << "\n";
  }

  out << "## Patch Plans\n\n";
  if (plan.patches.empty()) {
    out << "No patch plans were created.\n";
    return out.str();
  }

  for (const PatchPlan& patch : plan.patches) {
    out << "### Entity " << patch.patch.entity_index << ", Patch "
        << patch.patch.patch_index << "\n\n";
    out << "- Kind: " << mtb::map::patch_kind_name(patch.patch.kind) << "\n";
    out << "- Source offset: " << patch.patch.keyword_span.start << "\n";
    out << "- Strategy: " << planned_strategy_name(patch.strategy) << "\n";
    for (const std::string& note : patch.notes) {
      out << "- Note: " << note << "\n";
    }
    out << "\n";
  }

  return out.str();
}

}  // namespace mtb::conversion
