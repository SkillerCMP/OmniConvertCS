using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {
        // --- ARMAX command categories / sub‑types ---

        private const byte ARM_WRITE            = 0;
        private const byte ARM_EQUAL            = 1;
        private const byte ARM_NOT_EQUAL        = 2;
        private const byte ARM_LESS_SIGNED      = 3;
        private const byte ARM_GREATER_SIGNED   = 4;
        private const byte ARM_LESS_UNSIGNED    = 5;
        private const byte ARM_GREATER_UNSIGNED = 6;
        private const byte ARM_AND              = 7;

        // "write" sub‑types
        private const byte ARM_DIRECT   = 0;
        private const byte ARM_POINTER  = 1;
        private const byte ARM_INCREMENT= 2;
        private const byte ARM_HOOK     = 3;

        // test sub‑types
        private const byte ARM_SKIP_1      = 0;
        private const byte ARM_SKIP_2      = 1;
        private const byte ARM_SKIP_N      = 2;
        private const byte ARM_SKIP_ALL_INV= 3;

        // --- ARMAX helper constants / masks ---

        private const uint ARM_CMD_SPECIAL    = 0;
        private const uint ARM_CMD_RESUME     = 0x40000000;
        private const int  ARM_MAX_INCREMENT  = 127;
        private const uint ARM_ENABLE_STD     = 0x0003FF00;
        private const uint ARM_ENABLE_INT     = 0x0002FF01;
        private const uint ARM_ENABLE_THREAD  = 0x0001FF01; // not normally used

        private const uint MASK_ADDR      = 0x01FFFFFF;
        private const uint MASK_LOHALF    = 0x0000FFFF;
        private const uint MASK_HIHALF    = 0xFFFF0000;
        private const uint MASK_LOBYTE32  = 0x000000FF;
        private const uint MASK_HIBYTE32  = 0xFF000000;
    }
}

