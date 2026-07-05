#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace omni::translate {

enum class ErrorCode {
    invalid_code,
    value_increment,
    copy_bytes,
    excess_offsets,
    offset_value_32,
    offset_value_16,
    offset_value_8,
    bitwise,
    timer,
    all_off,
    comparison,
    pointer_size,
    test_size,
    serial_size,
    semantic_loss,
    unsupported_hook,
    unsupported_control,
    count_overflow
};

[[nodiscard]] std::string_view error_text(ErrorCode code) noexcept;

class TranslationError final : public std::runtime_error {
public:
    TranslationError(ErrorCode code, std::size_t word_index, std::string detail = {});

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] std::size_t word_index() const noexcept { return word_index_; }

private:
    ErrorCode code_;
    std::size_t word_index_;
};

} // namespace omni::translate
