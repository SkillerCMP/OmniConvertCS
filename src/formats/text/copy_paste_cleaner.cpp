#include "formats/text/copy_paste_cleaner.hpp"

#include "util/hex.hpp"
#include "util/newlines.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace omni::formats::text {
namespace {

const std::regex code_pair_regex(
    R"(([0-9A-Fa-f]{8})[ \t]+([0-9A-Fa-f]{8}))",
    std::regex::ECMAScript | std::regex::optimize);
const std::regex single_code_pair_regex(
    R"(^[ \t]*[0-9A-Fa-f]{8}[ \t]+[0-9A-Fa-f]{8}[ \t]*$)",
    std::regex::ECMAScript | std::regex::optimize);

std::string trim_right(std::string value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }
    return value;
}

std::string trim_commas_and_space(std::string value) {
    value = hex::trim(value);
    while (!value.empty() &&
           (value.back() == ',' ||
            std::isspace(static_cast<unsigned char>(value.back())) != 0)) {
        value.pop_back();
    }
    return value;
}

std::string lower_ascii(std::string_view text) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
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

std::size_t find_last_case_insensitive(std::string_view text,
                                       std::string_view needle) {
    if (needle.empty() || text.size() < needle.size()) return std::string::npos;
    const std::string lower_text = lower_ascii(text);
    const std::string lower_needle = lower_ascii(needle);
    return lower_text.rfind(lower_needle);
}

bool is_single_code_pair_line(std::string_view line) {
    return std::regex_match(std::string(line), single_code_pair_regex);
}

bool parser_reserved_after_plus(std::string_view text) noexcept {
    if (text.empty()) return false;
    const char first = text.front();
    if (first == '$' || first == '#' || first == ';' || first == '+' ||
        first == '!' || first == '^') {
        return true;
    }
    return text.rfind("//", 0U) == 0U ||
           starts_with_case_insensitive(text, "%credits:");
}

std::optional<std::string> gamehacking_crypt_descriptor(std::string_view line) {
    const std::string trimmed = hex::trim(line);
    const std::string lower = lower_ascii(trimmed);

    if (lower == "codebreaker v7+" || lower == "code breaker v7+") {
        return std::string("Codebreaker v7+");
    }
    if (lower == "codebreaker v1-6" || lower == "code breaker v1-6") {
        return std::string("Codebreaker v1-6");
    }
    if (lower == "gameshark v3/4" || lower == "game shark v3/4") {
        return std::string("GameShark v3/4");
    }
    if (lower == "gameshark v5" || lower == "game shark v5") {
        return std::string("GameShark v5");
    }
    if (lower == "xploder v4" || lower == "xploder v4+") {
        return std::string("Xploder v4+");
    }
    if (lower == "xploder v5") return std::string("Xploder v5");
    if (lower == "action replay max" || lower == "ar max") {
        return std::string("Action Replay MAX");
    }
    return std::nullopt;
}

void apply_pending_credits(std::vector<std::string>& output,
                           std::optional<std::string>& pending_credits) {
    if (!pending_credits || pending_credits->empty() || output.empty()) return;

    std::string& label = output.back();
    if (label.empty() || is_single_code_pair_line(label)) return;

    const std::size_t by = find_last_case_insensitive(label, " by ");
    if (by != std::string::npos) label.erase(by);
    label = trim_right(std::move(label));
    label += " by ";
    label += *pending_credits;
    pending_credits.reset();
}

void apply_credits_to_last_label(std::vector<std::string>& output,
                                 std::optional<std::string>& pending_credits,
                                 std::string credits) {
    credits = hex::trim(credits);
    if (credits.empty()) return;

    for (auto it = output.rbegin(); it != output.rend(); ++it) {
        if (it->empty() || is_single_code_pair_line(*it)) continue;
        const std::size_t by = find_last_case_insensitive(*it, " by ");
        if (by != std::string::npos) it->erase(by);
        *it = trim_right(std::move(*it));
        *it += " by ";
        *it += credits;
        return;
    }
    pending_credits = std::move(credits);
}

