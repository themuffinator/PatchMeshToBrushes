#include "conversion/BrushPlanner.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "requirement failed: " << message << "\n";
    std::exit(1);
  }
}

mtb::map::PatchControlPoint point(double x, double y, double z) {
  return {{x, y, z}, {x / 64.0, y / 64.0}, {}};
}

mtb::map::PatchSummary patch_from_grid(
    std::vector<std::vector<mtb::map::PatchControlPoint>> grid,
    std::size_t patch_index = 0, std::string material = "textures/test/source") {
  mtb::map::PatchSummary patch;
  patch.entity_index = 0;
  patch.patch_index = patch_index;
  patch.material = std::move(material);
  patch.dimensions.rows = grid.size();
  patch.dimensions.columns = grid.empty() ? 0 : grid.front().size();
  patch.dimensions.parsed = true;
  patch.control_points = std::move(grid);
  return patch;
}

mtb::map::PatchSummary planar_patch(double x0, double x1, double y0, double y1,
                                    double z = 0.0,
                                    std::size_t patch_index = 0) {
  return patch_from_grid({
                             {point(x0, y0, z), point((x0 + x1) * 0.5, y0, z),
                              point(x1, y0, z)},
                             {point(x0, (y0 + y1) * 0.5, z),
                              point((x0 + x1) * 0.5, (y0 + y1) * 0.5, z),
                              point(x1, (y0 + y1) * 0.5, z)},
                             {point(x0, y1, z), point((x0 + x1) * 0.5, y1, z),
                              point(x1, y1, z)},
                         },
                         patch_index);
}

mtb::map::PatchSummary planar_subdivided_patch() {
  std::vector<std::vector<mtb::map::PatchControlPoint>> grid;
  for (int row = 0; row < 5; ++row) {
    std::vector<mtb::map::PatchControlPoint> controls;
    for (int column = 0; column < 5; ++column) {
      controls.push_back(point(static_cast<double>(column) * 32.0,
                               static_cast<double>(row) * 32.0, 0.0));
    }
    grid.push_back(std::move(controls));
  }
  return patch_from_grid(std::move(grid));
}

mtb::map::PatchSummary curved_sheet(std::size_t patch_index = 0) {
  return patch_from_grid({
                             {point(0.0, 0.0, 0.0), point(32.0, 0.0, 0.0),
                              point(64.0, 0.0, 0.0)},
                             {point(0.0, 32.0, 0.0), point(32.0, 32.0, 32.0),
                              point(64.0, 32.0, 0.0)},
                             {point(0.0, 64.0, 0.0), point(32.0, 64.0, -12.0),
                              point(64.0, 64.0, 0.0)},
                         },
                         patch_index);
}

mtb::map::PatchSummary cylinder_side(double radius = 64.0,
                                     std::size_t patch_index = 0) {
  std::vector<std::vector<mtb::map::PatchControlPoint>> grid;
  for (double z : {0.0, 64.0, 128.0}) {
    std::vector<mtb::map::PatchControlPoint> row;
    for (double degrees : {0.0, 45.0, 90.0}) {
      const double radians = degrees * 3.14159265358979323846 / 180.0;
      row.push_back(point(std::cos(radians) * radius,
                          std::sin(radians) * radius, z));
    }
    grid.push_back(row);
  }
  return patch_from_grid(grid, patch_index);
}

mtb::map::PatchSummary cap_for_cylinder(const mtb::map::PatchSummary& side,
                                        std::size_t patch_index = 1) {
  const auto& edge = side.control_points.front();
  return patch_from_grid({
                             {edge[0], edge[1], edge[2]},
                             {point(32.0, 0.0, 0.0), point(24.0, 24.0, 0.0),
                              point(0.0, 32.0, 0.0)},
                             {point(32.0, 8.0, 0.0), point(24.0, 24.0, 0.0),
                              point(8.0, 32.0, 0.0)},
                         },
                         patch_index);
}

mtb::map::PatchSummary sphere_patch(double radius = 64.0,
                                    std::size_t patch_index = 0) {
  std::vector<std::vector<mtb::map::PatchControlPoint>> grid;
  for (double polar : {35.0, 55.0, 75.0}) {
    std::vector<mtb::map::PatchControlPoint> row;
    for (double azimuth : {10.0, 35.0, 60.0}) {
      const double p = polar * 3.14159265358979323846 / 180.0;
      const double a = azimuth * 3.14159265358979323846 / 180.0;
      row.push_back(point(std::sin(p) * std::cos(a) * radius,
                          std::sin(p) * std::sin(a) * radius,
                          std::cos(p) * radius));
    }
    grid.push_back(row);
  }
  return patch_from_grid(grid, patch_index);
}

mtb::conversion::BrushPlanningResult plan_brushes(
    const std::vector<mtb::map::PatchSummary>& patches) {
  mtb::conversion::ConversionSettings settings;
  settings.vertex_epsilon = 0.25;
  settings.coplanar_epsilon = 0.01;
  settings.min_brush_thickness = 8.0;
  settings.brush_plan_chord_error = 4.0;
  settings.brush_plan_max_depth = 3;
  const mtb::conversion::PatchTopology topology =
      mtb::conversion::PatchTopologyBuilder(settings).build(patches);
  return mtb::conversion::BrushPlanner(settings).plan(patches, topology);
}

