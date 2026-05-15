#pragma once

#include "conversion/ConversionSettings.hpp"
#include "conversion/PatchTopology.hpp"
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
  std::size_t assembly_index = 0;
  bool has_assembly = false;
  std::vector<std::string> notes;
};

struct AssemblyPlan {
  std::size_t assembly_index = 0;
  AssemblyKind kind = AssemblyKind::Arbitrary;
  std::vector<std::size_t> patch_indices;
  std::size_t connection_count = 0;
  std::size_t unmatched_boundary_spans = 0;
};

struct ConversionPlan {
  std::size_t entity_count = 0;
  std::size_t brush_count = 0;
  std::size_t parser_diagnostic_count = 0;
  std::size_t topology_connection_count = 0;
  std::size_t skipped_topology_patch_count = 0;
  std::vector<AssemblyPlan> assemblies;
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
