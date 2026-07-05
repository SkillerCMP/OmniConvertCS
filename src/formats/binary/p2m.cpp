#include "formats/binary/application_formats.hpp"

#include "devices/gameshark/gs3_file.hpp"
#include "formats/binary/binary_common.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

namespace omni::formats::binary {
namespace {

constexpr std::uint32_t p2m_file_id = 0x534D3250U;
constexpr std::uint32_t p2m_file_version = 0x32563250U;
constexpr std::size_t p2m_header_size = 0x48U;
constexpr std::size_t p2m_arcstat_size = 8U;
constexpr std::size_t p2m_fd_size = 0x80U;
constexpr std::size_t p2m_file_count = 2U;
constexpr std::size_t p2m_name_size = 0x38U;
constexpr std::size_t p2m_fname_size = 0x20U;
constexpr std::uint32_t attr_dir = 0x00008427U;
constexpr std::uint32_t attr_file = 0x00008417U;
constexpr std::uint32_t term_file = 0x00000000U;
constexpr std::uint32_t term_game_name = 0x20000000U;
constexpr std::uint32_t term_code_name = 0x40000000U;
constexpr std::uint32_t term_code_line = 0x80000000U;
constexpr std::size_t gs_name_max = 99U;
constexpr std::string_view root_name = "BLAZE_USA";
constexpr std::string_view user_name = "user.dat";
constexpr std::string_view footer = "Cheat File";

std::string block_name(const text::CheatBlock& block) {
    std::string name = block.label.value_or(block.cheat.name);
    if (name.empty()) name = "Unnamed Cheat";
    if (name.size() > gs_name_max) name.resize(gs_name_max);
    return name;
}

bool ascii_iequals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0U; index < left.size(); ++index) {
        const auto lhs = static_cast<unsigned char>(left[index]);
        const auto rhs = static_cast<unsigned char>(right[index]);
        if (std::tolower(lhs) != std::tolower(rhs)) return false;
    }
    return true;
}

struct DateFields {
    std::uint8_t second{};
    std::uint8_t minute{};
    std::uint8_t hour{};
    std::uint8_t day{};
    std::uint8_t month{};
    std::uint16_t year{};
};

DateFields current_date() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {static_cast<std::uint8_t>(local.tm_sec),
            static_cast<std::uint8_t>(local.tm_min),
            static_cast<std::uint8_t>(local.tm_hour),
            static_cast<std::uint8_t>(local.tm_mday),
            static_cast<std::uint8_t>(local.tm_mon + 1),
            static_cast<std::uint16_t>(local.tm_year + 1900)};
}

void write_date(common::Writer& writer, const DateFields& date) {
    writer.u8(0U);
    writer.u8(date.second);
    writer.u8(date.minute);
    writer.u8(date.hour);
    writer.u8(date.day);
    writer.u8(date.month);
    writer.u16le(date.year);
}

void write_descriptor(common::Writer& writer, const DateFields& date,
                      std::uint32_t attributes, std::string_view name,
                      std::uint32_t offset, std::uint32_t size,
                      std::uint32_t crc) {
    write_date(writer, date);
    write_date(writer, date);
    writer.u32le(0U);
    writer.u32le(attributes);
    writer.u32le(0U);
    writer.u32le(0U);
    writer.fixed_ascii(name, p2m_fname_size);
    writer.u32le(offset);
    writer.u32le(size);
    writer.u32le(crc);
    writer.u32le(0U);
    writer.zeros(0x30U);
}

std::string read_wide_with_tag(common::Reader& reader, std::uint32_t expected,
                               std::vector<std::string>& warnings) {
    const std::string value = reader.utf16le_z();
    if (reader.remaining() >= 4U) {
        const std::uint32_t tag = reader.u32le();
        if (tag != expected) warnings.emplace_back("P2M user.dat contains a nonstandard record tag");
    }
    return value;
}

} // namespace

