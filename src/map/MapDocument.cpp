#include "map/MapDocument.hpp"

#include <charconv>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mtb::map {
namespace {

enum class TokenKind {
  End,
  Atom,
  String,
  OpenBrace,
  CloseBrace,
  OpenParen,
  CloseParen,
};

struct Token {
  TokenKind kind = TokenKind::End;
  SourceSpan span;
  std::string_view text;
};

bool is_patch_keyword(std::string_view token) {
  return token == "patchDef" || token == "patchDef2" || token == "patchDef3";
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

bool parse_int(std::string_view value, int& out) {
  const char* begin = value.data();
  const char* end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, out);
  return result.ec == std::errc{} && result.ptr == end;
}

bool parse_size(std::string_view value, std::size_t& out) {
  int parsed = 0;
  if (!parse_int(value, parsed) || parsed < 0) {
    return false;
  }

  out = static_cast<std::size_t>(parsed);
  return true;
}

bool parse_double(std::string_view value, double& out) {
  const std::string text(value);
  char* parsed_end = nullptr;
  errno = 0;
  const double parsed = std::strtod(text.c_str(), &parsed_end);
  if (parsed_end != text.c_str() + text.size() || errno == ERANGE ||
      !std::isfinite(parsed)) {
    return false;
  }

  out = parsed;
  return true;
}

class TokenCursor {
 public:
  TokenCursor(std::string_view source, std::size_t begin, std::size_t end)
      : source_(source), position_(begin), end_(end) {}

  [[nodiscard]] std::size_t position() const { return position_; }
  void set_position(std::size_t position) { position_ = position; }

  Token next() {
    skip_trivia();

    if (position_ >= end_) {
      return {TokenKind::End, {position_, position_}, {}};
    }

    const std::size_t start = position_;
    const char value = source_[position_++];

    switch (value) {
      case '{':
        return {TokenKind::OpenBrace, {start, position_}, source_.substr(start, 1)};
      case '}':
        return {TokenKind::CloseBrace, {start, position_}, source_.substr(start, 1)};
      case '(':
        return {TokenKind::OpenParen, {start, position_}, source_.substr(start, 1)};
      case ')':
        return {TokenKind::CloseParen, {start, position_}, source_.substr(start, 1)};
      case '"':
        return read_string(start);
      default:
        --position_;
        return read_atom();
    }
  }

 private:
  void skip_trivia() {
    while (position_ < end_) {
      if (std::isspace(static_cast<unsigned char>(source_[position_]))) {
        ++position_;
        continue;
      }

      if (source_[position_] != '/' || position_ + 1 >= end_) {
        return;
      }

      const char next = source_[position_ + 1];
      if (next == '/') {
        position_ += 2;
        while (position_ < end_ && source_[position_] != '\n') {
          ++position_;
        }
        continue;
      }

      if (next == '*') {
        position_ += 2;
        while (position_ + 1 < end_ &&
               !(source_[position_] == '*' && source_[position_ + 1] == '/')) {
          ++position_;
        }
        if (position_ + 1 < end_) {
          position_ += 2;
        }
        continue;
      }

      return;
    }
  }

  Token read_string(std::size_t start) {
    const std::size_t text_start = position_;
    while (position_ < end_) {
      if (source_[position_] == '\\' && position_ + 1 < end_) {
        position_ += 2;
        continue;
      }

      if (source_[position_] == '"') {
        const std::size_t text_end = position_;
        ++position_;
        return {TokenKind::String,
                {start, position_},
                source_.substr(text_start, text_end - text_start)};
      }

      ++position_;
    }

    return {TokenKind::String,
            {start, position_},
            source_.substr(text_start, position_ - text_start)};
  }

  Token read_atom() {
    const std::size_t start = position_;
    while (position_ < end_) {
      const char value = source_[position_];
      if (std::isspace(static_cast<unsigned char>(value)) || value == '{' ||
          value == '}' || value == '(' || value == ')' || value == '"') {
        break;
      }

      if (value == '/' && position_ + 1 < end_ &&
          (source_[position_ + 1] == '/' || source_[position_ + 1] == '*')) {
        break;
      }

      ++position_;
    }

    if (position_ == start) {
      ++position_;
    }

    return {TokenKind::Atom,
            {start, position_},
            source_.substr(start, position_ - start)};
  }

