#pragma once

#include "core/cheat.hpp"

#include <cstdint>

namespace omni::devices::codebreaker {

[[nodiscard]] CodePair encrypt_v1(CodePair code) noexcept;
[[nodiscard]] CodePair decrypt_v1(CodePair code) noexcept;

void encrypt_v1(std::uint32_t& address, std::uint32_t& value) noexcept;
void decrypt_v1(std::uint32_t& address, std::uint32_t& value) noexcept;

} // namespace omni::devices::codebreaker
