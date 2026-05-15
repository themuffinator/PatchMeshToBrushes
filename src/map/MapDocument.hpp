#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mtb::map {

enum class PatchKind {
  PatchDef,
  PatchDef2,
  PatchDef3,
};

struct SourceSpan {
  std::size_t start = 0;
  std::size_t end = 0;
};

struct PatchSummary {
  PatchKind kind = PatchKind::PatchDef;
  SourceSpan keyword_span;
  std::size_t entity_index = 0;
  std::size_t patch_index = 0;
};

class MapDocument {
 public:
  static MapDocument parse(std::string source);

  [[nodiscard]] std::string_view source() const;
  [[nodiscard]] const std::vector<PatchSummary>& patches() const;

 private:
  explicit MapDocument(std::string source);

  std::string source_;
  std::vector<PatchSummary> patches_;
};

std::string_view patch_kind_name(PatchKind kind);

}  // namespace mtb::map
