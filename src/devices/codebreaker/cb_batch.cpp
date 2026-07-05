#include "devices/codebreaker/cb_batch.hpp"

#include "devices/codebreaker/cb1_crypto.hpp"
#include "devices/codebreaker/cb7_crypto.hpp"

#include <stdexcept>

namespace omni::devices::codebreaker {
namespace {

void require_pairs(const std::vector<std::uint32_t>& words) {
    if ((words.size() % 2U) != 0U) {
        throw std::invalid_argument("CodeBreaker encryption requires complete address/value pairs");
    }
}

} // namespace

void encrypt_v1(std::vector<std::uint32_t>& words) {
    require_pairs(words);
    for (std::size_t index = 0U; index < words.size(); index += 2U) {
        encrypt_v1(words[index], words[index + 1U]);
    }
}

void decrypt_v1(std::vector<std::uint32_t>& words) {
    require_pairs(words);
    for (std::size_t index = 0U; index < words.size(); index += 2U) {
        decrypt_v1(words[index], words[index + 1U]);
    }
}

void encrypt_common_v7(std::vector<std::uint32_t>& words) {
    require_pairs(words);
    V7Context context;
    context.set_common_v7();
    for (std::size_t index = 0U; index < words.size(); index += 2U) {
        context.encrypt(words[index], words[index + 1U]);
    }
}

void decrypt_common_v7(std::vector<std::uint32_t>& words) {
    require_pairs(words);
    V7Context context;
    context.set_common_v7();
    for (std::size_t index = 0U; index < words.size(); index += 2U) {
        context.decrypt(words[index], words[index + 1U]);
    }
}

} // namespace omni::devices::codebreaker
