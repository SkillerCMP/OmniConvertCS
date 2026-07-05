#pragma once

#include <cstdint>
#include <vector>

namespace omni::devices::gameshark {

void crypt_file_data(std::vector<std::uint8_t>& data);
[[nodiscard]] std::uint32_t file_crc32(const std::vector<std::uint8_t>& data);

} // namespace omni::devices::gameshark
