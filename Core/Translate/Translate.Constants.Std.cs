using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {
// --- Standard-format command IDs (upper nibble of the address) ---

        private const byte STD_WRITE_BYTE         = 0; // 8-bit write
        private const byte STD_WRITE_HALF         = 1; // 16-bit write
        private const byte STD_WRITE_WORD         = 2; // 32-bit write
        private const byte STD_INCREMENT_WRITE    = 3; // read-increment-write
        private const byte STD_WRITE_WORD_MULTI   = 4; // multi-address write
        private const byte STD_COPY_BYTES         = 5; // copy bytes
        private const byte STD_POINTER_WRITE      = 6; // pointer-based write
        private const byte STD_BITWISE_OPERATE    = 7; // bitwise op (CB7+ only)
        private const byte GS5_VERIFIER           = STD_BITWISE_OPERATE;
        private const byte STD_ML_TEST            = 8; // master-level test
        private const byte AR2_COND_HOOK          = STD_ML_TEST; // AR2 uses STD_ML_TEST as conditional hook
        private const byte STD_COND_HOOK          = 9; // conditional hook
        private const byte STD_ML_WRITE_WORD      = 0xA; // master-level write
        private const byte STD_TIMER              = 0xB; // timer / loop
        private const byte STD_TEST_ALL           = 0xC; // execute all remaining
        private const byte STD_TEST_SINGLE        = 0xD; // execute next code
        private const byte STD_TEST_MULTI         = 0xE; // execute next N codes
        private const byte STD_UNCOND_HOOK        = 0xF; // unconditional hook




        // --- Write / test sizes ---

        private const byte SIZE_BYTE    = 0;
        private const byte SIZE_HALF    = 1;
        private const byte SIZE_WORD    = 2;
        private const byte SIZE_SPECIAL = 3;

private static readonly byte[] ARSize   = { SIZE_BYTE, SIZE_BYTE, SIZE_BYTE, SIZE_HALF, SIZE_HALF, SIZE_WORD, SIZE_WORD };
        private static readonly byte[] STDSize  = { SIZE_BYTE, SIZE_BYTE, SIZE_HALF, SIZE_HALF, SIZE_WORD, SIZE_WORD, SIZE_WORD };
        private static readonly byte[] STDCompToArm = { ARM_EQUAL, ARM_NOT_EQUAL, ARM_LESS_UNSIGNED, ARM_GREATER_UNSIGNED, ARM_AND, ARM_AND };
        private static readonly uint[] valmask  = { 0xFFu, 0xFFFFu, 0xFFFFFFFFu };
    }
}

