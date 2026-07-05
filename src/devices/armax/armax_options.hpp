#pragma once

#include "core/cheat.hpp"

#include <cstdint>
#include <vector>

namespace omni::devices::armax {

inline constexpr std::uint32_t expansion_data_folder = 0x0800U;
inline constexpr std::uint32_t flags_folder = 0x5U << 20U;
inline constexpr std::uint32_t flags_folder_member = 0x4U << 20U;
inline constexpr std::uint32_t flags_disc_hash = 0x7U << 20U;

[[nodiscard]] std::uint32_t compute_disc_hash(
    const std::vector<std::uint8_t>& system_cnf,
    const std::vector<std::uint8_t>& elf_prefix);

void enable_expansion(std::vector<std::uint32_t>& words) noexcept;
void make_folder(std::vector<std::uint32_t>& words, std::uint32_t game_id,
                 std::uint8_t region);
void make_folder(std::vector<std::uint32_t>& words, std::uint32_t game_id,
                 std::uint8_t region, std::uint32_t nonce);
void finalize_cheat(Cheat& cheat, std::uint32_t disc_hash,
                    bool make_folders, std::uint32_t& current_folder_id);

} // namespace omni::devices::armax
