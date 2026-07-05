#pragma once

#include "core/crypt.hpp"

#include <optional>
#include <string_view>

namespace omni {

// User-facing format identity. Several products share the same crypto but must
// remain distinct in the GUI and future semantic translation layer.
enum class CodeFormat {
    standard_raw,
    inline_headers,
    pnach_raw,
    ps2_mips_r5900,
    armax_raw,
    ar12_raw,
    codebreaker_raw,
    gameshark12_raw,
    gameshark3_raw,

    action_replay1,
    action_replay2,
    action_replay_max,
    codebreaker1_6,
    codebreaker7_common,
    gameshark_interact1,
    gameshark_interact2,
    gameshark_madcatz34,
    gameshark_madcatz5,
    swap_magic_coder3,
    xploder1_3,
    xploder4,
    xploder5
};

[[nodiscard]] std::optional<CodeFormat> parse_code_format(std::string_view text);
[[nodiscard]] std::string_view code_format_token(CodeFormat format) noexcept;
[[nodiscard]] std::string_view code_format_name(CodeFormat format) noexcept;
[[nodiscard]] Crypt crypt_for_format(CodeFormat format) noexcept;
[[nodiscard]] Device device_for_format(CodeFormat format) noexcept;
[[nodiscard]] bool code_format_is_encrypted(CodeFormat format) noexcept;

} // namespace omni
