#include "cli/Arguments.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
  {
    const mtb::cli::ParseResult result =
        mtb::cli::parse_arguments({"input.map", "--dry-run"});
    assert(result.errors.empty());
    assert(result.options.input_path == "input.map");
    assert(result.options.dry_run);
    assert(result.options.min_thickness == 8.0);
  }

  {
    const mtb::cli::ParseResult result = mtb::cli::parse_arguments(
        {"input.map", "-o", "output.map", "--min-thickness", "12.5",
         "--replace-patches", "--report", "report.md"});
    assert(result.errors.empty());
    assert(result.options.output_path == "output.map");
    assert(result.options.report_path == "report.md");
    assert(result.options.min_thickness == 12.5);
    assert(result.options.replace_patches);
    assert(!result.options.preserve_patches);
  }

  {
    const mtb::cli::ParseResult result =
        mtb::cli::parse_arguments({"--min-thickness", "0"});
    assert(!result.errors.empty());
  }

  return 0;
}
