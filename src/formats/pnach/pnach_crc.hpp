#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace omni::formats::pnach {

struct CrcEntry {
    std::uint32_t crc{};
    std::string game_name;
    std::string elf_name;

    [[nodiscard]] std::string crc_hex() const;
    [[nodiscard]] std::string display_text() const;
};

[[nodiscard]] std::string normalize_elf_name(std::string_view elf_name);
[[nodiscard]] std::uint32_t compute_elf_crc(const std::filesystem::path& elf_path);
[[nodiscard]] std::vector<CrcEntry> load_crc_entries(const std::filesystem::path& mapping_path);
void save_crc_entries(const std::filesystem::path& mapping_path,
                      const std::vector<CrcEntry>& entries);
[[nodiscard]] std::vector<CrcEntry> get_crc_entries(const std::filesystem::path& mapping_path);
[[nodiscard]] std::optional<std::string> try_get_game_name(
    const std::filesystem::path& mapping_path, std::uint32_t crc);
[[nodiscard]] std::optional<std::string> try_get_elf_name(
    const std::filesystem::path& mapping_path, std::uint32_t crc);
void add_or_update_crc_entry(const std::filesystem::path& mapping_path,
                             std::uint32_t crc, std::string_view game_name,
                             std::string_view elf_name);

} // namespace omni::formats::pnach
