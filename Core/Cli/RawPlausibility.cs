using System;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static class RawPlausibility
    {
        private static bool IsCb7MasterPrologueLine(uint w1)
        {
            // CB7 "master/enabler" prologue commonly emits:
            //   FEXXXXXX XXXXXXXX
            //   000FFFFE VVVVVVVV
            //   000FFFFF VVVVVVVV
            // before the real memory write lines.
            //
            // These prologue lines are not actual EE RAM writes, and the FE-line
            // often carries a non-RAM "address" that would fail the 0x01FFFFFF
            // plausibility check even when decryption is correct.

            // Skip any FE?????? line
            if ( (w1 & 0xFF000000u) == 0xFE000000u )
                return true;

            // Skip 0x000FFFFE / 0x000FFFFF "register" lines
            uint a = w1 & 0x0FFFFFFFu;
            return a == 0x000FFFFEu || a == 0x000FFFFFu;
        }

        // Applies only to RAW-family outputs (RAW/GRAW/CRAW). Returns true if we validated and passed.
        public static RawPlausibilityResult ValidateFirstCheckableAddress(cheat_t cheat)
        => ValidateFirstCheckableAddress(cheat, ConvertCore.Crypt.CRYPT_RAW, AnalyzeMode.Analyze);

        // Applies only to RAW-family outputs (RAW/GRAW/CRAW). Returns validity of the first checkable line.
        // When outputCrypt==CRYPT_CRAW and mode is Analyze/AnalyzeFix, we enable extra strict checks specific to CRAW output.
        public static RawPlausibilityResult ValidateFirstCheckableAddress(cheat_t cheat, ConvertCore.Crypt outputCrypt, AnalyzeMode mode)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));

            if (cheat.codecnt <= 0 || cheat.code == null || cheat.code.Count == 0)
            {
                return new RawPlausibilityResult
                {
                    IsValid = false,
                    CheckedWord = 0,
                    CheckedAddr28 = 0,
                    Rule = "Empty",
                    FailReason = "no code words"
                };
            }

            // Basic structural sanity: codecnt should be even (pairs of words).
            if ((cheat.codecnt & 1) != 0)
            {
                return new RawPlausibilityResult
                {
                    IsValid = false,
                    CheckedWord = 0,
                    CheckedAddr28 = 0,
                    Rule = "OddCodeCount",
                    FailReason = "codecnt is odd"
                };
            }

            int lineCount = cheat.codecnt / 2;

            // Scan forward to find the first "checkable" line, skipping known CB7 master/enabler prologue
            // lines (FE****** + 000FFFFE/000FFFFF). We validate only ONE line per cheat.
            bool skippedCb7Prologue = false;

            bool strictCrawChecks = (mode != AnalyzeMode.None) && (outputCrypt == ConvertCore.Crypt.CRYPT_CRAW);
            for (int i = 0; i + 1 < cheat.codecnt; i += 2)
            {
                uint w1 = cheat.code[i];
                uint w2 = cheat.code[i + 1];

                if (IsCb7MasterPrologueLine(w1))
                {
                    skippedCb7Prologue = true;
                    continue;
                }

                int li = i / 2;
                bool isLastLine = (li == lineCount - 1);

                uint typeNibble = (w1 >> 28) & 0xFu;
                uint w1hi16 = w1 & 0xFFFF0000u;

                // --- One-line-only plausibility rules (applied to the FIRST checkable line only) ---
// Extra strict checks for CRAW output during analysis (-a / -af with CRYPT_CRAW output).
if (strictCrawChecks)
{
    // C-type is technically valid, but it's risky/ambiguous; force manual review.
    if (typeNibble == 0xCu)
    {
        return new RawPlausibilityResult
        {
            IsValid = false,
            CheckedWord = w1,
            CheckedAddr28 = w1 & 0x0FFFFFFFu,
            FailReason = "C-type output should be manually reviewed for CRYPT_CRAW analysis",
            Rule = "TypeCNeedsDoubleCheck",
        };
    }

    // 6-type pointer-write structural / width / alignment checks (based on RAW type tables).
    if (typeNibble == 0x6u)
    {
        if (isLastLine)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w1,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type requires a following 000Gnnnn/offset line",
                Rule = "Type6MissingSecondLine",
            };
        }

        uint w3 = cheat.code[i + 2];
        uint w4 = cheat.code[i + 3];

        uint w3hi16 = w3 & 0xFFFF0000u;
        uint g = (w3hi16 >> 16) & 0xFu;
        uint n = w3 & 0xFFFFu;

        // Expect: 0000nnnn / 0001nnnn / 0002nnnn
        if (!(w3hi16 == 0x00000000u || w3hi16 == 0x00010000u || w3hi16 == 0x00020000u))
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w3,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type second-line header is not 000Gnnnn (G must be 0,1,2)",
                Rule = "Type6BadHeader",
            };
        }

        // Address alignment heuristics based on G (8/16/32-bit write size).
        if (g == 1u && (w1 & 1u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w1,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type (16-bit) address must be 2-byte aligned",
                Rule = "Type6Unaligned16",
            };
        }
        if (g == 2u && (w1 & 3u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w1,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type (32-bit) address must be 4-byte aligned",
                Rule = "Type6Unaligned32",
            };
        }

        // Value width heuristics based on G.
        if (g == 0u && (w2 & 0xFFFFFF00u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w2,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type (8-bit) value must fit in 0x00..0xFF (upper 24 bits must be 0)",
                Rule = "Type6ValueTooWide8",
            };
        }
        if (g == 1u && (w2 & 0xFFFF0000u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w2,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type (16-bit) value must fit in 0x0000..0xFFFF (upper 16 bits must be 0)",
                Rule = "Type6ValueTooWide16",
            };
        }

        // Offset sanity (keep it within 25-bit range for CRAW analysis).
        if (w4 > 0x01FFFFFFu)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w4,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type offset is outside 25-bit range (0x0..0x1FFFFFF)",
                Rule = "Type6OffsetOutOfRange25",
            };
        }

        // Pointer-offset count sanity: for n<=1, only p0 is needed; for n>1, expect p0..pn (n+1 offsets).
        int requiredOffsets = (n <= 1u) ? 1 : checked((int)n + 1);
        int availableOffsets = cheat.codecnt - (i + 3); // words from p0 onward (w4 included)
        if (availableOffsets < requiredOffsets)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w3,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "6-type pointer offsets appear truncated for the declared dereference count",
                Rule = "Type6PointerOffsetsTruncated",
            };
        }
    }

    // 7-type bitwise (8/16-bit) checks based on op-encoding in the value word.
    if (typeNibble == 0x7u)
    {
        uint hi16 = w2 & 0xFFFF0000u;

        bool is8 =
            hi16 == 0x00000000u || // OR  8-bit: 000000vv
            hi16 == 0x00200000u || // AND 8-bit: 002000vv
            hi16 == 0x00400000u;   // XOR 8-bit: 004000vv

        bool is16 =
            hi16 == 0x00100000u || // OR  16-bit: 0010vvvv
            hi16 == 0x00300000u || // AND 16-bit: 0030vvvv
            hi16 == 0x00500000u;   // XOR 16-bit: 0050vvvv

        if (!is8 && !is16)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w2,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "7-type value word does not match expected 8/16-bit op encoding",
                Rule = "Type7BadOpEncoding",
            };
        }

        // For 8-bit ops, vv must live in the lowest byte (the middle byte must be 0).
        if (is8 && (w2 & 0x0000FF00u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w2,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "7-type (8-bit) value must be 0x00..0xFF (byte-only)",
                Rule = "Type7Bad8BitValue",
            };
        }

        // For 16-bit ops, enforce 2-byte alignment on the address.
        if (is16 && (w1 & 1u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w1,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "7-type (16-bit) address must be 2-byte aligned",
                Rule = "Type7Unaligned16",
            };
        }
    }

    // 3-type (30G0xxxx): requires a following "XXXXXXXX 00000000" line and value width based on G.
    //   G=0/1 => 8-bit  (XXXXXXXX must fit 0x00..0xFF)
    //   G=2/3 => 16-bit (XXXXXXXX must fit 0x0000..0xFFFF)
    if (w1hi16 == 0x30000000u || w1hi16 == 0x30100000u || w1hi16 == 0x30200000u || w1hi16 == 0x30300000u)
    {
        if (isLastLine)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w1,
                CheckedAddr28 = w2 & 0x0FFFFFFFu,
                FailReason = "3-type requires a following value line (XXXXXXXX 00000000)",
                Rule = "Type3MissingSecondLine",
            };
        }

        uint w3 = cheat.code[i + 2];
        uint w4 = cheat.code[i + 3];
        uint g3 = (w1hi16 >> 20) & 0xFu; // 0..3

        if (w4 != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w4,
                CheckedAddr28 = w2 & 0x0FFFFFFFu,
                FailReason = "3-type second line must end with 00000000",
                Rule = "Type3SecondLineMustEndWith0",
            };
        }

        if ((g3 == 0u || g3 == 1u) && (w3 & 0xFFFFFF00u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w3,
                CheckedAddr28 = w2 & 0x0FFFFFFFu,
                FailReason = "3-type (8-bit) value must fit in 0x00..0xFF (upper 24 bits must be 0)",
                Rule = "Type3ValueTooWide8",
            };
        }

        if ((g3 == 2u || g3 == 3u) && (w3 & 0xFFFF0000u) != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w3,
                CheckedAddr28 = w2 & 0x0FFFFFFFu,
                FailReason = "3-type (16-bit) value must fit in 0x0000..0xFFFF (upper 16 bits must be 0)",
                Rule = "Type3ValueTooWide16",
            };
        }
    }

    // 4/5-type: for CRAW analysis enforce the trailing word on the second line is 00000000.
    if (typeNibble == 0x4u || typeNibble == 0x5u)
    {
        if (isLastLine)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w1,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "Type 4/5 requires a following line",
                Rule = (typeNibble == 0x4u) ? "Type4MissingSecondLine" : "Type5MissingSecondLine",
            };
        }

        uint w3 = cheat.code[i + 2];
        uint w4 = cheat.code[i + 3];

        if (w4 != 0u)
        {
            return new RawPlausibilityResult
            {
                IsValid = false,
                CheckedWord = w4,
                CheckedAddr28 = w1 & 0x0FFFFFFFu,
                FailReason = "Type 4/5 second line must end with 00000000",
                Rule = (typeNibble == 0x4u) ? "Type4SecondLineMustEndWith0" : "Type5SecondLineMustEndWith0",
            };
        }

        // Additional 5-type structural rule: second line is an address word that must start with 0 and be within 0x0..0x01FFFFFF.
        if (typeNibble == 0x5u)
        {
            if ( (w3 & 0xF0000000u) != 0u )
            {
                return new RawPlausibilityResult
                {
                    IsValid = false,
                    CheckedWord = w3,
                    CheckedAddr28 = w3 & 0x0FFFFFFFu,
                    FailReason = "5-type second-line address must start with 0 (0iiiiiii)",
                    Rule = "Type5SecondLineAddrMustStartWith0",
                };
            }

            if ( (w3 & 0x0FFFFFFFu) > 0x01FFFFFFu )
            {
                return new RawPlausibilityResult
                {
                    IsValid = false,
                    CheckedWord = w3,
                    CheckedAddr28 = w3 & 0x0FFFFFFFu,
                    FailReason = "5-type second-line address must be within 0x0..0x01FFFFFF",
                    Rule = "Type5SecondLineAddrOutOfRange25",
                };
            }
        }
    }

}


                // No B-type exists in RAW-family outputs.
                if (typeNibble == 0xBu)
                {
                    return new RawPlausibilityResult
                    {
                        IsValid = false,
                        CheckedWord = w1,
                        CheckedAddr28 = w1 & 0x0FFFFFFFu,
                        Rule = "TypeBNotAllowed",
                        FailReason = "B-type is not a valid RAW-family code type",
                        Note = skippedCb7Prologue ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : null
                    };
                }


