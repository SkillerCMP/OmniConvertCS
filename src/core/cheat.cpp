#include "core/cheat.hpp"

#include <algorithm>

namespace omni {

void Cheat::append_word(std::uint32_t word) {
    words.push_back(word);
}

void Cheat::append_pair(std::uint32_t address, std::uint32_t value) {
    words.push_back(address);
    words.push_back(value);
}

void Cheat::prepend_word(std::uint32_t word) {
    words.insert(words.begin(), word);
}

void Cheat::remove_words(std::size_t start, std::size_t count) {
    if (start >= words.size() || count == 0) {
        return;
    }
    const std::size_t end = std::min(words.size(), start + count);
    words.erase(words.begin() + static_cast<std::ptrdiff_t>(start),
                words.begin() + static_cast<std::ptrdiff_t>(end));
}

void Cheat::clear() noexcept {
    id = 0;
    name.clear();
    comment.clear();
    flags.fill(0);
    words.clear();
    state = 0;
}

std::vector<CodePair> Cheat::pairs() const {
    std::vector<CodePair> result;
    result.reserve((words.size() + 1U) / 2U);
    for (std::size_t index = 0; index < words.size(); index += 2U) {
        const std::uint32_t value = (index + 1U < words.size()) ? words[index + 1U] : 0U;
        result.push_back(CodePair{words[index], value});
    }
    return result;
}

} // namespace omni
