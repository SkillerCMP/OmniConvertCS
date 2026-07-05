#include "devices/gameshark/gs3_crypto.hpp"

#include "devices/gameshark/gs3_verifier.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace omni::devices::gameshark {
namespace {

constexpr std::uint32_t decrypt_mask = 0xF1FFFFFFU;

constexpr std::size_t crypt_x1 = 0U;
constexpr std::size_t crypt_2 = 1U;
constexpr std::size_t crypt_1 = 2U;
constexpr std::size_t crypt_3 = 3U;
constexpr std::size_t crypt_4 = crypt_3;
constexpr std::size_t crypt_x2 = crypt_3;

[[nodiscard]] constexpr std::uint8_t get_byte(std::uint32_t word,
                                              unsigned number) noexcept {
    return static_cast<std::uint8_t>(word >> ((number - 1U) * 8U));
}

[[nodiscard]] constexpr std::uint32_t bytes_to_word(std::uint32_t a,
                                                    std::uint32_t b,
                                                    std::uint32_t c,
                                                    std::uint32_t d) noexcept {
    return (a << 24U) | (b << 16U) | (c << 8U) | d;
}

[[nodiscard]] constexpr std::uint32_t bytes_to_half(std::uint32_t a,
                                                    std::uint32_t b) noexcept {
    return (a << 8U) | b;
}

void validate_words(const std::vector<std::uint32_t>& words) {
    if ((words.size() & 1U) != 0U) {
        throw std::invalid_argument(
            "GameShark/Xploder encryption requires complete address/value pairs");
    }
}

} // namespace

CodePair Context::decrypt(CodePair code) {
    std::uint32_t temporary = 0U;
    std::uint32_t temporary2 = 0U;
    std::uint32_t mask = 0U;

    if (line_two_) {
        switch (second_line_key_) {
            case 1U:
            case 2U: {
                const CryptTable& table = decrypt_tables_[crypt_x1];
                for (std::size_t i = 0; i < table.size; ++i) {
                    const std::uint32_t flag = i < 32U
                        ? ((code.value ^ (halfword_seeds[1] << 13U)) >> i) & 1U
                        : ((code.address ^ (halfword_seeds[1] << 2U)) >>
                           (i - 32U)) & 1U;
                    if (flag != 0U) {
                        if (table.values[i] < 32U) {
                            temporary2 |= 1U << table.values[i];
                        } else {
                            temporary |= 1U << (table.values[i] - 32U);
                        }
                    }
                }
                code.address = temporary ^ (halfword_seeds[2] << 3U);
                code.value = temporary2 ^ halfword_seeds[2];
                line_two_ = false;
                return code;
            }
            case 3U:
            case 4U: {
                const CryptTable& table = decrypt_tables_[crypt_x2];
                temporary = code.address ^ (halfword_seeds[1] << 3U);
                code.address = bytes_to_word(
                    table.values[get_byte(temporary, 4U)],
                    table.values[get_byte(temporary, 3U)],
                    table.values[get_byte(temporary, 2U)],
                    table.values[get_byte(temporary, 1U)]) ^
                    (halfword_seeds[2] << 16U);

                temporary = code.value ^ (halfword_seeds[1] << 9U);
                code.value = bytes_to_word(
                    table.values[get_byte(temporary, 4U)],
                    table.values[get_byte(temporary, 3U)],
                    table.values[get_byte(temporary, 2U)],
                    table.values[get_byte(temporary, 1U)]) ^
                    (halfword_seeds[2] << 5U);
                line_two_ = false;
                return code;
            }
            default:
                line_two_ = false;
                return code;
        }
    }

    const std::uint32_t command = code.address & 0xFE000000U;
    const std::uint8_t key = static_cast<std::uint8_t>((code.address >> 25U) & 7U);
    if (command >= 0x30000000U && command <= 0x6FFFFFFFU) {
        line_two_ = true;
        second_line_key_ = key;
    }

    if ((command >> 28U) == 7U) return code;

    switch (key) {
        case 0U:
            break;
        case 1U: {
            const CryptTable& table = decrypt_tables_[crypt_1];
            temporary = (code.address & 0x01FFFFFFU) ^
                        (halfword_seeds[1] << 8U);
            for (std::size_t i = 0; i < table.size; ++i) {
                mask |= ((temporary & (1U << i)) >> i) << table.values[i];
            }
            code.address = ((mask ^ halfword_seeds[2]) | command) & decrypt_mask;
            break;
        }
        case 2U: {
            const CryptTable& table = decrypt_tables_[crypt_2];
            code.address = (code.address & 0x01FFFFFFU) ^
                           (halfword_seeds[1] << 1U);
            for (std::size_t i = 0; i < table.size; ++i) {
                const std::uint32_t flag = i < 32U
                    ? ((code.value ^ (halfword_seeds[1] << 16U)) >> i) & 1U
                    : (code.address >> (i - 32U)) & 1U;
                if (flag != 0U) {
                    if (table.values[i] < 32U) {
                        temporary2 |= 1U << table.values[i];
                    } else {
                        temporary |= 1U << (table.values[i] - 32U);
                    }
                }
            }
            code.address = ((temporary ^ (halfword_seeds[2] << 8U)) | command) &
                           decrypt_mask;
            code.value = temporary2 ^ halfword_seeds[2];
            break;
        }
        case 3U: {
            const CryptTable& table = decrypt_tables_[crypt_3];
            temporary = code.address ^ (halfword_seeds[1] << 8U);
            code.address = ((bytes_to_half(
                table.values[get_byte(temporary, 1U)],
                table.values[get_byte(temporary, 2U)]) |
                (temporary & 0xFFFF0000U)) ^
                (halfword_seeds[2] << 4U)) & decrypt_mask;
            break;
        }
        case 4U: {
            const CryptTable& table = decrypt_tables_[crypt_4];
            temporary = code.address ^ (halfword_seeds[1] << 8U);
            code.address = ((bytes_to_half(
                table.values[get_byte(temporary, 2U)],
                table.values[get_byte(temporary, 1U)]) |
                (temporary & 0xFFFF0000U)) ^
                (halfword_seeds[2] << 4U)) & decrypt_mask;

            temporary = code.value ^ (halfword_seeds[1] << 9U);
            code.value = bytes_to_word(
                table.values[get_byte(temporary, 4U)],
                table.values[get_byte(temporary, 3U)],
                table.values[get_byte(temporary, 2U)],
                table.values[get_byte(temporary, 1U)]) ^
                (halfword_seeds[2] << 5U);
            break;
        }
        case 5U:
        case 6U:
            break;
        case 7U:
            update_seeds(code);
            break;
        default:
            break;
    }

    return code;
}

