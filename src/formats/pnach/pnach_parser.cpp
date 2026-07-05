#include "formats/pnach/pnach_parser.hpp"

#include "util/hex.hpp"
#include "util/newlines.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace omni::formats::pnach {
namespace {

std::string trim(std::string_view value) {
    return hex::trim(value);
}

bool starts_with_icase(std::string_view value, std::string_view prefix) noexcept {
    if (value.size() < prefix.size()) return false;
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        const auto lhs = static_cast<unsigned char>(value[index]);
        const auto rhs = static_cast<unsigned char>(prefix[index]);
        if (std::tolower(lhs) != std::tolower(rhs)) return false;
    }
    return true;
}

std::vector<std::string> split_commas(std::string_view line) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t comma = line.find(',', start);
        const std::size_t end = comma == std::string_view::npos ? line.size() : comma;
        const std::string part = trim(line.substr(start, end - start));
        if (!part.empty()) parts.push_back(part);
        if (comma == std::string_view::npos) break;
        start = comma + 1U;
    }
    return parts;
}

std::optional<std::uint32_t> parse_address_token(std::string token) {
    const std::size_t comment = token.find_first_of(" \t#;/");
    if (comment != std::string::npos) token.erase(comment);
    if (token.size() < 6U || token.size() > 8U) return std::nullopt;
    return hex::parse_u32(token);
}

bool is_raw_value_nibble(char c) noexcept {
    return hex::is_hex_digit(c) || c == '?' || c == 'X' || c == 'x';
}

struct SanitizedValue {
    std::uint32_t value{};
    std::string mask;
};

std::optional<SanitizedValue> sanitize_value_token(std::string token) {
    const std::size_t comment = token.find_first_of(" \t#;/");
    if (comment != std::string::npos) token.erase(comment);
    token = trim(token);
    if (token.size() >= 2U && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        token.erase(0, 2);
    }
    if (token.empty() || token.size() > 8U) return std::nullopt;
    if (!std::all_of(token.begin(), token.end(), is_raw_value_nibble)) return std::nullopt;

    if (token.size() < 8U) token.insert(token.begin(), 8U - token.size(), '0');

    std::string clean(8U, '0');
    std::string mask(8U, '\0');
    bool has_mask = false;
    for (std::size_t index = 0; index < 8U; ++index) {
        const char c = token[index];
        if (hex::is_hex_digit(c)) {
            clean[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else {
            mask[index] = c;
            has_mask = true;
        }
    }

    const auto parsed = hex::parse_u32(clean);
    if (!parsed) return std::nullopt;
    return SanitizedValue{*parsed, has_mask ? mask : std::string{}};
}

std::string collapse_spaces(std::string value) {
    std::string output;
    output.reserve(value.size());
    bool previous_space = false;
    for (const char c : value) {
        const bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
        if (space) {
            if (!previous_space && !output.empty()) output.push_back(' ');
        } else {
            output.push_back(c);
        }
        previous_space = space;
    }
    while (!output.empty() && output.back() == ' ') output.pop_back();
    return output;
}

std::string normalize_author(std::string author) {
    author = collapse_spaces(trim(author));
    while (!author.empty() && (author.back() == ',' || author.back() == ';')) author.pop_back();
    author = trim(author);

    bool has_letter = false;
    bool all_upper = true;
    bool has_digit = false;
    for (const char c : author) {
        const auto value = static_cast<unsigned char>(c);
        if (std::isdigit(value) != 0) has_digit = true;
        if (std::isalpha(value) != 0) {
            has_letter = true;
            if (std::isupper(value) == 0) all_upper = false;
        }
    }

    if (has_letter && all_upper && !has_digit) {
        bool new_word = true;
        for (char& c : author) {
            const auto value = static_cast<unsigned char>(c);
            if (std::isalpha(value) != 0) {
                c = static_cast<char>(new_word ? std::toupper(value) : std::tolower(value));
                new_word = false;
            } else {
                new_word = std::isspace(value) != 0;
            }
        }
    }
    return author;
}

std::string strip_existing_by(std::string label) {
    std::string lower(label);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    const std::size_t by = lower.find(" by ");
    if (by != std::string::npos) label.erase(by);
    return trim(label);
}

} // namespace

std::vector<text::CheatBlock> parse_text(std::string_view input) {
    std::vector<text::CheatBlock> blocks;
    text::CheatBlock* current = nullptr;
    std::optional<std::string> current_group;

    auto start_block = [&](std::optional<std::string> label) -> text::CheatBlock& {
        blocks.push_back(text::CheatBlock{});
        current = &blocks.back();
        current->label = std::move(label);
        return *current;
    };

    auto ensure_block = [&]() -> text::CheatBlock& {
        if (current == nullptr) return start_block(std::nullopt);
        return *current;
    };

    const std::string normalized = newlines::to_lf(input);
    std::istringstream lines(normalized);
    std::string raw_line;

    while (std::getline(lines, raw_line)) {
        const std::string line = trim(raw_line);
        if (line.empty() || starts_with_icase(line, "//") || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (starts_with_icase(line, "gametitle=") ||
            starts_with_icase(line, "comment=") ||
            starts_with_icase(line, "crc=") ||
            starts_with_icase(line, "description=")) {
            continue;
        }

        if (starts_with_icase(line, "author=")) {
            const std::string author = normalize_author(line.substr(7U));
            if (!author.empty() && current != nullptr) {
                std::string label = strip_existing_by(current->label.value_or(std::string{}));
                current->label = label.empty() ? std::string("by ") + author
                                               : label + " by " + author;
            }
            continue;
        }

        if (line.size() >= 3U && line.front() == '[' && line.back() == ']') {
            const std::string inner = trim(std::string_view(line).substr(1U, line.size() - 2U));
            if (inner.empty()) continue;

            std::optional<std::string> group_name;
            std::string code_name = inner;
            const std::size_t slash = inner.find('\\');
            if (slash != std::string::npos) {
                const std::string group = trim(std::string_view(inner).substr(0U, slash));
                if (!group.empty()) group_name = group;
                code_name = trim(std::string_view(inner).substr(slash + 1U));
            }

            if (group_name && (!current_group || *current_group != *group_name)) {
                start_block(*group_name);
                current_group = *group_name;
            }
            if (!code_name.empty()) start_block(code_name);
            continue;
        }

        if (starts_with_icase(line, "patch=")) {
            const std::vector<std::string> parts = split_commas(line);
            if (parts.size() >= 5U) {
                const auto address = parse_address_token(parts[2]);
                const auto value = sanitize_value_token(parts[4]);
                if (address && value) {
                    text::CheatBlock& block = ensure_block();
                    const std::size_t word_index = block.cheat.word_count();
                    block.cheat.append_pair(*address, value->value);
                    block.original_lines.push_back(raw_line);
                    if (!value->mask.empty()) {
                        block.wildcards.push_back(text::WildcardMask{word_index, value->mask});
                    }
                }
            }
            continue;
        }

        // PNACH input intentionally ignores non-section/non-patch content.
    }

    return blocks;
}

} // namespace omni::formats::pnach
