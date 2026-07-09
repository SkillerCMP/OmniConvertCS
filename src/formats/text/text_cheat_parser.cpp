#include "formats/text/text_cheat_parser.hpp"

#include "devices/armax/armax_codec.hpp"
#include "util/hex.hpp"
#include "util/newlines.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace omni::formats::text {
namespace {

std::vector<std::string> split_whitespace(std::string_view line) {
    std::vector<std::string> parts;
    std::istringstream input{std::string(line)};
    std::string part;
    while (input >> part) parts.push_back(std::move(part));
    return parts;
}

bool is_comment(std::string_view line) {
    return line.rfind("//", 0) == 0 || line.rfind('#', 0) == 0 || line.rfind(';', 0) == 0;
}

std::optional<std::uint32_t> parse_address_token(std::string token) {
    const std::size_t comment = token.find_first_of(" \t#;/");
    if (comment != std::string::npos) token.erase(comment);
    if (token.size() < 6U || token.size() > 8U) return std::nullopt;
    return hex::parse_u32(token);
}

bool is_raw_value_nibble(char c) {
    return hex::is_hex_digit(c) || c == '?' || c == 'X' || c == 'x';
}

struct SanitizedValue {
    std::uint32_t value{};
    std::string mask;
};

std::optional<SanitizedValue> sanitize_value_token(std::string token) {
    token = hex::trim(token);
    if (token.size() >= 2U && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) token.erase(0, 2);
    if (token.empty() || token.size() > 8U) return std::nullopt;
    if (!std::all_of(token.begin(), token.end(), is_raw_value_nibble)) return std::nullopt;

    if (token.size() < 8U) token.insert(token.begin(), 8U - token.size(), '0');

    std::string clean(8U, '0');
    std::string mask(8U, '\0');
    bool has_mask = false;
    for (std::size_t i = 0; i < 8U; ++i) {
        const char c = token[i];
        if (hex::is_hex_digit(c)) {
            clean[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else {
            mask[i] = c;
            has_mask = true;
        }
    }

    const auto parsed = hex::parse_u32(clean);
    if (!parsed) return std::nullopt;
    return SanitizedValue{*parsed, has_mask ? mask : std::string{}};
}

bool looks_like_armax_code(std::string_view line) noexcept {
    return line.size() == devices::armax::encrypted_line_length &&
           line[4] == '-' && line[9] == '-';
}

std::vector<CheatBlock> parse_impl(std::string_view input, Crypt input_crypt, bool inline_mode) {
    std::vector<CheatBlock> blocks;
    CheatBlock* current = nullptr;

    auto start_block = [&](std::optional<std::string> label,
                           std::optional<std::string> raw_label,
                           TextBlockKind kind = TextBlockKind::normal) {
        blocks.push_back(CheatBlock{});
        current = &blocks.back();
        current->kind = kind;
        current->label = std::move(label);
        if (raw_label && !raw_label->empty()) current->original_lines.push_back(std::move(*raw_label));
    };

    auto ensure_block = [&]() -> CheatBlock& {
        if (current == nullptr) start_block(std::nullopt, std::nullopt);
        return *current;
    };

    const std::string normalized = newlines::to_lf(input);
    std::istringstream lines(normalized);
    std::string raw_line;

    while (std::getline(lines, raw_line)) {
        std::string raw = raw_line;
        while (!raw.empty() && std::isspace(static_cast<unsigned char>(raw.back())) != 0) raw.pop_back();
        std::string line = hex::trim(raw);
        if (line.empty() || is_comment(line)) continue;

        // CMP/OmniConvertCS text markers. These are structural metadata, not
        // literal cheat names. Keeping this normalization in the shared parser
        // makes GUI, CLI, RAW, and encrypted ARMAX input behave identically.
        if (line.front() == '^') continue;

        if (line == "!!") {
            start_block(std::nullopt, raw, TextBlockKind::cmp_group_close);
            continue;
        }

        if (line.front() == '!') {
            std::string group = hex::trim(std::string_view(line).substr(1U));
            if (!group.empty() && group.back() == ':') {
                group.pop_back();
                group = hex::trim(group);
            }
            start_block(std::move(group), raw, TextBlockKind::cmp_group_open);
            continue;
        }

        if (line.front() == '+') {
            start_block(hex::trim(std::string_view(line).substr(1U)), raw);
            continue;
        }

        if (line.size() >= 9U) {
            std::string prefix = line.substr(0U, 9U);
            std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (prefix == "%credits:") {
                const std::string credits = hex::trim(std::string_view(line).substr(9U));
                if (!credits.empty() && current != nullptr && current->label) {
                    std::string existing = *current->label;
                    std::string lower(existing);
                    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    const std::size_t by = lower.find(" by ");
                    if (by != std::string::npos) existing.erase(by);
                    current->label = hex::trim(existing) + " by " + credits;
                }
                continue;
            }
        }

        bool dollar_code = false;
        if (line.front() == '$') {
            line = hex::trim(std::string_view(line).substr(1U));
            if (line.empty()) continue;
            dollar_code = true;
        }

        const std::vector<std::string> parts = split_whitespace(line);

        // INLINE mode follows OmniConvertCS: every block may declare a
        // different CRYPT_XXXX tag, so ARMAX 4-4-5 text rows must be accepted
        // alongside ordinary hexadecimal rows. As in the C# parser, use the
        // last whitespace token as the ARMAX candidate.
        if (input_crypt == Crypt::armax || inline_mode) {
            const std::string_view candidate = parts.empty()
                                                   ? std::string_view(line)
                                                   : std::string_view(parts.back());
            bool parity_valid = false;
            const auto decoded = devices::armax::decode_line(candidate, &parity_valid);
            if (decoded) {
                if (!parity_valid) {
                    throw std::invalid_argument(
                        "ARMAX encrypted line has an invalid parity bit: " +
                        std::string(candidate));
                }
                CheatBlock& block = ensure_block();
                block.cheat.append_pair(decoded->address, decoded->value);
                block.has_armax_encrypted_line = true;
                block.original_lines.push_back(raw);
                continue;
            }
            if (looks_like_armax_code(candidate)) {
                throw std::invalid_argument("Invalid ARMAX encrypted line: " +
                                            std::string(candidate));
            }
        }

        if (parts.size() >= 2U) {
            const auto address = parse_address_token(parts[0]);
            const auto value = sanitize_value_token(parts[1]);
            if (address && value) {
                if (input_crypt == Crypt::armax && !inline_mode) {
                    throw std::invalid_argument(
                        "ARMAX encrypted input expects 4-4-5 code lines, not RAW hexadecimal rows");
                }
                CheatBlock& block = ensure_block();
                const std::size_t word_index = block.cheat.word_count();
                block.cheat.append_pair(*address, value->value);
                block.original_lines.push_back(raw);
                if (!value->mask.empty()) block.wildcards.push_back(WildcardMask{word_index, value->mask});
                continue;
            }
        }

        if (dollar_code) continue; // Ignore $delete/$insert and other directives.
        start_block(line, raw);
    }

    return blocks;
}

std::string read_file_text(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open input file: " + path);
    std::ostringstream data;
    data << input.rdbuf();
    return data.str();
}

} // namespace

std::vector<CheatBlock> parse_text(std::string_view input) {
    return parse_impl(input, Crypt::raw, false);
}

std::vector<CheatBlock> parse_text(std::string_view input, Crypt input_crypt) {
    return parse_impl(input, input_crypt, false);
}


std::vector<CheatBlock> parse_inline_text(std::string_view input) {
    return parse_impl(input, Crypt::raw, true);
}

std::vector<CheatBlock> parse_file(const std::string& path) {
    return parse_impl(read_file_text(path), Crypt::raw, false);
}

std::vector<CheatBlock> parse_file(const std::string& path, Crypt input_crypt) {
    return parse_impl(read_file_text(path), input_crypt, false);
}

std::vector<CheatBlock> parse_inline_file(const std::string& path) {
    return parse_impl(read_file_text(path), Crypt::raw, true);
}

} // namespace omni::formats::text
