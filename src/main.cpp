#include "cli/Arguments.hpp"
#include "conversion/BrushBuilder.hpp"
#include "conversion/ConversionPlanner.hpp"
#include "io/FileSystem.hpp"
#include "map/MapDocument.hpp"
#include "MeshToBrushesVersion.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> collect_args(int argc, char** argv) {
  std::vector<std::string> args;
  for (int index = 1; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }
  return args;
}

mtb::conversion::ConversionSettings settings_from_options(
    const mtb::cli::Options& options) {
  mtb::conversion::ConversionSettings settings;
  settings.min_brush_thickness = options.min_thickness;
  settings.preserve_patches = options.preserve_patches;
  settings.replace_patches = options.replace_patches;
  return settings;
}

void print_plan_summary(const mtb::conversion::ConversionPlan& plan) {
  std::cout << "Discovered patches: " << plan.patches.size() << "\n";
  for (const mtb::conversion::PatchPlan& patch : plan.patches) {
    std::cout << "  entity " << patch.patch.entity_index << ", patch "
              << patch.patch.patch_index << ", "
              << mtb::map::patch_kind_name(patch.patch.kind) << " at offset "
              << patch.patch.keyword_span.start;
    if (patch.has_brush_strategy) {
      std::cout << ", brush plan "
                << mtb::conversion::brush_planning_strategy_name(
                       patch.brush_strategy);
    } else {
      std::cout << ", strategy "
                << mtb::conversion::planned_strategy_name(patch.strategy);
    }
    std::cout << "\n";
  }

  for (const std::string& diagnostic : plan.diagnostics) {
    std::cout << "diagnostic: " << diagnostic << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  const mtb::cli::ParseResult parse =
      mtb::cli::parse_arguments(collect_args(argc, argv));

  if (parse.options.show_help) {
    std::cout << mtb::cli::usage();
    return parse.errors.empty() ? 0 : 1;
  }

  if (parse.options.show_version) {
    std::cout << mtb::kProgramName << " " << mtb::kVersion << "\n";
    return parse.errors.empty() ? 0 : 1;
  }

  if (!parse.errors.empty()) {
    for (const std::string& error : parse.errors) {
      std::cerr << "error: " << error << "\n";
    }
    std::cerr << "\n" << mtb::cli::usage();
    return 1;
  }

  try {
    const std::string source = mtb::io::read_text_file(parse.options.input_path);
    const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);
    std::cout << "Parsed entities: " << document.entities().size() << "\n";
    std::cout << "Parsed brushes: " << document.brushes().size() << "\n";
    std::cout << "Parser diagnostics: " << document.diagnostics().size() << "\n";

    const mtb::conversion::ConversionSettings settings =
        settings_from_options(parse.options);
    const mtb::conversion::ConversionPlanner planner(settings);
    const mtb::conversion::ConversionPlan plan = planner.plan(document);

    print_plan_summary(plan);

    if (parse.options.report_path.has_value()) {
      mtb::io::write_text_file(
          *parse.options.report_path,
          mtb::conversion::render_markdown_report(plan, settings));
      std::cout << "Wrote report: " << parse.options.report_path->string()
                << "\n";
    }

    if (parse.options.dry_run) {
      return 0;
    }

    const mtb::conversion::BrushBuilder builder;
    const mtb::conversion::BrushBuildResult build = builder.build(document, plan);
    for (const std::string& diagnostic : build.diagnostics) {
      std::cerr << "diagnostic: " << diagnostic << "\n";
    }

    if (!build.implemented) {
      std::cerr << "error: conversion output is not implemented yet; rerun with "
                   "--dry-run for analysis only\n";
      return 2;
    }

    const std::filesystem::path output_path =
        parse.options.output_path.value_or(parse.options.input_path);
    mtb::io::write_text_file(output_path, build.map_text);
    std::cout << "Wrote map: " << output_path.string() << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "fatal: " << error.what() << "\n";
    return 1;
  }
}
