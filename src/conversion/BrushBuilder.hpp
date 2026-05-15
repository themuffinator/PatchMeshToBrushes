#pragma once

#include "conversion/ConversionPlanner.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace mtb::conversion {

struct BrushBuildResult {
  bool implemented = true;
  std::string map_text;
  std::size_t generated_group_count = 0;
  std::size_t generated_brush_count = 0;
  std::size_t replaced_patch_count = 0;
  std::vector<std::string> diagnostics;
};

class BrushBuilder {
 public:
  [[nodiscard]] BrushBuildResult build(const mtb::map::MapDocument& document,
                                       const ConversionPlan& plan) const;
};

}  // namespace mtb::conversion