LoadResult load_p2m(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < p2m_header_size) {
        throw std::runtime_error("File is too small to be a valid P2M archive");
    }
    common::Reader reader(bytes);
    if (reader.u32le() != p2m_file_id) {
        throw std::runtime_error("P2M file ID mismatch (expected P2MS)");
    }
    reader.seek(p2m_header_size);
    const std::uint32_t num_files = reader.u32le();
    static_cast<void>(reader.u32le());
    if (num_files != p2m_file_count) {
        throw std::runtime_error("Unsupported P2M archive file count");
    }

    const std::size_t descriptor_base = reader.position();
    if (bytes.size() < descriptor_base + p2m_fd_size * p2m_file_count) {
        throw std::runtime_error("P2M file descriptors are truncated");
    }

    std::uint32_t user_offset = 0U;
    std::uint32_t user_size = 0U;
    bool found = false;
    for (std::size_t index = 0U; index < p2m_file_count; ++index) {
        common::Reader descriptor(bytes, descriptor_base + index * p2m_fd_size);
        descriptor.skip(32U);
        const std::string name = descriptor.fixed_ascii(p2m_fname_size);
        const std::uint32_t offset = descriptor.u32le();
        const std::uint32_t size = descriptor.u32le();
        if (ascii_iequals(name, user_name)) {
            user_offset = offset;
            user_size = size;
            found = true;
            break;
        }
    }
    if (!found || user_size == 0U) {
        throw std::runtime_error("P2M archive does not contain a valid user.dat entry");
    }
    const std::size_t start = p2m_header_size + user_offset;
    if (start > bytes.size() || user_size > bytes.size() - start) {
        throw std::runtime_error("P2M user.dat entry runs past end of file");
    }
    std::vector<std::uint8_t> user(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                                   bytes.begin() + static_cast<std::ptrdiff_t>(start + user_size));
    devices::gameshark::crypt_file_data(user);

    LoadResult result;
    result.input_format = CodeFormat::gameshark_madcatz5;
    common::Reader data(user);
    const std::uint32_t game_count = data.u32le();
    const std::uint32_t cheat_count = data.u32le();
    static_cast<void>(data.u32le());
    const std::uint16_t duplicate_count = data.u16le();
    if (game_count == 0U) throw std::runtime_error("P2M archive reports zero games");
    if (duplicate_count != static_cast<std::uint16_t>(cheat_count & 0xFFFFU)) {
        result.warnings.emplace_back("P2M duplicate cheat count does not match the main count");
    }
    result.game_name = read_wide_with_tag(data, term_game_name, result.warnings);

    for (std::uint32_t index = 0U; index < cheat_count; ++index) {
        text::CheatBlock block;
        const std::uint16_t line_count = data.u16le();
        block.label = read_wide_with_tag(data, term_code_name, result.warnings);
        block.cheat.name = block.label.value_or("");
        block.cheat.words.reserve(static_cast<std::size_t>(line_count) * 2U);
        for (std::uint16_t line = 0U; line < line_count; ++line) {
            block.cheat.words.push_back(data.u32le());
            block.cheat.words.push_back(data.u32le());
            if (data.u32le() != term_code_line) {
                throw std::runtime_error("Unexpected record type in P2M user.dat");
            }
        }
        if (index == 0U && !block.cheat.empty()) {
            block.cheat.flags[Cheat::flag_master_code] = 1U;
        }
        result.blocks.push_back(std::move(block));
    }
    return result;
}

