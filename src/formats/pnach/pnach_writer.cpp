#include "formats/pnach/pnach_writer.hpp"

#include "omni/identity.hpp"

#include "core/inline_crypt.hpp"
#include "util/hex.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace omni::formats::pnach {
namespace {

std::string apply_mask(std::string value, const std::string& mask) {
    if (value.size() != 8U || mask.size() != 8U) return value;
    for (std::size_t index = 0; index < 8U; ++index) {
        if (mask[index] != '\0') value[index] = mask[index];
    }
    return value;
}

std::string trim(std::string_view value) {
    return hex::trim(value);
}

std::optional<std::pair<std::string, std::string>> split_author(std::string_view input) {
    std::string text = trim(input);
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const std::size_t by = lower.find(" by ");
    if (by == std::string::npos) return std::nullopt;
    std::string name = trim(std::string_view(text).substr(0U, by));
    std::string author = trim(std::string_view(text).substr(by + 4U));
    if (name.empty() || author.empty()) return std::nullopt;
    return std::make_pair(std::move(name), std::move(author));
}

std::string join_path(const std::vector<std::string>& groups,
                      std::string header) {
    std::string output;
    for (const std::string& group : groups) {
        if (group.empty()) continue;
        if (!output.empty()) output += '\\';
        output += group;
    }
    if (!header.empty()) {
        if (!output.empty()) output += '\\';
        output += std::move(header);
    }
    return output;
}

} // namespace

std::string write_text(const std::vector<text::CheatBlock>& blocks,
                       const WriteOptions& options) {
    std::string output;
    output += "// " + std::string(omni::identity::converted_by) + "\n";
    if (options.include_crc && options.crc.has_value()) {
        const std::string game_name = options.game_name.empty() ? "Unknown Game" : options.game_name;
        output += "// GameName: " + game_name + "\n";
        output += "// CRC: " + hex::format_u32(*options.crc) + "\n";
    } else {
        output += "// NO CRC/NAME SET\n";
    }
    output += '\n';

    std::vector<std::string> current_groups;
    std::size_t code_number = 0U;

    for (const text::CheatBlock& block : blocks) {
        std::string header = block.label.value_or(block.cheat.name);
        header = strip_crypt_tag(header);

        if (block.kind == text::TextBlockKind::cmp_group_close) {
            if (!current_groups.empty()) current_groups.pop_back();
            continue;
        }

        if (block.cheat.empty()) {
            const std::string group = trim(header);
            if (group.empty()) {
                current_groups.clear();
            } else if (block.kind == text::TextBlockKind::cmp_group_open) {
                current_groups.push_back(group);
            } else {
                current_groups.clear();
                current_groups.push_back(group);
            }
            continue;
        }

        ++code_number;
        if (trim(header).empty()) header = "Code " + std::to_string(code_number);

        std::optional<std::string> author;
        if (const auto split = split_author(header)) {
            header = split->first;
            author = split->second;
        }

        const std::string final_name = join_path(current_groups, header);
        if (block.conversion_error) {
            output += "// [ERROR] " + final_name + "\n";
            std::size_t begin = 0U;
            while (begin <= block.conversion_error->size()) {
                const std::size_t end = block.conversion_error->find('\n', begin);
                output += "// " + block.conversion_error->substr(
                    begin, end == std::string::npos ? std::string::npos : end - begin) + "\n";
                if (end == std::string::npos) break;
                begin = end + 1U;
            }
            output += '\n';
            continue;
        }

        output += '[' + final_name + "]\n";
        if (author && !author->empty()) output += "author=" + *author + "\n";

        std::unordered_map<std::size_t, std::string> masks;
        for (const text::WildcardMask& wildcard : block.wildcards) {
            masks[wildcard.word_index] = wildcard.value_mask;
        }

        for (std::size_t index = 0; index < block.cheat.word_count(); index += 2U) {
            const std::uint32_t address = block.cheat.words[index];
            const std::uint32_t value = index + 1U < block.cheat.word_count()
                                            ? block.cheat.words[index + 1U]
                                            : 0U;
            std::string value_text = hex::format_u32(value);
            const auto mask = masks.find(index);
            if (mask != masks.end()) value_text = apply_mask(std::move(value_text), mask->second);
            output += "patch=1,EE," + hex::format_u32(address) + ",extended," + value_text + "\n";
        }
        output += '\n';
    }

    return output;
}

} // namespace omni::formats::pnach