void apply_gamehacking_crypt(std::vector<std::string>& output,
                             std::string_view descriptor) {
    for (auto it = output.rbegin(); it != output.rend(); ++it) {
        if (it->empty() || is_single_code_pair_line(*it)) continue;

        std::string label = trim_commas_and_space(*it);
        const std::size_t existing_crypt =
            find_last_case_insensitive(label, " , crypt ");
        if (existing_crypt != std::string::npos) label.erase(existing_crypt);
        label = trim_commas_and_space(std::move(label));

        const std::size_t by = find_last_case_insensitive(label, " by ");
        if (by != std::string::npos) {
            const std::string base = trim_commas_and_space(label.substr(0U, by));
            const std::string author =
                trim_commas_and_space(label.substr(by + 4U));
            label = base;
            if (!author.empty()) {
                label += " , by ";
                label += author;
            }
        }

        label += " , Crypt ";
        label += descriptor;
        *it = std::move(label);
        return;
    }
}

} // namespace

std::string clean_copy_paste_text(std::string_view input) {
    if (input.empty()) return {};

    const std::string normalized = newlines::to_lf(input);
    std::istringstream lines(normalized);
    std::vector<std::string> output;
    std::optional<std::string> pending_credits;
    std::string raw_line;

    while (std::getline(lines, raw_line)) {
        std::string line = trim_right(std::move(raw_line));
        if (hex::trim(line).empty()) {
            output.emplace_back();
            continue;
        }

        std::string trimmed = hex::trim(line);

        if (trimmed.front() == '^') continue;
        if (trimmed == "$") continue;

        if (starts_with_case_insensitive(trimmed, "%credits:")) {
            apply_credits_to_last_label(
                output, pending_credits,
                std::string(trimmed.substr(std::string_view("%credits:").size())));
            continue;
        }

        if (trimmed.front() == '+') {
            std::string rest = hex::trim(std::string_view(trimmed).substr(1U));
            // Keep the quote marker when removing it would expose a parser
            // directive/comment prefix. Ordinary CMP '+' label markers are
            // stripped for the same display behavior as OmniConvertCS.
            line = parser_reserved_after_plus(rest) ? "+" + rest : std::move(rest);
            trimmed = hex::trim(line);
        } else if (trimmed.front() == '!' && trimmed != "!!") {
            line = hex::trim(std::string_view(trimmed).substr(1U));
            trimmed = hex::trim(line);
        }

        if (!trimmed.empty() && trimmed.front() == '$') {
            const std::string rest =
                hex::trim(std::string_view(trimmed).substr(1U));
            if (is_single_code_pair_line(rest)) {
                line = rest;
                trimmed = rest;
            } else {
                continue;
            }
        }

        if (const auto descriptor = gamehacking_crypt_descriptor(trimmed)) {
            apply_gamehacking_crypt(output, *descriptor);
            continue;
        }

        const std::string searchable = line;
        std::sregex_iterator match(searchable.begin(), searchable.end(), code_pair_regex);
        const std::sregex_iterator end;
        if (match == end) {
            output.push_back(trim_right(std::move(line)));
            apply_pending_credits(output, pending_credits);
            continue;
        }

        const std::smatch first = *match;
        std::string before = trim_right(searchable.substr(0U, static_cast<std::size_t>(first.position())));
        if (hex::trim(before) == "$") before.clear();
        if (!before.empty()) {
            output.push_back(std::move(before));
            apply_pending_credits(output, pending_credits);
        }

        std::size_t cursor = 0U;
        for (; match != end; ++match) {
            output.push_back(hex::trim(match->str()));
            cursor = static_cast<std::size_t>(match->position() + match->length());
        }

        if (cursor < searchable.size()) {
            std::string tail = hex::trim(std::string_view(searchable).substr(cursor));
            if (!tail.empty()) {
                output.push_back(std::move(tail));
                apply_pending_credits(output, pending_credits);
            }
        }
    }

    while (!output.empty() && output.back().empty()) output.pop_back();

    std::string cleaned;
    for (std::size_t index = 0U; index < output.size(); ++index) {
        if (index != 0U) cleaned += "\r\n";
        cleaned += output[index];
    }
    return cleaned;
}

} // namespace omni::formats::text
