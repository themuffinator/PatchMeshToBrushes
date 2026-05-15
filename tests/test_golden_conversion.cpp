#include "conversion/BrushBuilder.hpp"
#include "io/FileSystem.hpp"
#include "map/MapDocument.hpp"

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

std::string normalize_newlines(std::string text) {
  std::string normalized;
  normalized.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\r') {
      continue;
    }
    normalized.push_back(text[index]);
  }
  return normalized;
}

void require_equal_text(const std::string& actual, const std::string& expected,
                        const char* label) {
  const std::string normalized_actual = normalize_newlines(actual);
  const std::string normalized_expected = normalize_newlines(expected);
  if (normalized_actual == normalized_expected) {
    return;
  }

  std::cerr << "golden mismatch: " << label << "\n";
  const std::size_t limit =
      std::min(normalized_actual.size(), normalized_expected.size());
  std::size_t position = 0;
  while (position < limit &&
         normalized_actual[position] == normalized_expected[position]) {
    ++position;
  }
  std::cerr << "first difference at byte " << position << "\n";
  std::cerr << "actual tail:\n"
            << normalized_actual.substr(position, 240) << "\n";
  std::cerr << "expected tail:\n"
            << normalized_expected.substr(position, 240) << "\n";
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
  require(argc == 2, "golden fixture directory path is required");

  const std::filesystem::path fixture_dir = argv[1];
  const std::string source =
      normalize_newlines(
          mtb::io::read_text_file(fixture_dir / "planar_patch_input.map"));
  const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);
  require(document.diagnostics().empty(), "golden input parses cleanly");

  mtb::conversion::ConversionSettings preserve_settings;
  preserve_settings.preserve_patches = true;
  preserve_settings.replace_patches = false;
  mtb::conversion::ConversionPlan preserve_plan;
  const mtb::conversion::BrushBuildResult preserve_build =
      build(document, preserve_settings, preserve_plan);
  require(preserve_build.generated_group_count == 1,
          "golden preserve emits one group");
  require(preserve_build.generated_brush_count == 1,
          "golden preserve emits one brush");
  require_equal_text(
      preserve_build.map_text,
      mtb::io::read_text_file(fixture_dir /
                              "planar_patch_preserve_expected.map"),
      "preserve map");
  require_equal_text(
      mtb::conversion::render_markdown_report(preserve_plan, preserve_settings),
      mtb::io::read_text_file(fixture_dir /
                              "planar_patch_report_expected.md"),
      "preserve report");

  mtb::conversion::ConversionSettings replace_settings;
  replace_settings.preserve_patches = false;
  replace_settings.replace_patches = true;
  mtb::conversion::ConversionPlan replace_plan;
  const mtb::conversion::BrushBuildResult replace_build =
      build(document, replace_settings, replace_plan);
  require(replace_build.generated_group_count == 1,
          "golden replace emits one group");
  require(replace_build.generated_brush_count == 1,
          "golden replace emits one brush");
  require(replace_build.replaced_patch_count == 1,
          "golden replace removes one patch");
  require_equal_text(
      replace_build.map_text,
      mtb::io::read_text_file(fixture_dir /
                              "planar_patch_replace_expected.map"),
      "replace map");

  return 0;
}
