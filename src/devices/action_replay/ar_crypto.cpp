#include "devices/action_replay/ar_crypto.hpp"

#include <array>
#include <cstddef>

namespace omni::devices::action_replay {
namespace {

constexpr std::array<std::array<std::uint8_t, 32>, 4> table{{
    {{0x00, 0x1F, 0x9B, 0x69, 0xA5, 0x80, 0x90, 0xB2, 0xD7, 0x44, 0xEC, 0x75, 0x3B, 0x62, 0x0C, 0xA3,
      0xA6, 0xE4, 0x1F, 0x4C, 0x05, 0xE4, 0x44, 0x6E, 0xD9, 0x5B, 0x34, 0xE6, 0x08, 0x31, 0x91, 0x72}},
    {{0x00, 0xAE, 0xF3, 0x7B, 0x12, 0xC9, 0x83, 0xF0, 0xA9, 0x57, 0x50, 0x08, 0x04, 0x81, 0x02, 0x21,
      0x96, 0x09, 0x0F, 0x90, 0xC3, 0x62, 0x27, 0x21, 0x3B, 0x22, 0x4E, 0x88, 0xF5, 0xC5, 0x75, 0x91}},
    {{0x00, 0xE3, 0xA2, 0x45, 0x40, 0xE0, 0x09, 0xEA, 0x42, 0x65, 0x1C, 0xC1, 0xEB, 0xB0, 0x69, 0x14,
      0x01, 0xD2, 0x8E, 0xFB, 0xFA, 0x86, 0x09, 0x95, 0x1B, 0x61, 0x14, 0x0E, 0x99, 0x21, 0xEC, 0x40}},
    {{0x00, 0x25, 0x6D, 0x4F, 0xC5, 0xCA, 0x04, 0x39, 0x3A, 0x7D, 0x0D, 0xF1, 0x43, 0x05, 0x71, 0x66,
      0x82, 0x31, 0x21, 0xD8, 0xFE, 0x4D, 0xC2, 0xC8, 0xCC, 0x09, 0xA0, 0x06, 0x49, 0xD5, 0xF1, 0x83}}
}};

constexpr std::uint8_t nibble_flip(std::uint8_t value) noexcept {
    return static_cast<std::uint8_t>((value << 4U) | (value >> 4U));
}

std::array<std::uint8_t, 4> word_bytes(std::uint32_t code) noexcept {
    return {{
        static_cast<std::uint8_t>(code & 0xFFU),
        static_cast<std::uint8_t>((code >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((code >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((code >> 24U) & 0xFFU)
    }};
}

std::uint32_t make_word(const std::array<std::uint8_t, 4>& bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

} // namespace

std::array<std::uint8_t, 4> seed_bytes(std::uint32_t key) noexcept {
    return {{
        static_cast<std::uint8_t>((key >> 24U) & 0xFFU),
        static_cast<std::uint8_t>((key >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((key >> 8U) & 0xFFU),
        static_cast<std::uint8_t>(key & 0xFFU)
    }};
}

std::uint32_t seed_key(const std::array<std::uint8_t, 4>& bytes) noexcept {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
           (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) |
           static_cast<std::uint32_t>(bytes[3]);
}

std::uint32_t decrypt_word(std::uint32_t code, std::uint8_t type, std::uint8_t seed) noexcept {
    if (type == 7U) {
        if ((seed & 1U) != 0U) {
            type = 1U;
        } else {
            return ~code;
        }
    }

    auto bytes = word_bytes(code);
    const std::size_t s = static_cast<std::size_t>(seed & 0x1FU);

    switch (type) {
        case 0U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] ^ table[0][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] ^ table[1][s]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] ^ table[2][s]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] ^ table[3][s]);
            break;
        case 1U:
            bytes[3] = static_cast<std::uint8_t>(nibble_flip(bytes[3]) ^ table[0][s]);
            bytes[2] = static_cast<std::uint8_t>(nibble_flip(bytes[2]) ^ table[2][s]);
            bytes[1] = static_cast<std::uint8_t>(nibble_flip(bytes[1]) ^ table[3][s]);
            bytes[0] = static_cast<std::uint8_t>(nibble_flip(bytes[0]) ^ table[1][s]);
            break;
        case 2U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] + table[0][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] + table[1][s]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] + table[2][s]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] + table[3][s]);
            break;
        case 3U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] - table[3][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] - table[2][s]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] - table[1][s]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] - table[0][s]);
            break;
        case 4U:
            bytes[3] = static_cast<std::uint8_t>((bytes[3] ^ table[0][s]) + table[0][s]);
            bytes[2] = static_cast<std::uint8_t>((bytes[2] ^ table[3][s]) + table[3][s]);
            bytes[1] = static_cast<std::uint8_t>((bytes[1] ^ table[1][s]) + table[1][s]);
            bytes[0] = static_cast<std::uint8_t>((bytes[0] ^ table[2][s]) + table[2][s]);
            break;
        case 5U:
            bytes[3] = static_cast<std::uint8_t>((bytes[3] - table[1][s]) ^ table[0][s]);
            bytes[2] = static_cast<std::uint8_t>((bytes[2] - table[2][s]) ^ table[1][s]);
            bytes[1] = static_cast<std::uint8_t>((bytes[1] - table[3][s]) ^ table[2][s]);
            bytes[0] = static_cast<std::uint8_t>((bytes[0] - table[0][s]) ^ table[3][s]);
            break;
        case 6U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] + table[0][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] - table[1][(s + 1U) & 0x1FU]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] + table[2][(s + 2U) & 0x1FU]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] - table[3][(s + 3U) & 0x1FU]);
            break;
        default:
            break;
    }

    return make_word(bytes);
}

