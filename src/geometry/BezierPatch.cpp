#include "geometry/BezierPatch.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

namespace mtb::geometry {
namespace {

double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

BezierControlPoint lerp(const BezierControlPoint& lhs,
                        const BezierControlPoint& rhs, double t) {
  return {
      lhs.position * (1.0 - t) + rhs.position * t,
      lhs.uv * (1.0 - t) + rhs.uv * t,
  };
}

BezierControlPoint quadratic(const BezierControlPoint& a,
                             const BezierControlPoint& b,
                             const BezierControlPoint& c, double t) {
  return lerp(lerp(a, b, t), lerp(b, c, t), t);
}

SurfaceSample make_sample(const BezierControlPoint& point, double u, double v) {
  return {point.position, point.uv, u, v};
}

Vec3 bilinear_center(const SurfaceSample& a, const SurfaceSample& b,
                     const SurfaceSample& c, const SurfaceSample& d) {
  return (a.position + b.position + c.position + d.position) * 0.25;
}

double triangle_area(const Vec3& a, const Vec3& b, const Vec3& c) {
  return length(cross(b - a, c - a)) * 0.5;
}

double quad_area(const SurfaceSample& a, const SurfaceSample& b,
                 const SurfaceSample& c, const SurfaceSample& d) {
  return triangle_area(a.position, b.position, c.position) +
         triangle_area(a.position, c.position, d.position);
}

class AdaptiveSampler {
 public:
  AdaptiveSampler(const BezierPatchGrid& patch, double max_chord_error,
                  int max_depth)
      : patch_(patch),
        max_chord_error_(std::max(0.0, max_chord_error)),
        max_depth_(std::clamp(max_depth, 0, 20)),
        scale_(1 << std::clamp(max_depth, 0, 20)) {}

  SampledSurface sample() {
    if (!patch_.is_valid_quake_grid()) {
      return std::move(surface_);
    }

    const std::size_t tile_rows = (patch_.rows() - 1) / 2;
    const std::size_t tile_columns = (patch_.columns() - 1) / 2;
    for (std::size_t tile_row = 0; tile_row < tile_rows; ++tile_row) {
      for (std::size_t tile_column = 0; tile_column < tile_columns;
           ++tile_column) {
        const double u0 = static_cast<double>(tile_column) /
                          static_cast<double>(tile_columns);
        const double u1 = static_cast<double>(tile_column + 1) /
                          static_cast<double>(tile_columns);
        const double v0 =
            static_cast<double>(tile_row) / static_cast<double>(tile_rows);
        const double v1 = static_cast<double>(tile_row + 1) /
                          static_cast<double>(tile_rows);
        add_cell(u0, v0, u1, v1, 0);
      }
    }
    return std::move(surface_);
  }

 private:
  std::size_t sample_index(double u, double v) {
    const int key_u = static_cast<int>(std::llround(clamp01(u) * scale_));
    const int key_v = static_cast<int>(std::llround(clamp01(v) * scale_));
    const std::pair<int, int> key{key_u, key_v};
    const auto existing = sample_indices_.find(key);
    if (existing != sample_indices_.end()) {
      return existing->second;
    }

    const std::size_t index = surface_.samples.size();
    surface_.samples.push_back(patch_.evaluate(u, v));
    sample_indices_[key] = index;
    return index;
  }

  void add_cell(double u0, double v0, double u1, double v1, int depth) {
    const SurfaceSample a = patch_.evaluate(u0, v0);
    const SurfaceSample b = patch_.evaluate(u1, v0);
    const SurfaceSample c = patch_.evaluate(u1, v1);
    const SurfaceSample d = patch_.evaluate(u0, v1);
    const SurfaceSample center =
        patch_.evaluate((u0 + u1) * 0.5, (v0 + v1) * 0.5);
    const double chord_error = distance(center.position, bilinear_center(a, b, c, d));
    const bool degenerate_corners = quad_area(a, b, c, d) <= 1e-6;

    if ((chord_error > max_chord_error_ || degenerate_corners) &&
        depth < max_depth_) {
      const double mid_u = (u0 + u1) * 0.5;
      const double mid_v = (v0 + v1) * 0.5;
      add_cell(u0, v0, mid_u, mid_v, depth + 1);
      add_cell(mid_u, v0, u1, mid_v, depth + 1);
      add_cell(mid_u, mid_v, u1, v1, depth + 1);
      add_cell(u0, mid_v, mid_u, v1, depth + 1);
      return;
    }

    surface_.quads.push_back({
        sample_index(u0, v0),
        sample_index(u1, v0),
        sample_index(u1, v1),
        sample_index(u0, v1),
    });
  }

