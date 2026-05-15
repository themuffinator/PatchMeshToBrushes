#include "map/MapDocument.hpp"

#include <cassert>
#include <string>

int main() {
  const std::string source = R"MAP(
// patchDef2 in comments should not count
{
  "classname" "worldspawn"
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
  "literal" "patchDef3"
}
{
  {
    patchDef3
    {
    }
  }
}
)MAP";

  const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);
  assert(document.entities().size() == 2);
  assert(document.patches().size() == 2);
  assert(document.patches()[0].kind == mtb::map::PatchKind::PatchDef2);
  assert(document.patches()[0].entity_index == 0);
  assert(document.patches()[0].patch_index == 0);
  assert(document.patches()[0].material == "textures/base_wall/concrete");
  assert(document.patches()[0].dimensions.parsed);
  assert(document.patches()[0].dimensions.rows == 3);
  assert(document.patches()[0].dimensions.columns == 3);
  assert(document.patches()[0].control_point_count() == 9);
  assert(document.patches()[0].has_expected_grid_size());
  assert(document.patches()[1].kind == mtb::map::PatchKind::PatchDef3);
  assert(document.patches()[1].entity_index == 1);
  assert(document.patches()[1].patch_index == 0);
  assert(document.patches()[1].malformed);
  assert(!document.diagnostics().empty());

  return 0;
}
