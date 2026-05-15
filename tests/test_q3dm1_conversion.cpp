#include "conversion/BrushBuilder.hpp"
#include "io/FileSystem.hpp"
#include "map/MapDocument.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "requirement failed: " << message << "\n";
    std::exit(1);
  }
}

mtb::conversion::BrushBuildResult convert(const mtb::map::MapDocument& document,
                                          bool replace_patches) {
  mtb::conversion::ConversionSettings settings;
  settings.preserve_patches = !replace_patches;
  settings.replace_patches = replace_patches;
  const mtb::conversion::ConversionPlan plan =
      mtb::conversion::ConversionPlanner(settings).plan(document);
  return mtb::conversion::BrushBuilder().build(document, plan);
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 2);

  const std::filesystem::path fixture_path = argv[1];
  const std::string source = mtb::io::read_text_file(fixture_path);
  const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);

  require(document.diagnostics().empty(), "source fixture parses cleanly");
  require(document.patches().size() == 113, "source fixture patch count");

  const mtb::conversion::BrushBuildResult preserve = convert(document, false);
  require(preserve.generated_group_count == 63,
          "preserve generates one group per patch assembly");
  require(preserve.generated_brush_count == 2440,
          "preserve generated brush count");
  require(preserve.replaced_patch_count == 0, "preserve replaces no patches");
  const mtb::map::MapDocument preserve_output =
      mtb::map::MapDocument::parse(preserve.map_text);
  require(preserve_output.diagnostics().empty(), "preserve output parses");
  require(preserve_output.entities().size() == 251, "preserve entity count");
  require(preserve_output.brushes().size() == 3427, "preserve brush count");
  require(preserve_output.patches().size() == 113, "preserve patch count");

  const mtb::conversion::BrushBuildResult replace = convert(document, true);
  require(replace.generated_group_count == 63,
          "replace generates one group per patch assembly");
  require(replace.generated_brush_count == 2440,
          "replace generated brush count");
  require(replace.replaced_patch_count == 113, "replace removes source patches");
  require(replace.map_text.find("patchDef2") == std::string::npos,
          "replace output text contains no patchDef2 blocks");
  require(replace.map_text.find("brushDef") == std::string::npos,
          "q3dm1 legacy brush map does not receive mixed brushDef output");
  require(replace.map_text.find("\"_mtb_brush_format\" \"legacy\"") !=
              std::string::npos,
          "q3dm1 generated groups record legacy brush output");
  const mtb::map::MapDocument replace_output =
      mtb::map::MapDocument::parse(replace.map_text);
  require(replace_output.diagnostics().empty(), "replace output parses");
  require(replace_output.entities().size() == 251, "replace entity count");
  require(replace_output.brushes().size() == 3427, "replace brush count");
  require(replace_output.patches().empty(), "replace removes all patches");

  return 0;
}
