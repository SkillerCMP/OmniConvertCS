using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace OmniconvertCS
{
    /// <summary>
    /// C# port of cbc.c / cbc.h (CodeBreaker .cbc Day 1 file writer).
    /// This implementation mirrors cbcCreateFile() as closely as possible.
    /// It can output both:
    ///   - v7 CBC (Day1): 64-byte title header + encrypted payload
    ///   - v8 CBC (CFU): 344-byte cbc_hdr_t + encrypted payload
    /// </summary>
    internal static class CbcWriter
    {
        private const int CB7_HDR_SIZE = 64;

        // cbc_hdr_t layout from cb2util's cbc.c:
        //   u32 fileid;      // "CFU",1 = 0x01554643 (little-endian on disk)
        //   u8  rsasig[256]; // banner/signature (any 256 bytes are accepted)
        //   u32 cbvers;      // e.g. 0x00000800 for CB 8.x
        //   char gametitle[72];
        //   u32 dataoff;     // offset of encrypted data section
        //   u32 zero;        // reserved
        //   u8  data[0];
        //
        // sizeof(cbc_hdr_t) = 4 + 256 + 4 + 72 + 4 + 4 = 344 bytes.
        private const int  CB8_HDR_SIZE    = 344;
        private const uint CBC_FILE_ID     = 0x01554643u;  // "CFU\x01"
        private const uint CBC_V8_VERSION  = 0x00000800u;  // matches cb2util's default cbvers

        // Simple 256-byte "signature" banner. CodeBreaker doesn't validate RSA here;
        // it mostly cares about the presence/size. We just repeat an ASCII message.
        private static readonly byte[] V8Signature = CreateV8Signature();

        /// <summary>
        /// 1:1 equivalent of:
        ///   int cbcCreateFile(cheat_t *cheat, game_t *game, char *szFileName, u8 doheadings);
        /// Returns 0 on success (and on I/O failure, which already shows a MsgBox),
        /// and 1 on validation/allocation errors, matching the original C behaviour.
        /// </summary>
        public static int CreateFile(List<cheat_t> cheats, game_t game, string fileName, bool doHeadings)
        {
            if (cheats == null) throw new ArgumentNullException(nameof(cheats));
            if (game == null) throw new ArgumentNullException(nameof(game));
            if (fileName == null) throw new ArgumentNullException(nameof(fileName));

            // u32 filesize = CB7_HDR_SIZE, datasize = 0, size, i;
            // u8 namemax = CB7_HDR_SIZE - 1; // assume the name can only be 63 chars
            // u8 name[CB7_HDR_SIZE], *buffer, *cbcdata, mcode = 0;
            // u16 numcheats = 0, numlines;
            uint datasize = 0;
            const int namemax = CB7_HDR_SIZE - 1; // 63
            byte mcode = 0;
            ushort numcheats = 0;

            // First cheat in the list – used for master-code checks.
            cheat_t? first = cheats.Count > 0 ? cheats[0] : null;

            // Header game name (for both v7 and v8).
            string rawGameName = game.name ?? string.Empty;
            int headerNameLen = rawGameName.Length;
            if (headerNameLen > namemax)
                headerNameLen = namemax;

            // Name as bytes (ASCII-ish) for payload & headers.
            var headerNameBytes = new byte[headerNameLen];
            for (int i = 0; i < headerNameLen; i++)
                headerNameBytes[i] = (byte)(rawGameName[i] & 0xFF);

            // Size of the game name for the list itself (unpadded):
            // datasize += strlen(name) + 1;
            datasize += (uint)(headerNameLen + 1);

            // Number of cheats in file (u16):
            // datasize += sizeof(u16);
            datasize += sizeof(ushort);

            // Total up size of data and check that the codes will make a valid file.
            for (int index = 0; index < cheats.Count; index++)
            {
                var cheat = cheats[index];
                if (cheat == null)
                    continue;

                if (ReferenceEquals(cheat, first))
                {
                    if (cheat.flag[Cheat.FLAG_MCODE] == 0)
                    {
                        // First cheat must be the master code.
                        break;
                    }
                    else
                    {
                        mcode++;
                    }
                }
                else if (cheat.flag[Cheat.FLAG_MCODE] != 0)
                {
                    // Any other master code counts as an additional one.
                    mcode++;
                    break;
                }

                if (cheat.state == 0 && (doHeadings || cheat.codecnt > 0))
                {
                    int size = cheat.name?.Length ?? 0;
                    if (size > namemax) size = namemax;

                    // strlen + term null + switch byte + line count (u16)
                    datasize += (uint)(size + 2 + sizeof(ushort));
                    // code words (u32 * codecnt)
                    datasize += (uint)(sizeof(uint) * cheat.codecnt);
                    numcheats++;
                }
            }

            // ERROR BLOCK (direct port of cbc.c)
            if (mcode == 0)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "A .cbc file cannot be... created without a master code as the first code in the list.");
                return 1;
            }

            if (mcode > 1)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "A .cbc file cannot be created without more than one master code in the list.");
                return 1;
            }

            if (datasize == 0 || numcheats < 1)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "After careful analysis, it seems no cheats are valid for CodeBreaker.");
                return 1;
            }

            // Decide header type (v7 vs v8) based on global setting.
            bool saveAsV8 = ConvertCore.g_cbcSaveVersion == 8;
            uint headerSize = saveAsV8 ? (uint)CB8_HDR_SIZE : (uint)CB7_HDR_SIZE;

            uint filesize = headerSize + datasize;
            byte[] cbcdata;
            try
            {
                cbcdata = new byte[checked((int)filesize)];
            }
            catch (OverflowException)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Unable to allocate output buffer for CBC file");
                return 1;
            }

            // Equivalent to:
            // memset(buffer, 0, filesize);
            Array.Clear(cbcdata, 0, cbcdata.Length);

            int offset = 0;

            if (!saveAsV8)
            {
                // ================================
                // v7 header: 64-byte title block
                // ================================
                var nameBlock = new byte[CB7_HDR_SIZE];
                Array.Clear(nameBlock, 0, nameBlock.Length);
                Array.Copy(headerNameBytes, 0, nameBlock, 0, headerNameBytes.Length);
                // name[63] is already 0 due to Clear.
                Array.Copy(nameBlock, 0, cbcdata, 0, CB7_HDR_SIZE);
                offset = CB7_HDR_SIZE;
            }
            else
            {
                // ================================
                // v8 header: cbc_hdr_t (CFU)
                // ================================
                // u32 fileid ("CFU",1)
                WriteUInt32LE(cbcdata, ref offset, CBC_FILE_ID);

                // u8 rsasig[256] – banner/signature
                Array.Copy(V8Signature, 0, cbcdata, offset, V8Signature.Length);
                offset += V8Signature.Length;

                // u32 cbvers
                WriteUInt32LE(cbcdata, ref offset, CBC_V8_VERSION);

                // char gametitle[72]
                var gameTitleBytes = new byte[72];
                int titleCopyLen = headerNameBytes.Length;
                if (titleCopyLen > 71) titleCopyLen = 71;
                if (titleCopyLen > 0)
                    Array.Copy(headerNameBytes, 0, gameTitleBytes, 0, titleCopyLen);
                // last byte left as 0 due to Array.Clear at top
                Array.Copy(gameTitleBytes, 0, cbcdata, offset, gameTitleBytes.Length);
                offset += gameTitleBytes.Length;

                // u32 dataoff – offset of data section
                WriteUInt32LE(cbcdata, ref offset, (uint)CB8_HDR_SIZE);

                // u32 zero (reserved)
                WriteUInt32LE(cbcdata, ref offset, 0u);

                // offset should now be CB8_HDR_SIZE
                // We rely on that but don't hard-assert to avoid runtime exceptions.
            }

            // Now the list data region (identical for v7 and v8):
            // strcpy(buffer, name);
            Array.Copy(headerNameBytes, 0, cbcdata, offset, headerNameBytes.Length);
            offset += headerNameBytes.Length;
            cbcdata[offset++] = 0; // terminating null

            // memcpy(buffer, &numcheats, sizeof(u16));
            cbcdata[offset++] = (byte)(numcheats & 0xFF);
            cbcdata[offset++] = (byte)((numcheats >> 8) & 0xFF);

            // Second pass: write cheats.
            for (int index = 0; index < cheats.Count; index++)
            {
                var cheat = cheats[index];
                if (cheat == null)
                    continue;

                if (cheat.state != 0)
                    continue;

                if (!doHeadings && cheat.codecnt == 0)
                    continue;

                // size = strlen(cheat->name); if(size > namemax) size = namemax;
                string cheatName = cheat.name ?? string.Empty;
                int nameLen = cheatName.Length;
                if (nameLen > namemax)
                    nameLen = namemax;

                // strncpy(buffer, cheat->name, size);
                for (int i = 0; i < nameLen; i++)
                    cbcdata[offset + i] = (byte)(cheatName[i] & 0xFF);

                // buffer[size] = '\0';
                offset += nameLen;
                cbcdata[offset++] = 0;

                // switch byte:
                // if(cheat->codecnt == 0) *buffer = 4;
                byte sw = 0;
                if (cheat.codecnt == 0)
                    sw = 4;
                cbcdata[offset++] = sw;

                // numlines = cheat->codecnt >> 1;
                ushort numlines = (ushort)(cheat.codecnt >> 1);
                cbcdata[offset++] = (byte)(numlines & 0xFF);
                cbcdata[offset++] = (byte)((numlines >> 8) & 0xFF);

                // for(i = 0; i < cheat->codecnt; i++) memcpy(buffer, &cheat->code[i], sizeof(u32));
                for (int i = 0; i < cheat.codecnt; i++)
                {
                    uint word = cheat.code[i];
                    cbcdata[offset++] = (byte)(word & 0xFF);
                    cbcdata[offset++] = (byte)((word >> 8) & 0xFF);
                    cbcdata[offset++] = (byte)((word >> 16) & 0xFF);
                    cbcdata[offset++] = (byte)((word >> 24) & 0xFF);
                }
            }

            // Encrypt the data region (payload only), matching:
            //   buffer = cbcdata + headerSize;
            //   CBCryptFileData(buffer, datasize);
            int dataSizeInt = checked((int)datasize);
            var payload = new byte[dataSizeInt];
            int headerSizeInt = saveAsV8 ? CB8_HDR_SIZE : CB7_HDR_SIZE;
            Array.Copy(cbcdata, headerSizeInt, payload, 0, dataSizeInt);
            Cb2Crypto.CBCryptFileData(payload, dataSizeInt);
            Array.Copy(payload, 0, cbcdata, headerSizeInt, dataSizeInt);

            try
            {
                File.WriteAllBytes(fileName, cbcdata);
            }
            catch (IOException)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Could not open cbc file \"{0}\"", fileName);
                // Original returns FALSE (0) here; we mirror that.
                return 0;
            }

            return 0;
        }

        // ================================================================
        // Helpers
        // ================================================================
        private static void WriteUInt32LE(byte[] buffer, ref int offset, uint value)
        {
            buffer[offset++] = (byte)(value & 0xFF);
            buffer[offset++] = (byte)((value >> 8) & 0xFF);
            buffer[offset++] = (byte)((value >> 16) & 0xFF);
            buffer[offset++] = (byte)((value >> 24) & 0xFF);
        }

        private static byte[] CreateV8Signature()
        {
            // Simple repeated ASCII banner, padded/truncated to 256 bytes.
            const int sigLen = 256;
            string banner = "Created with OmniconvertCS CBC v8 ";
            byte[] bannerBytes = Encoding.ASCII.GetBytes(banner);
            var sig = new byte[sigLen];

            for (int i = 0; i < sigLen; i++)
            {
                sig[i] = bannerBytes[i % bannerBytes.Length];
            }

            return sig;
        }
    }
}
