#include "devices/codebreaker/cb_file_crypto.hpp"

#include "crypto/arcfour.hpp"

#include <array>
#include <cstdint>

namespace omni::devices::codebreaker {
namespace {

constexpr std::array<std::uint8_t, 1024> file_key{
#include "devices/codebreaker/cb_file_key.inc"
};

} // namespace

void crypt_file_data(std::vector<std::uint8_t>& data) {
    crypto::ArcFour context(file_key.data(), file_key.size());
    if (!data.empty()) context.crypt(data.data(), data.size());
}

} // namespace omni::devices::codebreaker
