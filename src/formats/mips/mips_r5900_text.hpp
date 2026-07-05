#pragma once

#include "formats/text/text_cheat_parser.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace omni::formats::mips_r5900 {

struct ParseResult {
    std::vector<text::CheatBlock> blocks;
    std::vector<std::string> errors;

    [[nodiscard]] bool success() const noexcept;
};

[[nodiscard]] ParseResult parse_text(std::string_view input);
[[nodiscard]] std::string write_text(const std::vector<text::CheatBlock>& blocks);

} // namespace omni::formats::mips_r5900
