#include "devices/codebreaker/cb1_crypto.hpp"

#include <array>
#include <cstddef>

namespace omni::devices::codebreaker {
namespace {

constexpr std::array<std::array<std::uint32_t, 16>, 3> seed_table{{
    {{0x0A0B8D9BU, 0x0A0133F8U, 0x0AF733ECU, 0x0A15C574U,
      0x0A50AC20U, 0x0A920FB9U, 0x0A599F0BU, 0x0A4AA0E3U,
      0x0A21C012U, 0x0A906254U, 0x0A31FD54U, 0x0A091C0EU,
      0x0A372B38U, 0x0A6F266CU, 0x0A61DD4AU, 0x0A0DBF92U}},
    {{0x00288596U, 0x0037DD28U, 0x003BEEF1U, 0x000BC822U,
      0x00BC935DU, 0x00A139F2U, 0x00E9BBF8U, 0x00F57F7BU,
      0x0090D704U, 0x001814D4U, 0x00C5848EU, 0x005B83E7U,
      0x00108CF7U, 0x0046CE5AU, 0x003A5BF4U, 0x006FAFFCU}},
    {{0x1DD9A10AU, 0xB95AB9B0U, 0x5CF5D328U, 0x95FE7F10U,
      0x8E2D6303U, 0x16BB6286U, 0xE389324CU, 0x07AC6EA8U,
      0xAA4811D8U, 0x76CE4E18U, 0xFE447516U, 0xF9CD94D0U,
      0x4C24DEDBU, 0x68275C4EU, 0x72494382U, 0xC8AA88E8U}},
}};

constexpr std::size_t command_index(std::uint32_t address) noexcept {
    return static_cast<std::size_t>(address >> 28U);
}

} // namespace

void encrypt_v1(std::uint32_t& address, std::uint32_t& value) noexcept {
    const std::size_t command = command_index(address);
    const std::uint32_t high_byte = address & 0xFF000000U;

    std::uint32_t permuted = ((address & 0x000000FFU) << 16U) |
                             ((address >> 8U) & 0x0000FFFFU);
    address = (high_byte | ((permuted + seed_table[1][command]) & 0x00FFFFFFU)) ^
              seed_table[0][command];

    if (command > 2U) {
        value = address ^ (value + seed_table[2][command]);
    }
}

void decrypt_v1(std::uint32_t& address, std::uint32_t& value) noexcept {
    const std::size_t command = command_index(address);

    if (command > 2U) {
        value = (address ^ value) - seed_table[2][command];
    }

    const std::uint32_t decoded = address ^ seed_table[0][command];
    const std::uint32_t unseeded = decoded - seed_table[1][command];
    address = (decoded & 0xFF000000U) |
              ((unseeded & 0x0000FFFFU) << 8U) |
              ((unseeded >> 16U) & 0x000000FFU);
}

CodePair encrypt_v1(CodePair code) noexcept {
    encrypt_v1(code.address, code.value);
    return code;
}

CodePair decrypt_v1(CodePair code) noexcept {
    decrypt_v1(code.address, code.value);
    return code;
}

} // namespace omni::devices::codebreaker
