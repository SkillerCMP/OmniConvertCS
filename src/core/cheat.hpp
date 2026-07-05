#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace omni {

struct CodePair {
    std::uint32_t address{};
    std::uint32_t value{};

    friend bool operator==(const CodePair& lhs, const CodePair& rhs) noexcept {
        return lhs.address == rhs.address && lhs.value == rhs.value;
    }
};

struct Cheat {
    static constexpr std::size_t name_max_size = 50;
    static constexpr std::size_t flag_default_on = 0;
    static constexpr std::size_t flag_master_code = 1;
    static constexpr std::size_t flag_comments = 2;

    std::uint32_t id{};
    std::string name;
    std::string comment;
    std::array<std::uint8_t, 3> flags{};
    std::vector<std::uint32_t> words;
    std::uint8_t state{};

    [[nodiscard]] std::size_t word_count() const noexcept { return words.size(); }
    [[nodiscard]] bool empty() const noexcept { return words.empty(); }

    void append_word(std::uint32_t word);
    void append_pair(std::uint32_t address, std::uint32_t value);
    void prepend_word(std::uint32_t word);
    void remove_words(std::size_t start, std::size_t count);
    void clear() noexcept;

    [[nodiscard]] std::vector<CodePair> pairs() const;
};

} // namespace omni
