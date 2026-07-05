#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <vector>

namespace omni::devices::armax {

inline constexpr std::uint32_t default_payload_key = 0x04030209U;

struct Metadata {
    std::uint32_t game_id{};
    std::uint8_t region{};
    std::size_t verifier_lines{};
};

[[nodiscard]] Metadata decrypt_full(std::vector<std::uint32_t>& words,
                                    std::uint32_t payload_key = default_payload_key);
void encrypt_full(std::vector<std::uint32_t>& words,
                  std::uint32_t payload_key = default_payload_key);

[[nodiscard]] std::size_t read_verifier_lines(const std::vector<std::uint32_t>& words);
[[nodiscard]] std::uint32_t random_verifier_nonce();
[[nodiscard]] std::array<std::uint32_t, 2> make_verifier(
    const std::vector<std::uint32_t>& payload, std::uint32_t game_id,
    std::uint8_t region);
[[nodiscard]] std::array<std::uint32_t, 2> make_verifier(
    const std::vector<std::uint32_t>& payload, std::uint32_t game_id,
    std::uint8_t region, std::uint32_t nonce) noexcept;
void prepend_verifier(std::vector<std::uint32_t>& payload, std::uint32_t game_id,
                      std::uint8_t region);
void prepend_verifier(std::vector<std::uint32_t>& payload, std::uint32_t game_id,
                      std::uint8_t region, std::uint32_t nonce);
[[nodiscard]] std::uint16_t crc16(const std::vector<std::uint32_t>& words) noexcept;
[[nodiscard]] std::uint8_t checksum_nibble(const std::vector<std::uint32_t>& words) noexcept;

} // namespace omni::devices::armax
