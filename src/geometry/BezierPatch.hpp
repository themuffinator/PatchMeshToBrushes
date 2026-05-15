#pragma once

#include "geometry/Vec.hpp"

#include <cstddef>
#include <vector>

namespace mtb::geometry {

struct BezierControlPoint {
  Vec3 position;
  Vec2 uv;
};

struct SurfaceSample {
  Vec3 position;
  Vec2 uv;
  double u = 0.0;
  double v = 0.0;
};

struct SurfaceQuad {
  std::size_t a = 0;
  std::size_t b = 0;
  std::size_t c = 0;
  std::size_t d = 0;
};

struct SampledSurface {
  std::vector<SurfaceSample> samples;
  std::vector<SurfaceQuad> quads;
};

class BezierPatchGrid {
 public:
  BezierPatchGrid() = default;
  explicit BezierPatchGrid(std::vector<std::vector<BezierControlPoint>> controls);

  [[nodiscard]] bool empty() const;
  [[nodiscard]] bool is_rectangular() const;
  [[nodiscard]] bool is_valid_quake_grid() const;
  [[nodiscard]] std::size_t rows() const;
  [[nodiscard]] std::size_t columns() const;
  [[nodiscard]] const BezierControlPoint& at(std::size_t row,
                                             std::size_t column) const;

  [[nodiscard]] SurfaceSample evaluate(double u, double v) const;

 private:
  std::vector<std::vector<BezierControlPoint>> controls_;
};

SampledSurface sample_surface_by_chord_error(const BezierPatchGrid& patch,
                                             double max_chord_error,
                                             int max_depth = 6);

}  // namespace mtb::geometry