std::uint32_t encrypt_word(std::uint32_t code, std::uint8_t type, std::uint8_t seed) noexcept {
    if (type == 7U) {
        if ((seed & 1U) != 0U) {
            type = 1U;
        } else {
            return ~code;
        }
    }

    auto bytes = word_bytes(code);
    const std::size_t s = static_cast<std::size_t>(seed & 0x1FU);

    switch (type) {
        case 0U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] ^ table[0][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] ^ table[1][s]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] ^ table[2][s]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] ^ table[3][s]);
            break;
        case 1U:
            bytes[3] = nibble_flip(static_cast<std::uint8_t>(bytes[3] ^ table[0][s]));
            bytes[2] = nibble_flip(static_cast<std::uint8_t>(bytes[2] ^ table[2][s]));
            bytes[1] = nibble_flip(static_cast<std::uint8_t>(bytes[1] ^ table[3][s]));
            bytes[0] = nibble_flip(static_cast<std::uint8_t>(bytes[0] ^ table[1][s]));
            break;
        case 2U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] - table[0][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] - table[1][s]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] - table[2][s]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] - table[3][s]);
            break;
        case 3U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] + table[3][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] + table[2][s]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] + table[1][s]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] + table[0][s]);
            break;
        case 4U:
            bytes[3] = static_cast<std::uint8_t>((bytes[3] - table[0][s]) ^ table[0][s]);
            bytes[2] = static_cast<std::uint8_t>((bytes[2] - table[3][s]) ^ table[3][s]);
            bytes[1] = static_cast<std::uint8_t>((bytes[1] - table[1][s]) ^ table[1][s]);
            bytes[0] = static_cast<std::uint8_t>((bytes[0] - table[2][s]) ^ table[2][s]);
            break;
        case 5U:
            bytes[3] = static_cast<std::uint8_t>((bytes[3] ^ table[0][s]) + table[1][s]);
            bytes[2] = static_cast<std::uint8_t>((bytes[2] ^ table[1][s]) + table[2][s]);
            bytes[1] = static_cast<std::uint8_t>((bytes[1] ^ table[2][s]) + table[3][s]);
            bytes[0] = static_cast<std::uint8_t>((bytes[0] ^ table[3][s]) + table[0][s]);
            break;
        case 6U:
            bytes[3] = static_cast<std::uint8_t>(bytes[3] - table[0][s]);
            bytes[2] = static_cast<std::uint8_t>(bytes[2] + table[1][(s + 1U) & 0x1FU]);
            bytes[1] = static_cast<std::uint8_t>(bytes[1] - table[2][(s + 2U) & 0x1FU]);
            bytes[0] = static_cast<std::uint8_t>(bytes[0] + table[3][(s + 3U) & 0x1FU]);
            break;
        default:
            break;
    }

    return make_word(bytes);
}

} // namespace omni::devices::action_replay
