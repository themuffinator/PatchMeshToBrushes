#pragma once

#include "conversion/ConversionSettings.hpp"
#include "map/MapDocument.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace mtb::conversion {

enum class PlannedStrategy {
  PendingGeometryAnalysis,
  NeedsParserRepair,
};

struct PatchPlan {
  mtb::map::PatchSummary patch;
  PlannedStrategy strategy = PlannedStrategy::PendingGeometryAnalysis;
  std::vector<std::string> notes;
};

struct ConversionPlan {
  std::size_t entity_count = 0;
  std::size_t brush_count = 0;
  std::size_t parser_diagnostic_count = 0;
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
