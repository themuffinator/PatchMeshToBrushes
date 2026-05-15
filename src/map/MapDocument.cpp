#include "map/MapDocument.hpp"

#include <cctype>
#include <string_view>
#include <utility>

namespace mtb::map {
namespace {

bool is_identifier_start(char value) {
  return std::isalpha(static_cast<unsigned char>(value)) || value == '_';
}

bool is_identifier_continue(char value) {
  return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
}

PatchKind kind_from_identifier(std::string_view token) {
  if (token == "patchDef2") {
    return PatchKind::PatchDef2;
  }

  if (token == "patchDef3") {
    return PatchKind::PatchDef3;
  }

  return PatchKind::PatchDef;
}

class Scanner {
 public:
  explicit Scanner(std::string_view source) : source_(source) {}

  std::vector<PatchSummary> scan() {
    std::vector<PatchSummary> patches;
    std::size_t entity_depth = 0;
    std::size_t entity_index = 0;
    std::size_t patch_index = 0;

    while (position_ < source_.size()) {
      if (skip_whitespace_or_comment()) {
        continue;
      }

      const char value = source_[position_];

      if (value == '"') {
        skip_quoted_string();
        continue;
      }

      if (value == '{') {
        if (entity_depth == 0) {
          patch_index = 0;
        }
        ++entity_depth;
        ++position_;
        continue;
      }

      if (value == '}') {
        if (entity_depth > 0) {
          --entity_depth;
          if (entity_depth == 0) {
            ++entity_index;
          }
        }
        ++position_;
        continue;
      }

      if (is_identifier_start(value)) {
        const std::size_t start = position_;
        ++position_;
        while (position_ < source_.size() &&
               is_identifier_continue(source_[position_])) {
          ++position_;
        }

        const std::string_view token =
            source_.substr(start, position_ - start);
        if (token == "patchDef" || token == "patchDef2" ||
            token == "patchDef3") {
          patches.push_back({
              kind_from_identifier(token),
              {start, position_},
              entity_index,
              patch_index++,
          });
        }
        continue;
      }

      ++position_;
    }

    return patches;
  }

 private:
  bool skip_whitespace_or_comment() {
    if (std::isspace(static_cast<unsigned char>(source_[position_]))) {
      ++position_;
      return true;
    }

    if (source_[position_] != '/' || position_ + 1 >= source_.size()) {
      return false;
    }

    const char next = source_[position_ + 1];
    if (next == '/') {
      position_ += 2;
      while (position_ < source_.size() && source_[position_] != '\n') {
        ++position_;
      }
      return true;
    }

    if (next == '*') {
      position_ += 2;
      while (position_ + 1 < source_.size() &&
             !(source_[position_] == '*' && source_[position_ + 1] == '/')) {
        ++position_;
      }
      if (position_ + 1 < source_.size()) {
        position_ += 2;
      }
      return true;
    }

    return false;
  }

  void skip_quoted_string() {
    ++position_;
    while (position_ < source_.size()) {
      if (source_[position_] == '\\' && position_ + 1 < source_.size()) {
        position_ += 2;
        continue;
      }

      if (source_[position_] == '"') {
        ++position_;
        break;
      }

      ++position_;
    }
  }

  std::string_view source_;
  std::size_t position_ = 0;
};

}  // namespace

MapDocument MapDocument::parse(std::string source) {
  MapDocument document(std::move(source));
  document.patches_ = Scanner(document.source_).scan();
  return document;
}

std::string_view MapDocument::source() const {
  return source_;
}

const std::vector<PatchSummary>& MapDocument::patches() const {
  return patches_;
}

MapDocument::MapDocument(std::string source) : source_(std::move(source)) {}

std::string_view patch_kind_name(PatchKind kind) {
  switch (kind) {
    case PatchKind::PatchDef:
      return "patchDef";
    case PatchKind::PatchDef2:
      return "patchDef2";
    case PatchKind::PatchDef3:
      return "patchDef3";
  }

  return "unknown";
}

}  // namespace mtb::map
