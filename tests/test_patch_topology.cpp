#include "conversion/PatchTopology.hpp"

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
    std::size_t entity_index = 0, std::size_t patch_index = 0) {
  mtb::map::PatchSummary patch;
  patch.entity_index = entity_index;
  patch.patch_index = patch_index;
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
                         0, patch_index);
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
  return patch_from_grid(grid, 0, patch_index);
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
  return patch_from_grid(grid, 0, patch_index);
}

mtb::conversion::PatchTopology build(
    const std::vector<mtb::map::PatchSummary>& patches) {
  mtb::conversion::ConversionSettings settings;
  settings.vertex_epsilon = 0.25;
  settings.coplanar_epsilon = 0.01;
  return mtb::conversion::PatchTopologyBuilder(settings).build(patches);
}

}  // namespace

int main() {
  {
    const mtb::conversion::QuantizedVertex a =
        mtb::conversion::quantize_position({1.01, 2.02, 3.03}, 0.25);
    const mtb::conversion::QuantizedVertex b =
        mtb::conversion::quantize_position({1.04, 2.00, 3.01}, 0.25);
    require(a == b, "nearby vertices quantize to the same key");
  }

  {
    const std::vector<mtb::map::PatchSummary> patches{
        planar_patch(0.0, 64.0, 0.0, 64.0, 0.0, 0),
        planar_patch(64.0, 128.0, 0.0, 64.0, 0.0, 1),
    };
    const mtb::conversion::PatchTopology topology = build(patches);
    require(topology.boundaries.size() == 2, "two patch boundaries");
    require(topology.connections.size() == 1, "one shared edge connection");
    require(topology.connections[0].shared_vertex_count >= 3,
            "shared edge vertices detected");
    require(topology.assemblies.size() == 1, "shared patches form one assembly");
    require(topology.assemblies[0].patch_indices.size() == 2,
            "assembly contains two patches");
    require(topology.assemblies[0].kind == mtb::conversion::AssemblyKind::Planar,
            "shared planar patches classify planar");
  }

  {
    const std::vector<mtb::map::PatchSummary> patches{
        planar_patch(0.0, 128.0, 0.0, 64.0, 0.0, 0),
        planar_patch(32.0, 96.0, 64.0, 128.0, 0.0, 1),
    };
    const mtb::conversion::PatchTopology topology = build(patches);
    require(topology.connections.size() == 1, "one overlapping span connection");
    require(topology.connections[0].kind ==
                mtb::conversion::PatchConnectionKind::OverlappingBoundarySpan,
            "connection is overlap-based");
    require(topology.connections[0].overlapping_span_count > 0,
            "overlapping span count recorded");
  }

  {
    const std::vector<mtb::map::PatchSummary> patches{cylinder_side()};
    const mtb::conversion::PatchTopology topology = build(patches);
    require(topology.assemblies.size() == 1, "single cylinder assembly");
    require(topology.assemblies[0].kind ==
                mtb::conversion::AssemblyKind::Cylindrical,
            "cylinder side classifies cylindrical");
  }

  {
    const std::vector<mtb::map::PatchSummary> patches{sphere_patch()};
    const mtb::conversion::PatchTopology topology = build(patches);
    require(topology.assemblies.size() == 1, "single sphere assembly");
    require(topology.assemblies[0].kind ==
                mtb::conversion::AssemblyKind::Spherical,
            "sphere patch classifies spherical");
  }

  {
    mtb::map::PatchSummary side = cylinder_side(64.0, 0);
    mtb::map::PatchSummary cap =
        planar_patch(0.0, 64.0, 0.0, 64.0, 0.0, 1);
    cap.control_points[0][0].position = side.control_points[0][0].position;
    cap.control_points[0][1].position = side.control_points[0][1].position;
    cap.control_points[0][2].position = side.control_points[0][2].position;

    const mtb::conversion::PatchTopology topology = build({side, cap});
    require(topology.assemblies.size() == 1, "capped pair forms one assembly");
    require(topology.assemblies[0].kind == mtb::conversion::AssemblyKind::Capped,
            "mixed planar and curved connected patches classify capped");
  }

  return 0;
}
