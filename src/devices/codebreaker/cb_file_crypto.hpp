#pragma once

#include <cstdint>
#include <vector>

namespace omni::devices::codebreaker {

void crypt_file_data(std::vector<std::uint8_t>& data);

} // namespace omni::devices::codebreaker
