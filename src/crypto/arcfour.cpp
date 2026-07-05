#include "crypto/arcfour.hpp"

#include <stdexcept>
#include <utility>

namespace omni::crypto {

ArcFour::ArcFour(const std::uint8_t* key, std::size_t key_size) {
    reset(key, key_size);
}

void ArcFour::reset(const std::uint8_t* key, std::size_t key_size) {
    if (key == nullptr || key_size == 0U) {
        throw std::invalid_argument("ARCFOUR requires a non-empty key");
    }

    for (std::size_t index = 0U; index < permutation_.size(); ++index) {
        permutation_[index] = static_cast<std::uint8_t>(index);
    }

    index1_ = 0U;
    index2_ = 0U;

    std::uint8_t shuffle_index = 0U;
    for (std::size_t index = 0U; index < permutation_.size(); ++index) {
        shuffle_index = static_cast<std::uint8_t>(
            shuffle_index + permutation_[index] + key[index % key_size]);
        std::swap(permutation_[index], permutation_[shuffle_index]);
    }
}

void ArcFour::crypt(std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr) return;

    for (std::size_t index = 0U; index < size; ++index) {
        index1_ = static_cast<std::uint8_t>(index1_ + 1U);
        index2_ = static_cast<std::uint8_t>(index2_ + permutation_[index1_]);
        std::swap(permutation_[index1_], permutation_[index2_]);

        const std::uint8_t stream_index = static_cast<std::uint8_t>(
            permutation_[index1_] + permutation_[index2_]);
        data[index] ^= permutation_[stream_index];
    }
}

} // namespace omni::crypto
