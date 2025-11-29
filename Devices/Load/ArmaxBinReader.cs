using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace OmniconvertCS
{
    /// <summary>
    /// Reader for AR MAX .bin codelist files (PS2_CODELIST layout).
    /// Mirrors the layout used by ArmaxBinWriter.
    /// </summary>
    internal static class ArmaxBinReader
    {
        private const string LIST_IDENT = "PS2_CODELIST";
        private const int    LIST_HEADER_SIZE = 36; // 12-byte ident + 6 * 4-byte fields

  
        public sealed class Result
        {
            public string GameName { get; set; } = string.Empty;
            public uint   GameId   { get; set; }
            public List<cheat_t> Cheats { get; } = new List<cheat_t>();
        }
    // EXISTING: public static Result Load(string fileName) { ... }

        public sealed class TextResult
        {
            /// <summary>Total number of games reported in the AR MAX list header.</summary>
            public uint GameCount { get; set; }

            /// <summary>
            /// If GameCount == 1, this is that game's name. Otherwise it is left empty.
            /// </summary>
            public string GameName { get; set; } = string.Empty;

            /// <summary>Final text lines to show in the input window.</summary>
            public List<string> Lines { get; } = new List<string>();
        }

        // Minimum size of an encoded cheat entry in bytes.
        // Used for sanity checks when trying to distinguish between slightly
        // different per-game header layouts in some official lists.
        private const int MIN_CHEAT_BYTES =
            4   // cheat id
          + 2   // at least one wchar of name
          + 2   // name terminator
          + 2   // comment terminator (can be empty)
          + 3   // flag bytes (default/mcode/comments)
          + 4   // wordCount field
          + 4;  // at least one 32-bit code word

        private static bool IsReasonableCheatCount(uint cheatCount, int cheatDataPos, int end)
        {
            // Zero cheats is allowed.
            if (cheatCount == 0)
                return true;

            // Hard safety upper bound – no real list will have this many cheats per game.
            if (cheatCount > 10000)
                return false;

            long minBytesNeeded = (long)cheatCount * MIN_CHEAT_BYTES;
            return cheatDataPos >= 0 &&
                   cheatDataPos <= end &&
                   cheatDataPos + minBytesNeeded <= end;
        }

        /// <summary>
        /// After we've read gameId and gameName, AR MAX uses at least two
        /// slightly different layouts for the rest of the game header.
        ///
        /// Layout A (what ArmaxBinWriter emits):
        ///   [extra:ushort][cheatCount:uint]
        ///
        /// Layout B (seen in some official lists):
        ///   [warning:wchar*][cheatCount:uint][ignored:uint]
        ///
        /// On success this method advances <paramref name="pos"/> so that it
        /// points at the first cheat entry for this game.
        /// </summary>
        private static bool TryReadGameCheatHeader(
            byte[] buf,
            ref int pos,
            int end,
            out ushort extra,
            out uint cheatCount,
            out int cheatDataPos)
        {
            int afterNamePos = pos;

            // -----------------------
            // Try layout A first.
            // -----------------------
            if (afterNamePos + 2 + 4 <= end)
            {
                int tmpPos = afterNamePos;

                ushort extraA = ReadUInt16LE(buf, ref tmpPos);
                uint cheatCountA = ReadUInt32LE(buf, ref tmpPos);

                if (IsReasonableCheatCount(cheatCountA, tmpPos, end))
                {
                    extra = extraA;
                    cheatCount = cheatCountA;
                    cheatDataPos = tmpPos;
                    pos = tmpPos;
                    return true;
                }
            }

            // -----------------------
            // Layout B: warning string, then cheatCount and an ignored uint.
            // -----------------------
            int commentPos = afterNamePos;
            string warning;

            try
            {
                warning = ReadWideStringZeroTerminated(buf, ref commentPos);
            }
            catch (InvalidDataException)
            {
                extra = 0;
                cheatCount = 0;
                cheatDataPos = afterNamePos;
                pos = afterNamePos;
                return false;
            }

            if (commentPos + 4 > end)
            {
                extra = 0;
                cheatCount = 0;
                cheatDataPos = afterNamePos;
                pos = afterNamePos;
                return false;
            }

            uint cheatCountB = ReadUInt32LE(buf, ref commentPos);

            // Many lists store an extra uint32 after the count; we just skip it.
            if (commentPos + 4 <= end)
            {
                ReadUInt32LE(buf, ref commentPos); // ignored
            }

            if (IsReasonableCheatCount(cheatCountB, commentPos, end))
            {
                extra = 0;
                cheatCount = cheatCountB;
                cheatDataPos = commentPos;
                pos = commentPos;
                return true;
            }

            extra = 0;
            cheatCount = 0;
            cheatDataPos = afterNamePos;
            pos = afterNamePos;
            return false;
        }

        /// <summary>
        /// Load an AR MAX .bin file and return text lines ready to drop into
        /// the input window. Codes are formatted as ARMAX encrypted lines
        /// (XXXXX-XXXXX-XXXXX).
        ///
        /// If there is only a single game in the list, its name is returned
        /// via TextResult.GameName and NOT printed in the text.
        ///
        /// If there are multiple games, each section is prefixed with
        /// "Game Name: &lt;name&gt;".
        /// </summary>
        public static TextResult LoadAsArmaxEncryptedTextWithGames(string fileName)
        {
            if (fileName == null) throw new ArgumentNullException(nameof(fileName));

            byte[] buf = File.ReadAllBytes(fileName);
            if (buf.Length < LIST_HEADER_SIZE)
                throw new InvalidDataException("File is too small to be a valid AR MAX list.");

            int offset = 0;

            // ident[12]
            string ident = Encoding.ASCII.GetString(buf, 0, 12).TrimEnd('\0');
            if (!string.Equals(ident, LIST_IDENT, StringComparison.Ordinal))
                throw new InvalidDataException("File does not start with PS2_CODELIST header.");

            offset = 12;

            // list_t header (6 x uint32 after ident)
            uint version = ReadUInt32LE(buf, ref offset);
            uint unknown = ReadUInt32LE(buf, ref offset);
            uint size    = ReadUInt32LE(buf, ref offset);
            uint crc     = ReadUInt32LE(buf, ref offset);
            uint gamecnt = ReadUInt32LE(buf, ref offset);
            uint codecnt = ReadUInt32LE(buf, ref offset);

            if (gamecnt == 0)
                throw new InvalidDataException("AR MAX list reports zero games.");

            int pos = LIST_HEADER_SIZE;
            int end = buf.Length;

            var result = new TextResult();
            result.GameCount = gamecnt;

            bool includeGameHeadings = (gamecnt > 1);

            for (uint g = 0; g < gamecnt; g++)
            {
                // game->id
                if (pos + 4 > end)
                    throw new InvalidDataException("Truncated game header (id).");

                uint gameId = ReadUInt32LE(buf, ref pos);

                // game->name (UTF-16LE, 0-terminated)
                string gameName = ReadWideStringZeroTerminated(buf, ref pos);

                ushort extra;
                uint cheatCount;
                int cheatDataPos;

                if (!TryReadGameCheatHeader(buf, ref pos, end, out extra, out cheatCount, out cheatDataPos))
                    throw new InvalidDataException("Unsupported or corrupt AR MAX game header.");

                // Single-game case: remember the name for the Game Name textbox.
                if (gamecnt == 1 && g == 0)
                {
                    result.GameName = gameName;
                }

                // Multi-game: show a heading in the text.
                if (includeGameHeadings)
                {
                    // Blank line between game sections (but not before the first).
                    if (result.Lines.Count > 0 && result.Lines[result.Lines.Count - 1].Length != 0)
                        result.Lines.Add(string.Empty);

                    result.Lines.Add("Game Name: " + gameName);
                }

                for (uint c = 0; c < cheatCount; c++)
                {
                    // cheat->id
                    if (pos + 4 > end)
                        throw new InvalidDataException("Truncated cheat entry (id).");

                    uint cheatId = ReadUInt32LE(buf, ref pos);

                    // cheat->name
                    string cheatName = ReadWideStringZeroTerminated(buf, ref pos);

                    // cheat->comment (can be empty string)
                    string comment = ReadWideStringZeroTerminated(buf, ref pos);

                    if (!string.IsNullOrWhiteSpace(cheatName))
                        result.Lines.Add(cheatName);

                    // Flags
                    if (pos + 3 > end)
                        throw new InvalidDataException("Truncated cheat entry (flags).");

                    byte flagDefaultOn = buf[pos++];
                    byte flagMcode     = buf[pos++];
                    byte flagComments  = buf[pos++];

                    // Code count
                    if (pos + 4 > end)
                        throw new InvalidDataException("Truncated cheat entry (code count).");

                    uint wordCount = ReadUInt32LE(buf, ref pos);
                    if (wordCount > int.MaxValue)
                        throw new InvalidDataException("Unreasonably large code count in AR MAX cheat.");

                    int needed = (int)wordCount * 4;
                    if (pos + needed > end)
                        throw new InvalidDataException("Truncated code data in AR MAX cheat.");

                    // Read all words for this cheat
                    var words = new List<uint>();
                    for (uint w = 0; w < wordCount; w++)
                    {
                        uint word = ReadUInt32LE(buf, ref pos);
                        words.Add(word);
                    }

                    // Format as ARMAX encrypted lines: two 32-bit words per line.
                    int pairCount = words.Count / 2;
                    for (int p = 0; p < pairCount; p++)
                    {
                        uint w0 = words[p * 2];
                        uint w1 = words[p * 2 + 1];

                        string code = Armax.FormatEncryptedLine(w0, w1);
                        result.Lines.Add(code);
                    }

                    // If we had an odd trailing word, it will be ignored here. That should
                    // not happen for normal ARMAX lists, but this keeps us from crashing
                    // on slightly malformed entries.

                    // Blank line between cheats
                    result.Lines.Add(string.Empty);
                }
            }

            // Trim trailing blank lines
            while (result.Lines.Count > 0 && result.Lines[result.Lines.Count - 1].Length == 0)
                result.Lines.RemoveAt(result.Lines.Count - 1);

            return result;
        }
    /// <summary>
    /// Convenience helper: load an AR MAX .bin and return the contents
    /// as ARMAX "ABC" encrypted lines (ready to drop into the textbox).
    ///
    /// Format:
    ///   CheatName
    ///   XXXXX-XXXXX-XXXXX
    ///   XXXXX-XXXXX-XXXXX
    ///   [blank line]
    ///   NextCheatName
    ///   ...
    /// </summary>
    public static List<string> LoadAsArmaxEncryptedLines(string fileName)
    {
        var result = Load(fileName);
        var lines  = new List<string>();

        foreach (var cheat in result.Cheats)
        {
            if (cheat == null)
                continue;

            // Cheat name
            if (!string.IsNullOrWhiteSpace(cheat.name))
                lines.Add(cheat.name);

            // Raw encrypted words from the .bin (2 words per ARMAX code)
            var words = cheat.code;
            if (words == null || words.Count == 0)
                continue;

            int wordCount = words.Count;

            // ARMAX codes are 2x 32-bit words; if something odd slipped in,
            // just ignore the last dangling word.
            if ((wordCount & 1) != 0)
                wordCount--;

            for (int i = 0; i < wordCount; i += 2)
            {
                uint w0 = words[i];
                uint w1 = words[i + 1];

                // This returns the dashed form: "8RQ5-D1B2-WAT38"
                string code = Armax.FormatEncryptedLine(w0, w1);
                lines.Add(code);
            }

            // Blank line between cheats for readability
            if (lines.Count > 0 && lines[lines.Count - 1].Length != 0)
                lines.Add(string.Empty);
        }

        return lines;
    }

        public static Result Load(string fileName)
        {
            if (fileName is null) throw new ArgumentNullException(nameof(fileName));

            byte[] buf = File.ReadAllBytes(fileName);
            if (buf.Length < LIST_HEADER_SIZE)
                throw new InvalidDataException("File is too small to be a valid AR MAX list.");

            int offset = 0;

            // ident[12]
            string ident = Encoding.ASCII.GetString(buf, 0, 12).TrimEnd('\0');
            if (!string.Equals(ident, LIST_IDENT, StringComparison.Ordinal))
                throw new InvalidDataException("File does not start with PS2_CODELIST header.");

            offset = 12;

            // version, unknown, size, crc, gamecnt, codecnt
            uint version = ReadUInt32LE(buf, ref offset);
            uint unknown = ReadUInt32LE(buf, ref offset);
            uint size    = ReadUInt32LE(buf, ref offset);
            uint crc     = ReadUInt32LE(buf, ref offset);
            uint gamecnt = ReadUInt32LE(buf, ref offset);
            uint codecnt = ReadUInt32LE(buf, ref offset);

            if (gamecnt == 0)
                throw new InvalidDataException("AR MAX list reports zero games.");

            // Payload starts immediately after the 36-byte list_t header.
            int payloadOffset = LIST_HEADER_SIZE;
            if (payloadOffset > buf.Length)
                throw new InvalidDataException("AR MAX payload offset is beyond end of file.");

            var result = new Result();

            int pos = payloadOffset;
            int end = buf.Length;

            for (uint g = 0; g < gamecnt; g++)
{
    uint gameId = ReadUInt32LE(buf, ref pos);
    string gameName = ReadWideStringZeroTerminated(buf, ref pos);

    // From this point on, AR MAX has at least two layouts for the game header.
    // Try our "simple" layout first (what ArmaxBinWriter emits). If that would
    // imply an impossible cheat count, fall back to the layout that contains
    // an extra warning string before the count.
    ushort extra;
    uint cheatCount;
    int cheatDataPos;

    if (!TryReadGameCheatHeader(buf, ref pos, end, out extra, out cheatCount, out cheatDataPos))
    {
        throw new InvalidDataException(
            $"Unsupported or corrupt AR MAX game header at index {g}.");
    }

                // We only surface the first game in the UI – same as writer behaviour.
                if (g == 0)
                {
                    result.GameName = gameName;
                    result.GameId   = gameId;
                }

                for (uint c = 0; c < cheatCount; c++)
                {
                    if (pos + 4 > end)
                        throw new InvalidDataException("Truncated cheat entry (id).");

                    uint cheatId = ReadUInt32LE(buf, ref pos);

                    string cheatName = ReadWideStringZeroTerminated(buf, ref pos);
                    string comment   = ReadWideStringZeroTerminated(buf, ref pos);

                    if (pos + 3 > end)
                        throw new InvalidDataException("Truncated cheat entry (flags).");

                    byte flagDefaultOn = buf[pos++];
                    byte flagMcode     = buf[pos++];
                    byte flagComments  = buf[pos++];

                    if (pos + 4 > end)
                        throw new InvalidDataException("Truncated cheat entry (code count).");

                    uint wordCount = ReadUInt32LE(buf, ref pos);
                    if (wordCount > int.MaxValue)
                        throw new InvalidDataException("Unreasonably large code count in AR MAX cheat.");

                    int needed = (int)wordCount * 4;
                    if (pos + needed > end)
                        throw new InvalidDataException("Truncated code data in AR MAX cheat.");

                    var cheat = new cheat_t
                    {
                        id       = cheatId,
                        name     = cheatName ?? string.Empty,
                        comment  = comment ?? string.Empty,
                        state    = 0 // enabled
                    };

                    // Preserve flags from file.
                    if (cheat.flag.Length >= 3)
                    {
                        cheat.flag[Cheat.FLAG_DEFAULT_ON] = flagDefaultOn;
                        cheat.flag[Cheat.FLAG_MCODE]      = flagMcode;
                        cheat.flag[Cheat.FLAG_COMMENTS]   = flagComments;
                    }

                    for (uint w = 0; w < wordCount; w++)
                    {
                        uint word = ReadUInt32LE(buf, ref pos);
                        cheat.code.Add(word);
                    }

                    result.Cheats.Add(cheat);
                }
            }

            return result;
        }

        private static uint ReadUInt32LE(byte[] buffer, ref int offset)
        {
            if (offset + 4 > buffer.Length)
                throw new InvalidDataException("Unexpected end of file while reading UInt32.");

            uint value =
                (uint)(buffer[offset]
                | (buffer[offset + 1] << 8)
                | (buffer[offset + 2] << 16)
                | (buffer[offset + 3] << 24));

            offset += 4;
            return value;
        }

        private static ushort ReadUInt16LE(byte[] buffer, ref int offset)
        {
            if (offset + 2 > buffer.Length)
                throw new InvalidDataException("Unexpected end of file while reading UInt16.");

            ushort value = (ushort)(buffer[offset] | (buffer[offset + 1] << 8));
            offset += 2;
            return value;
        }

        private static string ReadWideStringZeroTerminated(byte[] buffer, ref int offset)
        {
            int start = offset;
            int len   = buffer.Length;

            while (true)
            {
                if (offset + 2 > len)
                    throw new InvalidDataException("Unterminated UTF-16 string in AR MAX file.");

                ushort ch = (ushort)(buffer[offset] | (buffer[offset + 1] << 8));
                offset += 2;

                if (ch == 0)
                {
                    int byteCount = offset - start - 2;
                    if (byteCount <= 0)
                        return string.Empty;

                    return Encoding.Unicode.GetString(buffer, start, byteCount);
                }
            }
        }
    }
}