  std::string_view source_;
  std::size_t position_ = 0;
  std::size_t end_ = 0;
};

void skip_quoted_string(std::string_view source, std::size_t& position,
                        std::size_t limit) {
  ++position;
  while (position < limit) {
    if (source[position] == '\\' && position + 1 < limit) {
      position += 2;
      continue;
    }

    if (source[position] == '"') {
      ++position;
      return;
    }

    ++position;
  }
}

void skip_comment(std::string_view source, std::size_t& position,
                  std::size_t limit) {
  if (position + 1 >= limit || source[position] != '/') {
    return;
  }

  if (source[position + 1] == '/') {
    position += 2;
    while (position < limit && source[position] != '\n') {
      ++position;
    }
    return;
  }

  if (source[position + 1] == '*') {
    position += 2;
    while (position + 1 < limit &&
           !(source[position] == '*' && source[position + 1] == '/')) {
      ++position;
    }
    if (position + 1 < limit) {
      position += 2;
    }
  }
}

std::optional<SourceSpan> find_matching_brace(std::string_view source,
                                              std::size_t open_offset,
                                              std::size_t limit) {
  if (open_offset >= limit || source[open_offset] != '{') {
    return std::nullopt;
  }

  std::size_t position = open_offset;
  std::size_t depth = 0;
  while (position < limit) {
    const char value = source[position];

    if (value == '"') {
      skip_quoted_string(source, position, limit);
      continue;
    }

    if (value == '/' && position + 1 < limit &&
        (source[position + 1] == '/' || source[position + 1] == '*')) {
      skip_comment(source, position, limit);
      continue;
    }

    if (value == '{') {
      ++depth;
      ++position;
      continue;
    }

    if (value == '}') {
      --depth;
      ++position;
      if (depth == 0) {
        return SourceSpan{open_offset, position};
      }
      continue;
    }

    ++position;
  }

  return std::nullopt;
}

std::optional<Token> first_token_inside(std::string_view source, SourceSpan span) {
  if (span.end <= span.start + 1) {
    return std::nullopt;
  }

  TokenCursor cursor(source, span.start + 1, span.end - 1);
  const Token token = cursor.next();
  if (token.kind == TokenKind::End) {
    return std::nullopt;
  }

  return token;
}

SourceSpan span_from(const Token& start, const Token& end) {
  return {start.span.start, end.span.end};
}

}  // namespace

class MapParser {
 public:
  explicit MapParser(std::string source) : document_(std::move(source)) {}

  MapDocument parse() {
    parse_entities();
    return std::move(document_);
  }

 private:
  void add_diagnostic(DiagnosticSeverity severity, SourceSpan span,
                      std::string message) {
    document_.diagnostics_.push_back({severity, span, std::move(message)});
  }

  void parse_entities() {
    TokenCursor cursor(document_.source_, 0, document_.source_.size());
    while (true) {
      const Token token = cursor.next();
      if (token.kind == TokenKind::End) {
        break;
      }

      if (token.kind != TokenKind::OpenBrace) {
        add_diagnostic(DiagnosticSeverity::Warning, token.span,
                       "Skipping token outside of an entity.");
        continue;
      }

      const std::optional<SourceSpan> span =
          find_matching_brace(document_.source_, token.span.start,
                              document_.source_.size());
      if (!span.has_value()) {
        add_diagnostic(DiagnosticSeverity::Error, token.span,
                       "Unclosed entity block.");
        break;
      }

      Entity entity;
      entity.span = *span;
      entity.entity_index = document_.entities_.size();
      parse_entity_body(entity);
      document_.entities_.push_back(std::move(entity));
      cursor.set_position(span->end);
    }
  }

  void parse_entity_body(Entity& entity) {
    TokenCursor cursor(document_.source_, entity.span.start + 1,
                       entity.span.end - 1);
    std::size_t brush_index = 0;
    std::size_t patch_index = 0;

    while (true) {
      const Token token = cursor.next();
      if (token.kind == TokenKind::End) {
        return;
      }

      if (token.kind == TokenKind::String) {
        parse_key_value(entity, cursor, token);
        continue;
      }

      if (token.kind == TokenKind::OpenBrace) {
        const std::optional<SourceSpan> primitive_span =
            find_matching_brace(document_.source_, token.span.start,
                                entity.span.end - 1);
        if (!primitive_span.has_value()) {
          add_diagnostic(DiagnosticSeverity::Error, token.span,
                         "Unclosed primitive block.");
          return;
        }

        parse_primitive(entity, *primitive_span, brush_index, patch_index);
        cursor.set_position(primitive_span->end);
        continue;
      }

      add_diagnostic(DiagnosticSeverity::Warning, token.span,
                     "Skipping unexpected token in entity body.");
    }
  }

