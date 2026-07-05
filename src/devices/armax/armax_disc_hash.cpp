#include "devices/armax/armax_disc_hash.hpp"

#include "devices/armax/armax_options.hpp"
#include "util/hex.hpp"
#include "util/newlines.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace omni::devices::armax {
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

std::string clean_boot_value(std::string value) {
    value = trim(value);
    if (value.size() >= 2U &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1U, value.size() - 2U);
    }

    const std::string lower = lower_ascii(value);
    const std::size_t cdrom = lower.find("cdrom0:");
    if (cdrom != std::string::npos) value.erase(0U, cdrom + 7U);

    while (!value.empty() && (value.front() == '\\' || value.front() == '/' ||
                              std::isspace(static_cast<unsigned char>(value.front())))) {
        value.erase(value.begin());
    }

    const std::size_t version = value.find(';');
    if (version != std::string::npos) value.resize(version);
    value = trim(value);
    while (!value.empty() && (value.back() == '\\' || value.back() == '/')) {
        value.pop_back();
    }
    return value;
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

std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path,
                                          std::size_t maximum) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open file: " + path.string());

    std::vector<std::uint8_t> bytes;
    if (maximum == static_cast<std::size_t>(-1)) {
        bytes.assign(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
    } else {
        bytes.resize(maximum);
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(maximum));
        bytes.resize(static_cast<std::size_t>(input.gcount()));
    }
    if (input.bad()) throw std::runtime_error("Unable to read file: " + path.string());
    return bytes;
}

std::optional<std::filesystem::path> find_case_insensitive_child(
    const std::filesystem::path& directory, std::string_view name) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) break;
        if (iequals(entry.path().filename().string(), name)) return entry.path();
    }
    return std::nullopt;
}

bool entry_less(const DiscHashEntry& left, const DiscHashEntry& right) {
    const std::string left_name = lower_ascii(left.game_name);
    const std::string right_name = lower_ascii(right.game_name);
    if (left_name != right_name) return left_name < right_name;
    if (left.hash != right.hash) return left.hash < right.hash;
    return lower_ascii(left.elf_name) < lower_ascii(right.elf_name);
}

} // namespace

std::string DiscHashEntry::hash_hex() const {
    return hex::format_u32(hash);
}

std::string DiscHashEntry::display_text() const {
    std::string text = hash_hex() + " - " + game_name;
    if (!elf_name.empty()) text += " (" + elf_name + ')';
    return text;
}

std::optional<std::string> parse_system_cnf_boot_path(
    std::string_view system_cnf_text) {
    std::istringstream lines(newlines::to_lf(system_cnf_text));
    std::string line;
    while (std::getline(lines, line)) {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = lower_ascii(trim(std::string_view(line).substr(0U, equals)));
        if (key != "boot2" && key != "boot") continue;
        std::string value = clean_boot_value(line.substr(equals + 1U));
        if (!value.empty()) return value;
    }

    const std::string lower = lower_ascii(system_cnf_text);
    const std::size_t cdrom = lower.find("cdrom0:");
    if (cdrom == std::string::npos) return std::nullopt;
    std::size_t end = system_cnf_text.find_first_of(";\r\n", cdrom + 7U);
    if (end == std::string_view::npos) end = system_cnf_text.size();
    std::string fallback = clean_boot_value(
        std::string(system_cnf_text.substr(cdrom, end - cdrom)));
    if (fallback.empty()) return std::nullopt;
    return fallback;
}

std::filesystem::path resolve_system_cnf_elf(
    const std::filesystem::path& system_cnf_path,
    std::string_view boot_path) {
    std::string normalized = clean_boot_value(std::string(boot_path));
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    if (normalized.empty()) return {};

    const std::filesystem::path root = system_cnf_path.parent_path().empty()
        ? std::filesystem::current_path()
        : system_cnf_path.parent_path();
    std::filesystem::path exact = root / std::filesystem::path(normalized);
    std::error_code error;
    if (std::filesystem::is_regular_file(exact, error)) return exact;

    std::filesystem::path current = root;
    std::istringstream components(normalized);
    std::string component;
    bool resolved_all = true;
    while (std::getline(components, component, '/')) {
        if (component.empty() || component == ".") continue;
        const auto child = find_case_insensitive_child(current, component);
        if (!child) {
            resolved_all = false;
            break;
        }
        current = *child;
    }
    if (resolved_all && std::filesystem::is_regular_file(current, error)) return current;

    const std::filesystem::path filename = std::filesystem::path(normalized).filename();
    if (!filename.empty()) {
        if (const auto root_match = find_case_insensitive_child(root, filename.string());
            root_match && std::filesystem::is_regular_file(*root_match, error)) {
            return *root_match;
        }
    }
    return {};
}

