#pragma once

#include "formats/text/text_cheat_parser.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace omni::formats::pnach {

struct WriteOptions {
    std::string game_name;
    bool include_crc{};
    std::optional<std::uint32_t> crc;
};

[[nodiscard]] std::string write_text(const std::vector<text::CheatBlock>& blocks,
                                     const WriteOptions& options = {});

} // namespace omni::formats::pnach
