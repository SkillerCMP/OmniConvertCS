#include "devices/armax/armax_codec.hpp"

#include <array>
#include <cctype>
#include <stdexcept>

namespace omni::devices::armax {
namespace {

int alphabet_value(char value) noexcept {
    const unsigned char ch = static_cast<unsigned char>(value);
    const char upper = static_cast<char>(std::toupper(ch));
    const std::size_t index = alphabet.find(upper);
    return index == std::string_view::npos ? -1 : static_cast<int>(index);
}

std::array<char, 13> remove_dashes(std::string_view text) noexcept {
    std::array<char, 13> result{};
    std::size_t output = 0U;
    for (const char raw : text) {
        if (raw == '-') continue;
        if (output >= result.size()) return {};
        const unsigned char ch = static_cast<unsigned char>(raw);
        result[output++] = static_cast<char>(std::toupper(ch));
    }
    if (output != result.size()) return {};
    return result;
}

std::uint8_t parity64(std::uint32_t first, std::uint32_t second) noexcept {
    std::uint8_t parity = 0U;
    for (std::uint32_t bit = 0U; bit < 32U; ++bit) {
        parity ^= static_cast<std::uint8_t>((first >> bit) & 1U);
        parity ^= static_cast<std::uint8_t>((second >> bit) & 1U);
    }
    return static_cast<std::uint8_t>(parity & 1U);
}

} // namespace

bool is_encrypted_line(std::string_view text) noexcept {
    if (text.size() != encrypted_line_length || text[4] != '-' || text[9] != '-') return false;
    for (std::size_t index = 0U; index < text.size(); ++index) {
        if (index == 4U || index == 9U) continue;
        if (alphabet_value(text[index]) < 0) return false;
    }
    return true;
}

std::optional<CodePair> decode_line(std::string_view text, bool* parity_valid) noexcept {
    if (parity_valid != nullptr) *parity_valid = false;
    if (!is_encrypted_line(text)) return std::nullopt;

    const auto chars = remove_dashes(text);
    std::array<std::uint32_t, 13> values{};
    for (std::size_t index = 0U; index < chars.size(); ++index) {
        const int value = alphabet_value(chars[index]);
        if (value < 0) return std::nullopt;
        values[index] = static_cast<std::uint32_t>(value);
    }

    std::uint32_t first = 0U;
    for (std::size_t index = 0U; index < 6U; ++index) {
        const std::uint32_t shift = static_cast<std::uint32_t>(((5U - index) * 5U) + 2U);
        first |= values[index] << shift;
    }
    first |= values[6] >> 3U;

    std::uint32_t second = 0U;
    for (std::size_t index = 0U; index < 6U; ++index) {
        const std::uint32_t shift = static_cast<std::uint32_t>(((5U - index) * 5U) + 4U);
        second |= values[index + 6U] << shift;
    }
    second |= values[12] >> 1U;

    const bool valid = parity64(first, second) == static_cast<std::uint8_t>(values[12] & 1U);
    if (parity_valid != nullptr) *parity_valid = valid;
    return CodePair{first, second};
}

std::string encode_line(const CodePair& pair) {
    std::string result(15U, '\0');
    result[4] = '-';
    result[9] = '-';

    std::size_t output = 0U;
    for (std::size_t index = 0U; index < 6U; ++index) {
        if (output == 4U || output == 9U) ++output;
        const std::uint32_t shift = static_cast<std::uint32_t>(((5U - index) * 5U) + 2U);
        result[output++] = alphabet[(pair.address >> shift) & 0x1FU];
    }

    result[output++] = alphabet[((pair.address << 3U) | (pair.value >> 29U)) & 0x1FU];

    for (std::size_t index = 1U; index < 6U; ++index) {
        if (output == 4U || output == 9U) ++output;
        const std::uint32_t shift = static_cast<std::uint32_t>(((5U - index) * 5U) + 4U);
        result[output++] = alphabet[(pair.value >> shift) & 0x1FU];
    }

    if (output == 4U || output == 9U) ++output;
    const std::uint32_t final_value = ((pair.value << 1U) & 0x1EU) |
                                      static_cast<std::uint32_t>(parity64(pair.address, pair.value));
    result[output] = alphabet[final_value];
    return result;
}

} // namespace omni::devices::armax
