using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {
        private static uint ADDR(uint a)        => a & MASK_ADDR;
        private static uint LOHALF(uint a)      => a & MASK_LOHALF;
        private static uint HIHALF(uint a)      => (a >> 16) & MASK_LOHALF;
        private static uint LOBYTE32(uint a)    => a & MASK_LOBYTE32;
        private static uint HIBYTE32(uint a)    => (a >> 24) & MASK_LOBYTE32;

        private static byte  GET_STD_CMD(uint a)     => (byte)((a >> 28) & 0xF);
        private static byte  GET_ARM_CMD(uint a)     => (byte)((a >> 24) & 0xFE);
        private static byte  GET_ARM_TYPE(uint a)    => (byte)((a >> 27) & 7);
        private static byte  GET_ARM_SUBTYPE(uint a) => (byte)((a >> 30) & 3);
        private static byte  GET_ARM_SIZE(uint a)    => (byte)((a >> 25) & 3);

        private static uint MAKE_ARM_CMD(byte type, byte subtype, byte size)
            => (uint)((subtype << 30) | (type << 27) | (size << 25));

        private static uint MAKE_STD_CMD(byte cmd)
            => (uint)(cmd << 28);

        private static uint MAKEWORD32(uint val, byte size)
            => size == SIZE_BYTE
                   ? (val << 24) | (val << 16) | (val << 8) | val
                   : (val << 16) | val;

        private static uint MAKEHALF(uint val, byte size)
            => size == SIZE_BYTE ? ((val << 8) | val) & 0xFFFFu : val & 0xFFFFu;
    }
}

