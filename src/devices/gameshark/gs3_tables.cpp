#include "devices/gameshark/gs3_crypto.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace omni::devices::gameshark {
namespace {

constexpr std::uint32_t sign_mask = 0x80000000U;
constexpr std::uint32_t lower_mask = 0x7FFFFFFFU;
constexpr std::uint32_t mt_mask_b = 0x9D2C5680U;
constexpr std::uint32_t mt_mask_c = 0xEFC60000U;
constexpr std::size_t mt_recurrence_offset = 397U;
constexpr std::size_t mt_break_offset = 227U;
constexpr std::uint32_t mt_multiplier = 69069U;
constexpr std::array<std::uint32_t, 2> decision_matrix{0U, 0x9908B0DFU};

constexpr std::size_t crypt_x1 = 0U;
constexpr std::size_t crypt_2 = 1U;
constexpr std::size_t crypt_1 = 2U;
constexpr std::size_t crypt_3 = 3U;

} // namespace

void Context::MtState::initialize(std::uint32_t seed) {
    std::uint32_t working_seed = seed;
    constexpr std::uint32_t mask = 0xFFFF0000U;

    for (std::uint32_t& entry : state_) {
        const std::uint32_t most = working_seed & mask;
        working_seed = working_seed * mt_multiplier + 1U;
        const std::uint32_t least = working_seed & mask;
        entry = (least >> 16U) | most;
        working_seed = working_seed * mt_multiplier + 1U;
    }
    pass_count_ = state_.size();
}

std::uint32_t Context::MtState::next() {
    if (pass_count_ >= state_.size()) {
        if (pass_count_ == state_.size() + 1U) {
            initialize(0x1105U);
        }

        std::ptrdiff_t offset = static_cast<std::ptrdiff_t>(mt_recurrence_offset);
        for (std::size_t i = 0; i < state_.size() - 1U; ++i) {
            if (i == mt_break_offset) {
                offset = -static_cast<std::ptrdiff_t>(mt_break_offset);
            }

            const std::uint32_t mixed =
                (state_[i] & sign_mask) | (state_[i + 1U] & lower_mask);
            const auto source = static_cast<std::size_t>(
                static_cast<std::ptrdiff_t>(i) + offset);
            state_[i] = (mixed >> 1U) ^ state_[source] ^
                        decision_matrix[mixed & 1U];
        }

        const std::uint32_t mixed =
            (state_[0] & lower_mask) | (state_.back() & sign_mask);
        state_.back() = (mixed >> 1U) ^ state_[mt_recurrence_offset - 1U] ^
                        decision_matrix[mixed & 1U];
        pass_count_ = 0U;
    }

    std::uint32_t value = state_[pass_count_++];
    value ^= value >> 11U;
    value ^= (value << 7U) & mt_mask_b;
    value ^= (value << 15U) & mt_mask_c;
    value ^= value >> 18U;
    return value;
}

Context::Context() {
    encrypt_tables_[crypt_x1].size = 0x40U;
    encrypt_tables_[crypt_2].size = 0x39U;
    encrypt_tables_[crypt_1].size = 0x19U;
    encrypt_tables_[crypt_3].size = 0x100U;

    for (std::size_t i = 0; i < decrypt_tables_.size(); ++i) {
        decrypt_tables_[i].size = encrypt_tables_[i].size;
    }
    reset();
}

void Context::reset() {
    line_two_ = false;
    second_line_key_ = 0U;
    static_cast<void>(build_seeds(
        static_cast<std::uint16_t>(halfword_seeds[0]),
        static_cast<std::uint16_t>(halfword_seeds[1]),
        static_cast<std::uint16_t>(halfword_seeds[2])));
}

void Context::build_seed_table(CryptTable& table, std::uint16_t seed) {
    for (std::size_t i = 0; i < table.size; ++i) {
        table.values[i] = static_cast<std::uint8_t>(i);
    }

    MtState state;
    state.initialize(seed);

    std::size_t passes = table.size * 2U;
    while (passes-- > 0U) {
        const std::size_t first = state.next() % table.size;
        const std::size_t second = state.next() % table.size;
        std::swap(table.values[first], table.values[second]);
    }
}

void Context::reverse_seed_table(CryptTable& destination,
                                 const CryptTable& source) {
    for (std::size_t i = 0; i < source.size; ++i) {
        destination.values[source.values[i]] = static_cast<std::uint8_t>(i);
    }
}

std::uint8_t Context::build_seeds(std::uint16_t seed1,
                                  std::uint16_t seed2,
                                  std::uint16_t seed3) {
    for (std::size_t i = 0; i < encrypt_tables_.size(); ++i) {
        int shift = 5 - static_cast<int>(i);
        if (shift < 3) shift = 0;
        build_seed_table(encrypt_tables_[i],
                         static_cast<std::uint16_t>(seed1 >> shift));
        reverse_seed_table(decrypt_tables_[i], encrypt_tables_[i]);
    }

    const std::uint32_t result =
        (((((static_cast<std::uint32_t>(seed1) >> 8U) | 1U) + seed2 - seed3) -
          (((static_cast<std::uint32_t>(seed2) >> 8U) & 0xFFU) >> 2U)) +
         (seed1 & 0xFCU) + (static_cast<std::uint32_t>(seed3) >> 8U) +
         0xFF9AU);
    return static_cast<std::uint8_t>(result);
}

void Context::update_seeds(const CodePair& code) {
    const auto seed1 = static_cast<std::uint16_t>(code.address & 0xFFFFU);
    const auto seed2 = static_cast<std::uint16_t>(code.address >> 16U);
    const auto seed3 = static_cast<std::uint16_t>(code.value & 0xFFFFU);
    static_cast<void>(build_seeds(seed1, seed2, seed3));
}

} // namespace omni::devices::gameshark
