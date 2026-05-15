#pragma once

#include "geometry/Vec.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mtb::map {

class MapParser;

enum class PatchKind {
  PatchDef,
  PatchDef2,
  PatchDef3,
};

struct SourceSpan {
  std::size_t start = 0;
  std::size_t end = 0;
};

enum class DiagnosticSeverity {
  Warning,
  Error,
};

struct ParseDiagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Warning;
  SourceSpan span;
  std::string message;
};

struct EntityKeyValue {
  std::string key;
  std::string value;
  SourceSpan span;
  SourceSpan key_span;
  SourceSpan value_span;
};

struct BrushSummary {
  SourceSpan span;
  std::size_t entity_index = 0;
  std::size_t brush_index = 0;
  std::size_t face_count = 0;
  bool uses_brush_def = false;
};

struct PatchDimensions {
  SourceSpan span;
  std::size_t rows = 0;
  std::size_t columns = 0;
  int contents = 0;
  int flags = 0;
  int value = 0;
  bool parsed = false;
};

struct PatchControlPoint {
  geometry::Vec3 position;
  geometry::Vec2 uv;
  SourceSpan span;
};

struct PatchSummary {
  PatchKind kind = PatchKind::PatchDef;
  SourceSpan span;
  SourceSpan keyword_span;
  SourceSpan body_span;
  std::size_t entity_index = 0;
  std::size_t patch_index = 0;
  std::string material;
  SourceSpan material_span;
  PatchDimensions dimensions;
  std::vector<std::vector<PatchControlPoint>> control_points;
  bool malformed = false;

  [[nodiscard]] std::size_t control_point_count() const;
  [[nodiscard]] bool has_expected_grid_size() const;
};

struct Entity {
  SourceSpan span;
  std::size_t entity_index = 0;
  std::vector<EntityKeyValue> key_values;
  std::vector<BrushSummary> brushes;
  std::vector<PatchSummary> patches;
};

class MapDocument {
 public:
  static MapDocument parse(std::string source);

  [[nodiscard]] std::string_view source() const;
  [[nodiscard]] const std::vector<Entity>& entities() const;
  [[nodiscard]] const std::vector<BrushSummary>& brushes() const;
  [[nodiscard]] const std::vector<PatchSummary>& patches() const;
  [[nodiscard]] const std::vector<ParseDiagnostic>& diagnostics() const;

 private:
  friend class MapParser;

  explicit MapDocument(std::string source);

  std::string source_;
  std::vector<Entity> entities_;
  std::vector<BrushSummary> brushes_;
  std::vector<PatchSummary> patches_;
  std::vector<ParseDiagnostic> diagnostics_;
};

std::string_view patch_kind_name(PatchKind kind);
std::string_view diagnostic_severity_name(DiagnosticSeverity severity);

}  // namespace mtb::map
