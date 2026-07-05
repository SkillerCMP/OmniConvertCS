#include "util/newlines.hpp"

namespace omni::newlines {
namespace {

template <typename Character>
std::basic_string<Character> normalize_to_lf(std::basic_string_view<Character> text) {
    std::basic_string<Character> result;
    result.reserve(text.size());

    for (std::size_t index = 0; index < text.size(); ++index) {
        const Character character = text[index];
        if (character == static_cast<Character>('\r')) {
            result.push_back(static_cast<Character>('\n'));
            if ((index + 1U) < text.size() &&
                text[index + 1U] == static_cast<Character>('\n')) {
                ++index;
            }
            continue;
        }
        result.push_back(character);
    }

    return result;
}

template <typename Character>
std::basic_string<Character> normalize_to_crlf(std::basic_string_view<Character> text) {
    std::basic_string<Character> result;
    result.reserve(text.size() + (text.size() / 8U));

    for (std::size_t index = 0; index < text.size(); ++index) {
        const Character character = text[index];

        if (character == static_cast<Character>('\r')) {
            result.push_back(static_cast<Character>('\r'));
            result.push_back(static_cast<Character>('\n'));
            if ((index + 1U) < text.size() &&
                text[index + 1U] == static_cast<Character>('\n')) {
                ++index;
            }
            continue;
        }

        if (character == static_cast<Character>('\n')) {
            result.push_back(static_cast<Character>('\r'));
            result.push_back(static_cast<Character>('\n'));
            continue;
        }

        result.push_back(character);
    }

    return result;
}

} // namespace

std::string to_lf(std::string_view text) {
    return normalize_to_lf<char>(text);
}

std::wstring to_lf(std::wstring_view text) {
    return normalize_to_lf<wchar_t>(text);
}

std::string to_crlf(std::string_view text) {
    return normalize_to_crlf<char>(text);
}

std::wstring to_crlf(std::wstring_view text) {
    return normalize_to_crlf<wchar_t>(text);
}

} // namespace omni::newlines
