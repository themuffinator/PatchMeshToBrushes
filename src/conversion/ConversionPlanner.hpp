#pragma once

#include "conversion/ConversionSettings.hpp"
#include "map/MapDocument.hpp"

#include <string>
#include <vector>

namespace mtb::conversion {

enum class PlannedStrategy {
  PendingDetailedParse,
};

struct PatchPlan {
  mtb::map::PatchSummary patch;
  PlannedStrategy strategy = PlannedStrategy::PendingDetailedParse;
  std::vector<std::string> notes;
};

struct ConversionPlan {
  std::vector<PatchPlan> patches;
  std::vector<std::string> diagnostics;
};

class ConversionPlanner {
 public:
  explicit ConversionPlanner(ConversionSettings settings);

  [[nodiscard]] ConversionPlan plan(const mtb::map::MapDocument& document) const;

 private:
  ConversionSettings settings_;
};

std::string planned_strategy_name(PlannedStrategy strategy);

std::string render_markdown_report(const ConversionPlan& plan,
                                   const ConversionSettings& settings);

}  // namespace mtb::conversion