  void parse_key_value(Entity& entity, TokenCursor& cursor, const Token& key) {
    const Token value = cursor.next();
    if (value.kind != TokenKind::String) {
      add_diagnostic(DiagnosticSeverity::Error, key.span,
                     "Entity key is missing a quoted value.");
      return;
    }

    entity.key_values.push_back({
        std::string(key.text),
        std::string(value.text),
        span_from(key, value),
        key.span,
        value.span,
    });
  }

  void parse_primitive(Entity& entity, SourceSpan primitive_span,
                       std::size_t& brush_index, std::size_t& patch_index) {
    const std::optional<Token> first =
        first_token_inside(document_.source_, primitive_span);
    if (!first.has_value()) {
      add_diagnostic(DiagnosticSeverity::Warning, primitive_span,
                     "Skipping empty primitive block.");
      return;
    }

    if (first->kind == TokenKind::Atom && is_patch_keyword(first->text)) {
      PatchSummary patch =
          parse_patch(entity.entity_index, patch_index, primitive_span, *first);
      entity.patches.push_back(patch);
      document_.patches_.push_back(std::move(patch));
      ++patch_index;
      return;
    }

    BrushSummary brush = parse_brush(entity.entity_index, brush_index,
                                     primitive_span, *first);
    entity.brushes.push_back(brush);
    document_.brushes_.push_back(brush);
    ++brush_index;
  }

  BrushSummary parse_brush(std::size_t entity_index, std::size_t brush_index,
                           SourceSpan span, const Token& first) const {
    BrushSummary brush;
    brush.span = span;
    brush.entity_index = entity_index;
    brush.brush_index = brush_index;
    brush.uses_brush_def =
        first.kind == TokenKind::Atom && first.text == "brushDef";
    brush.face_count = brush.uses_brush_def ? count_brush_def_faces(span)
                                            : count_legacy_brush_faces(span);
    return brush;
  }

  std::size_t count_legacy_brush_faces(SourceSpan span) const {
    TokenCursor cursor(document_.source_, span.start + 1, span.end - 1);
    std::size_t point_groups = 0;

    while (true) {
      const Token token = cursor.next();
      if (token.kind == TokenKind::End) {
        break;
      }

      if (token.kind != TokenKind::OpenParen) {
        continue;
      }

      const std::size_t before_group = cursor.position();
      if (consume_numeric_group(cursor, 3)) {
        ++point_groups;
      } else {
        cursor.set_position(before_group);
      }
    }

    return point_groups / 3;
  }

  std::size_t count_brush_def_faces(SourceSpan span) const {
    TokenCursor cursor(document_.source_, span.start + 1, span.end - 1);
    std::size_t face_count = 0;

    while (true) {
      const Token token = cursor.next();
      if (token.kind == TokenKind::End) {
        break;
      }

      if (token.kind == TokenKind::OpenParen &&
          consume_brush_def_plane(cursor)) {
        ++face_count;
      }
    }

    return face_count;
  }

  bool consume_brush_def_plane(TokenCursor& cursor) const {
    const std::size_t after_first_open = cursor.position();
    if (consume_numeric_group(cursor, 4)) {
      return true;
    }

    cursor.set_position(after_first_open);
    if (!consume_numeric_group(cursor, 3)) {
      return false;
    }

    for (int point_index = 1; point_index < 3; ++point_index) {
      const Token open = cursor.next();
      if (open.kind != TokenKind::OpenParen ||
          !consume_numeric_group(cursor, 3)) {
        return false;
      }
    }

    return true;
  }

  bool consume_numeric_group(TokenCursor& cursor, std::size_t expected) const {
    std::size_t count = 0;
    while (true) {
      const Token token = cursor.next();
      if (token.kind == TokenKind::End) {
        return false;
      }

      if (token.kind == TokenKind::CloseParen) {
        return count == expected;
      }

      if (token.kind != TokenKind::Atom) {
        return false;
      }

      double ignored = 0.0;
      if (!parse_double(token.text, ignored)) {
        return false;
      }

      ++count;
    }
  }