DiscHashResult compute_disc_hash_from_files(
    const std::filesystem::path& system_cnf_path,
    const std::optional<std::filesystem::path>& elf_override) {
    const std::vector<std::uint8_t> system_bytes =
        read_file_bytes(system_cnf_path, static_cast<std::size_t>(-1));
    if (system_bytes.empty()) throw std::runtime_error("SYSTEM.CNF is empty");

    const std::string system_text(system_bytes.begin(), system_bytes.end());
    const auto boot = parse_system_cnf_boot_path(system_text);
    if (!boot) throw std::runtime_error("Could not find BOOT2/BOOT ELF in SYSTEM.CNF");

    std::filesystem::path elf_path;
    if (elf_override && !elf_override->empty()) {
        elf_path = *elf_override;
    } else {
        elf_path = resolve_system_cnf_elf(system_cnf_path, *boot);
        if (elf_path.empty()) {
            throw std::runtime_error(
                "The ELF referenced by SYSTEM.CNF was not found beside SYSTEM.CNF");
        }
    }

    const std::vector<std::uint8_t> elf_bytes =
        read_file_bytes(elf_path, disc_hash_elf_prefix_size);
    if (elf_bytes.empty()) throw std::runtime_error("Selected PS2 ELF is empty");

    return DiscHashResult{compute_disc_hash(system_bytes, elf_bytes),
                          system_cnf_path, elf_path, *boot};
}

std::vector<DiscHashEntry> load_disc_hash_entries(
    const std::filesystem::path& mapping_path) {
    std::vector<DiscHashEntry> entries;
    std::ifstream input(mapping_path);
    if (!input) {
        if (std::filesystem::exists(mapping_path)) {
            throw std::runtime_error("Unable to open ArmaxDiscHash.json");
        }
        return entries;
    }

    const std::string file_text((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
    if (input.bad()) throw std::runtime_error("Unable to read ArmaxDiscHash.json");

    std::istringstream lines(newlines::to_lf(file_text));
    std::string raw;
    while (std::getline(lines, raw)) {
        const std::string line = trim(raw);
        if (line.empty() || line.rfind('#', 0U) == 0U || line.rfind("//", 0U) == 0U) {
            continue;
        }

        const std::vector<std::string> parts = split_gt(line);
        DiscHashEntry entry;
        std::optional<std::uint32_t> parsed_hash;
        if (parts.size() == 3U) {
            entry.elf_name = parts[0];
            parsed_hash = hex::parse_u32(parts[1]);
            entry.game_name = parts[2];
        } else if (parts.size() == 2U) {
            parsed_hash = hex::parse_u32(parts[0]);
            entry.game_name = parts[1];
        } else {
            continue;
        }
        if (!parsed_hash || entry.game_name.empty()) continue;
        entry.hash = *parsed_hash;
        entries.push_back(std::move(entry));
    }
    return entries;
}

void save_disc_hash_entries(const std::filesystem::path& mapping_path,
                            const std::vector<DiscHashEntry>& entries) {
    std::vector<DiscHashEntry> ordered = entries;
    std::stable_sort(ordered.begin(), ordered.end(), entry_less);

    if (const std::filesystem::path parent = mapping_path.parent_path(); !parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) throw std::runtime_error("Unable to create the ArmaxDiscHash.json folder");
    }

    std::ofstream output(mapping_path, std::ios::trunc);
    if (!output) throw std::runtime_error("Unable to create ArmaxDiscHash.json");
    for (const DiscHashEntry& entry : ordered) {
        if (!entry.elf_name.empty()) output << entry.elf_name << '>';
        output << entry.hash_hex() << '>' << entry.game_name << '\n';
    }
    if (!output) throw std::runtime_error("Unable to save ArmaxDiscHash.json");
}

std::vector<DiscHashEntry> get_disc_hash_entries(
    const std::filesystem::path& mapping_path) {
    std::vector<DiscHashEntry> entries = load_disc_hash_entries(mapping_path);
    std::stable_sort(entries.begin(), entries.end(), entry_less);
    return entries;
}

std::optional<std::string> try_get_disc_hash_game_name(
    const std::filesystem::path& mapping_path, std::uint32_t hash) {
    const std::vector<DiscHashEntry> entries = load_disc_hash_entries(mapping_path);
    const auto match = std::find_if(entries.begin(), entries.end(),
                                    [hash](const DiscHashEntry& entry) {
        return entry.hash == hash;
    });
    if (match == entries.end()) return std::nullopt;
    return match->game_name;
}

void add_or_update_disc_hash_entry(const std::filesystem::path& mapping_path,
                                   std::uint32_t hash,
                                   std::string_view game_name,
                                   std::string_view elf_name) {
    const std::string clean_name = trim(game_name);
    if (clean_name.empty()) throw std::invalid_argument("Game name cannot be empty");
    const std::string clean_elf = trim(elf_name);

    std::vector<DiscHashEntry> entries = load_disc_hash_entries(mapping_path);
    const auto existing = std::find_if(entries.begin(), entries.end(),
                                       [hash, &clean_elf](const DiscHashEntry& entry) {
        return entry.hash == hash && iequals(entry.elf_name, clean_elf);
    });
    if (existing != entries.end()) {
        existing->game_name = clean_name;
    } else {
        entries.push_back(DiscHashEntry{hash, clean_name, clean_elf});
    }
    save_disc_hash_entries(mapping_path, entries);
}

} // namespace omni::devices::armax
