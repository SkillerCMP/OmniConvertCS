using System;
using System.Collections.Generic;
using System.IO;

namespace OmniconvertCS
{
    /// <summary>
    /// C# port of armlist.c / armlist.h (ARMAX .bin list writer).
    /// This implementation mirrors alCreateList() as closely as possible.
    /// </summary>
    internal static class ArmaxBinWriter
    {
        private const string LIST_IDENT = "PS2_CODELIST";
        private const uint LIST_VERSION = 0xD00DB00B;
        private const int NAME_MAX_SIZE = Cheat.NAME_MAX_SIZE;
        private const int LIST_HEADER_SIZE = 36; // sizeof(list_t) in C

        /// <summary>
        /// 1:1 equivalent of alCreateList(cheat_t *cheat, game_t *game, char *szFileName).
        /// Returns 0 on success, non-zero on failure.
        /// </summary>
        public static int CreateList(List<cheat_t> cheats, game_t game, string fileName)
        {
            if (cheats is null) throw new ArgumentNullException(nameof(cheats));
            if (game is null) throw new ArgumentNullException(nameof(game));
            if (fileName is null) throw new ArgumentNullException(nameof(fileName));

            // if(strlen(game->name) < 1) strncpy(game->name, defname, NAME_MAX_SIZE);
            if (string.IsNullOrEmpty(game.name))
                game.name = "Unnamed Game";

            uint size = 0;
            uint tsize;
            uint namesize;
            uint commentsize;
            bool mcode = false;

            // First pass: determine sizes, truncate strings, set flags, and count valid cheats.
            game.cnt = 0;
            var validCheats = new List<cheat_t>();

            foreach (var cheat in cheats)
            {
                if (cheat is null)
                    continue;

                if (cheat.codecnt > 0 && cheat.state == 0)
                {
                    validCheats.Add(cheat);

                    tsize = 0;
                    tsize += 4u * 2u; // id + cnt

                    // --- Name sizing / truncation (mirrors strlen/NAME_MAX_SIZE logic) ---
                    string name = cheat.name ?? string.Empty;
                    if (name.Length > NAME_MAX_SIZE)
                        name = name.Substring(0, NAME_MAX_SIZE);
                    namesize = (uint)name.Length;
                    namesize++; // account for terminating 0
                    cheat.name = name;

                    // --- Comment trimming at CR and truncation ---
                    string comment = cheat.comment ?? string.Empty;
                    int crIndex = comment.IndexOf('\r');
                    if (crIndex >= 0)
                        comment = comment.Substring(0, crIndex);
                    if (comment.Length > NAME_MAX_SIZE)
                        comment = comment.Substring(0, NAME_MAX_SIZE);
                    commentsize = (uint)comment.Length;
                    commentsize++; // + terminator
                    cheat.comment = comment;

                    tsize += (commentsize + namesize) * 2u; // wchar_t (UTF-16) == 2 bytes
                    tsize += 3u;                             // 3 flag bytes
                    tsize += (uint)cheat.codecnt * 4u;       // code words

                    size += tsize;

                    if (cheat.flag[Cheat.FLAG_MCODE] != 0)
                        mcode = true;

                    if (commentsize != 0)
                        cheat.flag[Cheat.FLAG_COMMENTS] = 1;

                    game.cnt++;
                }
            }

            if (size == 0)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "After careful analysis, it seems no cheats are valid for AR MAX.");
                return 1;
            }

