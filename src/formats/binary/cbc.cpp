#include "formats/binary/application_formats.hpp"

#include "devices/codebreaker/cb_file_crypto.hpp"
#include "formats/binary/binary_common.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace omni::formats::binary {
namespace {

constexpr std::size_t cb7_header_size = 64U;
constexpr std::size_t cb8_header_size = 344U;
constexpr std::uint32_t cbc_file_id = 0x01554643U;
constexpr std::uint32_t cbc_v8_version = 0x00000800U;
constexpr std::size_t name_max = 63U;

std::string block_name(const text::CheatBlock& block) {
    const std::string value = block.label.value_or(block.cheat.name);
    return value.empty() ? "Unnamed Cheat" : value.substr(0U, name_max);
}

std::vector<std::uint8_t> v8_signature() {
    constexpr std::string_view banner = "Created with OmniconvertCS CBC v8 ";
    std::vector<std::uint8_t> result(256U);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(banner[index % banner.size()]);
    }
    return result;
}

void parse_payload(LoadResult& result, std::vector<std::uint8_t> payload,
                   CodeFormat inferred_format, std::string header_name) {
    devices::codebreaker::crypt_file_data(payload);
    common::Reader reader(payload);
    std::string list_name = reader.ascii_z();
    if (list_name.empty()) list_name = std::move(header_name);
    result.game_name = std::move(list_name);
    result.input_format = inferred_format;
    const std::uint16_t count = reader.u16le();
    for (std::uint16_t index = 0U; index < count; ++index) {
        text::CheatBlock block;
        block.label = reader.ascii_z();
        block.cheat.name = block.label.value_or("");
        const std::uint8_t switch_byte = reader.u8();
        const std::uint16_t line_count = reader.u16le();
        if (reader.remaining() < static_cast<std::size_t>(line_count) * 8U) {
            throw std::runtime_error("CBC cheat data is truncated");
        }
        block.cheat.words.reserve(static_cast<std::size_t>(line_count) * 2U);
        for (std::uint16_t line = 0U; line < line_count; ++line) {
            block.cheat.words.push_back(reader.u32le());
            block.cheat.words.push_back(reader.u32le());
        }
        if (index == 0U && !block.cheat.empty()) {
            block.cheat.flags[Cheat::flag_master_code] = 1U;
        }
        if (switch_byte == 4U && line_count != 0U) {
            result.warnings.emplace_back(
                "CBC heading switch was set on an entry that also contains code lines");
        }
        result.blocks.push_back(std::move(block));
    }
}

} // namespace

LoadResult load_cbc(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < cb7_header_size) {
        throw std::runtime_error("CBC file is too small to be valid");
    }

    LoadResult result;
    common::Reader reader(bytes);
    const bool v8 = reader.u32le() == cbc_file_id;
    if (v8) {
        if (bytes.size() < cb8_header_size) {
            throw std::runtime_error("CBC v8 header is truncated");
        }
        reader.seek(4U + 256U);
        static_cast<void>(reader.u32le());
        const std::string header_name = reader.fixed_ascii(72U);
        const std::uint32_t data_offset = reader.u32le();
        static_cast<void>(reader.u32le());
        if (data_offset >= bytes.size()) {
            throw std::runtime_error("CBC v8 file has an invalid data offset");
        }
        common::Reader payload_reader(bytes, data_offset);
        parse_payload(result, payload_reader.bytes(payload_reader.remaining()),
                      CodeFormat::codebreaker7_common, header_name);
        result.warnings.emplace_back(
            "CBC v8 input was inferred as CodeBreaker V7+ Common encryption");
    } else {
        reader.seek(0U);
        const std::string header_name = reader.fixed_ascii(cb7_header_size);
        parse_payload(result, reader.bytes(reader.remaining()),
                      CodeFormat::codebreaker1_6, header_name);
    }
    return result;
}

std::vector<std::uint8_t> write_cbc(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options) {
    std::vector<const text::CheatBlock*> entries;
    for (const text::CheatBlock& block : blocks) {
        if (block.cheat.state != 0U) continue;
        if (!options.include_headings && block.cheat.empty()) continue;
        entries.push_back(&block);
    }
    if (entries.empty()) throw std::invalid_argument("No valid cheats are available for CBC export");
    if (entries.front()->cheat.flags[Cheat::flag_master_code] == 0U) {
        throw std::invalid_argument("CBC export requires the first entry to be the master code");
    }
    for (std::size_t index = 1U; index < entries.size(); ++index) {
        if (entries[index]->cheat.flags[Cheat::flag_master_code] != 0U) {
            throw std::invalid_argument("CBC export cannot contain more than one master code");
        }
    }
    if (entries.size() > 0xFFFFU) throw std::invalid_argument("CBC export contains too many cheats");

    const std::string game_name = options.game_name.substr(0U, name_max);
    common::Writer payload;
    payload.ascii_z(game_name);
    payload.u16le(static_cast<std::uint16_t>(entries.size()));
    for (const text::CheatBlock* block : entries) {
        if ((block->cheat.word_count() & 1U) != 0U) {
            throw std::invalid_argument("CBC export requires complete address/value pairs");
        }
        const std::size_t line_count = block->cheat.word_count() / 2U;
        if (line_count > 0xFFFFU) throw std::invalid_argument("A CBC cheat contains too many lines");
        payload.ascii_z(block_name(*block));
        payload.u8(block->cheat.empty() ? 4U : 0U);
        payload.u16le(static_cast<std::uint16_t>(line_count));
        for (const std::uint32_t word : block->cheat.words) payload.u32le(word);
    }

    std::vector<std::uint8_t> encrypted = payload.data();
    devices::codebreaker::crypt_file_data(encrypted);

    common::Writer output;
    if (options.cbc_version == CbcVersion::v8) {
        output.u32le(cbc_file_id);
        output.append(v8_signature());
        output.u32le(cbc_v8_version);
        output.fixed_ascii(game_name, 72U);
        output.u32le(static_cast<std::uint32_t>(cb8_header_size));
        output.u32le(0U);
    } else {
        output.fixed_ascii(game_name, cb7_header_size);
    }
    output.append(encrypted);
    return std::move(output).take();
}

} // namespace omni::formats::binary
