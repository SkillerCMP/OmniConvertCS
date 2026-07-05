#include "util/hex.hpp"

#include <charconv>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace omni::hex {

bool is_hex_digit(char value) noexcept {
    const unsigned char c = static_cast<unsigned char>(value);
    return std::isxdigit(c) != 0;
}

std::optional<std::uint32_t> parse_u32(std::string_view text) {
    std::string clean = trim(text);
    if (clean.size() >= 2U && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) {
        clean.erase(0, 2);
    }
    if (clean.empty() || clean.size() > 8U) return std::nullopt;

    std::uint32_t value{};
    const char* begin = clean.data();
    const char* end = clean.data() + clean.size();
    const auto [ptr, error] = std::from_chars(begin, end, value, 16);
    if (error != std::errc{} || ptr != end) return std::nullopt;
    return value;
}

std::string format_u32(std::uint32_t value) {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << value;
    return output.str();
}

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) ++begin;
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) --end;
    return std::string(text.substr(begin, end - begin));
}

} // namespace omni::hex