CodePair Context::encrypt(CodePair code, std::uint8_t key) {
    const std::uint32_t command = code.address & 0xFE000000U;
    std::uint32_t temporary = 0U;
    std::uint32_t temporary2 = 0U;
    std::uint32_t mask = 0U;

    // Type-7 code rows are passed through by the GS3/GS5 cipher. They are not
    // V5 verifier rows unless they match the verifier layout at the beginning
    // of a block, and the selected output key does not apply to them.
    if ((command >> 28U) == 7U) {
        return code;
    }

    if (!(key > 0U && key < 8U)) {
        key = static_cast<std::uint8_t>((code.address >> 25U) & 7U);
    }

    if (line_two_) {
        switch (second_line_key_) {
            case 1U:
            case 2U: {
                const CryptTable& table = encrypt_tables_[crypt_x1];
                code.address ^= halfword_seeds[2] << 3U;
                code.value ^= halfword_seeds[2];
                for (std::size_t i = 0; i < table.size; ++i) {
                    const std::uint32_t flag = i < 32U
                        ? (code.value >> i) & 1U
                        : (code.address >> (i - 32U)) & 1U;
                    if (flag != 0U) {
                        if (table.values[i] < 32U) {
                            temporary2 |= 1U << table.values[i];
                        } else {
                            temporary |= 1U << (table.values[i] - 32U);
                        }
                    }
                }
                code.address = temporary ^ (halfword_seeds[1] << 2U);
                code.value = temporary2 ^ (halfword_seeds[1] << 13U);
                line_two_ = false;
                return code;
            }
            case 3U:
            case 4U: {
                const CryptTable& table = encrypt_tables_[crypt_x2];
                temporary = code.address ^ (halfword_seeds[2] << 16U);
                code.address = bytes_to_word(
                    table.values[get_byte(temporary, 4U)],
                    table.values[get_byte(temporary, 3U)],
                    table.values[get_byte(temporary, 2U)],
                    table.values[get_byte(temporary, 1U)]) ^
                    (halfword_seeds[1] << 3U);

                temporary = code.value ^ (halfword_seeds[2] << 5U);
                code.value = bytes_to_word(
                    table.values[get_byte(temporary, 4U)],
                    table.values[get_byte(temporary, 3U)],
                    table.values[get_byte(temporary, 2U)],
                    table.values[get_byte(temporary, 1U)]) ^
                    (halfword_seeds[1] << 9U);
                line_two_ = false;
                return code;
            }
            default:
                line_two_ = false;
                return code;
        }
    }

    if (command >= 0x30000000U && command <= 0x6FFFFFFFU) {
        line_two_ = true;
        second_line_key_ = key;
    }

    switch (key) {
        case 0U:
            break;
        case 1U: {
            const CryptTable& table = encrypt_tables_[crypt_1];
            temporary = (code.address & 0x01FFFFFFU) ^ halfword_seeds[2];
            for (std::size_t i = 0; i < table.size; ++i) {
                mask |= ((temporary & (1U << i)) >> i) << table.values[i];
            }
            code.address = (mask ^ (halfword_seeds[1] << 8U)) | command;
            break;
        }
        case 2U: {
            const CryptTable& table = encrypt_tables_[crypt_2];
            code.address = (code.address ^ (halfword_seeds[2] << 8U)) &
                           0x01FFFFFFU;
            code.value ^= halfword_seeds[2];
            for (std::size_t i = 0; i < table.size; ++i) {
                const std::uint32_t flag = i < 32U
                    ? (code.value >> i) & 1U
                    : (code.address >> (i - 32U)) & 1U;
                if (flag != 0U) {
                    if (table.values[i] < 32U) {
                        temporary2 |= 1U << table.values[i];
                    } else {
                        temporary |= 1U << (table.values[i] - 32U);
                    }
                }
            }
            code.address = (temporary ^ (halfword_seeds[1] << 1U)) | command;
            code.value = temporary2 ^ (halfword_seeds[1] << 16U);
            break;
        }
        case 3U: {
            const CryptTable& table = encrypt_tables_[crypt_3];
            temporary = (code.address & 0xF1FFFFFFU) ^
                        (halfword_seeds[2] << 4U);
            code.address = (bytes_to_half(
                table.values[get_byte(temporary, 1U)],
                table.values[get_byte(temporary, 2U)]) |
                (temporary & 0xFFFF0000U)) ^
                (halfword_seeds[1] << 8U);
            break;
        }
        case 4U: {
            const CryptTable& table = encrypt_tables_[crypt_4];
            temporary = (code.address & 0xF1FFFFFFU) ^
                        (halfword_seeds[2] << 4U);
            code.address = (bytes_to_half(
                table.values[get_byte(temporary, 2U)],
                table.values[get_byte(temporary, 1U)]) |
                (temporary & 0xFFFF0000U)) ^
                (halfword_seeds[1] << 8U);

            temporary = code.value ^ (halfword_seeds[2] << 5U);
            code.value = bytes_to_word(
                table.values[get_byte(temporary, 4U)],
                table.values[get_byte(temporary, 3U)],
                table.values[get_byte(temporary, 2U)],
                table.values[get_byte(temporary, 1U)]) ^
                (halfword_seeds[1] << 9U);
            break;
        }
        case 5U:
        case 6U:
            break;
        case 7U:
            update_seeds(code);
            break;
        default:
            break;
    }

    code.address |= static_cast<std::uint32_t>(key) << 25U;
    return code;
}

void Context::encrypt_words(std::vector<std::uint32_t>& words,
                            std::uint8_t key) {
    validate_words(words);
    line_two_ = false;
    second_line_key_ = 0U;

    for (std::size_t i = 0; i < words.size(); i += 2U) {
        const CodePair encrypted = encrypt({words[i], words[i + 1U]}, key);
        words[i] = encrypted.address;
        words[i + 1U] = encrypted.value;
    }
}

void Context::decrypt_words(std::vector<std::uint32_t>& words,
                            bool remove_verifiers) {
    validate_words(words);
    line_two_ = false;
    second_line_key_ = 0U;

    // A V5 verifier, when present, is only the first row of a cheat block.
    // Do not discard every type-7 row: genuine GS3 payload commands also use
    // type 7 and must survive decryption unchanged.
    if (remove_verifiers && words.size() >= 2U &&
        looks_like_verifier({words[0], words[1]})) {
        words.erase(words.begin(), words.begin() + 2);
    }

    for (std::size_t i = 0; i < words.size();) {
        const CodePair decrypted = decrypt({words[i], words[i + 1U]});
        words[i] = decrypted.address;
        words[i + 1U] = decrypted.value;
        i += 2U;
    }
}


} // namespace omni::devices::gameshark
