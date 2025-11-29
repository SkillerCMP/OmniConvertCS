using System;
using System.Collections.Generic;
using System.Globalization;

namespace OmniconvertCS
{
    /// <summary>
        /// Helpers corresponding to cheat.c. This is a partial port focused on the
        /// container behaviour needed by the AR2 routines.
        /// </summary>
        public static class Cheat
        {
            public const int NAME_MAX_SIZE = 50;
            public const int CODE_INVALID  = (1 << 0);
            public const int CODE_NO_TARGET = (1 << 1);

            public const int FLAG_DEFAULT_ON = 0;
            public const int FLAG_MCODE      = 1;
            public const int FLAG_COMMENTS   = 2;
		
            private static uint g_folderId;
			
			// AR MAX header flags (must match armax.c / armax.h)
			private const uint ARMAX_FLAGS_FOLDER        = 0x5u << 20;
			private const uint ARMAX_FLAGS_FOLDER_MEMBER = 0x4u << 20;
			private const uint ARMAX_FLAGS_DISC_HASH     = 0x7u << 20;

            public static void cheatClearFolderId()
            {
                g_folderId = 0;
            }

            public static cheat_t cheatInit()
            {
                // The C version allocates and zero‑initializes a cheat_t.
                // Here we rely on C#'s default field initialization.
                return new cheat_t();
            }

            public static void cheatAppendOctet(cheat_t cheat, uint octet)
            {
                if (cheat is null) throw new ArgumentNullException(nameof(cheat));
                cheat.code.Add(octet);
            }

