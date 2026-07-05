#include "tests/crypto/vector_file.hpp"

#include "util/hex.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace omni::tests::crypto_vectors {
namespace {

bool is_upper_alnum(char value) noexcept {
    const unsigned char ch = static_cast<unsigned char>(value);
    return std::isdigit(ch) != 0 || (value >= 'A' && value <= 'Z');
}

bool is_armax_line(const std::string& line) noexcept {
    if (line.size() != 15U || line[4] != '-' || line[9] != '-') return false;
    for (std::size_t index = 0U; index < line.size(); ++index) {
        if (index == 4U || index == 9U) continue;
        if (!is_upper_alnum(line[index])) return false;
    }
    return true;
}

} // namespace

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open test vector: " + path.string());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<CodePair> parse_hex_pairs(const std::string& text) {
    std::vector<CodePair> result;
    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream fields(line);
        std::string first;
        std::string second;
        std::string extra;
        if (!(fields >> first >> second) || (fields >> extra)) continue;
        if (first.size() != 8U || second.size() != 8U) continue;
        if (!std::all_of(first.begin(), first.end(), hex::is_hex_digit) ||
            !std::all_of(second.begin(), second.end(), hex::is_hex_digit)) {
            continue;
        }

        const auto address = hex::parse_u32(first);
        const auto value = hex::parse_u32(second);
        if (address && value) result.push_back(CodePair{*address, *value});
    }

    return result;
}

std::vector<std::string> parse_armax_lines(const std::string& text) {
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = hex::trim(line);
        if (is_armax_line(line)) result.push_back(std::move(line));
    }

    return result;
}

} // namespace omni::tests::crypto_vectors