  PatchSummary parse_patch(std::size_t entity_index, std::size_t patch_index,
                           SourceSpan span, const Token& keyword) {
    PatchSummary patch;
    patch.kind = kind_from_identifier(keyword.text);
    patch.span = span;
    patch.keyword_span = keyword.span;
    patch.entity_index = entity_index;
    patch.patch_index = patch_index;

    TokenCursor outer(document_.source_, keyword.span.end, span.end - 1);
    const Token body_open = outer.next();
    if (body_open.kind != TokenKind::OpenBrace) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, keyword.span,
                     "Patch keyword is missing its body block.");
      return patch;
    }

    const std::optional<SourceSpan> body_span =
        find_matching_brace(document_.source_, body_open.span.start, span.end - 1);
    if (!body_span.has_value()) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, body_open.span,
                     "Patch body block is not closed.");
      return patch;
    }

    patch.body_span = *body_span;
    TokenCursor body(document_.source_, body_span->start + 1, body_span->end - 1);

    parse_patch_material(patch, body);
    parse_patch_dimensions(patch, body);
    parse_patch_control_grid(patch, body);
    validate_patch_grid(patch);

    return patch;
  }

  void parse_patch_material(PatchSummary& patch, TokenCursor& body) {
    const Token material = body.next();
    if (material.kind != TokenKind::Atom && material.kind != TokenKind::String) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, patch.keyword_span,
                     "Patch is missing a material token.");
      return;
    }

    patch.material = std::string(material.text);
    patch.material_span = material.span;
  }

  void parse_patch_dimensions(PatchSummary& patch, TokenCursor& body) {
    const Token open = body.next();
    if (open.kind != TokenKind::OpenParen) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, patch.keyword_span,
                     "Patch is missing its dimension tuple.");
      return;
    }

    std::vector<Token> values;
    Token close;
    while (true) {
      const Token token = body.next();
      if (token.kind == TokenKind::End) {
        patch.malformed = true;
        add_diagnostic(DiagnosticSeverity::Error, open.span,
                       "Patch dimension tuple is not closed.");
        return;
      }

      if (token.kind == TokenKind::CloseParen) {
        close = token;
        break;
      }

      values.push_back(token);
    }

    patch.dimensions.span = span_from(open, close);
    if (values.size() < 5) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, open.span,
                     "Patch dimension tuple must contain five values.");
      return;
    }

    if (!parse_size(values[0].text, patch.dimensions.rows) ||
        !parse_size(values[1].text, patch.dimensions.columns) ||
        !parse_int(values[2].text, patch.dimensions.contents) ||
        !parse_int(values[3].text, patch.dimensions.flags) ||
        !parse_int(values[4].text, patch.dimensions.value)) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, open.span,
                     "Patch dimension tuple contains invalid numeric values.");
      return;
    }

    patch.dimensions.parsed = true;
  }

  void parse_patch_control_grid(PatchSummary& patch, TokenCursor& body) {
    const Token open = body.next();
    if (open.kind != TokenKind::OpenParen) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, patch.keyword_span,
                     "Patch is missing its control point grid.");
      return;
    }

    while (true) {
      const Token row_or_close = body.next();
      if (row_or_close.kind == TokenKind::End) {
        patch.malformed = true;
        add_diagnostic(DiagnosticSeverity::Error, open.span,
                       "Patch control point grid is not closed.");
        return;
      }

      if (row_or_close.kind == TokenKind::CloseParen) {
        return;
      }

      if (row_or_close.kind != TokenKind::OpenParen) {
        patch.malformed = true;
        add_diagnostic(DiagnosticSeverity::Error, row_or_close.span,
                       "Patch grid row must begin with '('.");
        recover_until_row_or_grid_boundary(body);
        continue;
      }

      patch.control_points.push_back(parse_patch_row(patch, body, row_or_close));
    }
  }

  std::vector<PatchControlPoint> parse_patch_row(PatchSummary& patch,
                                                 TokenCursor& body,
                                                 const Token& row_open) {
    std::vector<PatchControlPoint> row;

    while (true) {
      const Token point_or_close = body.next();
      if (point_or_close.kind == TokenKind::End) {
        patch.malformed = true;
        add_diagnostic(DiagnosticSeverity::Error, row_open.span,
                       "Patch grid row is not closed.");
        return row;
      }

      if (point_or_close.kind == TokenKind::CloseParen) {
        return row;
      }

      if (point_or_close.kind != TokenKind::OpenParen) {
        patch.malformed = true;
        add_diagnostic(DiagnosticSeverity::Error, point_or_close.span,
                       "Patch control point must begin with '('.");
        continue;
      }

      row.push_back(parse_control_point(patch, body, point_or_close));
    }
  }

  PatchControlPoint parse_control_point(PatchSummary& patch, TokenCursor& body,
                                        const Token& point_open) {
    PatchControlPoint point;
    std::vector<Token> values;
    Token close;

    while (true) {
      const Token token = body.next();
      if (token.kind == TokenKind::End) {
        patch.malformed = true;
        add_diagnostic(DiagnosticSeverity::Error, point_open.span,
                       "Patch control point is not closed.");
        point.span = point_open.span;
        return point;
      }

      if (token.kind == TokenKind::CloseParen) {
        close = token;
        break;
      }

      values.push_back(token);
    }

    point.span = span_from(point_open, close);
    if (values.size() < 5) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, point_open.span,
                     "Patch control point must contain five values.");
      return point;
    }

    if (!parse_double(values[0].text, point.position.x) ||
        !parse_double(values[1].text, point.position.y) ||
        !parse_double(values[2].text, point.position.z) ||
        !parse_double(values[3].text, point.uv.u) ||
        !parse_double(values[4].text, point.uv.v)) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, point_open.span,
                     "Patch control point contains invalid numeric values.");
    }

    return point;
  }

  void recover_until_row_or_grid_boundary(TokenCursor& body) const {
    std::size_t depth = 0;
    while (true) {
      const Token token = body.next();
      if (token.kind == TokenKind::End) {
        return;
      }

      if (token.kind == TokenKind::OpenParen) {
        ++depth;
        continue;
      }

      if (token.kind == TokenKind::CloseParen) {
        if (depth == 0) {
          return;
        }
        --depth;
      }
    }
  }

  void validate_patch_grid(PatchSummary& patch) {
    if (!patch.dimensions.parsed) {
      return;
    }

    if (patch.control_points.size() != patch.dimensions.rows) {
      patch.malformed = true;
      add_diagnostic(DiagnosticSeverity::Error, patch.dimensions.span,
                     "Patch row count does not match its dimensions.");
    }

    for (const std::vector<PatchControlPoint>& row : patch.control_points) {
      if (row.size() != patch.dimensions.columns) {
        patch.malformed = true;
        add_diagnostic(DiagnosticSeverity::Error, patch.dimensions.span,
                       "Patch column count does not match its dimensions.");
        return;
      }
    }
  }

  MapDocument document_;
};