            public static void cheatAppendCodeFromText(cheat_t cheat, string addr, string val)
            {
                if (cheat is null) throw new ArgumentNullException(nameof(cheat));
                if (addr is null) throw new ArgumentNullException(nameof(addr));
                if (val is null) throw new ArgumentNullException(nameof(val));

                uint code = uint.Parse(addr, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
                cheat.code.Add(code);
                code = uint.Parse(val, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
                cheat.code.Add(code);
            }

            public static void cheatPrependOctet(cheat_t cheat, uint octet)
            {
                if (cheat is null) throw new ArgumentNullException(nameof(cheat));
                cheat.code.Insert(0, octet);
            }

            /// <summary>
/// 1:1 port of cheatRemoveOctets() from cheat.c.
/// Removes <paramref name="count"/> octets starting at <paramref name="start"/>,
/// using the same slightly odd "start-- then copy from the tail" behavior
/// as the original C implementation.
/// </summary>
public static void cheatRemoveOctets(cheat_t cheat, int start, int count)
{
    if (cheat is null) throw new ArgumentNullException(nameof(cheat));

    int codecnt = cheat.code.Count;
    if (start >= codecnt || start < 0 || count < 1)
        return;

    // C: if (start + count > cheat->codecnt) { cheat->codecnt -= count; return; }
    if (start + count > codecnt)
    {
        int newCount = codecnt - count;
        if (newCount < 0) newCount = 0;
        // Drop the last (codecnt - newCount) entries
        if (newCount < codecnt)
            cheat.code.RemoveRange(newCount, codecnt - newCount);
        return;
    }

    int i = start + count - 1;
    int j = codecnt - i;

    // C does start-- then copies forward
    start--;

    while (j-- > 0)
    {
        cheat.code[start] = cheat.code[i];
        start++;
        i++;
    }

    // C: cheat->codecnt -= count;
    int finalCount = codecnt - count;
    if (finalCount < cheat.code.Count)
        cheat.code.RemoveRange(finalCount, cheat.code.Count - finalCount);
}
public static void cheatFinalizeData(
    cheat_t cheat,
    ConvertCore.Device device,
    uint discHash,
    bool makeFolders)
{
    if (cheat is null) throw new ArgumentNullException(nameof(cheat));
    if (cheat.state != 0) return;
    if (cheat.codecnt == 0) return;

    // --- AR MAX-specific bits: folders, folder members, disc hash ---
    if (device == ConvertCore.Device.DEV_ARMAX)
    {
        // cheat->id = ((cheat->code[0] & 0x7FFF) << 4) | ((cheat->code[1] >> 28) & 0xF);
        cheat.id = ((cheat.code[0] & 0x7FFFu) << 4)
                 | ((cheat.code[1] >> 28) & 0xFu);

        // If this is a folder header and folders are enabled, remember its ID.
        if ((cheat.code[1] & ARMAX_FLAGS_FOLDER) != 0 && makeFolders)
            g_folderId = cheat.id;

        // Any later cheat gets tagged as a member of the current folder.
        if (g_folderId > 0 && cheat.id != g_folderId)
        {
            Armax.ArmEnableExpansion(cheat);
            cheat.code[1] |= ARMAX_FLAGS_FOLDER_MEMBER | (g_folderId << 1) | 1u;
        }

        // M-code detection for AR MAX (used by disc hash).
        for (int i = 2; i + 1 < cheat.codecnt; i += 2)
        {
            if (cheat.code[i] >> 25 == 0x62)
            {
                cheat.flag[FLAG_MCODE] = 1;
                if (string.IsNullOrEmpty(cheat.name) && cheat.codecnt > 0)
                    cheat.name = "Enable Code (Must Be On)";
            }

            // Skip internal expansion codes (0 / 0x80000000..0x85FFFFFF)
            if (cheat.code[i] == 0 &&
                cheat.code[i + 1] >= 0x80000000u &&
                cheat.code[i + 1] <= 0x85FFFFFFu)
            {
                i += 2;
            }
        }

        // Disc hash (not active yet – discHash will be 0 for now)
        if (discHash > 0 && cheat.flag[FLAG_MCODE] != 0)
        {
            Armax.ArmEnableExpansion(cheat);
            cheat.code[1] |= ARMAX_FLAGS_DISC_HASH | (discHash >> 12);

            // Prepend the verifier twice (same as C's cheatPrependOctet dance)
            cheatPrependOctet(cheat, cheat.code[1]);
            cheatPrependOctet(cheat, cheat.code[1]);

            // Ensure at least 4 octets exist
            while (cheat.codecnt < 4)
                cheat.code.Add(0);

            cheat.code[2] = 0x00080000u | (discHash << 20);
            cheat.code[3] = 0;
        }
    }
    else
    {
        // Non-ARMAX master-code detection (useful but optional)
        for (int i = 0; i < cheat.codecnt; i += 2)
        {
            uint tmp = cheat.code[i] >> 28;

            // 0xF / 0x9 — (M) Enable types
            if (tmp == 0xFu || tmp == 0x9u)
            {
                cheat.flag[FLAG_MCODE] = 1;
                if (string.IsNullOrEmpty(cheat.name) && cheat.codecnt > 0)
                    cheat.name = "Enable Code (Must Be On)";
            }

            // AR1/AR2 master types
            if ((device == ConvertCore.Device.DEV_AR1 && tmp == 0x8u) ||
                (device == ConvertCore.Device.DEV_AR2 && tmp == 0x8u))
            {
                cheat.flag[FLAG_MCODE] = 1;
                if (string.IsNullOrEmpty(cheat.name) && cheat.codecnt > 0)
                    cheat.name = "Enable Code (Must Be On)";
                i += 2;
            }

            // Skip multi-line commands for scanning
            if (tmp >= 4 && tmp <= 6)
            {
                i += 2;
            }
            else if (tmp == 3)
            {
                uint tmp2 = (cheat.code[i] >> 20) & 0xFu;
                if (device == ConvertCore.Device.DEV_CB)
                    tmp2++;
                if (tmp2 > 4)
                    i += 2;
            }
        }
    }

    // Default name if still empty.
    if (string.IsNullOrEmpty(cheat.name))
        cheat.name = "Unnamed Cheat";
}

            /// <summary>
            /// Managed clean‑up. In the C version this frees all allocations;
            /// in C# we simply clear references.
            /// </summary>
            public static void cheatDestroy(cheat_t cheat)
            {
                if (cheat is null) return;

                cheat.code.Clear();
                cheat.name = string.Empty;
                cheat.comment = string.Empty;
                cheat.nxt = null;
            }
        }

}
