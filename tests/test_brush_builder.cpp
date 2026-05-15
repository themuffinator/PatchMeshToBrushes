#include "conversion/BrushBuilder.hpp"

#include "map/MapDocument.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "requirement failed: " << message << "\n";
    std::exit(1);
  }
}

std::size_t count_occurrences(const std::string& text,
                              const std::string& needle) {
  std::size_t count = 0;
  std::size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }
  return count;
}

std::string patch_block(double x0, double x1, double y0, double y1) {
  const double xm = (x0 + x1) * 0.5;
  const double ym = (y0 + y1) * 0.5;

  std::ostringstream out;
  out << "  {\n";
  out << "    patchDef2\n";
  out << "    {\n";
  out << "      textures/test/source\n";
  out << "      ( 3 3 0 0 0 )\n";
  out << "      (\n";
  out << "        ( ( " << x0 << " " << y0 << " 0 " << x0 / 64.0
      << " " << y0 / 64.0 << " ) ( " << xm << " " << y0 << " 0 "
      << xm / 64.0 << " " << y0 / 64.0 << " ) ( " << x1 << " " << y0
      << " 0 " << x1 / 64.0 << " " << y0 / 64.0 << " ) )\n";
  out << "        ( ( " << x0 << " " << ym << " 0 " << x0 / 64.0
      << " " << ym / 64.0 << " ) ( " << xm << " " << ym << " 0 "
      << xm / 64.0 << " " << ym / 64.0 << " ) ( " << x1 << " " << ym
      << " 0 " << x1 / 64.0 << " " << ym / 64.0 << " ) )\n";
  out << "        ( ( " << x0 << " " << y1 << " 0 " << x0 / 64.0
      << " " << y1 / 64.0 << " ) ( " << xm << " " << y1 << " 0 "
      << xm / 64.0 << " " << y1 / 64.0 << " ) ( " << x1 << " " << y1
      << " 0 " << x1 / 64.0 << " " << y1 / 64.0 << " ) )\n";
  out << "      )\n";
  out << "    }\n";
  out << "  }\n";
  return out.str();
}

std::string map_with_patches(const std::string& patches) {
  return "{\n\"classname\" \"worldspawn\"\n" + patches + "}\n";
}

mtb::conversion::BrushBuildResult build_map(const std::string& source,
                                            bool replace_patches = false) {
  const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);
  mtb::conversion::ConversionSettings settings;
  settings.preserve_patches = !replace_patches;
  settings.replace_patches = replace_patches;
  settings.vertex_epsilon = 0.25;
  const mtb::conversion::ConversionPlan plan =
      mtb::conversion::ConversionPlanner(settings).plan(document);
  return mtb::conversion::BrushBuilder().build(document, plan);
}

}  // namespace

int main() {
  {
    const std::string source =
        map_with_patches(patch_block(0.0, 64.0, 0.0, 64.0));
    const mtb::conversion::BrushBuildResult build = build_map(source);

    require(build.implemented, "builder is implemented");
    require(build.generated_group_count == 1, "one func_group generated");
    require(build.generated_brush_count == 1, "one planar brush generated");
    require(build.replaced_patch_count == 0, "preserve mode replaces nothing");
    require(build.map_text.find("patchDef2") != std::string::npos,
            "preserve mode keeps source patch");
    require(build.map_text.find("\"classname\" \"func_group\"") !=
                std::string::npos,
            "generated output is a func_group");
    require(build.map_text.find("\"_mtb_brush_format\" \"brushDef\"") !=
                std::string::npos,
            "patch-only maps use brushDef output");
    require(build.map_text.find("brushDef") != std::string::npos,
            "generated output contains brushDef");
    require(build.map_text.find("textures/test/source") != std::string::npos,
            "source material is emitted");
    require(build.map_text.find("textures/common/caulk") != std::string::npos,
            "support faces are caulked");

    const mtb::map::MapDocument output =
        mtb::map::MapDocument::parse(build.map_text);
    require(output.diagnostics().empty(), "preserve output parses cleanly");
    require(output.patches().size() == 1, "preserve output retains patch");
    require(output.brushes().size() == 1, "preserve output has generated brush");

    const mtb::conversion::BrushBuildResult second_build =
        build_map(build.map_text);
    const mtb::map::MapDocument second_output =
        mtb::map::MapDocument::parse(second_build.map_text);
    require(second_output.diagnostics().empty(), "rerun output parses cleanly");
    require(count_occurrences(second_build.map_text,
                              "\"classname\" \"func_group\"") == 1,
            "rerun replaces the previous generated group");
    require(second_output.brushes().size() == 1,
            "rerun does not duplicate generated brushes");
  }

  {
    const std::string source =
        map_with_patches(patch_block(0.0, 64.0, 0.0, 64.0));
    const mtb::conversion::BrushBuildResult build =
        build_map(source, true);

    require(build.generated_group_count == 1, "replace mode generates group");
    require(build.generated_brush_count == 1, "replace mode generates brush");
    require(build.replaced_patch_count == 1, "replace mode removes source patch");
    require(build.map_text.find("patchDef2") == std::string::npos,
            "replace mode removes patch block");

    const mtb::map::MapDocument output =
        mtb::map::MapDocument::parse(build.map_text);
    require(output.diagnostics().empty(), "replace output parses cleanly");
    require(output.patches().empty(), "replace output has no patches");
    require(output.brushes().size() == 1, "replace output has generated brush");
  }

  {
    const std::string source =
        map_with_patches(patch_block(0.0, 64.0, 0.0, 64.0) +
                         patch_block(64.0, 128.0, 0.0, 64.0));
    const mtb::conversion::BrushBuildResult build = build_map(source);

    require(build.generated_group_count == 1,
            "connected patches share one generated group");
    require(build.generated_brush_count == 2,
            "connected planar patches emit two grouped brushes");
    require(count_occurrences(build.map_text, "\"classname\" \"func_group\"") ==
                1,
            "all assembly brushes are in one func_group");
    require(build.map_text.find("\"_mtb_source_patches\" \"0:0 0:1\"") !=
                std::string::npos,
            "func_group records every source patch in the assembly");
  }

  {
    const std::string source =
        "{\n\"classname\" \"worldspawn\"\n"
        "  {\n"
        "    ( 0 0 0 ) ( 0 64 0 ) ( 64 0 0 ) textures/common/caulk 0 0 0 1 1 0 0 0\n"
        "    ( 0 0 -8 ) ( 64 0 -8 ) ( 0 64 -8 ) textures/common/caulk 0 0 0 1 1 0 0 0\n"
        "    ( 0 0 0 ) ( 64 0 0 ) ( 0 0 -8 ) textures/common/caulk 0 0 0 1 1 0 0 0\n"
        "  }\n" +
        patch_block(0.0, 64.0, 0.0, 64.0) + "}\n";
    const mtb::conversion::BrushBuildResult build =
        build_map(source, true);

    require(build.generated_group_count == 1,
            "legacy source map generates one group");
    require(build.map_text.find("\"_mtb_brush_format\" \"legacy\"") !=
                std::string::npos,
            "legacy source maps receive legacy brush output");
    require(build.map_text.find("brushDef") == std::string::npos,
            "legacy source maps do not mix brushDef output");
  }

  return 0;
}
