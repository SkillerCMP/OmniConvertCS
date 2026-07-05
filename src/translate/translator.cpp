#include "translate/translator.hpp"

#include "translate/translation_internal.hpp"

namespace omni::translate {

void translate_cheat(Cheat& cheat, Device input_device, Device output_device,
                     Report* report, TransposeMode mode) {
    if (cheat.empty() || input_device == output_device) return;

    Cheat translated;
    translated.id = cheat.id;
    translated.name = cheat.name;
    translated.comment = cheat.comment;
    translated.flags = cheat.flags;
    translated.state = cheat.state;

    std::size_t index = 0U;
    try {
        while (index < cheat.words.size()) {
            if (mode == TransposeMode::original) {
                if (input_device == Device::armax) {
                    detail::translate_original_armax_to_standard(
                        translated, cheat, index, output_device, report);
                } else if (output_device == Device::armax) {
                    detail::translate_original_standard_to_armax(
                        translated, cheat, index, input_device, report);
                } else {
                    detail::translate_original_standard_to_standard(
                        translated, cheat, index, input_device, output_device,
                        report);
                }
            } else if (input_device == Device::armax) {
                detail::translate_armax_to_standard(translated, cheat, index,
                                                    output_device, report);
            } else if (output_device == Device::armax) {
                detail::translate_standard_to_armax(translated, cheat, index,
                                                    input_device, report);
            } else {
                detail::translate_standard_to_standard(translated, cheat, index,
                                                       input_device, output_device,
                                                       report);
            }
        }
    } catch (...) {
        // OmniConvertCS always copies the destination buffer back even when a
        // legacy translator returns an error. This can intentionally turn a
        // failed/name-bearing entry into an empty organizer block. Strict mode
        // remains transactional and leaves the source untouched on failure.
        if (mode == TransposeMode::original) {
            cheat.words = std::move(translated.words);
        }
        throw;
    }

    cheat.words = std::move(translated.words);
}

} // namespace omni::translate
