#include "conversion/BrushBuilder.hpp"
#include "map/MapDocument.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void fail(const std::string& message) {
  std::cerr << message << "\n";
  std::exit(1);
}

void require(bool condition, const char* message) {
  if (!condition) {
    fail(std::string("requirement failed: ") + message);
  }
}

std::string valid_patch_map() {
  return R"MAP({
"classname" "worldspawn"
{
patchDef2
{
textures/test/fuzz
( 3 3 0 0 0 )
(
( ( 0 0 0 0 0 ) ( 32 0 0 0.5 0 ) ( 64 0 0 1 0 ) )
( ( 0 32 0 0 0.5 ) ( 32 32 4 0.5 0.5 ) ( 64 32 0 1 0.5 ) )
( ( 0 64 0 0 1 ) ( 32 64 0 0.5 1 ) ( 64 64 0 1 1 ) )
)
}
}
}
)MAP";
}

std::vector<std::string> handcrafted_cases() {
  return {
      "",
      "{",
      "}",
      "{ \"classname\" \"worldspawn\" ",
      "{ \"classname\" \"worldspawn\" { patchDef2 } }",
      "{ \"classname\" \"worldspawn\" { patchDef2 { } } }",
      "{ \"classname\" \"worldspawn\" { patchDef2 { textures/fuzz } } }",
      "{ \"classname\" \"worldspawn\" { patchDef2 { textures/fuzz "
      "( 3 3 0 0 0 ) } } }",
      "{ \"classname\" \"worldspawn\" { patchDef2 { textures/fuzz "
      "( three 3 0 0 0 ) ( ) } } }",
      "{ \"classname\" \"worldspawn\" { patchDef2 { textures/fuzz "
      "( 3 3 0 0 0 ) ( ( ( 0 0 0 0 0 ) ) ) } } }",
      "{ \"classname\" \"worldspawn\" { patchDef2 { textures/fuzz "
      "( 3 3 0 0 0 ) ( ( ( nan 0 0 0 0 ) ) ) } } }",
      "{ \"classname\" \"worldspawn\" { patchDef2 { \"textures/fuzz\" "
      "( 3 3 0 0 0 ) ( ( ( 0 0 0 0 ) ) ) } } }",
      "{ \"classname\" \"worldspawn\" // unterminated style bait\n"
      "{ brushDef { ( 0 0 0 ) ( 0 128 0 ) ( 128 0 0 ) "
      "( ( 1 0 0 ) ( 0 1 0 ) ) tex 0 0 0 } }",
      valid_patch_map() + valid_patch_map(),
  };
}

std::string generated_case(std::uint32_t seed) {
  static constexpr const char* tokens[] = {
      "{", "}", "(", ")", "\"classname\"", "\"worldspawn\"",
      "patchDef2", "patchDef3", "textures/fuzz/generated",
      "( 3 3 0 0 0 )", "0", "1", "-64", "64", "0.5",
      "( ( 0 0 0 0 0 ) )", "// comment\n", "/* block comment */",
  };

  std::uint32_t state = seed * 1664525u + 1013904223u;
  auto next = [&]() mutable {
    state = state * 1664525u + 1013904223u;
    return state;
  };

  std::ostringstream out;
  const std::size_t count = 16 + (next() % 48);
  for (std::size_t index = 0; index < count; ++index) {
    out << tokens[next() % (sizeof(tokens) / sizeof(tokens[0]))];
    if (next() % 5 == 0) {
      out << "\n";
    } else {
      out << " ";
    }
  }
  return out.str();
}

void exercise_case(const std::string& source, std::size_t case_index) {
  try {
    const mtb::map::MapDocument document = mtb::map::MapDocument::parse(source);
    mtb::conversion::ConversionSettings settings;
    settings.preserve_patches = true;
    settings.replace_patches = false;
    const mtb::conversion::ConversionPlan plan =
        mtb::conversion::ConversionPlanner(settings).plan(document);
    const std::string report =
        mtb::conversion::render_markdown_report(plan, settings);
    require(!report.empty(), "report rendering returns text");

    const mtb::conversion::BrushBuildResult build =
        mtb::conversion::BrushBuilder().build(document, plan);
    require(build.implemented, "builder remains implemented");
    const mtb::map::MapDocument reparsed =
        mtb::map::MapDocument::parse(build.map_text);
    (void)reparsed;
  } catch (const std::exception& error) {
    std::ostringstream message;
    message << "fuzz case " << case_index << " threw exception: "
            << error.what() << "\nsource:\n" << source;
    fail(message.str());
  } catch (...) {
    std::ostringstream message;
    message << "fuzz case " << case_index << " threw unknown exception\nsource:\n"
            << source;
    fail(message.str());
  }
}

}  // namespace

int main() {
  std::size_t case_index = 0;
  for (const std::string& source : handcrafted_cases()) {
    exercise_case(source, case_index++);
  }

  for (std::uint32_t seed = 1; seed <= 128; ++seed) {
    exercise_case(generated_case(seed), case_index++);
  }

  return 0;
}
