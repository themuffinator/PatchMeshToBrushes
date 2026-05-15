#include "conversion/BrushBuilder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace mtb::conversion {
namespace {

constexpr double kDefaultTextureScale = 1.0 / 32.0;
constexpr std::string_view kGeneratedGroupBegin =
    "// PatchMeshToBrushes begin:";
constexpr std::string_view kGeneratedGroupEnd = "// PatchMeshToBrushes end";
constexpr std::string_view kLegacyGeneratedGroupBegin = "// MeshToBrushes begin:";
constexpr std::string_view kLegacyGeneratedGroupEnd = "// MeshToBrushes end";
constexpr double kPlanePointSpan = 128.0;

enum class BrushWriteFormat {
  BrushDef,
  Legacy,
};

std::string format_double(double value) {
  if (std::abs(value) < 0.0000005) {
    value = 0.0;
  }

  std::ostringstream out;
  out << std::fixed << std::setprecision(6) << value;
  std::string text = out.str();
  while (text.size() > 1 && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  return text;
}

std::string escape_entity_value(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char item : value) {
    if (item == '\\' || item == '"') {
      escaped.push_back('\\');
    }
    escaped.push_back(item);
  }
  return escaped;
}

std::string source_patch_token(const mtb::map::PatchSummary& patch) {
  return std::to_string(patch.entity_index) + ":" +
         std::to_string(patch.patch_index);
}

std::string source_patch_description(const mtb::map::PatchSummary& patch) {
  return "entity " + std::to_string(patch.entity_index) + " patch " +
         std::to_string(patch.patch_index);
}

std::string join_source_patch_tokens(
    const std::vector<std::size_t>& patch_indices,
    const mtb::map::MapDocument& document) {
  std::ostringstream out;
  bool first = true;
  for (std::size_t patch_index : patch_indices) {
    if (patch_index >= document.patches().size()) {
      continue;
    }
    if (!first) {
      out << " ";
    }
    first = false;
    out << source_patch_token(document.patches()[patch_index]);
  }
  return out.str();
}

std::string join_source_patch_descriptions(
    const std::vector<std::size_t>& patch_indices,
    const mtb::map::MapDocument& document) {
  std::ostringstream out;
  bool first = true;
  for (std::size_t patch_index : patch_indices) {
    if (patch_index >= document.patches().size()) {
      continue;
    }
    if (!first) {
      out << ", ";
    }
    first = false;
    out << source_patch_description(document.patches()[patch_index]);
  }
  return out.str();
}

std::string join_source_entity_indices(
    const std::vector<std::size_t>& patch_indices,
    const mtb::map::MapDocument& document) {
  std::set<std::size_t> entity_indices;
  for (std::size_t patch_index : patch_indices) {
    if (patch_index < document.patches().size()) {
      entity_indices.insert(document.patches()[patch_index].entity_index);
    }
  }

  std::ostringstream out;
  bool first = true;
  for (std::size_t entity_index : entity_indices) {
    if (!first) {
      out << " ";
    }
    first = false;
    out << entity_index;
  }
  return out.str();
}

bool has_writable_brushes(const AssemblyBrushPlan& assembly) {
  for (const PlannedBrush& brush : assembly.brushes) {
    if (brush.valid) {
      return true;
    }
  }
  return false;
}

void write_texture_projection(std::ostream& out, const PlannedFace& face) {
  if (face.role == PlannedFaceRole::SourceSurface && face.has_uv_projection) {
    const mtb::geometry::PlanarUvProjection& projection = face.uv_projection;
    out << "( ( " << format_double(projection.u_from_s) << " "
        << format_double(projection.u_from_t) << " "
        << format_double(projection.u_offset) << " ) ( "
        << format_double(projection.v_from_s) << " "
        << format_double(projection.v_from_t) << " "
        << format_double(projection.v_offset) << " ) )";
    return;
  }

  out << "( ( " << format_double(kDefaultTextureScale)
      << " 0 0 ) ( 0 " << format_double(kDefaultTextureScale) << " 0 ) )";
}

std::array<mtb::geometry::Vec3, 3> plane_points(
    const mtb::geometry::Plane& input_plane) {
  // Q3 brush planes are serialized as three points; this order preserves the
  // plane normal expected by q3map's point-plane parser.
  const mtb::geometry::Plane plane = mtb::geometry::normalized_plane(input_plane);
  const mtb::geometry::Vec3 anchor = plane.normal * plane.distance;
  const mtb::geometry::Vec3 reference =
      std::abs(plane.normal.z) < 0.9 ? mtb::geometry::Vec3{0.0, 0.0, 1.0}
                                     : mtb::geometry::Vec3{0.0, 1.0, 0.0};
  const mtb::geometry::Vec3 tangent_u =
      mtb::geometry::normalized(mtb::geometry::cross(reference, plane.normal));
  const mtb::geometry::Vec3 tangent_v =
      mtb::geometry::cross(plane.normal, tangent_u);

  return {
      anchor,
      anchor + tangent_v * kPlanePointSpan,
      anchor + tangent_u * kPlanePointSpan,
  };
}

void write_point(std::ostream& out, const mtb::geometry::Vec3& point) {
  out << "( " << format_double(point.x) << " " << format_double(point.y) << " "
      << format_double(point.z) << " )";
}

void write_brush_def(std::ostream& out, const PlannedBrush& brush) {
  out << "  {\n";
  out << "    brushDef\n";
  out << "    {\n";

  for (const PlannedFace& face : brush.faces) {
    const std::string material =
        face.material.empty() ? "textures/common/caulk" : face.material;
    const std::array<mtb::geometry::Vec3, 3> points =
        plane_points(face.plane);
    out << "      ";
    write_point(out, points[0]);
    out << " ";
    write_point(out, points[1]);
    out << " ";
    write_point(out, points[2]);
    out << " ";
    write_texture_projection(out, face);
    out << " " << material << " 0 0 0\n";
  }

  out << "    }\n";
  out << "  }\n";
}

void write_legacy_brush(std::ostream& out, const PlannedBrush& brush) {
  out << "  {\n";

  for (const PlannedFace& face : brush.faces) {
    const std::string material =
        face.material.empty() ? "textures/common/caulk" : face.material;
    const std::array<mtb::geometry::Vec3, 3> points =
        plane_points(face.plane);
    out << "    ";
    write_point(out, points[0]);
    out << " ";
    write_point(out, points[1]);
    out << " ";
    write_point(out, points[2]);
    out << " " << material << " 0 0 0 1 1 0 0 0\n";
  }

  out << "  }\n";
}

BrushWriteFormat output_format_for(const mtb::map::MapDocument& document) {
  for (const mtb::map::BrushSummary& brush : document.brushes()) {
    if (!brush.uses_brush_def) {
      return BrushWriteFormat::Legacy;
    }
  }

  return BrushWriteFormat::BrushDef;
}

std::string format_name(BrushWriteFormat format) {
  switch (format) {
    case BrushWriteFormat::BrushDef:
      return "brushDef";
    case BrushWriteFormat::Legacy:
      return "legacy";
  }

  return "unknown";
}

std::string write_assembly_group(const AssemblyBrushPlan& assembly,
                                 const mtb::map::MapDocument& document,
                                 BrushBuildResult& result,
                                 BrushWriteFormat format) {
  std::ostringstream out;
  const std::string source_patches =
      join_source_patch_tokens(assembly.patch_indices, document);
  const std::string source_entities =
      join_source_entity_indices(assembly.patch_indices, document);

  out << "// PatchMeshToBrushes begin: source patch assembly "
      << assembly.assembly_index << "\n";
  out << "// PatchMeshToBrushes source patches: "
      << join_source_patch_descriptions(assembly.patch_indices, document)
      << "\n";
  out << "{\n";
  out << "\"classname\" \"func_group\"\n";
  out << "\"_mtb_assembly\" \"" << assembly.assembly_index << "\"\n";
  out << "\"_mtb_strategy\" \""
      << brush_planning_strategy_name(assembly.primary_strategy) << "\"\n";
  out << "\"_mtb_brush_format\" \"" << format_name(format) << "\"\n";
  out << "\"_mtb_source_entities\" \"" << escape_entity_value(source_entities)
      << "\"\n";
  out << "\"_mtb_source_patches\" \"" << escape_entity_value(source_patches)
      << "\"\n";
  out << "\"_mtb_generated_by\" \"PatchMeshToBrushes\"\n";

  for (const PlannedBrush& brush : assembly.brushes) {
    if (!brush.valid) {
      result.diagnostics.push_back(
          "Skipped invalid planned brush in assembly " +
          std::to_string(assembly.assembly_index) + ".");
      continue;
    }

    if (format == BrushWriteFormat::Legacy) {
      write_legacy_brush(out, brush);
    } else {
      write_brush_def(out, brush);
    }
    ++result.generated_brush_count;
  }

  out << "}\n";
  out << "// PatchMeshToBrushes end: source patch assembly "
      << assembly.assembly_index << "\n";
  return out.str();
}

std::string remove_spans(std::string_view source,
                         std::vector<mtb::map::SourceSpan> spans) {
  std::sort(spans.begin(), spans.end(),
            [](const mtb::map::SourceSpan& lhs,
               const mtb::map::SourceSpan& rhs) {
              return lhs.start > rhs.start;
            });

  std::string rewritten(source);
  std::size_t previous_start = source.size();
  for (const mtb::map::SourceSpan span : spans) {
    if (span.start >= span.end || span.end > rewritten.size() ||
        span.end > previous_start) {
      continue;
    }
    rewritten.erase(span.start, span.end - span.start);
    previous_start = span.start;
  }
  return rewritten;
}

std::vector<mtb::map::SourceSpan> existing_generated_group_spans(
    std::string_view source) {
  const auto collect_spans =
      [&](std::string_view begin_marker, std::string_view end_marker,
          std::vector<mtb::map::SourceSpan>& raw_spans) {
        std::size_t search_from = 0;
        while (true) {
          const std::size_t begin = source.find(begin_marker, search_from);
          if (begin == std::string_view::npos) {
            break;
          }

          const std::size_t end_marker_offset = source.find(end_marker, begin);
          if (end_marker_offset == std::string_view::npos) {
            search_from = begin + begin_marker.size();
            continue;
          }

          std::size_t start = begin;
          while (start > 0 &&
                 std::isspace(static_cast<unsigned char>(source[start - 1]))) {
            --start;
          }
          if (start < begin) {
            if (source[start] == '\r' && start + 1 < begin &&
                source[start + 1] == '\n') {
              start += 2;
            } else if (source[start] == '\r' || source[start] == '\n') {
              ++start;
            }
          }

          std::size_t end = source.find('\n', end_marker_offset);
          end = end == std::string_view::npos ? source.size() : end + 1;
          while (end < source.size() &&
                 std::isspace(static_cast<unsigned char>(source[end]))) {
            ++end;
          }
          raw_spans.push_back({start, end});
          search_from = end;
        }
      };

  std::vector<mtb::map::SourceSpan> raw_spans;
  collect_spans(kGeneratedGroupBegin, kGeneratedGroupEnd, raw_spans);
  collect_spans(kLegacyGeneratedGroupBegin, kLegacyGeneratedGroupEnd, raw_spans);

  std::sort(raw_spans.begin(), raw_spans.end(),
            [](const mtb::map::SourceSpan& lhs,
               const mtb::map::SourceSpan& rhs) {
              return lhs.start < rhs.start;
            });
  std::vector<mtb::map::SourceSpan> merged;
  for (const mtb::map::SourceSpan span : raw_spans) {
    if (merged.empty() || span.start > merged.back().end) {
      merged.push_back(span);
      continue;
    }
    merged.back().end = std::max(merged.back().end, span.end);
  }
  return merged;
}

void append_generated_groups(std::string& map_text,
                             const std::vector<std::string>& groups) {
  if (groups.empty()) {
    return;
  }

  if (!map_text.empty() && map_text.back() != '\n') {
    map_text.push_back('\n');
  }
  map_text.push_back('\n');

  for (const std::string& group : groups) {
    map_text += group;
    if (!map_text.empty() && map_text.back() != '\n') {
      map_text.push_back('\n');
    }
    map_text.push_back('\n');
  }
}

}  // namespace

