#include "io/FileSystem.hpp"
#include "map/MapDocument.hpp"

#include <cassert>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
  assert(argc == 2);

  const std::filesystem::path fixture_path = argv[1];
  assert(std::filesystem::exists(fixture_path));

  const std::string source = mtb::io::read_text_file(fixture_path);
  const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);

  assert(document.patches().size() == 113);

  for (const mtb::map::PatchSummary& patch : document.patches()) {
    assert(patch.kind == mtb::map::PatchKind::PatchDef2);
  }

  assert(document.patches().front().entity_index == 0);
  assert(document.patches().front().patch_index == 0);
  assert(document.patches().back().entity_index == 101);
  assert(document.patches().back().patch_index == 2);

  return 0;
}