  const BezierPatchGrid& patch_;
  double max_chord_error_ = 0.0;
  int max_depth_ = 0;
  int scale_ = 1;
  SampledSurface surface_;
  std::map<std::pair<int, int>, std::size_t> sample_indices_;
};

}  // namespace

BezierPatchGrid::BezierPatchGrid(
    std::vector<std::vector<BezierControlPoint>> controls)
    : controls_(std::move(controls)) {}

bool BezierPatchGrid::empty() const {
  return controls_.empty();
}

bool BezierPatchGrid::is_rectangular() const {
  if (controls_.empty() || controls_.front().empty()) {
    return false;
  }

  const std::size_t width = controls_.front().size();
  for (const std::vector<BezierControlPoint>& row : controls_) {
    if (row.size() != width) {
      return false;
    }
  }

  return true;
}

bool BezierPatchGrid::is_valid_quake_grid() const {
  return is_rectangular() && rows() >= 3 && columns() >= 3 && rows() % 2 == 1 &&
         columns() % 2 == 1;
}

std::size_t BezierPatchGrid::rows() const {
  return controls_.size();
}

std::size_t BezierPatchGrid::columns() const {
  return controls_.empty() ? 0 : controls_.front().size();
}

const BezierControlPoint& BezierPatchGrid::at(std::size_t row,
                                              std::size_t column) const {
  return controls_.at(row).at(column);
}

SurfaceSample BezierPatchGrid::evaluate(double u, double v) const {
  if (!is_valid_quake_grid()) {
    throw std::runtime_error("BezierPatchGrid requires an odd rectangular grid of at least 3x3");
  }

  const std::size_t tile_rows = (rows() - 1) / 2;
  const std::size_t tile_columns = (columns() - 1) / 2;

  const double scaled_u = clamp01(u) * static_cast<double>(tile_columns);
  const double scaled_v = clamp01(v) * static_cast<double>(tile_rows);
  const std::size_t tile_column =
      std::min(static_cast<std::size_t>(std::floor(scaled_u)), tile_columns - 1);
  const std::size_t tile_row =
      std::min(static_cast<std::size_t>(std::floor(scaled_v)), tile_rows - 1);

  const double local_u = scaled_u - static_cast<double>(tile_column);
  const double local_v = scaled_v - static_cast<double>(tile_row);
  const std::size_t base_column = tile_column * 2;
  const std::size_t base_row = tile_row * 2;

  const BezierControlPoint row0 = quadratic(at(base_row, base_column),
                                            at(base_row, base_column + 1),
                                            at(base_row, base_column + 2), local_u);
  const BezierControlPoint row1 = quadratic(at(base_row + 1, base_column),
                                            at(base_row + 1, base_column + 1),
                                            at(base_row + 1, base_column + 2),
                                            local_u);
  const BezierControlPoint row2 = quadratic(at(base_row + 2, base_column),
                                            at(base_row + 2, base_column + 1),
                                            at(base_row + 2, base_column + 2),
                                            local_u);

  return make_sample(quadratic(row0, row1, row2, local_v), clamp01(u), clamp01(v));
}

SampledSurface sample_surface_by_chord_error(const BezierPatchGrid& patch,
                                             double max_chord_error,
                                             int max_depth) {
  return AdaptiveSampler(patch, max_chord_error, max_depth).sample();
}

}  // namespace mtb::geometry
