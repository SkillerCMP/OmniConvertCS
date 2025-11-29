using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace OmniconvertCS
{
    internal static class CbcReader
    {
        private const int CB7_HDR_SIZE = 64;

        // CBC v8+ "CFU" file ID ("CFU", 1) – bytes on disk: 43 46 55 01
        private const uint CBC_FILE_ID = 0x01554643u;

        public sealed class Result
        {
            public string GameName { get; init; } = string.Empty;
            public List<cheat_t> Cheats { get; } = new();
        }

        public static Result Load(string fileName)
        {
            if (fileName is null) throw new ArgumentNullException(nameof(fileName));

            byte[] buf = File.ReadAllBytes(fileName);
            if (buf.Length < CB7_HDR_SIZE)
                throw new InvalidDataException("CBC file is too small to be valid.");

            // Detect CBC v8+ ("CFU") via fileid at offset 0
            if (IsCbcV8(buf))
            {
                return LoadV8(buf);
            }

            // Fallback: CBC v7 (64-byte title header)
            return LoadV7(buf);
        }

        // ================================================================
        // v7: 64-byte gametitle header + encrypted payload
        // ================================================================

        private static Result LoadV7(byte[] fileData)
        {
            int buflen = fileData.Length;

            // Header is first 64 bytes
            int headerOffset = 0;
            string headerName = ReadAsciiZeroTerminated(fileData, ref headerOffset, CB7_HDR_SIZE);

            int payloadOffset = CB7_HDR_SIZE;
            int payloadLength = buflen - payloadOffset;
            if (payloadLength <= 0)
                throw new InvalidDataException("CBC v7 file has no payload data.");

            var payload = new byte[payloadLength];
            Array.Copy(fileData, payloadOffset, payload, 0, payloadLength);

            // Same crypto as writer: cb_crypt_data(hdr->data, datalen)
            Cb2Crypto.CBCryptFileData(payload, payloadLength);

            // First field in payload is another copy of the game name (ASCIIZ)
            int offset = 0;
            string listName = ReadAsciiZeroTerminated(payload, ref offset);
            if (string.IsNullOrEmpty(listName))
                listName = headerName;

            // Optional: sanity check like cb2util does:
            // if (listName != headerName) → "invalid CBC v7 header"
            // We'll be lenient and just trust listName here.

            if (offset + 2 > payloadLength)
                throw new InvalidDataException("CBC v7 file truncated before cheat count.");

            ushort numCheats = ReadUInt16LE(payload, ref offset);

            var result = new Result { GameName = listName };
            ParseCheatsFromPayload(result.Cheats, payload, offset, payloadLength, numCheats);
            return result;
        }

        // ================================================================
        // v8: "CFU" header + RSA + cbvers + 72-byte title + dataoff
        //     then encrypted payload just like v7
        // ================================================================

        private static Result LoadV8(byte[] fileData)
        {
            int buflen = fileData.Length;
            if (buflen < 344) // sizeof(cbc_hdr_t) in cb2util
                throw new InvalidDataException("CBC v8 file header is too small.");

            // Layout of cbc_hdr_t (little-endian):
            // 0x00: u32 fileid  ("CFU",1) = 0x01554643
            // 0x04: u8  rsasig[256]
            // 0x104: u32 cbvers
            // 0x108: char gametitle[72]
            // 0x150: u32 dataoff  (offset of data section)
            // 0x154: u32 zero
            // 0x158: u8  data[0]

            int offFileId = 0;
			uint fileId = ReadUInt32LE(fileData, ref offFileId);
			if (fileId != CBC_FILE_ID)
			throw new InvalidDataException("Not a CBC v8 file (invalid file ID).");

            // We don't need cbvers or zero; we only care about title and dataoff
            int gametitleOffset = 4 + 256 + 4;    // 0x108
            string headerName = ReadAsciiZeroTerminated(fileData, ref gametitleOffset, 72);

            // dataoff is at 4 + 256 + 4 + 72 = 0x150
            int dataoffPos = 4 + 256 + 4 + 72;
            int dummy = dataoffPos;
            uint dataoff = ReadUInt32LE(fileData, ref dummy);

            if (dataoff >= buflen)
                throw new InvalidDataException("CBC v8 file has invalid data offset.");

            int payloadOffset = (int)dataoff;
            int payloadLength = buflen - payloadOffset;
            if (payloadLength <= 0)
                throw new InvalidDataException("CBC v8 file has no payload data.");

            var payload = new byte[payloadLength];
            Array.Copy(fileData, payloadOffset, payload, 0, payloadLength);

            // Same crypto as v7: cb_crypt_data(buf + hdr->dataoff, datalen)
            Cb2Crypto.CBCryptFileData(payload, payloadLength);

            // Payload format is identical to v7: name (ASCIIZ) + u16 numCheats + cheats
            int offset = 0;
            string listName = ReadAsciiZeroTerminated(payload, ref offset);
            if (string.IsNullOrEmpty(listName))
                listName = headerName;

            if (offset + 2 > payloadLength)
                throw new InvalidDataException("CBC v8 file truncated before cheat count.");

            ushort numCheats = ReadUInt16LE(payload, ref offset);

            var result = new Result { GameName = listName };
            ParseCheatsFromPayload(result.Cheats, payload, offset, payloadLength, numCheats);
            return result;
        }

        private static bool IsCbcV8(byte[] buf)
{
    if (buf.Length < 4)
        return false;

    int off = 0;
    uint id = ReadUInt32LE(buf, ref off);
    return id == CBC_FILE_ID;
}

        // ================================================================
        // Common payload parser: same for v7 and v8 after decryption
        // ================================================================

        private static void ParseCheatsFromPayload(
            List<cheat_t> cheats,
            byte[] payload,
            int offset,
            int payloadLength,
            ushort numCheats)
        {
            for (int c = 0; c < numCheats && offset < payloadLength; c++)
            {
                // Cheat name (ASCIIZ)
                string cheatName = ReadAsciiZeroTerminated(payload, ref offset);

                if (offset >= payloadLength)
                    break;

                // Switch byte: 0 = normal cheat, 4 = heading-only (folder)
                byte sw = payload[offset++];

                if (offset + 2 > payloadLength)
                    break;

                // Number of lines (u16). Each line is 2 x u32 (addr, value).
                ushort numLines = ReadUInt16LE(payload, ref offset);
                int wordCount = numLines * 2;

                var cheat = new cheat_t
                {
                    name = cheatName ?? string.Empty,
                    state = 0
                };

                for (int i = 0; i < wordCount && offset + 4 <= payloadLength; i++)
                {
                    uint word = ReadUInt32LE(payload, ref offset);
                    cheat.code.Add(word);
                }

                // If sw == 4 && numLines == 0, this is a heading-only entry (group).
                // We keep it as a cheat with name + 0 code lines; the rest of the
                // pipeline will preserve it as folders/headings for CB/RAW/etc.

                cheats.Add(cheat);
            }
        }

        // === Helpers ======================================================

        private static string ReadAsciiZeroTerminated(byte[] data, ref int offset, int maxLength)
        {
            int end = Math.Min(offset + maxLength, data.Length);
            return ReadAsciiZeroTerminatedCore(data, ref offset, end);
        }

        private static string ReadAsciiZeroTerminated(byte[] data, ref int offset)
        {
            int end = data.Length;
            return ReadAsciiZeroTerminatedCore(data, ref offset, end);
        }

        private static string ReadAsciiZeroTerminatedCore(byte[] data, ref int offset, int end)
        {
            var sb = new StringBuilder();
            while (offset < end)
            {
                byte b = data[offset++];
                if (b == 0)
                    break;
                sb.Append((char)b);
            }
            return sb.ToString();
        }

        private static ushort ReadUInt16LE(byte[] data, ref int offset)
        {
            if (offset + 2 > data.Length)
                throw new InvalidDataException("Unexpected end of CBC file (u16).");

            ushort v = (ushort)(data[offset] | (data[offset + 1] << 8));
            offset += 2;
            return v;
        }

        private static uint ReadUInt32LE(byte[] data, ref int offset)
        {
            if (offset + 4 > data.Length)
                throw new InvalidDataException("Unexpected end of CBC file (u32).");

            uint v = (uint)(
                data[offset] |
                (data[offset + 1] << 8) |
                (data[offset + 2] << 16) |
                (data[offset + 3] << 24));
            offset += 4;
            return v;
        }
    }
}
