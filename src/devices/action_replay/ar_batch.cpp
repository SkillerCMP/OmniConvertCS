#include "devices/action_replay/ar_batch.hpp"

#include <stdexcept>

namespace omni::devices::action_replay {
namespace {

void require_pairs(const std::vector<std::uint32_t>& words) {
    if ((words.size() & 1U) != 0U) {
        throw std::invalid_argument("Action Replay code data must contain complete 64-bit pairs");
    }
}

} // namespace

Context::Context(std::uint32_t key) noexcept : seed_(seed_bytes(key)) {}

void Context::set_seed(std::uint32_t key) noexcept {
    seed_ = seed_bytes(key);
}

std::uint32_t Context::get_seed() const noexcept {
    return seed_key(seed_);
}

void Context::decrypt_ar2(std::vector<std::uint32_t>& words, bool remove_key_codes) {
    require_pairs(words);

    std::size_t index = 0U;
    while (index + 1U < words.size()) {
        words[index] = decrypt_word(words[index], seed_[0], seed_[1]);
        words[index + 1U] = decrypt_word(words[index + 1U], seed_[2], seed_[3]);

        if (words[index] == ar2_key_address) {
            set_seed(words[index + 1U]);
            if (remove_key_codes) {
                words.erase(words.begin() + static_cast<std::ptrdiff_t>(index),
                            words.begin() + static_cast<std::ptrdiff_t>(index + 2U));
                continue;
            }
        }

        index += 2U;
    }
}

void Context::encrypt_ar2(std::vector<std::uint32_t>& words) {
    require_pairs(words);

    for (std::size_t index = 0U; index + 1U < words.size(); index += 2U) {
        const bool key_code = words[index] == ar2_key_address;
        const std::uint32_t next_seed = key_code ? words[index + 1U] : 0U;

        words[index] = encrypt_word(words[index], seed_[0], seed_[1]);
        words[index + 1U] = encrypt_word(words[index + 1U], seed_[2], seed_[3]);

        if (key_code) set_seed(next_seed);
    }
}

void decrypt_ar1(std::vector<std::uint32_t>& words) {
    require_pairs(words);
    const auto seed = seed_bytes(ar1_seed);
    for (std::size_t index = 0U; index + 1U < words.size(); index += 2U) {
        words[index] = decrypt_word(words[index], seed[0], seed[1]);
        words[index + 1U] = decrypt_word(words[index + 1U], seed[2], seed[3]);
    }
}

void encrypt_ar1(std::vector<std::uint32_t>& words) {
    require_pairs(words);
    const auto seed = seed_bytes(ar1_seed);
    for (std::size_t index = 0U; index + 1U < words.size(); index += 2U) {
        words[index] = encrypt_word(words[index], seed[0], seed[1]);
        words[index + 1U] = encrypt_word(words[index + 1U], seed[2], seed[3]);
    }
}

void prepend_ar2_key_code(std::vector<std::uint32_t>& words, std::uint32_t key) {
    require_pairs(words);
    words.insert(words.begin(), {ar2_key_address, key});
}

} // namespace omni::devices::action_replay
