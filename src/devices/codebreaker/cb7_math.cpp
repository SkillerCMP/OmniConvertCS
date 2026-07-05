#include "devices/codebreaker/cb7_math.hpp"

namespace omni::devices::codebreaker::cb7_math {
namespace {

std::uint64_t add_mod(std::uint64_t left,
                      std::uint64_t right,
                      std::uint64_t modulus) noexcept {
    return left >= modulus - right ? left - (modulus - right) : left + right;
}

std::uint64_t multiply_mod(std::uint64_t left,
                           std::uint64_t right,
                           std::uint64_t modulus) noexcept {
    left %= modulus;
    right %= modulus;

    std::uint64_t result = 0U;
    while (right != 0U) {
        if ((right & 1U) != 0U) result = add_mod(result, left, modulus);
        right >>= 1U;
        if (right != 0U) left = add_mod(left, left, modulus);
    }
    return result;
}

std::uint64_t power_mod(std::uint64_t base,
                        std::uint64_t exponent,
                        std::uint64_t modulus) noexcept {
    std::uint64_t result = 1U;
    base %= modulus;

    while (exponent != 0U) {
        if ((exponent & 1U) != 0U) result = multiply_mod(result, base, modulus);
        exponent >>= 1U;
        if (exponent != 0U) base = multiply_mod(base, base, modulus);
    }
    return result;
}

} // namespace

std::uint32_t multiplicative_inverse(std::uint32_t word) noexcept {
    if (word == 1U) return 1U;

    std::uint32_t a2 = (0U - word) % word;
    if (a2 == 0U) return 1U;

    std::uint32_t t1 = 1U;
    std::uint32_t a3 = word;
    std::uint32_t a0 = 0U - (0xFFFFFFFFU / word);

    do {
        const std::uint32_t quotient = a3 / a2;
        const std::uint32_t remainder = a3 % a2;
        a3 = a2;
        const std::uint32_t previous = a0;
        a2 = remainder;
        a0 = t1 - quotient * previous;
        t1 = previous;
    } while (a2 != 0U);

    return t1;
}

std::uint32_t multiply_encrypt(std::uint32_t value,
                               std::uint32_t factor) noexcept {
    return value * (factor | 1U);
}

std::uint32_t multiply_decrypt(std::uint32_t value,
                               std::uint32_t factor) noexcept {
    return value * multiplicative_inverse(factor | 1U);
}

void rsa_transform(std::uint32_t& address,
                   std::uint32_t& value,
                   std::uint64_t exponent) noexcept {
    const std::uint64_t code = (static_cast<std::uint64_t>(address) << 32U) |
                               static_cast<std::uint64_t>(value);
    if (code >= rsa_modulus) return;

    const std::uint64_t transformed = power_mod(code, exponent, rsa_modulus);
    address = static_cast<std::uint32_t>(transformed >> 32U);
    value = static_cast<std::uint32_t>(transformed);
}

} // namespace omni::devices::codebreaker::cb7_math