            if (!mcode)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "File cannot be created without a master code in the list.");
                return 1;
            }

            // --- Add game header size (id + cnt + game name + extra wchar) ---
            size += 4u * 2u; // id + cnt

            string gname = game.name ?? string.Empty;
            uint gameNameLen = (uint)Math.Min(gname.Length, NAME_MAX_SIZE);
            uint gameNameBytes = (gameNameLen + 1u) * 2u; // name + terminator
            size += gameNameBytes + 2u;                   // plus extra wchar

            // total bytes = header (list_t) + payload (size)
            uint totalSize = size + (uint)LIST_HEADER_SIZE;
            var fullbuf = new byte[totalSize];

            // --- Build payload (game + cheats) starting after the header ---
            int bufferOffset = LIST_HEADER_SIZE;

            // game->id
            WriteUInt32LE(fullbuf, bufferOffset, game.id);
            bufferOffset += 4;

            // game->name as UTF-16LE, truncated to NAME_MAX_SIZE
            if (gname.Length > NAME_MAX_SIZE)
                gname = gname.Substring(0, NAME_MAX_SIZE);

            for (int i = 0; i < gname.Length; i++)
            {
                WriteUInt16LE(fullbuf, bufferOffset, gname[i]);
                bufferOffset += 2;
            }

            // terminating wchar_t
            WriteUInt16LE(fullbuf, bufferOffset, '\0');
            bufferOffset += 2;

            // extra wchar_t
            WriteUInt16LE(fullbuf, bufferOffset, '\0');
            bufferOffset += 2;

            // game->cnt
            WriteUInt32LE(fullbuf, bufferOffset, game.cnt);
            bufferOffset += 4;

            // --- Cheat entries ---
            foreach (var cheat in validCheats)
            {
                // id
                WriteUInt32LE(fullbuf, bufferOffset, cheat.id);
                bufferOffset += 4;

                // name (UNICODE wide chars + terminator)
                string name = cheat.name ?? string.Empty;
                if (name.Length > NAME_MAX_SIZE)
                    name = name.Substring(0, NAME_MAX_SIZE);

                for (int i = 0; i < name.Length; i++)
                {
                    WriteUInt16LE(fullbuf, bufferOffset, name[i]);
                    bufferOffset += 2;
                }
                WriteUInt16LE(fullbuf, bufferOffset, '\0');
                bufferOffset += 2;

                // comment (UNICODE wide chars + terminator)
                string comment = cheat.comment ?? string.Empty;
                if (comment.Length > NAME_MAX_SIZE)
                    comment = comment.Substring(0, NAME_MAX_SIZE);

                for (int i = 0; i < comment.Length; i++)
                {
                    WriteUInt16LE(fullbuf, bufferOffset, comment[i]);
                    bufferOffset += 2;
                }
                WriteUInt16LE(fullbuf, bufferOffset, '\0');
                bufferOffset += 2;

                // 3 flag bytes
                fullbuf[bufferOffset++] = cheat.flag[Cheat.FLAG_DEFAULT_ON];
                fullbuf[bufferOffset++] = cheat.flag[Cheat.FLAG_MCODE];
                fullbuf[bufferOffset++] = cheat.flag[Cheat.FLAG_COMMENTS];

                // code count
                WriteUInt32LE(fullbuf, bufferOffset, (uint)cheat.codecnt);
                bufferOffset += 4;

                // raw code words
                foreach (uint word in cheat.code)
                {
                    WriteUInt32LE(fullbuf, bufferOffset, word);
                    bufferOffset += 4;
                }
            }

            // payloadSize == 'size' from the C code
            uint payloadSize = size;
            uint listSizeField = payloadSize + 4u * 2u; // list.size = size + 8

            // --- Write list_t header with crc placeholder ---
            int headerOffset = 0;

            // l_ident[12]
            var identBytes = System.Text.Encoding.ASCII.GetBytes(LIST_IDENT);
            int iIdent = 0;
            for (; iIdent < identBytes.Length && iIdent < 12; iIdent++)
                fullbuf[headerOffset + iIdent] = identBytes[iIdent];
            for (; iIdent < 12; iIdent++)
                fullbuf[headerOffset + iIdent] = 0;
            headerOffset += 12;

            // version
            WriteUInt32LE(fullbuf, headerOffset, LIST_VERSION);
            headerOffset += 4;

            // unknown
            WriteUInt32LE(fullbuf, headerOffset, 0);
            headerOffset += 4;

            // size (payload + 8 bytes for gamecnt+codecnt)
            WriteUInt32LE(fullbuf, headerOffset, listSizeField);
            headerOffset += 4;

            // crc placeholder (0)
            WriteUInt32LE(fullbuf, headerOffset, 0);
            int crcFieldOffset = headerOffset;
            headerOffset += 4;

            // gamecnt = 1 (there is always exactly one game in the list)
			WriteUInt32LE(fullbuf, headerOffset, 1u);
			headerOffset += 4;

            // codecnt (same as game->cnt in original)
            WriteUInt32LE(fullbuf, headerOffset, game.cnt);
            headerOffset += 4;

            // --- Compute CRC over [gamecnt .. gamecnt + list.size) ---
            uint initialCrc = 0xFFFFFFFFu;
            int crcStart = LIST_HEADER_SIZE - 4 * 2; // sizeof(list_t) - 8 -> &gamecnt
            uint crc = Crc32.Compute(
                new ReadOnlySpan<byte>(fullbuf, crcStart, (int)listSizeField),
                initialCrc);

            // Patch CRC field
            WriteUInt32LE(fullbuf, crcFieldOffset, crc);

            // --- Finally, write the buffer to disk ---
            try
            {
                File.WriteAllBytes(fileName, fullbuf);
            }
            catch (IOException)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Could not open MAX file \"{0}\"", fileName);
                return 0; // Matches FALSE return in C on IO failure
            }

            return 0;
        }

        private static void WriteUInt32LE(byte[] buffer, int offset, uint value)
        {
            buffer[offset + 0] = (byte)(value & 0xFF);
            buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
            buffer[offset + 2] = (byte)((value >> 16) & 0xFF);
            buffer[offset + 3] = (byte)((value >> 24) & 0xFF);
        }

        private static void WriteUInt16LE(byte[] buffer, int offset, int value)
        {
            unchecked
            {
                buffer[offset + 0] = (byte)(value & 0xFF);
                buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
            }
        }
    }
}
