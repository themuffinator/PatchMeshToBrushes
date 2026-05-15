#pragma once

#include "conversion/ConversionSettings.hpp"
#include "geometry/Vec.hpp"
#include "map/MapDocument.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mtb::conversion {

enum class BoundarySide {
  Top,
  Right,
  Bottom,
  Left,
};

enum class PatchConnectionKind {
  SharedBoundaryVertex,
  OverlappingBoundarySpan,
};

enum class AssemblyKind {
  Planar,
  Cylindrical,
  Spherical,
  Capped,
  Arbitrary,
};

struct QuantizedVertex {
  std::int64_t x = 0;
  std::int64_t y = 0;
  std::int64_t z = 0;
};

struct BoundarySample {
  geometry::Vec3 position;
  QuantizedVertex quantized;
  std::size_t control_index = 0;
};

struct BoundarySpan {
  BoundarySide side = BoundarySide::Top;
  std::vector<BoundarySample> samples;
};

struct PatchBoundary {
  std::size_t patch_index = 0;
  std::size_t entity_index = 0;
  std::size_t patch_index_in_entity = 0;
  std::vector<BoundarySpan> spans;
};

struct PatchConnection {
  std::size_t patch_a = 0;
  std::size_t patch_b = 0;
  PatchConnectionKind kind = PatchConnectionKind::SharedBoundaryVertex;
  std::size_t shared_vertex_count = 0;
  std::size_t overlapping_span_count = 0;
};

struct PatchAssembly {
  std::size_t assembly_index = 0;
  std::vector<std::size_t> patch_indices;
  std::vector<std::size_t> connection_indices;
  AssemblyKind kind = AssemblyKind::Arbitrary;
  std::size_t unmatched_boundary_spans = 0;
};

struct PatchTopology {
  std::vector<PatchBoundary> boundaries;
  std::vector<PatchConnection> connections;
  std::vector<PatchAssembly> assemblies;
  std::vector<std::size_t> skipped_patch_indices;
};

class PatchTopologyBuilder {
 public:
  explicit PatchTopologyBuilder(ConversionSettings settings);

  [[nodiscard]] PatchTopology build(
      const std::vector<mtb::map::PatchSummary>& patches) const;

 private:
  ConversionSettings settings_;
};

bool operator<(const QuantizedVertex& lhs, const QuantizedVertex& rhs);
bool operator==(const QuantizedVertex& lhs, const QuantizedVertex& rhs);

QuantizedVertex quantize_position(const geometry::Vec3& position,
                                  double epsilon);

std::string boundary_side_name(BoundarySide side);
std::string patch_connection_kind_name(PatchConnectionKind kind);
std::string assembly_kind_name(AssemblyKind kind);

}  // namespace mtb::conversion