// 1-type: address must be 2-byte aligned (last digit 0,2,4,6,8,A,C,E)
if (typeNibble == 0x1u)
{
    // NOTE: can't name this 'addr28' because we declare 'addr28' later in this scope
    // when validating the first checkable address word.
    uint w1Addr28 = w1 & 0x0FFFFFFFu;
    if ((w1Addr28 & 0x1u) != 0)
    {
        return new RawPlausibilityResult
        {
            IsValid = false,
            CheckedWord = w1,
            CheckedAddr28 = w1Addr28,
            Rule = "Type1AddrMisaligned",
            FailReason = "Type 1 address must be 2-byte aligned (last digit 0,2,4,6,8,A,C,E)",
            Note = skippedCb7Prologue ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : null
        };
    }
}

// 2-type: address must be 4-byte aligned (last digit 0,4,8,C)
if (typeNibble == 0x2u)
{
    uint w1Addr28 = w1 & 0x0FFFFFFFu;
    if ((w1Addr28 & 0x3u) != 0)
    {
        return new RawPlausibilityResult
        {
            IsValid = false,
            CheckedWord = w1,
            CheckedAddr28 = w1Addr28,
            Rule = "Type2AddrMisaligned",
            FailReason = "Type 2 address must be 4-byte aligned (last digit 0,4,8,C)",
            Note = skippedCb7Prologue ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : null
        };
    }
}

                // 0-type: value must fit 8-bit (000000VV)
                if (typeNibble == 0x0u && (w2 & 0xFFFFFF00u) != 0)
                {
                    return new RawPlausibilityResult
                    {
                        IsValid = false,
                        CheckedWord = w1,
                        CheckedAddr28 = w1 & 0x0FFFFFFFu,
                        Rule = "Type0ValueTooWide",
                        FailReason = "Type 0 value must fit 8 bits (000000VV)",
                        Note = skippedCb7Prologue ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : null
                    };
                }

                // 1-type: value must fit 16-bit (0000VVVV)
                if (typeNibble == 0x1u && (w2 & 0xFFFF0000u) != 0)
                {
                    return new RawPlausibilityResult
                    {
                        IsValid = false,
                        CheckedWord = w1,
                        CheckedAddr28 = w1 & 0x0FFFFFFFu,
                        Rule = "Type1ValueTooWide",
                        FailReason = "Type 1 value must fit 16 bits (0000VVVV)",
                        Note = skippedCb7Prologue ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : null
                    };
                }

                // D/E/4/3040/3050 need a following line; if they are the last line, fail.
                if (isLastLine)
                {
                    if (typeNibble == 0x4u)
                    {
                        return new RawPlausibilityResult
                        {
                            IsValid = false,
                            CheckedWord = w1,
                            CheckedAddr28 = w1 & 0x0FFFFFFFu,
                            Rule = "Type4MissingSecondLine",
                            FailReason = "Type 4 requires a following line",
                        };
                    }

                    if (typeNibble == 0xDu || typeNibble == 0xEu)
                    {
                        return new RawPlausibilityResult
                        {
                            IsValid = false,
                            CheckedWord = w1,
                            CheckedAddr28 = w1 & 0x0FFFFFFFu,
                            Rule = "TypeDOrEMissingSecondLine",
                            FailReason = "Type D/E requires a following line",
                        };
                    }

                    if (w1hi16 == 0x30400000u || w1hi16 == 0x30500000u)
                    {
                        return new RawPlausibilityResult
                        {
                            IsValid = false,
                            CheckedWord = w1,
                            CheckedAddr28 = w1 & 0x0FFFFFFFu,
                            Rule = "3040Or3050MissingSecondLine",
                            FailReason = "3040/3050 requires a following line",
                        };
                    }
                }

                // Determine which word encodes the absolute address for this line.
                uint addressWord = w1;
                string rule = "w1.addr28";

                // 3-type family: address lives in w2 (target address).
                if (w1hi16 == 0x30000000u || w1hi16 == 0x30100000u ||
                    w1hi16 == 0x30200000u || w1hi16 == 0x30300000u)
                {
                    addressWord = w2;
                    rule = "3-type uses w2.addr28";
                }
                // 3040/3050 (INC32/DEC32): address lives in w2.
                else if (w1hi16 == 0x30400000u || w1hi16 == 0x30500000u)
                {
                    addressWord = w2;
                    rule = "3040/3050 uses w2.addr28";
                }
                // E-type family: address lives in w2 (0/1/2/3XXXXXXXX form).
                else if (typeNibble == 0xEu)
                {
                    addressWord = w2;
                    rule = "E-type uses w2.addr28";
                }
                // else: default to w1 low-28 bits (includes 0/1/2/4/5/6/7/9/A/F/etc).

                uint addr28 = addressWord & 0x0FFFFFFFu;
                bool ok = addr28 <= 0x01FFFFFFu;

                return new RawPlausibilityResult
                {
                    IsValid = ok,
                    CheckedWord = addressWord,
                    CheckedAddr28 = addr28,
                    Rule = rule,
                    FailReason = ok ? null : "addr28 > 0x01FFFFFF",
                    Note = skippedCb7Prologue ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : null
                };
            }

            // Only prologue / non-checkable lines (ex: master code only). Don't fail the cheat.
            return new RawPlausibilityResult
            {
                IsValid = true,
                CheckedWord = 0,
                CheckedAddr28 = 0,
                Rule = "NoCheckableLine",
                FailReason = null,
                Note = skippedCb7Prologue ? "Skipped CB7 master prologue (FE/FFFFFE/FFFFF)" : null
            };
        }
    }
}
