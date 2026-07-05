#include "devices/armax/armax_options.hpp"
#include "devices/armax/armax_crypto.hpp"

#include "formats/binary/binary_common.hpp"

#include <cstddef>

namespace omni::devices::armax {

std::uint32_t compute_disc_hash(
    const std::vector<std::uint8_t>& system_cnf,
    const std::vector<std::uint8_t>& elf_prefix) {
    const std::uint32_t system_crc =
        formats::binary::common::crc32_standard(system_cnf, 0U,
                                                system_cnf.size(), 0xFFFFFFFFU);
    const std::uint32_t elf_crc =
        formats::binary::common::crc32_standard(elf_prefix, 0U,
                                                elf_prefix.size(), 0xFFFFFFFFU);
    return elf_crc ^ system_crc;
}

void enable_expansion(std::vector<std::uint32_t>& words) noexcept {
    if (words.size() < 2U) return;
    if ((words[1] & 0x00800000U) != 0U) words[1] ^= 0x00800000U;
}

void make_folder(std::vector<std::uint32_t>& words, std::uint32_t game_id,
                 std::uint8_t region) {
    make_folder(words, game_id, region, random_verifier_nonce());
}

void make_folder(std::vector<std::uint32_t>& words, std::uint32_t game_id,
                 std::uint8_t region, std::uint32_t nonce) {
    const std::uint32_t low_nonce = nonce & 0x7FFFU;
    const std::uint32_t high_nonce = (nonce >> 15U) & 0xFU;
    const std::uint32_t second = (high_nonce << 28U) |
                                 ((static_cast<std::uint32_t>(region) & 3U) << 24U) |
                                 flags_folder | expansion_data_folder;
    const std::uint32_t first = ((game_id & 0x1FFFU) << 15U) | low_nonce;
    words.insert(words.begin(), {first, second});
}

void finalize_cheat(Cheat& cheat, std::uint32_t disc_hash,
                    bool make_folders, std::uint32_t& current_folder_id) {
    if (cheat.state != 0U || cheat.words.size() < 2U) return;

    cheat.id = ((cheat.words[0] & 0x7FFFU) << 4U) |
               ((cheat.words[1] >> 28U) & 0xFU);

    if ((cheat.words[1] & flags_folder) != 0U && make_folders) {
        current_folder_id = cheat.id;
    }

    if (current_folder_id > 0U && cheat.id != current_folder_id) {
        enable_expansion(cheat.words);
        cheat.words[1] |= flags_folder_member |
                          (current_folder_id << 1U) | 1U;
    }

    bool master = cheat.flags[Cheat::flag_master_code] != 0U;
    for (std::size_t index = 2U; index + 1U < cheat.words.size(); index += 2U) {
        if ((cheat.words[index] >> 25U) == 0x62U) {
            master = true;
        }

        if (cheat.words[index] == 0U &&
            cheat.words[index + 1U] >= 0x80000000U &&
            cheat.words[index + 1U] <= 0x85FFFFFFU) {
            index += 2U;
        }
    }
    cheat.flags[Cheat::flag_master_code] = master ? 1U : 0U;

    if (disc_hash > 0U && master) {
        enable_expansion(cheat.words);
        cheat.words[1] |= flags_disc_hash | (disc_hash >> 12U);
        const std::uint32_t expansion = 0x00080000U | (disc_hash << 20U);
        cheat.words.insert(cheat.words.begin() + 2, {expansion, 0U});
    }

    if (cheat.name.empty()) cheat.name = "Unnamed Cheat";
}

} // namespace omni::devices::armax