std::vector<std::uint8_t> write_p2m(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options) {
    std::vector<const text::CheatBlock*> entries;
    for (const text::CheatBlock& block : blocks) {
        if (block.cheat.state != 0U) continue;
        if (!options.include_headings && block.cheat.empty()) continue;
        entries.push_back(&block);
    }
    if (entries.empty()) throw std::invalid_argument("No valid cheats are available for P2M export");
    if (entries.front()->cheat.flags[Cheat::flag_master_code] == 0U) {
        throw std::invalid_argument("P2M export requires the first entry to be the master code");
    }
    for (std::size_t index = 1U; index < entries.size(); ++index) {
        if (entries[index]->cheat.flags[Cheat::flag_master_code] != 0U) {
            throw std::invalid_argument("P2M export cannot contain more than one master code");
        }
    }

    common::Writer user;
    std::uint32_t total_lines = 0U;
    for (const text::CheatBlock* block : entries) {
        if ((block->cheat.word_count() & 1U) != 0U) {
            throw std::invalid_argument("P2M export requires complete address/value pairs");
        }
        total_lines += static_cast<std::uint32_t>(block->cheat.word_count() / 2U);
    }
    user.u32le(1U);
    user.u32le(static_cast<std::uint32_t>(entries.size()));
    user.u32le(total_lines);
    user.u16le(static_cast<std::uint16_t>(entries.size()));
    user.utf16le_z(options.game_name, gs_name_max);
    user.u32le(term_game_name);

    for (std::size_t index = 0U; index < entries.size(); ++index) {
        const text::CheatBlock& block = *entries[index];
        const std::size_t line_count = block.cheat.word_count() / 2U;
        if (line_count > 0xFFFFU) throw std::invalid_argument("A P2M cheat contains too many lines");
        user.u16le(static_cast<std::uint16_t>(line_count));
        std::string name = block_name(block);
        if (index == 0U && !name.empty() && name.front() != '{') name = "{5" + name;
        if (block.cheat.empty() && !name.empty() && name.front() != '{') name = "{3" + name;
        user.utf16le_z(name, gs_name_max);
        user.u32le(term_code_name);
        for (std::size_t word = 0U; word < block.cheat.word_count(); word += 2U) {
            user.u32le(block.cheat.words[word]);
            user.u32le(block.cheat.words[word + 1U]);
            user.u32le(term_code_line);
        }
    }

    std::vector<std::uint8_t> encrypted_user = user.data();
    devices::gameshark::crypt_file_data(encrypted_user);
    const std::uint32_t user_crc = devices::gameshark::file_crc32(encrypted_user);
    const std::uint32_t user_offset = static_cast<std::uint32_t>(
        p2m_arcstat_size + p2m_fd_size * p2m_file_count);
    const std::uint32_t archive_size = static_cast<std::uint32_t>(encrypted_user.size());

    common::Writer output;
    output.u32le(p2m_file_id);
    const std::size_t filesize_offset = output.position();
    output.u32le(0U);
    // The native/C# writer reserves the final byte of the fixed title field
    // for a NUL terminator, so at most 55 visible bytes are serialized.
    const std::string archive_name = options.game_name.substr(0U, p2m_name_size - 1U);
    output.fixed_ascii(archive_name, p2m_name_size);
    output.u32le(p2m_file_version);
    output.u32le(0x0AU);
    output.u32le(static_cast<std::uint32_t>(p2m_file_count));
    output.u32le(archive_size);

    const DateFields date = current_date();
    write_descriptor(output, date, attr_dir, root_name, 0xFFFFFFFFU, 0xFFFFFFFFU, 0U);
    write_descriptor(output, date, attr_file, user_name, user_offset, archive_size, user_crc);
    output.append(encrypted_user);
    output.u32le(term_file);
    output.ascii(footer);

    std::vector<std::uint8_t> bytes = std::move(output).take();
    const std::uint32_t stored_size = static_cast<std::uint32_t>(bytes.size() - 12U);
    bytes[filesize_offset] = static_cast<std::uint8_t>(stored_size);
    bytes[filesize_offset + 1U] = static_cast<std::uint8_t>(stored_size >> 8U);
    bytes[filesize_offset + 2U] = static_cast<std::uint8_t>(stored_size >> 16U);
    bytes[filesize_offset + 3U] = static_cast<std::uint8_t>(stored_size >> 24U);
    return bytes;
}

} // namespace omni::formats::binary
