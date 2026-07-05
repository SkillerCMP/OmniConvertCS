#include "core/inline_crypt.hpp"

#include <algorithm>
#include <cctype>

namespace omni {
namespace {

constexpr std::string_view marker = "CRYPT_";

std::size_t find_case_insensitive(std::string_view text, std::string_view needle) {
    if (needle.empty() || text.size() < needle.size()) return std::string_view::npos;
    for (std::size_t i = 0; i + needle.size() <= text.size(); ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto a = static_cast<unsigned char>(text[i + j]);
            const auto b = static_cast<unsigned char>(needle[j]);
            if (std::toupper(a) != std::toupper(b)) {
                matches = false;
                break;
            }
        }
        if (matches) return i;
    }
    return std::string_view::npos;
}

struct TagRange {
    std::size_t begin{};
    std::size_t end{};
};

std::optional<TagRange> find_tag_range(std::string_view label) {
    const std::size_t crypt_pos = find_case_insensitive(label, marker);
    if (crypt_pos == std::string_view::npos) return std::nullopt;

    std::size_t begin = crypt_pos;
    while (begin > 0 && std::isspace(static_cast<unsigned char>(label[begin - 1])) != 0) --begin;
    if (begin > 0 && label[begin - 1] == ',') --begin;

    std::size_t end = crypt_pos + marker.size();
    while (end < label.size()) {
        const unsigned char c = static_cast<unsigned char>(label[end]);
        if (!(std::isalnum(c) != 0 || c == '_')) break;
        ++end;
    }
    return TagRange{begin, end};
}

std::string trim_right(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }
    return value;
}

} // namespace

std::optional<Crypt> crypt_from_label(std::string_view label) {
    const std::size_t pos = find_case_insensitive(label, marker);
    if (pos == std::string_view::npos) return std::nullopt;

    std::size_t end = pos + marker.size();
    while (end < label.size()) {
        const unsigned char c = static_cast<unsigned char>(label[end]);
        if (!(std::isalnum(c) != 0 || c == '_')) break;
        ++end;
    }
    return parse_crypt(label.substr(pos, end - pos));
}

std::string strip_crypt_tag(std::string_view label) {
    const auto range = find_tag_range(label);
    if (!range) return std::string(label);

    std::string result(label);
    result.erase(range->begin, range->end - range->begin);
    return trim_right(std::move(result));
}

std::string update_existing_crypt_tag(std::string_view label, Crypt output_crypt) {
    const auto range = find_tag_range(label);
    if (!range) return trim_right(std::string(label));

    std::string result(label);
    result.replace(range->begin, range->end - range->begin,
                   std::string(", ") + std::string(crypt_token(output_crypt)));
    return trim_right(std::move(result));
}

} // namespace omni
