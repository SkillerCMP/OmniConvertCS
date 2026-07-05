#pragma once

#include "core/cheat.hpp"
#include "devices/codebreaker/cb7_tables.hpp"

#include <array>
#include <cstdint>

namespace omni::devices::codebreaker {

class V7Context {
public:
    V7Context() noexcept;

    void reset() noexcept;
    void set_common_v7();

    void encrypt(std::uint32_t& address, std::uint32_t& value);
    void decrypt(std::uint32_t& address, std::uint32_t& value);

    [[nodiscard]] CodePair encrypt(CodePair code);
    [[nodiscard]] CodePair decrypt(CodePair code);

private:
    using SeedTable = cb7_tables::SeedTable;
    using Key = cb7_tables::Key;

    SeedTable seeds_{};
    Key key_{};
    Key old_key_{};
    bool v7_enabled_{};
    bool v7_initialized_{};
    bool beefcode_f_{};

    void update_beefcode(bool initialize, std::uint32_t value);
    void encrypt_v7(std::uint32_t& address, std::uint32_t& value);
    void decrypt_v7(std::uint32_t& address, std::uint32_t& value);

    [[nodiscard]] std::uint32_t seed_word(std::size_t row,
                                          std::size_t index) const noexcept;
};

} // namespace omni::devices::codebreaker
