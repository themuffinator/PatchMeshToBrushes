#include "conversion/BrushBuilder.hpp"

#include <algorithm>
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
constexpr std::string_view kGeneratedGroupBegin = "// MeshToBrushes begin:";
constexpr std::string_view kGeneratedGroupEnd = "// MeshToBrushes end";

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

void write_brush_def(std::ostream& out, const PlannedBrush& brush) {
  out << "  {\n";
  out << "    brushDef\n";
  out << "    {\n";

  for (const PlannedFace& face : brush.faces) {
    const std::string material =
        face.material.empty() ? "textures/common/caulk" : face.material;
    const mtb::geometry::Plane plane =
        mtb::geometry::normalized_plane(face.plane);
    out << "      ( " << format_double(plane.normal.x) << " "
        << format_double(plane.normal.y) << " "
        << format_double(plane.normal.z) << " "
        << format_double(plane.distance) << " ) ";
    write_texture_projection(out, face);
    out << " " << material << " 0 0 0\n";
  }

  out << "    }\n";
  out << "  }\n";
}

std::string write_assembly_group(const AssemblyBrushPlan& assembly,
                                 const mtb::map::MapDocument& document,
                                 BrushBuildResult& result) {
  std::ostringstream out;
  const std::string source_patches =
      join_source_patch_tokens(assembly.patch_indices, document);
  const std::string source_entities =
      join_source_entity_indices(assembly.patch_indices, document);

  out << "// MeshToBrushes begin: source patch assembly "
      << assembly.assembly_index << "\n";
  out << "// MeshToBrushes source patches: "
      << join_source_patch_descriptions(assembly.patch_indices, document)
      << "\n";
  out << "{\n";
  out << "\"classname\" \"func_group\"\n";
  out << "\"_mtb_assembly\" \"" << assembly.assembly_index << "\"\n";
  out << "\"_mtb_strategy\" \""
      << brush_planning_strategy_name(assembly.primary_strategy) << "\"\n";
  out << "\"_mtb_source_entities\" \"" << escape_entity_value(source_entities)
      << "\"\n";
  out << "\"_mtb_source_patches\" \"" << escape_entity_value(source_patches)
      << "\"\n";
  out << "\"_mtb_generated_by\" \"MeshToBrushes\"\n";

  for (const PlannedBrush& brush : assembly.brushes) {
    if (!brush.valid) {
      result.diagnostics.push_back(
          "Skipped invalid planned brush in assembly " +
          std::to_string(assembly.assembly_index) + ".");
      continue;
    }

    write_brush_def(out, brush);
    ++result.generated_brush_count;
  }

  out << "}\n";
  out << "// MeshToBrushes end: source patch assembly "
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
  std::vector<mtb::map::SourceSpan> spans;
  std::size_t search_from = 0;
  while (true) {
    const std::size_t begin = source.find(kGeneratedGroupBegin, search_from);
    if (begin == std::string_view::npos) {
      return spans;
    }

    const std::size_t end_marker = source.find(kGeneratedGroupEnd, begin);
    if (end_marker == std::string_view::npos) {
      search_from = begin + kGeneratedGroupBegin.size();
      continue;
    }

    std::size_t end = source.find('\n', end_marker);
    end = end == std::string_view::npos ? source.size() : end + 1;
    spans.push_back({begin, end});
    search_from = end;
  }
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
  const std::vector<mtb::map::SourceSpan> existing_group_spans =
      existing_generated_group_spans(document.source());
  std::vector<mtb::map::SourceSpan> spans_to_remove = existing_group_spans;
  std::size_t patch_span_count = 0;
  std::set<std::size_t> converted_patch_indices;

  for (const AssemblyBrushPlan& assembly : plan.brush_assemblies) {
    if (!has_writable_brushes(assembly)) {
      result.diagnostics.push_back(
          "Assembly " + std::to_string(assembly.assembly_index) +
          " did not produce any valid brushDef output.");
      continue;
    }

    groups.push_back(write_assembly_group(assembly, document, result));
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
        " previous MeshToBrushes generated func_group block(s).");
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
