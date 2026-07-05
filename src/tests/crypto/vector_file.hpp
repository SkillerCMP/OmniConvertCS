#pragma once

#include "core/cheat.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace omni::tests::crypto_vectors {

[[nodiscard]] std::string read_text(const std::filesystem::path& path);
[[nodiscard]] std::vector<CodePair> parse_hex_pairs(const std::string& text);
[[nodiscard]] std::vector<std::string> parse_armax_lines(const std::string& text);

} // namespace omni::tests::crypto_vectors
