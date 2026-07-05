#pragma once

#include <optional>
#include <string_view>

namespace omni {

enum class Device {
    ar1,
    ar2,
    armax,
    codebreaker,
    gameshark3,
    standard
};

enum class Crypt {
    ar1,
    ar2,
    armax,
    codebreaker,
    codebreaker7_common,
    gameshark3,
    gameshark5,
    max_raw,
    raw,
    ar2_raw,
    codebreaker_raw,
    gameshark_raw
};

[[nodiscard]] std::optional<Crypt> parse_crypt(std::string_view text);
[[nodiscard]] std::string_view crypt_token(Crypt crypt) noexcept;
[[nodiscard]] Device device_for_crypt(Crypt crypt) noexcept;

} // namespace omni
