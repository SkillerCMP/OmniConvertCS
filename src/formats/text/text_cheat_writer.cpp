#include "formats/text/text_cheat_writer.hpp"

#include "core/inline_crypt.hpp"
#include "devices/armax/armax_codec.hpp"
#include "util/hex.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace omni::formats::text {
namespace {

std::string apply_mask(std::string value, const std::string& mask) {
    if (value.size() != 8U || mask.size() != 8U) return value;
    for (std::size_t index = 0; index < 8U; ++index) {
        if (mask[index] != '\0') value[index] = mask[index];
    }
    return value;
}

bool starts_with_case_insensitive(std::string_view text,
                                  std::string_view prefix) noexcept {
    if (text.size() < prefix.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), text.begin(),
                      [](char left, char right) {
                          return std::tolower(static_cast<unsigned char>(left)) ==
                                 std::tolower(static_cast<unsigned char>(right));
                      });
}

std::size_t find_case_insensitive(std::string_view text,
                                  std::string_view needle,
                                  std::size_t offset = 0U) noexcept {
    if (needle.empty() || offset > text.size() ||
        text.size() - offset < needle.size()) {
        return std::string_view::npos;
    }

    for (std::size_t index = offset;
         index + needle.size() <= text.size(); ++index) {
        bool matches = true;
        for (std::size_t part = 0U; part < needle.size(); ++part) {
            const unsigned char left =
                static_cast<unsigned char>(text[index + part]);
            const unsigned char right =
                static_cast<unsigned char>(needle[part]);
            if (std::tolower(left) != std::tolower(right)) {
                matches = false;
                break;
            }
        }
        if (matches) return index;
    }
    return std::string_view::npos;
}

std::string trim_copy(std::string_view value) {
    return hex::trim(value);
}

bool looks_like_raw_code_prefix(std::string_view label) {
    std::istringstream input{std::string(label)};
    std::string address;
    std::string value;
    if (!(input >> address >> value)) return false;

    if (address.size() < 6U || address.size() > 8U ||
        !std::all_of(address.begin(), address.end(), hex::is_hex_digit)) {
        return false;
    }

    if (value.empty() || value.size() > 8U) return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return hex::is_hex_digit(c) || c == '?' || c == 'X' || c == 'x';
    });
}

bool label_needs_escape(std::string_view label) noexcept {
    if (label.empty()) return false;

    // These prefixes have structural meaning to the shared text parser. A
    // leading '+' quotes the complete label and lets names such as "$500
    // Granted" and "# of Days Passed" survive a text round trip without
    // being mistaken for directives, comments, groups, or metadata.
    const char first = label.front();
    if (first == '+' || first == '!' || first == '^' || first == '$' ||
        first == '#' || first == ';') {
        return true;
    }
    if (label.rfind("//", 0U) == 0U) return true;
    if (looks_like_raw_code_prefix(label)) return true;
    return starts_with_case_insensitive(label, "%credits:");
}

bool is_explicit_group_open(const CheatBlock& block) noexcept {
    return block.kind == TextBlockKind::cmp_group_open;
}

bool is_explicit_group_close(const CheatBlock& block) noexcept {
    return block.kind == TextBlockKind::cmp_group_close;
}

bool is_plain_group_heading(const CheatBlock& block) {
    return block.kind == TextBlockKind::normal && block.cheat.empty() &&
           !block.conversion_error && block.label && !block.label->empty();
}

struct CmpLabel {
    std::string name;
    std::optional<std::string> credits;
};

CmpLabel split_cmp_credits(std::string header) {
    // Inline Omni/CMP credit convention examples:
    //   Code Name by Author , CRYPT_RAW
    //   Code Name , by Author , Crypt Codebreaker v7+
    // Use the last " by " segment so names containing an earlier occurrence
    // remain intact. The author runs to the next comma or the end of the name.
    std::size_t by = std::string_view::npos;
    std::size_t search = 0U;
    while (true) {
        const std::size_t found =
            find_case_insensitive(header, " by ", search);
        if (found == std::string_view::npos) break;
        by = found;
        search = found + 4U;
    }
    if (by == std::string_view::npos) return {std::move(header), std::nullopt};

    const std::size_t author_begin = by + 4U;
    const std::size_t comma = header.find(',', author_begin);
    const std::size_t author_end =
        comma == std::string::npos ? header.size() : comma;
    std::string author =
        trim_copy(std::string_view(header).substr(author_begin,
                                                  author_end - author_begin));
    if (author.empty()) return {std::move(header), std::nullopt};

    std::size_t remove_begin = by;
    std::size_t before = by;
    while (before > 0U &&
           std::isspace(static_cast<unsigned char>(header[before - 1U])) != 0) {
        --before;
    }
    if (before > 0U && header[before - 1U] == ',') {
        remove_begin = before - 1U;
    }

    std::string name = trim_copy(header.substr(0U, remove_begin));
    if (comma != std::string::npos) {
        const std::string suffix =
            trim_copy(std::string_view(header).substr(comma + 1U));
        if (!suffix.empty()) {
            name += " , ";
            name += suffix;
        }
    }
    return {std::move(name), std::move(author)};
}

