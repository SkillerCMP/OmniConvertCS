#pragma once

#include "core/cheat.hpp"
#include "core/crypt.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace omni::formats::text {

struct WildcardMask {
    std::size_t word_index{};
    std::string value_mask;
};

enum class TextBlockKind {
    normal,
    cmp_group_open,
    cmp_group_close,
};

struct CheatBlock {
    TextBlockKind kind{TextBlockKind::normal};
    std::optional<std::string> label;
    Cheat cheat;
    std::vector<WildcardMask> wildcards;
    bool has_armax_encrypted_line{};
    std::optional<std::string> conversion_error;
    std::vector<std::string> original_lines;
};

[[nodiscard]] std::vector<CheatBlock> parse_text(std::string_view input);
[[nodiscard]] std::vector<CheatBlock> parse_text(std::string_view input, Crypt input_crypt);
[[nodiscard]] std::vector<CheatBlock> parse_inline_text(std::string_view input);
[[nodiscard]] std::vector<CheatBlock> parse_file(const std::string& path);
[[nodiscard]] std::vector<CheatBlock> parse_file(const std::string& path, Crypt input_crypt);
[[nodiscard]] std::vector<CheatBlock> parse_inline_file(const std::string& path);

} // namespace omni::formats::text
