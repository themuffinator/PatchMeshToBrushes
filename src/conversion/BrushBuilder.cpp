#include "conversion/BrushBuilder.hpp"

#include <string>

namespace mtb::conversion {

BrushBuildResult BrushBuilder::build(const mtb::map::MapDocument& document,
                                     const ConversionPlan& plan) const {
  BrushBuildResult result;
  result.map_text = std::string(document.source());
  result.diagnostics.push_back(
      "Brush construction is not implemented yet; no map output was changed.");
  result.diagnostics.push_back(
      "Planned patch count: " + std::to_string(plan.patches.size()) + ".");
  return result;
}

}  // namespace mtb::conversion
