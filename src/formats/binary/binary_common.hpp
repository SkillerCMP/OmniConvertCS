#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace omni::formats::binary::common {

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data, std::size_t offset = 0U)
        : data_(data), offset_(offset) {}

    [[nodiscard]] std::size_t position() const noexcept { return offset_; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return offset_ <= data_.size() ? data_.size() - offset_ : 0U;
    }
    void seek(std::size_t offset);
    void skip(std::size_t count);
    [[nodiscard]] std::uint8_t u8();
    [[nodiscard]] std::uint16_t u16le();
    [[nodiscard]] std::uint32_t u32le();
    [[nodiscard]] std::string ascii_z(std::size_t max_length = static_cast<std::size_t>(-1));
    [[nodiscard]] std::string fixed_ascii(std::size_t length);
    [[nodiscard]] std::string utf16le_z();
    [[nodiscard]] std::vector<std::uint8_t> bytes(std::size_t count);

private:
    void require(std::size_t count) const;
    const std::vector<std::uint8_t>& data_;
    std::size_t offset_{};
};

class Writer {
public:
    explicit Writer(std::size_t reserve = 0U) { data_.reserve(reserve); }

    [[nodiscard]] std::size_t position() const noexcept { return data_.size(); }
    void u8(std::uint8_t value);
    void u16le(std::uint16_t value);
    void u32le(std::uint32_t value);
    void ascii(std::string_view text);
    void ascii_z(std::string_view text);
    void fixed_ascii(std::string_view text, std::size_t length);
    void utf16le_z(std::string_view text, std::size_t max_units = static_cast<std::size_t>(-1));
    void append(const std::vector<std::uint8_t>& bytes);
    void append(const std::uint8_t* data, std::size_t count);
    void zeros(std::size_t count);
    void patch_u32le(std::size_t offset, std::uint32_t value);

    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept { return data_; }
    [[nodiscard]] std::vector<std::uint8_t> take() && { return std::move(data_); }

private:
    std::vector<std::uint8_t> data_;
};

[[nodiscard]] std::vector<std::uint8_t> read_file(const std::string& path);
void write_file(const std::string& path, const std::vector<std::uint8_t>& bytes);
[[nodiscard]] std::uint32_t crc32_standard(const std::uint8_t* data,
                                           std::size_t size,
                                           std::uint32_t initial = 0xFFFFFFFFU);
[[nodiscard]] std::uint32_t crc32_standard(const std::vector<std::uint8_t>& data,
                                           std::size_t offset,
                                           std::size_t size,
                                           std::uint32_t initial = 0xFFFFFFFFU);
[[nodiscard]] std::string collapse_newlines(std::string text);

} // namespace omni::formats::binary::common