BrushBuildResult BrushBuilder::build(const mtb::map::MapDocument& document,
                                     const ConversionPlan& plan) const {
  BrushBuildResult result;
  std::vector<std::string> groups;
  const BrushWriteFormat output_format = output_format_for(document);
  const std::vector<mtb::map::SourceSpan> existing_group_spans =
      existing_generated_group_spans(document.source());
  std::vector<mtb::map::SourceSpan> spans_to_remove = existing_group_spans;
  std::size_t patch_span_count = 0;
  std::set<std::size_t> converted_patch_indices;

  for (const AssemblyBrushPlan& assembly : plan.brush_assemblies) {
    if (!has_writable_brushes(assembly)) {
      result.diagnostics.push_back(
          "Assembly " + std::to_string(assembly.assembly_index) +
          " did not produce any valid brush output.");
      continue;
    }

    groups.push_back(
        write_assembly_group(assembly, document, result, output_format));
    ++result.generated_group_count;
    for (std::size_t patch_index : assembly.patch_indices) {
      if (patch_index < document.patches().size() &&
          converted_patch_indices.insert(patch_index).second) {
        spans_to_remove.push_back(document.patches()[patch_index].span);
        ++patch_span_count;
      }
    }
  }

  result.map_text = remove_spans(
      document.source(), plan.replace_patches ? spans_to_remove
                                              : existing_group_spans);
  result.replaced_patch_count = plan.replace_patches ? patch_span_count : 0;

  append_generated_groups(result.map_text, groups);

  if (groups.empty() && !document.patches().empty()) {
    result.diagnostics.push_back("No generated func_group entities were written.");
  }
  if (!existing_group_spans.empty()) {
    result.diagnostics.push_back(
        "Removed " + std::to_string(existing_group_spans.size()) +
        " previous PatchMeshToBrushes generated func_group block(s).");
  }
  if (plan.preserve_patches && !plan.replace_patches) {
    result.diagnostics.push_back(
        "Preserved source patch blocks and appended generated func_group output.");
  }
  if (plan.replace_patches) {
    result.diagnostics.push_back(
        "Removed " + std::to_string(result.replaced_patch_count) +
        " converted source patch block(s) before appending generated func_group "
        "output.");
  }

  return result;
}

}  // namespace mtb::conversion
