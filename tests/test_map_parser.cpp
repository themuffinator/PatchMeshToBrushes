#include "map/MapDocument.hpp"

#include <cassert>
#include <string>
#include <string_view>

namespace {

std::string_view source_view(const mtb::map::MapDocument& document,
                             mtb::map::SourceSpan span) {
  return document.source().substr(span.start, span.end - span.start);
}

}  // namespace

int main() {
  const std::string source = R"MAP(
{
  "classname" "worldspawn"
  "message" "hello"
  // brush 0
  {
    ( 0 0 0 ) ( 64 0 0 ) ( 64 64 0 ) textures/common/caulk 0 0 0 1 1 0 0 0
    ( 0 0 8 ) ( 64 64 8 ) ( 64 0 8 ) textures/common/caulk 0 0 0 1 1 0 0 0
    ( 0 0 0 ) ( 0 0 8 ) ( 64 0 8 ) textures/common/caulk 0 0 0 1 1 0 0 0
  }
  {
    brushDef
    {
      ( 0 0 1 8 ) ( ( 1 0 0 ) ( 0 1 0 ) ) textures/common/caulk 0 0 0
    }
  }
  {
    patchDef2
    {
      textures/base_wall/concrete
      ( 3 3 0 0 0 )
      (
        ( ( 0 0 0 0 0 ) ( 64 0 0 1 0 ) ( 128 0 0 2 0 ) )
        ( ( 0 64 8 0 1 ) ( 64 64 16 1 1 ) ( 128 64 8 2 1 ) )
        ( ( 0 128 0 0 2 ) ( 64 128 0 1 2 ) ( 128 128 0 2 2 ) )
      )
    }
  }
}
{
  "classname" "func_group"
  {
    patchDef2
    {
      textures/bad
      ( 2 2 0 0 0 )
      (
        ( ( 0 0 0 0 0 ) )
      )
    }
  }
}
)MAP";

  const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);
  assert(document.entities().size() == 2);
  assert(document.entities()[0].key_values.size() == 2);
  assert(document.entities()[0].key_values[0].key == "classname");
  assert(document.entities()[0].key_values[0].value == "worldspawn");
  assert(document.entities()[0].key_values[1].key == "message");
  assert(document.entities()[0].key_values[1].value == "hello");

  assert(document.brushes().size() == 2);
  assert(document.brushes()[0].entity_index == 0);
  assert(document.brushes()[0].brush_index == 0);
  assert(document.brushes()[0].face_count == 3);
  assert(!document.brushes()[0].uses_brush_def);
  assert(document.brushes()[1].entity_index == 0);
  assert(document.brushes()[1].brush_index == 1);
  assert(document.brushes()[1].face_count == 1);
  assert(document.brushes()[1].uses_brush_def);

  assert(document.patches().size() == 2);
  const mtb::map::PatchSummary& patch = document.patches()[0];
  assert(!patch.malformed);
  assert(patch.kind == mtb::map::PatchKind::PatchDef2);
  assert(patch.entity_index == 0);
  assert(patch.patch_index == 0);
  assert(patch.material == "textures/base_wall/concrete");
  assert(patch.dimensions.parsed);
  assert(patch.dimensions.rows == 3);
  assert(patch.dimensions.columns == 3);
  assert(patch.dimensions.contents == 0);
  assert(patch.control_points.size() == 3);
  assert(patch.control_points[1][1].position.z == 16.0);
  assert(patch.control_points[2][2].uv.u == 2.0);
  assert(patch.control_points[2][2].uv.v == 2.0);
  assert(patch.control_point_count() == 9);
  assert(patch.has_expected_grid_size());
  assert(source_view(document, patch.span).starts_with("{"));
  assert(source_view(document, patch.span).find("patchDef2") !=
         std::string_view::npos);

  const mtb::map::PatchSummary& malformed = document.patches()[1];
  assert(malformed.malformed);
  assert(malformed.material == "textures/bad");
  assert(malformed.dimensions.parsed);
  assert(malformed.control_point_count() == 1);
  assert(!document.diagnostics().empty());

  return 0;
}
