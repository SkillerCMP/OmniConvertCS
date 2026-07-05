#include "devices/gameshark/gs3_file.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace omni::devices::gameshark {
namespace {

constexpr std::uint32_t sign_mask = 0x80000000U;
constexpr std::uint32_t lower_mask = 0x7FFFFFFFU;
constexpr std::uint32_t mask_b = 0x9D2C5680U;
constexpr std::uint32_t mask_c = 0xEFC60000U;
constexpr std::size_t recurrence_offset = 397U;
constexpr std::size_t break_offset = 227U;
constexpr std::uint32_t multiplier = 69069U;
constexpr std::array<std::uint32_t, 2> decision_matrix{0U, 0x9908B0DFU};
constexpr std::uint32_t crc_poly = 0x04C11DB7U;

class FileMt {
public:
    void initialize(std::uint32_t seed) {
        std::uint32_t working = seed;
        for (std::uint32_t& value : state_) {
            const std::uint32_t high = working & 0xFFFF0000U;
            working = working * multiplier + 1U;
            const std::uint32_t low = working & 0xFFFF0000U;
            value = (low >> 16U) | high;
            working = working * multiplier + 1U;
        }
        pass_ = state_.size();
    }

    std::uint32_t next() {
        if (pass_ >= state_.size()) {
            if (pass_ == state_.size() + 1U) initialize(0x1105U);
            std::ptrdiff_t offset = static_cast<std::ptrdiff_t>(recurrence_offset);
            for (std::size_t index = 0U; index < state_.size() - 1U; ++index) {
                if (index == break_offset) offset = -static_cast<std::ptrdiff_t>(break_offset);
                const std::uint32_t mixed =
                    (state_[index] & sign_mask) | (state_[index + 1U] & lower_mask);
                const std::size_t source = static_cast<std::size_t>(
                    static_cast<std::ptrdiff_t>(index) + offset);
                state_[index] = (mixed >> 1U) ^ state_[source] ^
                                decision_matrix[mixed & 1U];
            }
            const std::uint32_t mixed =
                (state_[0] & lower_mask) | (state_.back() & sign_mask);
            state_.back() = (mixed >> 1U) ^ state_[recurrence_offset - 1U] ^
                            decision_matrix[mixed & 1U];
            pass_ = 0U;
        }
        std::uint32_t value = state_[pass_++];
        value ^= value >> 11U;
        value ^= (value << 7U) & mask_b;
        value ^= (value << 15U) & mask_c;
        value ^= value >> 18U;
        return value;
    }

private:
    std::array<std::uint32_t, 624> state_{};
    std::size_t pass_{625U};
};

const std::array<std::uint32_t, 256>& crc_table() {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> values{};
        for (std::uint32_t index = 0U; index < values.size(); ++index) {
            std::uint32_t crc = index << 24U;
            for (unsigned bit = 0U; bit < 8U; ++bit) {
                crc = (crc & sign_mask) == 0U ? crc << 1U : (crc << 1U) ^ crc_poly;
            }
            values[index] = crc;
        }
        return values;
    }();
    return table;
}

} // namespace

void crypt_file_data(std::vector<std::uint8_t>& data) {
    FileMt state;
    state.initialize(static_cast<std::uint32_t>(data.size()));
    for (std::uint8_t& value : data) {
        value ^= static_cast<std::uint8_t>(state.next());
    }
}

std::uint32_t file_crc32(const std::vector<std::uint8_t>& data) {
    const auto& table = crc_table();
    std::uint32_t crc = 0U;
    for (const std::uint8_t value : data) {
        crc = (crc << 8U) ^ table[((crc >> 24U) ^ value) & 0xFFU];
    }
    return crc;
}

} // namespace omni::devices::gameshark
