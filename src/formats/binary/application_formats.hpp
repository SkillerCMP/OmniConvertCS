#pragma once

#include "core/code_format.hpp"
#include "formats/text/text_cheat_parser.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace omni::formats::binary {

enum class ApplicationFormat {
    armax_bin,
    codebreaker_cbc,
    gameshark_p2m,
    swap_magic_bin
};

enum class CbcVersion : std::uint8_t {
    v7 = 7,
    v8 = 8
};

struct LoadResult {
    std::string game_name;
    std::uint32_t game_id{};
    CodeFormat input_format{CodeFormat::standard_raw};
    std::vector<text::CheatBlock> blocks;
    std::vector<std::string> warnings;
};

struct WriteOptions {
    std::string game_name;
    std::uint32_t game_id{};
    CbcVersion cbc_version{CbcVersion::v7};
    bool include_headings{true};
};

[[nodiscard]] LoadResult load_armax_bin(const std::vector<std::uint8_t>& bytes);
[[nodiscard]] std::vector<std::uint8_t> write_armax_bin(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options);

[[nodiscard]] LoadResult load_cbc(const std::vector<std::uint8_t>& bytes);
[[nodiscard]] std::vector<std::uint8_t> write_cbc(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options);

[[nodiscard]] LoadResult load_p2m(const std::vector<std::uint8_t>& bytes);
[[nodiscard]] std::vector<std::uint8_t> write_p2m(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options);

[[nodiscard]] LoadResult load_swap_magic_bin(const std::vector<std::uint8_t>& bytes);
[[nodiscard]] std::vector<std::uint8_t> write_swap_magic_bin(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options);

[[nodiscard]] LoadResult load_application_file(ApplicationFormat format,
                                                const std::string& path);
void write_application_file(ApplicationFormat format, const std::string& path,
                            const std::vector<text::CheatBlock>& blocks,
                            const WriteOptions& options = {});

[[nodiscard]] std::string application_format_name(ApplicationFormat format);

} // namespace omni::formats::binary
