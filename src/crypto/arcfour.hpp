#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace omni::crypto {

class ArcFour {
public:
    ArcFour(const std::uint8_t* key, std::size_t key_size);

    void reset(const std::uint8_t* key, std::size_t key_size);
    void crypt(std::uint8_t* data, std::size_t size) noexcept;

private:
    std::array<std::uint8_t, 256> permutation_{};
    std::uint8_t index1_{};
    std::uint8_t index2_{};
};

} // namespace omni::crypto
