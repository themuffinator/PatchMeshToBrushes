#pragma once

#include <filesystem>
#include <string>

namespace mtb::io {

std::string read_text_file(const std::filesystem::path& path);

void write_text_file(const std::filesystem::path& path, const std::string& text);

}  // namespace mtb::io
