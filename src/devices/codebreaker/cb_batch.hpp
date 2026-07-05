#pragma once

#include <cstdint>
#include <vector>

namespace omni::devices::codebreaker {

void encrypt_v1(std::vector<std::uint32_t>& words);
void decrypt_v1(std::vector<std::uint32_t>& words);

void encrypt_common_v7(std::vector<std::uint32_t>& words);
void decrypt_common_v7(std::vector<std::uint32_t>& words);

} // namespace omni::devices::codebreaker