std::size_t PatchSummary::control_point_count() const {
  std::size_t count = 0;
  for (const std::vector<PatchControlPoint>& row : control_points) {
    count += row.size();
  }
  return count;
}

bool PatchSummary::has_expected_grid_size() const {
  if (!dimensions.parsed || control_points.size() != dimensions.rows) {
    return false;
  }

  for (const std::vector<PatchControlPoint>& row : control_points) {
    if (row.size() != dimensions.columns) {
      return false;
    }
  }

  return true;
}

MapDocument MapDocument::parse(std::string source) {
  return MapParser(std::move(source)).parse();
}

std::string_view MapDocument::source() const {
  return source_;
}

const std::vector<Entity>& MapDocument::entities() const {
  return entities_;
}

const std::vector<BrushSummary>& MapDocument::brushes() const {
  return brushes_;
}

const std::vector<PatchSummary>& MapDocument::patches() const {
  return patches_;
}

const std::vector<ParseDiagnostic>& MapDocument::diagnostics() const {
  return diagnostics_;
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

std::string_view diagnostic_severity_name(DiagnosticSeverity severity) {
  switch (severity) {
    case DiagnosticSeverity::Warning:
      return "warning";
    case DiagnosticSeverity::Error:
      return "error";
  }

  return "unknown";
}

}  // namespace mtb::map
