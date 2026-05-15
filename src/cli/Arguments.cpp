#include "cli/Arguments.hpp"

#include <charconv>
#include <sstream>

namespace mtb::cli {
namespace {

bool parse_double(const std::string& value, double& out) {
  const char* begin = value.data();
  const char* end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, out);
  return result.ec == std::errc{} && result.ptr == end;
}

bool needs_value(const std::vector<std::string>& args, std::size_t index) {
  return index + 1 >= args.size() || args[index + 1].starts_with("-");
}

}  // namespace

ParseResult parse_arguments(const std::vector<std::string>& args) {
  ParseResult result;
  bool input_seen = false;

  for (std::size_t index = 0; index < args.size(); ++index) {
    const std::string& arg = args[index];

    if (arg == "-h" || arg == "--help") {
      result.options.show_help = true;
      continue;
    }

    if (arg == "--version") {
      result.options.show_version = true;
      continue;
    }

    if (arg == "--dry-run") {
      result.options.dry_run = true;
      continue;
    }

    if (arg == "--preserve-patches") {
      result.options.preserve_patches = true;
      result.options.replace_patches = false;
      continue;
    }

    if (arg == "--replace-patches") {
      result.options.replace_patches = true;
      result.options.preserve_patches = false;
      continue;
    }

    if (arg == "-o" || arg == "--output") {
      if (needs_value(args, index)) {
        result.errors.push_back(arg + " requires a path");
        continue;
      }

      result.options.output_path = args[++index];
      continue;
    }

    if (arg == "--report") {
      if (needs_value(args, index)) {
        result.errors.push_back("--report requires a path");
        continue;
      }

      result.options.report_path = args[++index];
      continue;
    }

    if (arg == "--min-thickness") {
      if (needs_value(args, index)) {
        result.errors.push_back("--min-thickness requires a number");
        continue;
      }

      double parsed = 0.0;
      const std::string value = args[++index];
      if (!parse_double(value, parsed) || parsed <= 0.0) {
        result.errors.push_back("--min-thickness must be a positive number");
        continue;
      }

      result.options.min_thickness = parsed;
      continue;
    }

    if (arg.starts_with("-")) {
      result.errors.push_back("unknown option: " + arg);
      continue;
    }

    if (input_seen) {
      result.errors.push_back("unexpected positional argument: " + arg);
      continue;
    }

    result.options.input_path = arg;
    input_seen = true;
  }

  if (!result.options.show_help && !result.options.show_version &&
      result.options.input_path.empty()) {
    result.errors.push_back("input .map path is required");
  }

  return result;
}

std::string usage() {
  std::ostringstream out;
  out << "Usage: patch-mesh-to-brushes <input.map> [options]\n\n";
  out << "Options:\n";
  out << "  -o, --output <path>       Output .map path\n";
  out << "      --dry-run             Analyze only; do not write brushes\n";
  out << "      --report <path>       Write a markdown conversion report\n";
  out << "      --min-thickness <n>   Minimum flat extrusion thickness, default 8\n";
  out << "      --preserve-patches    Keep source patches and append brushes\n";
  out << "      --replace-patches     Replace converted patch blocks\n";
  out << "      --version             Print version\n";
  out << "  -h, --help                Show this help\n";
  return out.str();
}

}  // namespace mtb::cli
