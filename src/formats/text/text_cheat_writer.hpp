#pragma once

#include "formats/text/text_cheat_parser.hpp"
#include "core/crypt.hpp"

#include <string>
#include <vector>

namespace omni::formats::text {

struct WriteOptions {
    bool cmp_output_mode{};
};

[[nodiscard]] std::string write_text(const std::vector<CheatBlock>& blocks,
                                     Crypt output_crypt = Crypt::raw,
                                     WriteOptions options = {});

} // namespace omni::formats::text
