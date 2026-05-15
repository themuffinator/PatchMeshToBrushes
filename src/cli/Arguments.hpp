#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mtb::cli {

struct Options {
  std::filesystem::path input_path;
  std::optional<std::filesystem::path> output_path;
  std::optional<std::filesystem::path> report_path;
  double min_thickness = 8.0;
  bool dry_run = false;
  bool preserve_patches = true;
  bool replace_patches = false;
  bool show_help = false;
  bool show_version = false;
};

struct ParseResult {
  Options options;
  std::vector<std::string> errors;
};

ParseResult parse_arguments(const std::vector<std::string>& args);

std::string usage();

}  // namespace mtb::cli
