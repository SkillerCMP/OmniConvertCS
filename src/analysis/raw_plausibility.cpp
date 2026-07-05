#include "analysis/raw_plausibility.hpp"

#include <cstddef>

namespace omni::analysis {
namespace {

PlausibilityResult fail(std::uint32_t word, std::uint32_t addr,
                        const char* rule, const char* reason,
                        bool skipped = false) {
    return {false, word, addr, rule, reason,
            skipped ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : ""};
}

PlausibilityResult pass(std::uint32_t word, std::uint32_t addr,
                        const char* rule, bool skipped = false) {
    return {true, word, addr, rule, "",
            skipped ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : ""};
}

bool is_cb7_prologue(std::uint32_t word) noexcept {
    if ((word & 0xFF000000U) == 0xFE000000U) return true;
    const std::uint32_t addr = word & 0x0FFFFFFFU;
    return addr == 0x000FFFFEU || addr == 0x000FFFFFU;
}

} // namespace

bool is_raw_family(Crypt crypt) noexcept {
    // OmniConvertCS analysis is active only for RAW, CRAW, and GRAW.
    return crypt == Crypt::raw || crypt == Crypt::codebreaker_raw ||
           crypt == Crypt::gameshark_raw;
}

PlausibilityResult validate_raw_plausibility(const Cheat& cheat,
                                             Crypt output_crypt,
                                             AnalyzeMode mode) {
    if (cheat.words.empty()) return fail(0U, 0U, "Empty", "no code words");
    if ((cheat.words.size() & 1U) != 0U) {
        return fail(0U, 0U, "OddCodeCount", "code word count is odd");
    }

    const bool strict_craw = mode != AnalyzeMode::none &&
                             output_crypt == Crypt::codebreaker_raw;
    bool skipped = false;
    const std::size_t line_count = cheat.words.size() / 2U;

    for (std::size_t index = 0U; index + 1U < cheat.words.size(); index += 2U) {
        const std::uint32_t w1 = cheat.words[index];
        const std::uint32_t w2 = cheat.words[index + 1U];
        if (is_cb7_prologue(w1)) {
            skipped = true;
            continue;
        }

        const std::size_t line = index / 2U;
        const bool last = line + 1U == line_count;
        const std::uint32_t type = w1 >> 28U;
        const std::uint32_t hi16 = w1 & 0xFFFF0000U;
        const std::uint32_t default_addr = w1 & 0x0FFFFFFFU;

        if (strict_craw) {
            if (type == 0xCU) {
                return fail(w1, default_addr, "TypeCNeedsDoubleCheck",
                            "C-type output should be manually reviewed for CRAW analysis", skipped);
            }
            if (type == 0x6U) {
                if (last) return fail(w1, default_addr, "Type6MissingSecondLine",
                                      "6-type requires a following 000Gnnnn/offset line", skipped);
                const std::uint32_t w3 = cheat.words[index + 2U];
                const std::uint32_t w4 = cheat.words[index + 3U];
                const std::uint32_t w3hi = w3 & 0xFFFF0000U;
                const std::uint32_t width = (w3hi >> 16U) & 0xFU;
                const std::uint32_t derefs = w3 & 0xFFFFU;
                if (w3hi != 0x00000000U && w3hi != 0x00010000U &&
                    w3hi != 0x00020000U) {
                    return fail(w3, default_addr, "Type6BadHeader",
                                "6-type second-line header is not 000Gnnnn (G must be 0,1,2)", skipped);
                }
                if (width == 1U && (w1 & 1U) != 0U)
                    return fail(w1, default_addr, "Type6Unaligned16",
                                "6-type 16-bit address must be 2-byte aligned", skipped);
                if (width == 2U && (w1 & 3U) != 0U)
                    return fail(w1, default_addr, "Type6Unaligned32",
                                "6-type 32-bit address must be 4-byte aligned", skipped);
                if (width == 0U && (w2 & 0xFFFFFF00U) != 0U)
                    return fail(w2, default_addr, "Type6ValueTooWide8",
                                "6-type 8-bit value exceeds 0xFF", skipped);
                if (width == 1U && (w2 & 0xFFFF0000U) != 0U)
                    return fail(w2, default_addr, "Type6ValueTooWide16",
                                "6-type 16-bit value exceeds 0xFFFF", skipped);
                if (w4 > 0x01FFFFFFU)
                    return fail(w4, default_addr, "Type6OffsetOutOfRange25",
                                "6-type offset is outside 25-bit range", skipped);
                const std::size_t required = derefs <= 1U ? 1U : static_cast<std::size_t>(derefs) + 1U;
                const std::size_t available = cheat.words.size() - (index + 3U);
                if (available < required)
                    return fail(w3, default_addr, "Type6PointerOffsetsTruncated",
                                "6-type pointer offsets are truncated", skipped);
            }
            if (type == 0x7U) {
                const std::uint32_t op = w2 & 0xFFFF0000U;
                const bool is8 = op == 0x00000000U || op == 0x00200000U || op == 0x00400000U;
                const bool is16 = op == 0x00100000U || op == 0x00300000U || op == 0x00500000U;
                if (!is8 && !is16)
                    return fail(w2, default_addr, "Type7BadOpEncoding",
                                "7-type value word has an unknown operation encoding", skipped);
                if (is8 && (w2 & 0x0000FF00U) != 0U)
                    return fail(w2, default_addr, "Type7Bad8BitValue",
                                "7-type 8-bit value exceeds one byte", skipped);
                if (is16 && (w1 & 1U) != 0U)
                    return fail(w1, default_addr, "Type7Unaligned16",
                                "7-type 16-bit address must be 2-byte aligned", skipped);
            }
            if (hi16 >= 0x30000000U && hi16 <= 0x30300000U) {
                if (last) return fail(w1, w2 & 0x0FFFFFFFU, "Type3MissingSecondLine",
                                      "3-type requires a following value line", skipped);
                const std::uint32_t w3 = cheat.words[index + 2U];
                const std::uint32_t w4 = cheat.words[index + 3U];
                const std::uint32_t width = (hi16 >> 20U) & 0xFU;
                if (w4 != 0U) return fail(w4, w2 & 0x0FFFFFFFU,
                                         "Type3SecondLineMustEndWith0",
                                         "3-type second line must end with 00000000", skipped);
                if ((width == 0U || width == 1U) && (w3 & 0xFFFFFF00U) != 0U)
                    return fail(w3, w2 & 0x0FFFFFFFU, "Type3ValueTooWide8",
                                "3-type 8-bit value exceeds 0xFF", skipped);
                if ((width == 2U || width == 3U) && (w3 & 0xFFFF0000U) != 0U)
                    return fail(w3, w2 & 0x0FFFFFFFU, "Type3ValueTooWide16",
                                "3-type 16-bit value exceeds 0xFFFF", skipped);
            }
            if (type == 0x4U || type == 0x5U) {
                if (last) return fail(w1, default_addr,
                                      type == 0x4U ? "Type4MissingSecondLine" : "Type5MissingSecondLine",
                                      "Type 4/5 requires a following line", skipped);
                const std::uint32_t w3 = cheat.words[index + 2U];
                const std::uint32_t w4 = cheat.words[index + 3U];
                if (w4 != 0U) return fail(w4, default_addr,
                                         type == 0x4U ? "Type4SecondLineMustEndWith0" : "Type5SecondLineMustEndWith0",
                                         "Type 4/5 second line must end with 00000000", skipped);
                if (type == 0x5U && (w3 & 0xF0000000U) != 0U)
                    return fail(w3, w3 & 0x0FFFFFFFU, "Type5SecondLineAddrMustStartWith0",
                                "5-type second-line address must start with 0", skipped);
                if (type == 0x5U && (w3 & 0x0FFFFFFFU) > 0x01FFFFFFU)
                    return fail(w3, w3 & 0x0FFFFFFFU, "Type5SecondLineAddrOutOfRange25",
                                "5-type second-line address exceeds 0x01FFFFFF", skipped);
            }
        }

        if (type == 0xBU)
            return fail(w1, default_addr, "TypeBNotAllowed",
                        "B-type is not a valid RAW-family code type", skipped);
        if (type == 0x1U && (default_addr & 1U) != 0U)
            return fail(w1, default_addr, "Type1AddrMisaligned",
                        "Type 1 address must be 2-byte aligned", skipped);
        if (type == 0x2U && (default_addr & 3U) != 0U)
            return fail(w1, default_addr, "Type2AddrMisaligned",
                        "Type 2 address must be 4-byte aligned", skipped);
        if (type == 0x0U && (w2 & 0xFFFFFF00U) != 0U)
            return fail(w1, default_addr, "Type0ValueTooWide",
                        "Type 0 value must fit 8 bits", skipped);
        if (type == 0x1U && (w2 & 0xFFFF0000U) != 0U)
            return fail(w1, default_addr, "Type1ValueTooWide",
                        "Type 1 value must fit 16 bits", skipped);

        if (last && (type == 0x4U || type == 0xDU || type == 0xEU ||
                     hi16 == 0x30400000U || hi16 == 0x30500000U)) {
            return fail(w1, default_addr,
                        type == 0x4U ? "Type4MissingSecondLine" :
                        (type == 0xDU || type == 0xEU) ? "TypeDOrEMissingSecondLine" :
                        "3040Or3050MissingSecondLine",
                        "multi-line command is missing its following line", skipped);
        }

        std::uint32_t address_word = w1;
        const char* rule = "w1.addr28";
        if ((hi16 >= 0x30000000U && hi16 <= 0x30300000U) ||
            hi16 == 0x30400000U || hi16 == 0x30500000U || type == 0xEU) {
            address_word = w2;
            rule = type == 0xEU ? "E-type uses w2.addr28" : "3-type uses w2.addr28";
        }
        const std::uint32_t addr = address_word & 0x0FFFFFFFU;
        if (addr > 0x01FFFFFFU)
            return fail(address_word, addr, rule, "addr28 > 0x01FFFFFF", skipped);
        return pass(address_word, addr, rule, skipped);
    }

    return pass(0U, 0U, "NoCheckableLine", skipped);
}

} // namespace omni::analysis
