#pragma once

#include "devices/action_replay/ar_crypto.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace omni::devices::action_replay {

class Context {
public:
    explicit Context(std::uint32_t key = ar1_seed) noexcept;

    void set_seed(std::uint32_t key) noexcept;
    [[nodiscard]] std::uint32_t get_seed() const noexcept;

    void decrypt_ar2(std::vector<std::uint32_t>& words, bool remove_key_codes = true);
    void encrypt_ar2(std::vector<std::uint32_t>& words);

private:
    std::array<std::uint8_t, 4> seed_{};
};

void decrypt_ar1(std::vector<std::uint32_t>& words);
void encrypt_ar1(std::vector<std::uint32_t>& words);

void prepend_ar2_key_code(std::vector<std::uint32_t>& words, std::uint32_t key);

} // namespace omni::devices::action_replay