bool has_strategy(const mtb::conversion::AssemblyBrushPlan& assembly,
                  mtb::conversion::BrushPlanningStrategy strategy) {
  for (const mtb::conversion::PlannedBrush& brush : assembly.brushes) {
    if (brush.strategy == strategy) {
      return true;
    }
  }
  return false;
}

void require_all_valid(const mtb::conversion::BrushPlanningResult& result) {
  require(result.total_brush_count > 0, "brush planning produced brushes");
  if (result.invalid_brush_count != 0) {
    for (const mtb::conversion::AssemblyBrushPlan& assembly : result.assemblies) {
      for (const mtb::conversion::PlannedBrush& brush : assembly.brushes) {
        if (!brush.valid) {
          std::cerr << "invalid brush in assembly " << assembly.assembly_index
                    << " strategy "
                    << mtb::conversion::brush_planning_strategy_name(brush.strategy)
                    << " volume " << brush.volume << "\n";
          for (const std::string& diagnostic : brush.diagnostics) {
            std::cerr << "  " << diagnostic << "\n";
          }
        }
      }
    }
  }
  require(result.invalid_brush_count == 0, "all planned brushes are valid");
  for (const mtb::conversion::AssemblyBrushPlan& assembly : result.assemblies) {
    require(assembly.invalid_brush_count() == 0, "assembly has no invalid brushes");
  }
}

}  // namespace

int main() {
  {
    const mtb::conversion::BrushPlanningResult result =
        plan_brushes({planar_patch(0.0, 64.0, 0.0, 64.0)});
    require_all_valid(result);
    require(result.assemblies.size() == 1, "one planar assembly");
    require(result.assemblies[0].primary_strategy ==
                mtb::conversion::BrushPlanningStrategy::PlanarExtrusion,
            "planar assembly uses planar extrusion");
    require(result.assemblies[0].brushes.size() == 1,
            "planar patch cells are merged into one extrusion");
    require(result.assemblies[0].lattice.planar_merge_count == 1,
            "planar merge pass recorded");
    require(result.assemblies[0].brushes[0].faces.front().material ==
                "textures/test/source",
            "source face inherits patch material");
  }

  {
    const mtb::conversion::BrushPlanningResult result =
        plan_brushes({planar_subdivided_patch()});
    require_all_valid(result);
    require(result.assemblies[0].brushes.size() == 4,
            "planar 5x5 patch keeps its four authored subdivisions");
    require(result.assemblies[0].lattice.source_quad_count == 4,
            "subdivided planar patch records four source quads");
    require(result.assemblies[0].lattice.planar_merge_count == 4,
            "subdivided planar patch emits one extrusion per Bezier tile");
  }

  {
    const mtb::conversion::BrushPlanningResult result =
        plan_brushes({curved_sheet()});
    require_all_valid(result);
    require(result.assemblies[0].primary_strategy ==
                mtb::conversion::BrushPlanningStrategy::CurvedBackfaceWedge,
            "simple open curved sheet uses backface wedges");
    require(result.assemblies[0].brushes.size() > 1,
            "curved sheet is subdivided into wedge brushes");
  }

  {
    const mtb::conversion::BrushPlanningResult result =
        plan_brushes({cylinder_side()});
    require_all_valid(result);
    require(result.assemblies[0].primary_strategy ==
                mtb::conversion::BrushPlanningStrategy::CylinderSector,
            "cylinder assembly uses cylinder sectors");
    require(has_strategy(result.assemblies[0],
                         mtb::conversion::BrushPlanningStrategy::CylinderSector),
            "cylinder sector brushes exist");
  }

  {
    const mtb::conversion::BrushPlanningResult result =
        plan_brushes({sphere_patch()});
    require_all_valid(result);
    require(result.assemblies[0].primary_strategy ==
                mtb::conversion::BrushPlanningStrategy::SphereSector,
            "sphere assembly uses sphere sectors");
  }

  {
    const mtb::map::PatchSummary side = cylinder_side(64.0, 0);
    const mtb::map::PatchSummary cap = cap_for_cylinder(side, 1);
    const mtb::conversion::BrushPlanningResult result = plan_brushes({side, cap});
    require_all_valid(result);
    require(result.assemblies.size() == 1, "capped assembly remains grouped");
    require(has_strategy(result.assemblies[0],
                         mtb::conversion::BrushPlanningStrategy::CylinderSector),
            "capped assembly includes cylinder sector brushes");
    require(has_strategy(result.assemblies[0],
                         mtb::conversion::BrushPlanningStrategy::CapExtrusion),
            "capped assembly includes cap extrusion brushes");
  }

  {
    mtb::map::PatchSummary first = curved_sheet(0);
    mtb::map::PatchSummary second = curved_sheet(1);
    for (std::vector<mtb::map::PatchControlPoint>& row : second.control_points) {
      for (mtb::map::PatchControlPoint& control : row) {
        control.position.x += 64.0;
      }
    }
    for (std::size_t row = 0; row < first.control_points.size(); ++row) {
      second.control_points[row].front().position =
          first.control_points[row].back().position;
    }

    const mtb::conversion::BrushPlanningResult result =
        plan_brushes({first, second});
    require_all_valid(result);
    require(result.assemblies.size() == 1, "arbitrary connected patches group");
    require(result.assemblies[0].primary_strategy ==
                mtb::conversion::BrushPlanningStrategy::ArbitraryFallback,
            "complex arbitrary assembly uses fallback subdivision");
  }

  return 0;
}
