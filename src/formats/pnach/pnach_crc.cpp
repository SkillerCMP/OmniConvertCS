#include "formats/pnach/pnach_crc.hpp"

#include "util/hex.hpp"
#include "util/newlines.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace omni::formats::pnach {
namespace {

std::string trim(std::string_view value) {
    return hex::trim(value);
}

std::string lower_ascii(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool iequals(std::string_view left, std::string_view right) {
    return lower_ascii(left) == lower_ascii(right);
}

std::vector<std::string> split_gt(std::string_view line) {
    std::vector<std::string> parts;
    std::size_t start = 0U;
    for (;;) {
        const std::size_t next = line.find('>', start);
        if (next == std::string_view::npos) {
            parts.emplace_back(trim(line.substr(start)));
            break;
        }
        parts.emplace_back(trim(line.substr(start, next - start)));
        start = next + 1U;
    }
    return parts;
}

bool entry_less(const CrcEntry& left, const CrcEntry& right) {
    const std::string left_name = lower_ascii(left.game_name);
    const std::string right_name = lower_ascii(right.game_name);
    if (left_name != right_name) return left_name < right_name;
    if (left.crc != right.crc) return left.crc < right.crc;
    return lower_ascii(left.elf_name) < lower_ascii(right.elf_name);
}

} // namespace

std::string CrcEntry::crc_hex() const {
    return hex::format_u32(crc);
}

std::string CrcEntry::display_text() const {
    std::string text = crc_hex() + " - " + game_name;
    if (!elf_name.empty()) text += " (" + elf_name + ')';
    return text;
}

std::string normalize_elf_name(std::string_view elf_name) {
    std::string clean = trim(elf_name);
    if (clean.empty()) return {};

    try {
        clean = std::filesystem::path(clean).filename().string();
    } catch (...) {
        // Match the C# helper: keep the original text if path parsing fails.
    }

    if (clean.size() == 11U && clean[4] == '_' && clean[8] == '.') {
        bool prefix_ok = true;
        for (std::size_t index = 0U; index < 4U; ++index) {
            const unsigned char c = static_cast<unsigned char>(clean[index]);
            if (!std::isalpha(c)) {
                prefix_ok = false;
                break;
            }
        }
        bool digits_ok = true;
        for (std::size_t index : {5U, 6U, 7U, 9U, 10U}) {
            const unsigned char c = static_cast<unsigned char>(clean[index]);
            if (!std::isdigit(c)) {
                digits_ok = false;
                break;
            }
        }
        if (prefix_ok && digits_ok) {
            std::string normalized;
            normalized.reserve(10U);
            for (std::size_t index = 0U; index < 4U; ++index) {
                normalized.push_back(static_cast<char>(
                    std::toupper(static_cast<unsigned char>(clean[index]))));
            }
            normalized.push_back('-');
            normalized.append(clean, 5U, 3U);
            normalized.append(clean, 9U, 2U);
            return normalized;
        }
    }

    return clean;
}

std::uint32_t compute_elf_crc(const std::filesystem::path& elf_path) {
    std::ifstream input(elf_path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open the selected PS2 ELF");

    std::uint32_t crc = 0U;
    std::array<unsigned char, 4> bytes{};
    for (;;) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        const std::streamsize count = input.gcount();
        if (count < static_cast<std::streamsize>(bytes.size())) break;
        const std::uint32_t word = static_cast<std::uint32_t>(bytes[0]) |
                                   (static_cast<std::uint32_t>(bytes[1]) << 8U) |
                                   (static_cast<std::uint32_t>(bytes[2]) << 16U) |
                                   (static_cast<std::uint32_t>(bytes[3]) << 24U);
        crc ^= word;
    }
    if (input.bad()) throw std::runtime_error("Unable to read the selected PS2 ELF");
    return crc;
}

std::vector<CrcEntry> load_crc_entries(const std::filesystem::path& mapping_path) {
    std::vector<CrcEntry> entries;
    std::ifstream input(mapping_path);
    if (!input) {
        if (std::filesystem::exists(mapping_path)) {
            throw std::runtime_error("Unable to open PnachCRC.json");
        }
        return entries;
    }

    const std::string file_text((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
    if (input.bad()) throw std::runtime_error("Unable to read PnachCRC.json");

    std::istringstream lines(newlines::to_lf(file_text));
    std::string raw;
    while (std::getline(lines, raw)) {
        const std::string line = trim(raw);
        if (line.empty() || line.rfind('#', 0U) == 0U || line.rfind("//", 0U) == 0U) {
            continue;
        }

        const std::vector<std::string> parts = split_gt(line);
        CrcEntry entry;
        std::optional<std::uint32_t> parsed_crc;
        if (parts.size() == 3U) {
            entry.elf_name = parts[0];
            parsed_crc = hex::parse_u32(parts[1]);
            entry.game_name = parts[2];
        } else if (parts.size() == 2U) {
            parsed_crc = hex::parse_u32(parts[0]);
            entry.game_name = parts[1];
        } else {
            continue;
        }

        if (!parsed_crc) continue;
        entry.crc = *parsed_crc;
        entries.push_back(std::move(entry));
    }
    return entries;
}

void save_crc_entries(const std::filesystem::path& mapping_path,
                      const std::vector<CrcEntry>& entries) {
    std::vector<CrcEntry> ordered = entries;
    std::stable_sort(ordered.begin(), ordered.end(), entry_less);

    if (const std::filesystem::path parent = mapping_path.parent_path(); !parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) throw std::runtime_error("Unable to create the PnachCRC.json folder");
    }

    std::ofstream output(mapping_path, std::ios::trunc);
    if (!output) throw std::runtime_error("Unable to create PnachCRC.json");
    for (const CrcEntry& entry : ordered) {
        if (!entry.elf_name.empty()) output << entry.elf_name << '>';
        output << entry.crc_hex() << '>' << entry.game_name << '\n';
    }
    if (!output) throw std::runtime_error("Unable to save PnachCRC.json");
}

std::vector<CrcEntry> get_crc_entries(const std::filesystem::path& mapping_path) {
    std::vector<CrcEntry> entries = load_crc_entries(mapping_path);
    std::stable_sort(entries.begin(), entries.end(), entry_less);
    return entries;
}

std::optional<std::string> try_get_game_name(const std::filesystem::path& mapping_path,
                                             std::uint32_t crc) {
    const std::vector<CrcEntry> entries = load_crc_entries(mapping_path);
    const auto match = std::find_if(entries.begin(), entries.end(), [crc](const CrcEntry& entry) {
        return entry.crc == crc;
    });
    if (match == entries.end()) return std::nullopt;
    return match->game_name;
}

std::optional<std::string> try_get_elf_name(const std::filesystem::path& mapping_path,
                                           std::uint32_t crc) {
    const std::vector<CrcEntry> entries = load_crc_entries(mapping_path);
    const auto with_elf = std::find_if(entries.begin(), entries.end(), [crc](const CrcEntry& entry) {
        return entry.crc == crc && !entry.elf_name.empty();
    });
    if (with_elf != entries.end()) return with_elf->elf_name;

    const auto any = std::find_if(entries.begin(), entries.end(), [crc](const CrcEntry& entry) {
        return entry.crc == crc;
    });
    if (any == entries.end()) return std::nullopt;
    return any->elf_name;
}

void add_or_update_crc_entry(const std::filesystem::path& mapping_path,
                             std::uint32_t crc, std::string_view game_name,
                             std::string_view elf_name) {
    const std::string clean_name = trim(game_name);
    if (clean_name.empty()) throw std::invalid_argument("Game name cannot be empty");
    const std::string clean_elf = normalize_elf_name(elf_name);

    std::vector<CrcEntry> entries = load_crc_entries(mapping_path);
    const auto existing = std::find_if(entries.begin(), entries.end(),
                                       [crc, &clean_elf](const CrcEntry& entry) {
        return entry.crc == crc && iequals(entry.elf_name, clean_elf);
    });
    if (existing != entries.end()) {
        existing->game_name = clean_name;
    } else {
        entries.push_back(CrcEntry{crc, clean_name, clean_elf});
    }
    save_crc_entries(mapping_path, entries);
}

} // namespace omni::formats::pnach
