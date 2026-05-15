#pragma once

#include "conversion/ConversionPlanner.hpp"

#include <string>
#include <vector>

namespace mtb::conversion {

struct BrushBuildResult {
  bool implemented = false;
  std::string map_text;
  std::vector<std::string> diagnostics;
};

class BrushBuilder {
 public:
  [[nodiscard]] BrushBuildResult build(const mtb::map::MapDocument& document,
                                       const ConversionPlan& plan) const;
};

}  // namespace mtb::conversion
