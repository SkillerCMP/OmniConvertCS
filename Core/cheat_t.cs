using System;
using System.Collections.Generic;
using System.Globalization;

namespace OmniconvertCS
{
    /// <summary>
    /// Managed equivalent of the original cheat_t struct.
    /// Uses C# collections instead of manual malloc/realloc.
    /// </summary>
    public class cheat_t
    {
        public uint id;
        public string name = string.Empty;
        public string comment = string.Empty;

        // Flags: FLAG_DEFAULT_ON = 0, FLAG_MCODE = 1, FLAG_COMMENTS = 2
        public byte[] flag = new byte[3];

        // Dynamic list of 32‑bit code words (octets in the original code).
        public List<uint> code = new List<uint>();

        // For easier 1:1 porting of C routines that kept a separate
        // codecnt field, expose a read-only Count mirror.
        public int codecnt => code.Count;

        // Misc state (unused in the current port but kept for parity).
        public byte state;

        // Next pointer in the original linked list.
        public cheat_t? nxt;
    }

    /// <summary>
    /// Managed equivalent of game_t. Only the fields used by the original code are exposed.
    /// </summary>
    public class game_t
    {
        public uint id;
        public string name = string.Empty;
        public uint cnt;
    }
}
