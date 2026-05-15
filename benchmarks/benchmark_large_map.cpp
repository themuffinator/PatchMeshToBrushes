#include "conversion/BrushBuilder.hpp"
#include "io/FileSystem.hpp"
#include "map/MapDocument.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;

std::size_t parse_size_arg(const char* value, std::size_t fallback) {
  try {
    const unsigned long parsed = std::stoul(value);
    return parsed == 0 ? fallback : static_cast<std::size_t>(parsed);
  } catch (...) {
    return fallback;
  }
}

std::string repeated_source(const std::string& source, std::size_t repeat_count) {
  std::ostringstream out;
  for (std::size_t index = 0; index < repeat_count; ++index) {
    out << source;
    if (!source.empty() && source.back() != '\n') {
      out << '\n';
    }
  }
  return out.str();
}

double milliseconds_since(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: benchmark_large_map <map-path> [repeat-count] "
                 "[iterations]\n";
    return 2;
  }

  const std::filesystem::path map_path = argv[1];
  const std::size_t repeat_count =
      argc >= 3 ? parse_size_arg(argv[2], 1) : 1;
  const std::size_t iterations =
      argc >= 4 ? parse_size_arg(argv[3], 1) : 1;

  const std::string source =
      repeated_source(mtb::io::read_text_file(map_path), repeat_count);

  double parse_ms = 0.0;
  double plan_ms = 0.0;
  double build_ms = 0.0;
  std::size_t entity_count = 0;
  std::size_t patch_count = 0;
  std::size_t planned_brush_count = 0;
  std::size_t generated_group_count = 0;
  std::size_t generated_brush_count = 0;

  mtb::conversion::ConversionSettings settings;
  settings.preserve_patches = false;
  settings.replace_patches = true;

  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    const Clock::time_point parse_begin = Clock::now();
    const mtb::map::MapDocument document =
        mtb::map::MapDocument::parse(source);
    const Clock::time_point parse_end = Clock::now();

    const Clock::time_point plan_begin = Clock::now();
    const mtb::conversion::ConversionPlan plan =
        mtb::conversion::ConversionPlanner(settings).plan(document);
    const Clock::time_point plan_end = Clock::now();

    const Clock::time_point build_begin = Clock::now();
    const mtb::conversion::BrushBuildResult build =
        mtb::conversion::BrushBuilder().build(document, plan);
    const Clock::time_point build_end = Clock::now();

    parse_ms += milliseconds_since(parse_begin, parse_end);
    plan_ms += milliseconds_since(plan_begin, plan_end);
    build_ms += milliseconds_since(build_begin, build_end);
    entity_count = document.entities().size();
    patch_count = document.patches().size();
    planned_brush_count = plan.planned_brush_count;
    generated_group_count = build.generated_group_count;
    generated_brush_count = build.generated_brush_count;
  }

  const double divisor = static_cast<double>(iterations);
  std::cout << "MeshToBrushes large-map benchmark\n";
  std::cout << "map: " << map_path.string() << "\n";
  std::cout << "repeat count: " << repeat_count << "\n";
  std::cout << "iterations: " << iterations << "\n";
  std::cout << "entities: " << entity_count << "\n";
  std::cout << "patches: " << patch_count << "\n";
  std::cout << "planned brushes: " << planned_brush_count << "\n";
  std::cout << "generated func_groups: " << generated_group_count << "\n";
  std::cout << "generated brushes: " << generated_brush_count << "\n";
  std::cout << "avg parse ms: " << parse_ms / divisor << "\n";
  std::cout << "avg plan ms: " << plan_ms / divisor << "\n";
  std::cout << "avg build ms: " << build_ms / divisor << "\n";

  return 0;
}
