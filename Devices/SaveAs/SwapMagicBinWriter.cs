using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace OmniconvertCS
{
    /// <summary>
    /// C# port of scf.c/scf.h – creates a Swap Magic Coder style file.
    /// This mirrors scfCreateFile(cheat_t*, game_t*, char*).
    /// </summary>
    internal static class SwapMagicWriter
    {
        // maximum size of the name in a Swap Magic Coder File
        private const int  SCF_NAME_MAX_SIZE = 100;
        // The buttheads "encrypt" the CRC
        private const uint SCF_CRC_MASK      = 0x5FA27ED6u;

        // t_scf_header is 5 x u32
        private const int HEADER_SIZE = 5 * 4;

        /// <summary>
        /// 1:1 equivalent of:
        ///   int scfCreateFile(cheat_t *cheat, game_t *game, char *szFileName);
        /// Returns 0 on success, 1 on validation/I/O errors.
        /// </summary>
        public static int CreateFile(List<cheat_t> cheats, game_t game, string fileName)
        {
            if (cheats == null) throw new ArgumentNullException(nameof(cheats));
            if (game   == null) throw new ArgumentNullException(nameof(game));
            if (fileName == null) throw new ArgumentNullException(nameof(fileName));

            // --- First pass: count master codes and collect active cheats ---
            byte mcodeCount = 0;
            var activeCheats = new List<cheat_t>();

            foreach (var cheat in cheats)
            {
                if (cheat is null)
                    continue;

                if (cheat.flag[Cheat.FLAG_MCODE] != 0)
                    mcodeCount++;

                // C: if (!cheat->state && cheat->codecnt > 0)
                if (cheat.state == 0 && cheat.codecnt > 0)
                    activeCheats.Add(cheat);
            }

            if (mcodeCount == 0)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "File cannot be created without a master code in the list.");
                return 1;
            }

            if (mcodeCount > 1)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "File cannot be created with more than one enable code");
                return 1;
            }

            if (activeCheats.Count == 0)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "After careful analysis, it seems no cheats are valid for Swap Magic Coder.");
                return 1;
            }

            // --- Build payload into memory: [header placeholder][game][cheats...] ---
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms, Encoding.ASCII, leaveOpen: true);

            // Reserve header space
            ms.Position = HEADER_SIZE;

            // ===== Game block (t_scf_game) =====
            string gameName = game.name ?? string.Empty;
            if (gameName.Length > SCF_NAME_MAX_SIZE)
                gameName = gameName.Substring(0, SCF_NAME_MAX_SIZE);

            byte[] gameNameBytes = Encoding.ASCII.GetBytes(gameName);

            // num_cheats (u16)
            bw.Write((ushort)activeCheats.Count);

            // name_len (u8) – includes terminator
            byte gameNameLen = (byte)(gameNameBytes.Length + 1);
            bw.Write(gameNameLen);

            // game name bytes + trailing 0
            bw.Write(gameNameBytes);
            bw.Write((byte)0);

            // ===== Cheats (t_scf_cheat array) =====
            uint totalLines = 0;

            foreach (var cheat in activeCheats)
            {
                // num_lines = codecnt >> 1
                ushort linesForThisCheat = (ushort)(cheat.codecnt >> 1);
                totalLines += (uint)linesForThisCheat;

                // num_lines (u16)
                bw.Write(linesForThisCheat);

                // cheat name, truncated like the C code
                string cheatName = cheat.name ?? string.Empty;
                if (cheatName.Length > SCF_NAME_MAX_SIZE)
                {
                    cheatName = cheatName.Substring(0, SCF_NAME_MAX_SIZE);
                    // C code also clamps the in-memory name
                    cheat.name = cheatName;
                }

                byte[] cheatNameBytes = Encoding.ASCII.GetBytes(cheatName);
                byte cheatNameLen = (byte)(cheatNameBytes.Length + 1);

                // name_len (u8)
                bw.Write(cheatNameLen);
                // name + terminator
                bw.Write(cheatNameBytes);
                bw.Write((byte)0);

                // code pairs: C loops for (i = 0; i < cheat->codecnt; i += 2)
                // and writes code[i], code[i+1]. Here we just dump all code words
                // in order as 32-bit little-endian.
                foreach (uint word in cheat.code)
                {
                    bw.Write(word);
                }
            }

            bw.Flush();

            // --- Compute header fields and CRC ---
            byte[] full = ms.ToArray();
            uint dataSize  = (uint)(full.Length - HEADER_SIZE);
            uint numGames  = 1;
            uint numCheats = (uint)activeCheats.Count;
            uint numLines  = totalLines;

            // CRC over payload using GS/AR variation: gs3GenCrc32()
            var payload = new byte[dataSize];
            Buffer.BlockCopy(full, HEADER_SIZE, payload, 0, (int)dataSize);

            uint crcData = Gs3.GenCrc32(payload, dataSize);
            uint crc32   = SCF_CRC_MASK ^ crcData;

            // --- Write t_scf_header at the start (little-endian u32s) ---
            ms.Position = 0;
            using (var headerWriter = new BinaryWriter(ms, Encoding.ASCII, leaveOpen: true))
            {
                headerWriter.Write(crc32);    // crc32
                headerWriter.Write(dataSize); // datasize
                headerWriter.Write(numGames); // num_games
                headerWriter.Write(numCheats);// num_cheats
                headerWriter.Write(numLines); // num_lines
            }

            full = ms.ToArray();

            // --- Flush to disk ---
            try
            {
                File.WriteAllBytes(fileName, full);
            }
            catch (Exception)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Could not open bin file \"{0}\"", fileName);
                return 1;
            }

            return 0;
        }
    }
}
