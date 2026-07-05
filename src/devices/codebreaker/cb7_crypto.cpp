#include "devices/codebreaker/cb7_crypto.hpp"

#include "crypto/arcfour.hpp"
#include "devices/codebreaker/cb1_crypto.hpp"
#include "devices/codebreaker/cb7_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace omni::devices::codebreaker {
namespace {

std::uint32_t load_le32(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void store_le32(std::uint8_t* bytes, std::uint32_t value) noexcept {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8U);
    bytes[2] = static_cast<std::uint8_t>(value >> 16U);
    bytes[3] = static_cast<std::uint8_t>(value >> 24U);
}

std::array<std::uint8_t, 20> key_bytes(const cb7_tables::Key& key) noexcept {
    std::array<std::uint8_t, 20> bytes{};
    for (std::size_t index = 0U; index < key.size(); ++index) {
        store_le32(bytes.data() + index * 4U, key[index]);
    }
    return bytes;
}

void load_key(cb7_tables::Key& key,
              const std::array<std::uint8_t, 20>& bytes) noexcept {
    for (std::size_t index = 0U; index < key.size(); ++index) {
        key[index] = load_le32(bytes.data() + index * 4U);
    }
}

std::array<std::uint8_t, 8> pair_bytes(std::uint32_t address,
                                       std::uint32_t value) noexcept {
    std::array<std::uint8_t, 8> bytes{};
    store_le32(bytes.data(), address);
    store_le32(bytes.data() + 4U, value);
    return bytes;
}

void load_pair(const std::array<std::uint8_t, 8>& bytes,
               std::uint32_t& address,
               std::uint32_t& value) noexcept {
    address = load_le32(bytes.data());
    value = load_le32(bytes.data() + 4U);
}

} // namespace

V7Context::V7Context() noexcept {
    reset();
}

void V7Context::reset() noexcept {
    seeds_ = {};
    key_ = {};
    old_key_ = {};
    v7_enabled_ = false;
    v7_initialized_ = false;
    beefcode_f_ = false;
}

void V7Context::set_common_v7() {
    v7_enabled_ = true;
    update_beefcode(true, 0U);
    v7_initialized_ = true;
    beefcode_f_ = false;
}

std::uint32_t V7Context::seed_word(std::size_t row,
                                   std::size_t index) const noexcept {
    return load_le32(seeds_[row].data() + index * 4U);
}

void V7Context::update_beefcode(bool initialize, std::uint32_t value) {
    const std::array<std::uint8_t, 4> selector{{
        static_cast<std::uint8_t>(value),
        static_cast<std::uint8_t>(value >> 8U),
        static_cast<std::uint8_t>(value >> 16U),
        static_cast<std::uint8_t>(value >> 24U),
    }};

    if (initialize) {
        beefcode_f_ = false;
        key_ = cb7_tables::default_key;

        if (value != 0U) {
            seeds_ = cb7_tables::default_seeds;
            key_[0] = (static_cast<std::uint32_t>(seeds_[3][selector[3]]) << 24U) |
                      (static_cast<std::uint32_t>(seeds_[2][selector[2]]) << 16U) |
                      (static_cast<std::uint32_t>(seeds_[1][selector[1]]) << 8U) |
                      static_cast<std::uint32_t>(seeds_[0][selector[0]]);
            key_[1] = (static_cast<std::uint32_t>(seeds_[0][selector[3]]) << 24U) |
                      (static_cast<std::uint32_t>(seeds_[3][selector[2]]) << 16U) |
                      (static_cast<std::uint32_t>(seeds_[2][selector[1]]) << 8U) |
                      static_cast<std::uint32_t>(seeds_[1][selector[0]]);
            key_[2] = (static_cast<std::uint32_t>(seeds_[1][selector[3]]) << 24U) |
                      (static_cast<std::uint32_t>(seeds_[0][selector[2]]) << 16U) |
                      (static_cast<std::uint32_t>(seeds_[3][selector[1]]) << 8U) |
                      static_cast<std::uint32_t>(seeds_[2][selector[0]]);
            key_[3] = (static_cast<std::uint32_t>(seeds_[2][selector[3]]) << 24U) |
                      (static_cast<std::uint32_t>(seeds_[1][selector[2]]) << 16U) |
                      (static_cast<std::uint32_t>(seeds_[0][selector[1]]) << 8U) |
                      static_cast<std::uint32_t>(seeds_[3][selector[0]]);
        } else {
            seeds_ = {};
        }
    } else if (value != 0U) {
        key_[0] = (static_cast<std::uint32_t>(seeds_[3][selector[3]]) << 24U) |
                  (static_cast<std::uint32_t>(seeds_[2][selector[2]]) << 16U) |
                  (static_cast<std::uint32_t>(seeds_[1][selector[1]]) << 8U) |
                  static_cast<std::uint32_t>(seeds_[0][selector[0]]);
        key_[1] = (static_cast<std::uint32_t>(seeds_[0][selector[3]]) << 24U) |
                  (static_cast<std::uint32_t>(seeds_[3][selector[2]]) << 16U) |
                  (static_cast<std::uint32_t>(seeds_[2][selector[1]]) << 8U) |
                  static_cast<std::uint32_t>(seeds_[1][selector[0]]);
        key_[2] = (static_cast<std::uint32_t>(seeds_[1][selector[3]]) << 24U) |
                  (static_cast<std::uint32_t>(seeds_[0][selector[2]]) << 16U) |
                  (static_cast<std::uint32_t>(seeds_[3][selector[1]]) << 8U) |
                  static_cast<std::uint32_t>(seeds_[2][selector[0]]);
        key_[3] = (static_cast<std::uint32_t>(seeds_[2][selector[3]]) << 24U) |
                  (static_cast<std::uint32_t>(seeds_[1][selector[2]]) << 16U) |
                  (static_cast<std::uint32_t>(seeds_[0][selector[1]]) << 8U) |
                  static_cast<std::uint32_t>(seeds_[3][selector[0]]);
    } else {
        seeds_ = {};
        key_[0] = 0U;
        key_[1] = 0U;
        key_[2] = 0U;
        key_[3] = 0U;
    }

    for (std::size_t row = 0U; row < seeds_.size(); ++row) {
        auto bytes = key_bytes(key_);
        crypto::ArcFour context(bytes.data(), bytes.size());
        context.crypt(seeds_[row].data(), seeds_[row].size());
        context.crypt(bytes.data(), bytes.size());
        load_key(key_, bytes);
    }

    old_key_ = key_;
}

