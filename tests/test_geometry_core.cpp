#include "geometry/BezierPatch.hpp"
#include "geometry/PlaneFit.hpp"
#include "geometry/UvProjection.hpp"
#include "geometry/Winding.hpp"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

bool near(double lhs, double rhs, double epsilon = 1e-6) {
  return std::abs(lhs - rhs) <= epsilon;
}

mtb::geometry::BezierPatchGrid curved_patch() {
  using mtb::geometry::BezierControlPoint;
  std::vector<std::vector<BezierControlPoint>> controls;
  for (int row = 0; row < 3; ++row) {
    std::vector<BezierControlPoint> control_row;
    for (int column = 0; column < 3; ++column) {
      const double z = (row == 1 && column == 1) ? 32.0 : 0.0;
      control_row.push_back({
          {static_cast<double>(column) * 64.0, static_cast<double>(row) * 64.0, z},
          {static_cast<double>(column), static_cast<double>(row)},
      });
    }
    controls.push_back(control_row);
  }
  return mtb::geometry::BezierPatchGrid(controls);
}

mtb::geometry::BezierPatchGrid flat_patch() {
  using mtb::geometry::BezierControlPoint;
  std::vector<std::vector<BezierControlPoint>> controls;
  for (int row = 0; row < 3; ++row) {
    std::vector<BezierControlPoint> control_row;
    for (int column = 0; column < 3; ++column) {
      control_row.push_back({
          {static_cast<double>(column) * 16.0, static_cast<double>(row) * 16.0, 8.0},
          {static_cast<double>(column), static_cast<double>(row)},
      });
    }
    controls.push_back(control_row);
  }
  return mtb::geometry::BezierPatchGrid(controls);
}

mtb::geometry::BezierPatchGrid flat_subdivided_patch() {
  using mtb::geometry::BezierControlPoint;
  std::vector<std::vector<BezierControlPoint>> controls;
  for (int row = 0; row < 5; ++row) {
    std::vector<BezierControlPoint> control_row;
    for (int column = 0; column < 5; ++column) {
      control_row.push_back({
          {static_cast<double>(column) * 16.0, static_cast<double>(row) * 16.0, 8.0},
          {static_cast<double>(column), static_cast<double>(row)},
      });
    }
    controls.push_back(control_row);
  }
  return mtb::geometry::BezierPatchGrid(controls);
}

}  // namespace

int main() {
  {
    const mtb::geometry::BezierPatchGrid patch = curved_patch();
    const mtb::geometry::SurfaceSample center = patch.evaluate(0.5, 0.5);
    assert(near(center.position.x, 64.0));
    assert(near(center.position.y, 64.0));
    assert(near(center.position.z, 8.0));
    assert(near(center.uv.u, 1.0));
    assert(near(center.uv.v, 1.0));
  }

  {
    const mtb::geometry::SampledSurface flat =
        mtb::geometry::sample_surface_by_chord_error(flat_patch(), 0.01, 4);
    assert(flat.quads.size() == 1);
    assert(flat.samples.size() == 4);

    const mtb::geometry::SampledSurface subdivided_flat =
        mtb::geometry::sample_surface_by_chord_error(flat_subdivided_patch(),
                                                     0.01, 4);
    assert(subdivided_flat.quads.size() == 4);
    assert(subdivided_flat.samples.size() == 9);

    const mtb::geometry::SampledSurface curved =
        mtb::geometry::sample_surface_by_chord_error(curved_patch(), 0.5, 5);
    assert(curved.quads.size() > 1);
    assert(curved.samples.size() > 4);
  }

  {
    const std::vector<mtb::geometry::Vec3> planar_points{
        {0.0, 0.0, 2.0},
        {64.0, 0.0, 2.0},
        {64.0, 64.0, 2.0},
        {0.0, 64.0, 2.0},
        {32.0, 32.0, 2.0},
    };
    const mtb::geometry::PlanarityClassification planar =
        mtb::geometry::classify_planarity(planar_points, 0.001);
    assert(planar.planar);
    assert(planar.fit.valid);
    assert(planar.fit.max_abs_distance < 1e-6);

    std::vector<mtb::geometry::Vec3> nonplanar = planar_points;
    nonplanar.back().z = 9.0;
    const mtb::geometry::PlanarityClassification curved =
        mtb::geometry::classify_planarity(nonplanar, 0.001);
    assert(!curved.planar);
  }

  {
    const mtb::geometry::Plane plane =
        mtb::geometry::plane_from_point_normal({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
    const std::vector<mtb::geometry::Vec3> winding{
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {1.0, 1.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    const mtb::geometry::WindingValidation validation =
        mtb::geometry::validate_winding(winding, plane, 0.001, 0.001);
    assert(validation.valid);
    assert(near(validation.area, 1.0));
  }

  {
    const std::vector<mtb::geometry::Plane> box{
        {{1.0, 0.0, 0.0}, 2.0},
        {{-1.0, 0.0, 0.0}, 0.0},
        {{0.0, 1.0, 0.0}, 3.0},
        {{0.0, -1.0, 0.0}, 0.0},
        {{0.0, 0.0, 1.0}, 4.0},
        {{0.0, 0.0, -1.0}, 0.0},
    };
    const mtb::geometry::ConvexVolumeValidation validation =
        mtb::geometry::validate_convex_volume(box, 0.001, 0.001);
    assert(validation.valid);
    assert(validation.vertices.size() == 8);
    assert(near(validation.volume, 24.0));

    const mtb::geometry::ConvexVolumeValidation open =
        mtb::geometry::validate_convex_volume({box[0], box[1], box[2]}, 0.001, 0.001);
    assert(!open.valid);
  }

  {
    const std::vector<mtb::geometry::UvSample> samples{
        {{0.0, 0.0, 0.0}, {2.0, 3.0}},
        {{2.0, 0.0, 0.0}, {4.0, 3.0}},
        {{0.0, 4.0, 0.0}, {2.0, 7.0}},
        {{2.0, 4.0, 0.0}, {4.0, 7.0}},
    };
    const mtb::geometry::PlanarUvProjection projection =
        mtb::geometry::fit_planar_uv_projection(samples);
    assert(projection.valid);
    assert(projection.max_error < 1e-6);
    for (const mtb::geometry::UvSample& sample : samples) {
      const mtb::geometry::Vec2 uv =
          mtb::geometry::evaluate_uv_projection(projection, sample.position);
      assert(near(uv.u, sample.uv.u));
      assert(near(uv.v, sample.uv.v));
    }
  }

  return 0;
}
