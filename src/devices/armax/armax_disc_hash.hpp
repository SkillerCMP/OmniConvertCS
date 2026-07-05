#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace omni::devices::armax {

inline constexpr std::size_t disc_hash_elf_prefix_size = 256U * 1024U;

struct DiscHashResult {
    std::uint32_t hash{};
    std::filesystem::path system_cnf_path;
    std::filesystem::path elf_path;
    std::string boot_path;
};

struct DiscHashEntry {
    std::uint32_t hash{};
    std::string game_name;
    std::string elf_name;

    [[nodiscard]] std::string hash_hex() const;
    [[nodiscard]] std::string display_text() const;
};

[[nodiscard]] std::optional<std::string> parse_system_cnf_boot_path(
    std::string_view system_cnf_text);
[[nodiscard]] std::filesystem::path resolve_system_cnf_elf(
    const std::filesystem::path& system_cnf_path,
    std::string_view boot_path);
[[nodiscard]] DiscHashResult compute_disc_hash_from_files(
    const std::filesystem::path& system_cnf_path,
    const std::optional<std::filesystem::path>& elf_override = std::nullopt);

[[nodiscard]] std::vector<DiscHashEntry> load_disc_hash_entries(
    const std::filesystem::path& mapping_path);
void save_disc_hash_entries(const std::filesystem::path& mapping_path,
                            const std::vector<DiscHashEntry>& entries);
[[nodiscard]] std::vector<DiscHashEntry> get_disc_hash_entries(
    const std::filesystem::path& mapping_path);
[[nodiscard]] std::optional<std::string> try_get_disc_hash_game_name(
    const std::filesystem::path& mapping_path, std::uint32_t hash);
void add_or_update_disc_hash_entry(const std::filesystem::path& mapping_path,
                                   std::uint32_t hash,
                                   std::string_view game_name,
                                   std::string_view elf_name);

} // namespace omni::devices::armax
