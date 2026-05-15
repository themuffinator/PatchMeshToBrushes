#include "conversion/BrushBuilder.hpp"
#include "io/FileSystem.hpp"
#include "map/MapDocument.hpp"

#include <algorithm>
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

void require_equal(const std::string& lhs, const std::string& rhs,
                   const char* label) {
  if (lhs == rhs) {
    return;
  }

  std::size_t position = 0;
  const std::size_t limit = std::min(lhs.size(), rhs.size());
  while (position < limit && lhs[position] == rhs[position]) {
    ++position;
  }
  std::cerr << "deterministic mismatch in " << label << " at byte "
            << position << "\n";
  std::exit(1);
}

mtb::conversion::BrushBuildResult build(
    const mtb::map::MapDocument& document,
    const mtb::conversion::ConversionSettings& settings,
    mtb::conversion::ConversionPlan& plan) {
  plan = mtb::conversion::ConversionPlanner(settings).plan(document);
  return mtb::conversion::BrushBuilder().build(document, plan);
}

}  // namespace

int main(int argc, char** argv) {
  require(argc == 2, "q3dm1 fixture path is required");

  const std::string source = mtb::io::read_text_file(std::filesystem::path(argv[1]));
  const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);
  require(document.diagnostics().empty(), "fixture parses cleanly");

  mtb::conversion::ConversionSettings replace_settings;
  replace_settings.preserve_patches = false;
  replace_settings.replace_patches = true;

  mtb::conversion::ConversionPlan first_replace_plan;
  const mtb::conversion::BrushBuildResult first_replace =
      build(document, replace_settings, first_replace_plan);
  mtb::conversion::ConversionPlan second_replace_plan;
  const mtb::conversion::BrushBuildResult second_replace =
      build(document, replace_settings, second_replace_plan);

  require_equal(first_replace.map_text, second_replace.map_text,
                "replace map output");
  require_equal(
      mtb::conversion::render_markdown_report(first_replace_plan,
                                              replace_settings),
      mtb::conversion::render_markdown_report(second_replace_plan,
                                              replace_settings),
      "replace markdown report");
  require(first_replace.generated_group_count == 63,
          "replace generated group count remains stable");
  require(first_replace.generated_brush_count == 2440,
          "replace generated brush count remains stable");

  mtb::conversion::ConversionSettings preserve_settings;
  preserve_settings.preserve_patches = true;
  preserve_settings.replace_patches = false;

  mtb::conversion::ConversionPlan first_preserve_plan;
  const mtb::conversion::BrushBuildResult first_preserve =
      build(document, preserve_settings, first_preserve_plan);
  mtb::conversion::ConversionPlan second_preserve_plan;
  const mtb::conversion::BrushBuildResult second_preserve =
      build(document, preserve_settings, second_preserve_plan);
  require_equal(first_preserve.map_text, second_preserve.map_text,
                "preserve map output");

  const mtb::map::MapDocument rerun_document =
      mtb::map::MapDocument::parse(first_preserve.map_text);
  mtb::conversion::ConversionPlan rerun_plan;
  const mtb::conversion::BrushBuildResult rerun_preserve =
      build(rerun_document, preserve_settings, rerun_plan);
  require_equal(first_preserve.map_text, rerun_preserve.map_text,
                "preserve rerun map output");
  require(rerun_preserve.generated_group_count == 63,
          "rerun generated group count remains stable");
  require(rerun_preserve.generated_brush_count == 2440,
          "rerun generated brush count remains stable");

  return 0;
}
