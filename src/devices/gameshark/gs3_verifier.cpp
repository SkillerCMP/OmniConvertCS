#include "devices/gameshark/gs3_verifier.hpp"

#include <array>
#include <stdexcept>

namespace omni::devices::gameshark {
namespace {

void validate_words(const std::vector<std::uint32_t>& words) {
    if ((words.size() & 1U) != 0U) {
        throw std::invalid_argument(
            "GameShark/Xploder verification requires complete address/value pairs");
    }
}

[[nodiscard]] const std::array<std::uint16_t, 256>& ccitt_table() {
    static const std::array<std::uint16_t, 256> table = [] {
        std::array<std::uint16_t, 256> result{};
        constexpr std::uint16_t polynomial = 0x1021U;

        for (std::size_t i = 0; i < result.size(); ++i) {
            std::uint16_t value = static_cast<std::uint16_t>(i << 8U);
            for (int bit = 0; bit < 8; ++bit) {
                if ((value & 0x8000U) != 0U) {
                    value = static_cast<std::uint16_t>((value << 1U) ^ polynomial);
                } else {
                    value = static_cast<std::uint16_t>(value << 1U);
                }
            }
            result[i] = value;
        }
        return result;
    }();
    return table;
}

} // namespace

std::uint16_t ccitt_crc_words(const std::vector<std::uint32_t>& words) {
    const auto& table = ccitt_table();
    std::uint16_t crc = 0U;

    for (const std::uint32_t word : words) {
        const std::array<std::uint8_t, 4> bytes{
            static_cast<std::uint8_t>(word >> 24U),
            static_cast<std::uint8_t>(word >> 16U),
            static_cast<std::uint8_t>(word >> 8U),
            static_cast<std::uint8_t>(word),
        };
        for (const std::uint8_t byte : bytes) {
            const std::uint8_t index =
                static_cast<std::uint8_t>((crc >> 8U) ^ byte);
            crc = static_cast<std::uint16_t>((crc << 8U) ^ table[index]);
        }
    }
    return crc;
}

CodePair create_verifier(const std::vector<std::uint32_t>& words) {
    validate_words(words);
    return CodePair{
        0x76000000U |
            (static_cast<std::uint32_t>(ccitt_crc_words(words)) << 4U),
        0U,
    };
}

bool looks_like_verifier(CodePair code) noexcept {
    // GameShark/Xploder V5 verifier rows are 760xxxxx 00000000. The CRC
    // occupies address bits 4-19; all remaining bits are fixed.
    return (code.address & 0xFFF0000FU) == 0x76000000U &&
           code.value == 0U;
}

bool has_valid_verifier(const std::vector<std::uint32_t>& words) {
    validate_words(words);
    if (words.size() < 4U) return false;

    const CodePair supplied{words[0], words[1]};
    if (!looks_like_verifier(supplied)) return false;

    const std::vector<std::uint32_t> payload(words.begin() + 2,
                                             words.end());
    return supplied == create_verifier(payload);
}

void add_verifier(std::vector<std::uint32_t>& words) {
    const CodePair verifier = create_verifier(words);
    words.insert(words.begin(), {verifier.address, verifier.value});
}

} // namespace omni::devices::gameshark