void V7Context::encrypt_v7(std::uint32_t& address, std::uint32_t& value) {
    const std::uint32_t original_address = address;
    const std::uint32_t original_value = value;

    address = cb7_math::multiply_encrypt(address, old_key_[0] - old_key_[1]);
    value = cb7_math::multiply_encrypt(value, old_key_[2] + old_key_[3]);

    auto bytes = pair_bytes(address, value);
    const auto bytes_key = key_bytes(key_);
    crypto::ArcFour context(bytes_key.data(), bytes_key.size());
    context.crypt(bytes.data(), bytes.size());
    load_pair(bytes, address, value);

    cb7_math::rsa_transform(address, value, cb7_math::rsa_encrypt_exponent);

    for (std::size_t index = 0U; index < 64U; ++index) {
        address = ((address + seed_word(2U, index)) ^ seed_word(0U, index)) -
                  (value ^ seed_word(4U, index));
        value = ((value - seed_word(3U, index)) ^ seed_word(1U, index)) +
                (address ^ seed_word(4U, index));
    }

    if ((original_address & 0xFFFFFFFEU) == 0xBEEFC0DEU) {
        update_beefcode(false, original_value);
        return;
    }

    if (beefcode_f_) {
        const auto beef_key = pair_bytes(original_address, original_value);
        crypto::ArcFour beef_context(beef_key.data(), beef_key.size());
        for (auto& row : seeds_) beef_context.crypt(row.data(), row.size());
        beefcode_f_ = false;
    }
}

void V7Context::decrypt_v7(std::uint32_t& address, std::uint32_t& value) {
    for (std::size_t index = 64U; index-- > 0U;) {
        value = ((value - (address ^ seed_word(4U, index))) ^ seed_word(1U, index)) +
                seed_word(3U, index);
        address = ((address + (value ^ seed_word(4U, index))) ^ seed_word(0U, index)) -
                  seed_word(2U, index);
    }

    cb7_math::rsa_transform(address, value, cb7_math::rsa_decrypt_exponent);

    auto bytes = pair_bytes(address, value);
    const auto bytes_key = key_bytes(key_);
    crypto::ArcFour context(bytes_key.data(), bytes_key.size());
    context.crypt(bytes.data(), bytes.size());
    load_pair(bytes, address, value);

    address = cb7_math::multiply_decrypt(address, old_key_[0] - old_key_[1]);
    value = cb7_math::multiply_decrypt(value, old_key_[2] + old_key_[3]);

    if (beefcode_f_) {
        const auto beef_key = pair_bytes(address, value);
        crypto::ArcFour beef_context(beef_key.data(), beef_key.size());
        for (auto& row : seeds_) beef_context.crypt(row.data(), row.size());
        beefcode_f_ = false;
        return;
    }

    if ((address & 0xFFFFFFFEU) == 0xBEEFC0DEU) {
        update_beefcode(false, value);
    }
}

void V7Context::encrypt(std::uint32_t& address, std::uint32_t& value) {
    const std::uint32_t original_address = address;
    const std::uint32_t original_value = value;

    if (v7_enabled_) {
        encrypt_v7(address, value);
    } else {
        encrypt_v1(address, value);
    }

    if ((original_address & 0xFFFFFFFEU) == 0xBEEFC0DEU) {
        if (!v7_initialized_) {
            update_beefcode(true, original_value);
            v7_initialized_ = true;
        } else {
            update_beefcode(false, original_value);
        }
        v7_enabled_ = true;
        beefcode_f_ = (original_address & 1U) != 0U;
    }
}

void V7Context::decrypt(std::uint32_t& address, std::uint32_t& value) {
    if (v7_enabled_) {
        decrypt_v7(address, value);
    } else {
        decrypt_v1(address, value);
    }

    if ((address & 0xFFFFFFFEU) == 0xBEEFC0DEU) {
        if (!v7_initialized_) {
            update_beefcode(true, value);
            v7_initialized_ = true;
        } else {
            update_beefcode(false, value);
        }
        v7_enabled_ = true;
        beefcode_f_ = (address & 1U) != 0U;
    }
}

CodePair V7Context::encrypt(CodePair code) {
    encrypt(code.address, code.value);
    return code;
}

CodePair V7Context::decrypt(CodePair code) {
    decrypt(code.address, code.value);
    return code;
}

} // namespace omni::devices::codebreaker
