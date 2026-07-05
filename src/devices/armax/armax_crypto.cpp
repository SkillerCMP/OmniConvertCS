#include "devices/armax/armax_crypto.hpp"

#include "devices/action_replay/ar_batch.hpp"
#include "devices/armax/armax_tables.hpp"

#include <array>
#include <chrono>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>

namespace omni::devices::armax {
namespace {

using SeedArray = std::array<std::uint32_t, 32>;

std::uint32_t rotate_left(std::uint32_t value, std::uint32_t count) noexcept {
    return (value << count) | (value >> (32U - count));
}

std::uint32_t rotate_right(std::uint32_t value, std::uint32_t count) noexcept {
    return (value >> count) | (value << (32U - count));
}

std::uint32_t byte_swap(std::uint32_t value) noexcept {
    return (value << 24U) |
           ((value << 8U) & 0x00FF0000U) |
           ((value >> 8U) & 0x0000FF00U) |
           (value >> 24U);
}

SeedArray generate_seeds(bool reverse) noexcept {
    std::array<std::uint8_t, 56> bits{};
    std::array<std::uint8_t, 56> rotated{};
    std::array<std::uint8_t, 8> selected{};
    SeedArray seeds{};

    for (std::size_t index = 0U; index < bits.size(); ++index) {
        const std::uint8_t source = static_cast<std::uint8_t>(tables::generation_permutation[index] - 1U);
        const std::uint8_t masked = static_cast<std::uint8_t>(
            tables::generation_seed[source >> 3U] & tables::generation_masks[source & 7U]);
        bits[index] = masked == 0U ? 0U : 1U;
    }

    for (std::size_t round = 0U; round < 16U; ++round) {
        selected.fill(0U);
        const std::uint8_t amount = tables::generation_rotations[round];

        for (std::size_t index = 0U; index < rotated.size(); ++index) {
            std::size_t source = index + amount;
            if (index > 27U) {
                if (source > 55U) source -= 28U;
            } else if (source > 27U) {
                source -= 28U;
            }
            rotated[index] = bits[source];
        }

        for (std::size_t index = 0U; index < 48U; ++index) {
            if (rotated[static_cast<std::size_t>(tables::generation_selection[index] - 1U)] == 0U) continue;
            const std::size_t group = index / 6U;
            const std::size_t position = index - (group * 6U);
            selected[group] = static_cast<std::uint8_t>(
                selected[group] | static_cast<std::uint8_t>(tables::generation_masks[position] >> 2U));
        }

        seeds[round * 2U] = (static_cast<std::uint32_t>(selected[0]) << 24U) |
                            (static_cast<std::uint32_t>(selected[2]) << 16U) |
                            (static_cast<std::uint32_t>(selected[4]) << 8U) |
                            static_cast<std::uint32_t>(selected[6]);
        seeds[(round * 2U) + 1U] = (static_cast<std::uint32_t>(selected[1]) << 24U) |
                                   (static_cast<std::uint32_t>(selected[3]) << 16U) |
                                   (static_cast<std::uint32_t>(selected[5]) << 8U) |
                                   static_cast<std::uint32_t>(selected[7]);
    }

    if (!reverse) {
        std::size_t right = 31U;
        for (std::size_t left = 0U; left < 16U; left += 2U) {
            const std::uint32_t first = seeds[left];
            seeds[left] = seeds[right - 1U];
            seeds[right - 1U] = first;

            const std::uint32_t second = seeds[left + 1U];
            seeds[left + 1U] = seeds[right];
            seeds[right] = second;
            right -= 2U;
        }
    }

    return seeds;
}

const SeedArray& seeds() noexcept {
    static const SeedArray value = generate_seeds(false);
    return value;
}

std::uint32_t substitution_value(std::uint32_t primary, std::uint32_t secondary) noexcept {
    return tables::substitution[6][primary & 0x3FU] ^
           tables::substitution[4][(primary >> 8U) & 0x3FU] ^
           tables::substitution[2][(primary >> 16U) & 0x3FU] ^
           tables::substitution[0][(primary >> 24U) & 0x3FU] ^
           tables::substitution[7][secondary & 0x3FU] ^
           tables::substitution[5][(secondary >> 8U) & 0x3FU] ^
           tables::substitution[3][(secondary >> 16U) & 0x3FU] ^
           tables::substitution[1][(secondary >> 24U) & 0x3FU];
}

void scramble_one(std::uint32_t& address, std::uint32_t& value) noexcept {
    address = rotate_left(address, 4U);

    std::uint32_t temp = (address ^ value) & 0xF0F0F0F0U;
    value ^= temp;
    address = rotate_right(address ^ temp, 20U);

    temp = (address ^ value) & 0xFFFF0000U;
    value ^= temp;
    address = rotate_right(address ^ temp, 18U);

    temp = (address ^ value) & 0x33333333U;
    value ^= temp;
    address = rotate_right(address ^ temp, 6U);

    temp = (address ^ value) & 0x00FF00FFU;
    value ^= temp;
    address = rotate_left(address ^ temp, 9U);

    temp = (address ^ value) & 0xAAAAAAAAU;
    value = rotate_left(value ^ temp, 1U);
    address ^= temp;
}

void scramble_two(std::uint32_t& address, std::uint32_t& value) noexcept {
    address = rotate_right(address, 1U);

    std::uint32_t temp = (address ^ value) & 0xAAAAAAAAU;
    address ^= temp;
    value = rotate_right(value ^ temp, 9U);

    temp = (address ^ value) & 0x00FF00FFU;
    address ^= temp;
    value = rotate_left(value ^ temp, 6U);

    temp = (address ^ value) & 0x33333333U;
    address ^= temp;
    value = rotate_left(value ^ temp, 18U);

    temp = (address ^ value) & 0xFFFF0000U;
    address ^= temp;
    value = rotate_left(value ^ temp, 20U);

    temp = (address ^ value) & 0xF0F0F0F0U;
    address ^= temp;
    value = rotate_right(value ^ temp, 4U);
}

void unscramble_one(std::uint32_t& address, std::uint32_t& value) noexcept {
    value = rotate_left(value, 4U);

    std::uint32_t temp = (address ^ value) & 0xF0F0F0F0U;
    address ^= temp;
    value = rotate_right(value ^ temp, 20U);

    temp = (address ^ value) & 0xFFFF0000U;
    address ^= temp;
    value = rotate_right(value ^ temp, 18U);

    temp = (address ^ value) & 0x33333333U;
    address ^= temp;
    value = rotate_right(value ^ temp, 6U);

    temp = (address ^ value) & 0x00FF00FFU;
    address ^= temp;
    value = rotate_left(value ^ temp, 9U);

    temp = (address ^ value) & 0xAAAAAAAAU;
    address = rotate_left(address ^ temp, 1U);
    value ^= temp;
}

void unscramble_two(std::uint32_t& address, std::uint32_t& value) noexcept {
    value = rotate_right(value, 1U);

    std::uint32_t temp = (address ^ value) & 0xAAAAAAAAU;
    value ^= temp;
    address = rotate_right(address ^ temp, 9U);

    temp = (address ^ value) & 0x00FF00FFU;
    value ^= temp;
    address = rotate_left(address ^ temp, 6U);

    temp = (address ^ value) & 0x33333333U;
    value ^= temp;
    address = rotate_left(address ^ temp, 18U);

    temp = (address ^ value) & 0xFFFF0000U;
    value ^= temp;
    address = rotate_left(address ^ temp, 20U);

    temp = (address ^ value) & 0xF0F0F0F0U;
    value ^= temp;
    address = rotate_right(address ^ temp, 4U);
}

void decrypt_pair(std::uint32_t& first, std::uint32_t& second) noexcept {
    std::uint32_t address = byte_swap(first);
    std::uint32_t value = byte_swap(second);
    unscramble_one(address, value);

    std::size_t index = 0U;
    while (index < seeds().size()) {
        std::uint32_t primary = rotate_right(value, 4U) ^ seeds()[index++];
        std::uint32_t secondary = value ^ seeds()[index++];
        address ^= substitution_value(primary, secondary);

        primary = rotate_right(address, 4U) ^ seeds()[index++];
        secondary = address ^ seeds()[index++];
        value ^= substitution_value(primary, secondary);
    }

    unscramble_two(address, value);
    first = byte_swap(value);
    second = byte_swap(address);
}

void encrypt_pair(std::uint32_t& first, std::uint32_t& second) noexcept {
    std::uint32_t value = byte_swap(first);
    std::uint32_t address = byte_swap(second);
    scramble_one(address, value);

    std::size_t index = seeds().size();
    while (index > 0U) {
        std::uint32_t secondary = address ^ seeds()[--index];
        std::uint32_t primary = rotate_right(address, 4U) ^ seeds()[--index];
        value ^= substitution_value(primary, secondary);

        secondary = value ^ seeds()[--index];
        primary = rotate_right(value, 4U) ^ seeds()[--index];
        address ^= substitution_value(primary, secondary);
    }

    scramble_two(address, value);
    first = byte_swap(address);
    second = byte_swap(value);
}

struct BitCursor {
    std::size_t word{};
    std::uint32_t bit{};
};

bool read_bits(const std::vector<std::uint32_t>& words,
               BitCursor& cursor,
               std::uint32_t count,
               std::uint32_t& output) noexcept {
    output = 0U;
    while (count > 0U) {
        if (cursor.bit > 31U) {
            cursor.bit = 0U;
            ++cursor.word;
        }
        if (cursor.word >= words.size()) return false;
        output = (output << 1U) |
                 ((words[cursor.word] >> (31U - cursor.bit)) & 1U);
        ++cursor.bit;
        --count;
    }
    return true;
}

Metadata read_metadata(const std::vector<std::uint32_t>& words) {
    BitCursor cursor{0U, 4U};
    Metadata result{};
    std::uint32_t ignored = 0U;
    std::uint32_t region = 0U;
    if (!read_bits(words, cursor, 13U, result.game_id) ||
        !read_bits(words, cursor, 19U, ignored) ||
        !read_bits(words, cursor, 1U, ignored) ||
        !read_bits(words, cursor, 1U, ignored) ||
        !read_bits(words, cursor, 2U, region)) {
        throw std::invalid_argument("ARMAX verifier metadata is truncated");
    }
    result.region = static_cast<std::uint8_t>(region);
    return result;
}

void require_pairs(const std::vector<std::uint32_t>& words) {
    if (words.empty() || (words.size() & 1U) != 0U) {
        throw std::invalid_argument("ARMAX data must contain complete 64-bit code lines");
    }
    if (words.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
        throw std::invalid_argument("ARMAX code block is too large");
    }
}

} // namespace

std::uint16_t crc16(const std::vector<std::uint32_t>& words) noexcept {
    std::uint16_t result = 0U;
    for (const std::uint32_t word : words) {
        for (std::uint32_t byte = 0U; byte < 4U; ++byte) {
            const std::uint8_t value = static_cast<std::uint8_t>((word >> (byte * 8U)) ^ result);
            result = static_cast<std::uint16_t>(
                (tables::crc_high[(value >> 4U) & 0x0FU] ^
                 tables::crc_low[value & 0x0FU]) ^
                (result >> 8U));
        }
    }
    return result;
}

std::uint8_t checksum_nibble(const std::vector<std::uint32_t>& words) noexcept {
    const std::uint16_t value = crc16(words);
    return static_cast<std::uint8_t>(
        ((value >> 12U) ^ (value >> 8U) ^ (value >> 4U) ^ value) & 0x0FU);
}



std::uint32_t random_verifier_nonce() {
    // OmniConvertCS calls Random.Next() separately for the 15-bit verifier
    // suffix and the 4-bit high nibble every time a header is generated.
    // Keep one process-wide generator, just like its static Random instance,
    // and avoid an immediate duplicate so repeated Convert clicks visibly
    // produce a fresh header.
    static std::mutex mutex;
    static std::mt19937 generator([] {
        std::random_device device;
        const auto ticks = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::seed_seq seed{device(), device(), device(), device(),
                           static_cast<std::uint32_t>(ticks),
                           static_cast<std::uint32_t>(ticks >> 32U)};
        return std::mt19937(seed);
    }());
    static std::uint32_t previous = 0xFFFFFFFFU;
    static std::uniform_int_distribution<std::uint32_t> low_distribution(0U, 0x7FFFU);
    static std::uniform_int_distribution<std::uint32_t> high_distribution(0U, 0xFU);

    std::lock_guard<std::mutex> lock(mutex);
    std::uint32_t nonce = 0U;
    do {
        const std::uint32_t low = low_distribution(generator);
        const std::uint32_t high = high_distribution(generator);
        nonce = low | (high << 15U);
    } while (nonce == previous);
    previous = nonce;
    return nonce;
}

std::array<std::uint32_t, 2> make_verifier(
    const std::vector<std::uint32_t>& payload, std::uint32_t game_id,
    std::uint8_t region) {
    return make_verifier(payload, game_id, region, random_verifier_nonce());
}

std::array<std::uint32_t, 2> make_verifier(
    const std::vector<std::uint32_t>& payload, std::uint32_t game_id,
    std::uint8_t region, std::uint32_t nonce) noexcept {
    bool has_master_hook = false;
    for (std::size_t index = 0U; index + 1U < payload.size(); index += 2U) {
        if ((payload[index] >> 25U) == 0x62U) {
            has_master_hook = true;
            break;
        }
    }

    const std::uint32_t first = ((game_id & 0x1FFFU) << 15U) |
                                (nonce & 0x7FFFU);
    const std::uint32_t second = (((nonce >> 15U) & 0xFU) << 28U) |
                                 0x00800000U |
                                 (static_cast<std::uint32_t>(has_master_hook) << 27U) |
                                 ((static_cast<std::uint32_t>(region) & 3U) << 24U);
    return {first, second};
}

void prepend_verifier(std::vector<std::uint32_t>& payload, std::uint32_t game_id,
                      std::uint8_t region) {
    prepend_verifier(payload, game_id, region, random_verifier_nonce());
}

void prepend_verifier(std::vector<std::uint32_t>& payload, std::uint32_t game_id,
                      std::uint8_t region, std::uint32_t nonce) {
    const auto verifier = make_verifier(payload, game_id, region, nonce);
    payload.insert(payload.begin(), verifier.begin(), verifier.end());
}

std::size_t read_verifier_lines(const std::vector<std::uint32_t>& words) {
    require_pairs(words);
    static constexpr std::array<std::uint32_t, 8> expansion_sizes{{6U, 10U, 12U, 19U, 19U, 8U, 7U, 32U}};

    BitCursor cursor{1U, 8U};
    std::uint32_t terminator = 0U;
    if (!read_bits(words, cursor, 1U, terminator)) {
        throw std::invalid_argument("ARMAX verifier is truncated");
    }

    std::int64_t used_bits = 1;
    while (terminator == 0U) {
        std::uint32_t size_index = 0U;
        if (!read_bits(words, cursor, 3U, size_index)) {
            throw std::invalid_argument("ARMAX verifier expansion header is truncated");
        }
        used_bits += 3;

        const std::uint32_t expansion_size = expansion_sizes[size_index & 7U];
        std::uint32_t ignored = 0U;
        if (!read_bits(words, cursor, expansion_size, ignored) ||
            !read_bits(words, cursor, 1U, terminator)) {
            throw std::invalid_argument("ARMAX verifier expansion data is truncated");
        }
        used_bits += static_cast<std::int64_t>(expansion_size) + 1;
    }

    used_bits -= 24;
    std::size_t lines = 1U;
    while (used_bits > 0) {
        ++lines;
        used_bits -= 64;
    }
    if (lines * 2U > words.size()) {
        throw std::invalid_argument("ARMAX verifier occupies more lines than the code block contains");
    }
    return lines;
}

Metadata decrypt_full(std::vector<std::uint32_t>& words, std::uint32_t payload_key) {
    require_pairs(words);

    for (std::size_t index = 0U; index < words.size(); index += 2U) {
        decrypt_pair(words[index], words[index + 1U]);
    }

    Metadata metadata = read_metadata(words);
    const std::uint8_t stored_checksum = static_cast<std::uint8_t>(words[0] >> 28U);
    words[0] &= 0x0FFFFFFFU;
    if (stored_checksum != checksum_nibble(words)) {
        throw std::invalid_argument("ARMAX checksum verification failed");
    }

    metadata.verifier_lines = read_verifier_lines(words);
    const std::size_t payload_start = metadata.verifier_lines * 2U;
    if (payload_start < words.size()) {
        std::vector<std::uint32_t> payload(words.begin() + static_cast<std::ptrdiff_t>(payload_start),
                                           words.end());
        for (std::uint32_t& word : payload) word = byte_swap(word);

        devices::action_replay::Context context(payload_key);
        context.decrypt_ar2(payload, true);

        words.erase(words.begin() + static_cast<std::ptrdiff_t>(payload_start), words.end());
        words.insert(words.end(), payload.begin(), payload.end());
    }
    return metadata;
}

void encrypt_full(std::vector<std::uint32_t>& words, std::uint32_t payload_key) {
    require_pairs(words);
    if ((words[0] >> 28U) != 0U) {
        throw std::invalid_argument("ARMAX RAW verifier checksum nibble must be clear before encryption");
    }

    const std::size_t verifier_lines = read_verifier_lines(words);
    const std::size_t payload_start = verifier_lines * 2U;
    if (payload_start < words.size()) {
        std::vector<std::uint32_t> payload(words.begin() + static_cast<std::ptrdiff_t>(payload_start),
                                           words.end());
        devices::action_replay::Context context(payload_key);
        context.encrypt_ar2(payload);
        for (std::uint32_t& word : payload) word = byte_swap(word);

        words.erase(words.begin() + static_cast<std::ptrdiff_t>(payload_start), words.end());
        words.insert(words.end(), payload.begin(), payload.end());
    }

    words[0] |= static_cast<std::uint32_t>(checksum_nibble(words)) << 28U;
    for (std::size_t index = 0U; index < words.size(); index += 2U) {
        encrypt_pair(words[index], words[index + 1U]);
    }
}

} // namespace omni::devices::armax
