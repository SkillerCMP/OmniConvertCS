#pragma once

#include "formats/text/text_cheat_parser.hpp"

#include <string_view>
#include <vector>

namespace omni::formats::pnach {

[[nodiscard]] std::vector<text::CheatBlock> parse_text(std::string_view input);

} // namespace omni::formats::pnach
