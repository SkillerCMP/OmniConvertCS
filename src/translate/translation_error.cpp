#include "translate/translation_error.hpp"

#include <sstream>

namespace omni::translate {

std::string_view error_text(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::invalid_code:
            return "Code contains an invalid or truncated multi-line command";
        case ErrorCode::value_increment:
            return "Target device cannot represent the requested value increment";
        case ErrorCode::copy_bytes:
            return "Target device does not support the copy-bytes command";
        case ErrorCode::excess_offsets:
            return "Target device does not support the requested pointer depth";
        case ErrorCode::offset_value_32:
            return "Target device does not allow an offset on a 32-bit pointer write";
        case ErrorCode::offset_value_16:
            return "Target device cannot represent the 16-bit pointer offset";
        case ErrorCode::offset_value_8:
            return "Target device cannot represent the 8-bit pointer offset";
        case ErrorCode::bitwise:
            return "Target device does not support this bitwise operation";
        case ErrorCode::timer:
            return "Target device does not support this timer/control command";
        case ErrorCode::all_off:
            return "Target device does not support this all-remaining-lines gate";
        case ErrorCode::comparison:
            return "Target device cannot represent this comparison without changing its meaning";
        case ErrorCode::pointer_size:
            return "Target device does not support the requested pointer-write width";
        case ErrorCode::test_size:
            return "Target device does not support the requested comparison width";
        case ErrorCode::serial_size:
            return "Target device cannot represent this serial/repeated-write layout";
        case ErrorCode::semantic_loss:
            return "Conversion would silently change command lifetime or setup-time behavior";
        case ErrorCode::unsupported_hook:
            return "Target device hook/master format cannot represent this hook configuration exactly";
        case ErrorCode::unsupported_control:
            return "Target device cannot represent this internal control subtype";
        case ErrorCode::count_overflow:
            return "Target device cannot represent the requested conditional or repeat count";
    }
    return "Unknown translation error";
}

TranslationError::TranslationError(ErrorCode code, std::size_t word_index, std::string detail)
    : std::runtime_error([&]() {
          std::ostringstream message;
          message << error_text(code) << " at 32-bit word index " << word_index;
          if (!detail.empty()) message << ": " << detail;
          return message.str();
      }()),
      code_(code),
      word_index_(word_index) {}

} // namespace omni::translate
