#include "conversion/ConversionPlanner.hpp"

#include <sstream>
#include <string>
#include <utility>

namespace mtb::conversion {

ConversionPlanner::ConversionPlanner(ConversionSettings settings)
    : settings_(settings) {}

ConversionPlan ConversionPlanner::plan(const mtb::map::MapDocument& document) const {
  ConversionPlan plan;

  if (document.patches().empty()) {
    plan.diagnostics.push_back("No patchDef blocks were discovered.");
    return plan;
  }

  for (const mtb::map::PatchSummary& patch : document.patches()) {
    PatchPlan patch_plan;
    patch_plan.patch = patch;
    patch_plan.notes.push_back(
        "Detailed control grid parsing is pending; strategy will be selected "
        "after topology and curvature analysis.");
    patch_plan.notes.push_back(
        "Minimum brush thickness setting is " +
        std::to_string(settings_.min_brush_thickness) + " units.");
    plan.patches.push_back(std::move(patch_plan));
  }

  plan.diagnostics.push_back(
      "Patch discovery complete. Geometry conversion is scaffolded but not yet "
      "implemented.");
  return plan;
}

std::string planned_strategy_name(PlannedStrategy strategy) {
  switch (strategy) {
    case PlannedStrategy::PendingDetailedParse:
      return "pending detailed parse";
  }

  return "unknown";
}

std::string render_markdown_report(const ConversionPlan& plan,
                                   const ConversionSettings& settings) {
  std::ostringstream out;
  out << "# MeshToBrushes Conversion Report\n\n";
  out << "## Settings\n\n";
  out << "- Minimum brush thickness: " << settings.min_brush_thickness << "\n";
  out << "- Vertex epsilon: " << settings.vertex_epsilon << "\n";
  out << "- Coplanar epsilon: " << settings.coplanar_epsilon << "\n";
  out << "- Preserve patches: " << (settings.preserve_patches ? "yes" : "no")
      << "\n";
  out << "- Replace patches: " << (settings.replace_patches ? "yes" : "no")
      << "\n\n";

  out << "## Summary\n\n";
  out << "- Discovered patches: " << plan.patches.size() << "\n\n";

  if (!plan.diagnostics.empty()) {
    out << "## Diagnostics\n\n";
    for (const std::string& diagnostic : plan.diagnostics) {
      out << "- " << diagnostic << "\n";
    }
    out << "\n";
  }

  out << "## Patch Plans\n\n";
  if (plan.patches.empty()) {
    out << "No patch plans were created.\n";
    return out.str();
  }

  for (const PatchPlan& patch : plan.patches) {
    out << "### Entity " << patch.patch.entity_index << ", Patch "
        << patch.patch.patch_index << "\n\n";
    out << "- Kind: " << mtb::map::patch_kind_name(patch.patch.kind) << "\n";
    out << "- Source offset: " << patch.patch.keyword_span.start << "\n";
    out << "- Strategy: " << planned_strategy_name(patch.strategy) << "\n";
    for (const std::string& note : patch.notes) {
      out << "- Note: " << note << "\n";
    }
    out << "\n";
  }

  return out.str();
}

}  // namespace mtb::conversion
