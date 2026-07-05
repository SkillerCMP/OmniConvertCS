#pragma once

#include "core/cheat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace omni::devices::gameshark {

inline constexpr std::uint8_t default_key = 4U;

class Context {
public:
    Context();

    void reset();

    [[nodiscard]] CodePair encrypt(CodePair code, std::uint8_t key);
    [[nodiscard]] CodePair decrypt(CodePair code);

    void encrypt_words(std::vector<std::uint32_t>& words, std::uint8_t key);
    void decrypt_words(std::vector<std::uint32_t>& words,
                       bool remove_verifiers = true);

private:
    struct CryptTable {
        std::size_t size{};
        std::array<std::uint8_t, 256> values{};
    };

    class MtState {
    public:
        void initialize(std::uint32_t seed);
        [[nodiscard]] std::uint32_t next();

    private:
        std::array<std::uint32_t, 624> state_{};
        std::size_t pass_count_{624U};
    };

    static constexpr std::array<std::uint32_t, 3> halfword_seeds{
        0x6C27U, 0x1D38U, 0x7FE1U};

    void build_seed_table(CryptTable& table, std::uint16_t seed);
    void reverse_seed_table(CryptTable& destination, const CryptTable& source);
    [[nodiscard]] std::uint8_t build_seeds(std::uint16_t seed1,
                                           std::uint16_t seed2,
                                           std::uint16_t seed3);
    void update_seeds(const CodePair& code);

    std::array<CryptTable, 4> encrypt_tables_{};
    std::array<CryptTable, 4> decrypt_tables_{};
    bool line_two_{};
    std::uint8_t second_line_key_{};
};


} // namespace omni::devices::gameshark
