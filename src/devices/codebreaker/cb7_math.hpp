#pragma once

#include <cstdint>

namespace omni::devices::codebreaker::cb7_math {

constexpr std::uint64_t rsa_modulus = 0xFFFFFFFFFFFFFFF5ULL;
constexpr std::uint64_t rsa_decrypt_exponent = 11ULL;
constexpr std::uint64_t rsa_encrypt_exponent = 2682110966135737091ULL;

[[nodiscard]] std::uint32_t multiplicative_inverse(std::uint32_t word) noexcept;
[[nodiscard]] std::uint32_t multiply_encrypt(std::uint32_t value,
                                             std::uint32_t factor) noexcept;
[[nodiscard]] std::uint32_t multiply_decrypt(std::uint32_t value,
                                             std::uint32_t factor) noexcept;

void rsa_transform(std::uint32_t& address,
                   std::uint32_t& value,
                   std::uint64_t exponent) noexcept;

} // namespace omni::devices::codebreaker::cb7_math