void close_one_cmp_group(std::string& output, std::size_t& group_depth) {
    if (group_depth == 0U) return;
    output += "!!\n";
    --group_depth;
}

void close_all_cmp_groups(std::string& output, std::size_t& group_depth) {
    while (group_depth > 0U) close_one_cmp_group(output, group_depth);
}

void write_cmp_group_open(std::string& output, std::string header,
                          std::size_t& group_depth) {
    output += '!';
    output += trim_copy(header);
    output += ":\n";
    ++group_depth;
}

void write_plain_cmp_group_heading(std::string& output, std::string header,
                                   std::size_t& group_depth) {
    close_all_cmp_groups(output, group_depth);
    write_cmp_group_open(output, std::move(header), group_depth);
}

} // namespace

std::string write_text(const std::vector<CheatBlock>& blocks, Crypt output_crypt,
                       WriteOptions options) {
    std::string output;
    std::size_t cmp_group_depth = 0U;

    for (const CheatBlock& block : blocks) {
        std::string header = block.label.value_or(
            block.cheat.name.empty() ? "Unnamed Cheat" : block.cheat.name);
        header = update_existing_crypt_tag(header, output_crypt);

        if (block.conversion_error) {
            output += "// [ERROR] ";
            output += header.empty() ? "Code" : header;
            output += '\n';
            std::size_t begin = 0U;
            while (begin <= block.conversion_error->size()) {
                const std::size_t end = block.conversion_error->find('\n', begin);
                output += "// ";
                output += block.conversion_error->substr(
                    begin, end == std::string::npos ? std::string::npos
                                                    : end - begin);
                output += '\n';
                if (end == std::string::npos) break;
                begin = end + 1U;
            }
            output += '\n';
            continue;
        }

        if (is_explicit_group_close(block)) {
            if (options.cmp_output_mode) close_one_cmp_group(output, cmp_group_depth);
            continue;
        }

        if (options.cmp_output_mode && is_explicit_group_open(block)) {
            write_cmp_group_open(output, std::move(header), cmp_group_depth);
            continue;
        }

        if (options.cmp_output_mode && is_plain_group_heading(block)) {
            write_plain_cmp_group_heading(output, std::move(header), cmp_group_depth);
            continue;
        }

        CmpLabel cmp_label{std::move(header), std::nullopt};
        if (options.cmp_output_mode) {
            cmp_label = split_cmp_credits(std::move(cmp_label.name));
        }

        if (!cmp_label.name.empty()) {
            if (options.cmp_output_mode || label_needs_escape(cmp_label.name)) {
                output += '+';
            }
            output += cmp_label.name;
            output += '\n';
        }
        if (options.cmp_output_mode && cmp_label.credits) {
            output += "%Credits: ";
            output += *cmp_label.credits;
            output += '\n';
        }

        if (output_crypt == Crypt::armax) {
            for (const CodePair& pair : block.cheat.pairs()) {
                if (options.cmp_output_mode) output += '$';
                output += devices::armax::encode_line(pair);
                output += '\n';
            }
            continue;
        }

        std::unordered_map<std::size_t, std::string> masks;
        for (const WildcardMask& wildcard : block.wildcards) {
            masks[wildcard.word_index] = wildcard.value_mask;
        }

        for (std::size_t index = 0; index < block.cheat.word_count();
             index += 2U) {
            const std::uint32_t address = block.cheat.words[index];
            const std::uint32_t value =
                (index + 1U < block.cheat.word_count())
                    ? block.cheat.words[index + 1U]
                    : 0U;
            std::string value_text = hex::format_u32(value);
            const auto mask = masks.find(index);
            if (mask != masks.end()) {
                value_text = apply_mask(std::move(value_text), mask->second);
            }
            if (options.cmp_output_mode) output += '$';
            output += hex::format_u32(address);
            output += ' ';
            output += value_text;
            output += '\n';
        }
    }

    if (options.cmp_output_mode) close_all_cmp_groups(output, cmp_group_depth);
    return output;
}

} // namespace omni::formats::text
