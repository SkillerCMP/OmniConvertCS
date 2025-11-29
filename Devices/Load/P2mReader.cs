using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace OmniconvertCS
{
    /// <summary>
    /// Reader for GameShark v5+ / XP .p2m files.
    /// Parses the archive header and decrypts the embedded user.dat
    /// using the same GS3 logic as P2m.CreateFile.
    /// </summary>
    internal static class P2mReader
    {
        // "P2MS"
        private const uint P2M_FILE_ID = 0x534D3250u;

        // Fixed layout constants (must match P2m.cs).
        private const int P2M_NUM_FILES          = 2;
        private const int P2M_FD_SERIALIZED_SIZE = 0x80; // sizeof(p2mfd_t)
        private const int P2M_HEADER_SIZE        = 0x48; // sizeof(p2mheader_t)
        private const int P2M_ARCSTAT_SIZE       = 8;    // sizeof(p2marcstat_t)

        // Terminators used inside user.dat (must match P2m.cs).
        private const uint TERM_FILE      = 0x00000000u;
        private const uint TERM_GAME_NAME = 0x20000000u;
        private const uint TERM_CODE_NAME = 0x40000000u;
        private const uint TERM_CODE_LINE = 0x80000000u;

        public sealed class Result
        {
            public string GameName { get; set; } = string.Empty;
            public List<cheat_t> Cheats { get; } = new List<cheat_t>();
        }

        public static Result Load(string fileName)
        {
            if (fileName is null) throw new ArgumentNullException(nameof(fileName));

            byte[] buf = File.ReadAllBytes(fileName);
            if (buf.Length < P2M_HEADER_SIZE)
                throw new InvalidDataException("File is too small to be a valid P2M archive.");

            int offset = 0;

            uint fileId = ReadUInt32LE(buf, ref offset);
            if (fileId != P2M_FILE_ID)
                throw new InvalidDataException("P2M file ID mismatch (expected \"P2MS\").");

            // Skip rest of header – we already read fileId.
offset = P2M_HEADER_SIZE;

// Read arcstat (p2marcstat_t)
uint numFiles = ReadUInt32LE(buf, ref offset);
uint arcSize  = ReadUInt32LE(buf, ref offset); // Matches user.dat size in your sample

if (numFiles != P2M_NUM_FILES)
    throw new InvalidDataException(
        $"Unsupported P2M: expected {P2M_NUM_FILES} files, found {numFiles}.");

// File descriptors start here
int fdBase = offset;
if (fdBase + (P2M_FD_SERIALIZED_SIZE * P2M_NUM_FILES) > buf.Length)
    throw new InvalidDataException("P2M file descriptors are truncated.");

// Locate user.dat FD
uint userOffsetRel = 0;
uint userSize      = 0;
bool foundUserDat  = false;

for (int i = 0; i < P2M_NUM_FILES; i++)
{
    int fdOffset = fdBase + i * P2M_FD_SERIALIZED_SIZE;
    int fdPos    = fdOffset;

    // Skip created/modified + unknowns (two p2mdate_t + 4×u32 = 32 bytes)
    fdPos += 32;

    // name[0x20]
    string name = ReadFixedAscii(buf, ref fdPos, 0x20);

    // offset + size
    uint relOffset = ReadUInt32LE(buf, ref fdPos);
    uint size      = ReadUInt32LE(buf, ref fdPos);

    // Skip crc + unknown3 + desc[0x30]; we don't care here
    // fdPos += 8 + 0x30;

    if (string.Equals(name, "user.dat", StringComparison.OrdinalIgnoreCase))
    {
        userOffsetRel = relOffset;
        userSize      = size;
        foundUserDat  = true;
        break;
    }
}

if (!foundUserDat || userSize == 0)
    throw new InvalidDataException("P2M archive does not contain a valid user.dat entry.");

int userDataStartOffset = P2M_HEADER_SIZE + (int)userOffsetRel;
if (userDataStartOffset < 0 || userDataStartOffset + userSize > buf.Length)
    throw new InvalidDataException("P2M user.dat entry runs past end of file.");

byte[] userData = new byte[userSize];
Buffer.BlockCopy(buf, userDataStartOffset, userData, 0, (int)userSize);

// *** IMPORTANT: decrypt with EXACT on-disk size as seed ***
Gs3.CryptFileData(userData, userSize);

            var result = new Result();

            int pos = 0;
            int end = userData.Length;

            // user.dat header:
            //   uint numgames;
            //   uint numcheats;
            //   uint numlines;
            //   ushort numcheatsAgain;
            if (pos + 4 * 3 + 2 > end)
                throw new InvalidDataException("Truncated user.dat header in P2M.");

            uint numGames      = ReadUInt32LE(userData, ref pos);
            uint numCheats     = ReadUInt32LE(userData, ref pos);
            uint numLines      = ReadUInt32LE(userData, ref pos);
            ushort cheatCount2 = ReadUInt16LE(userData, ref pos);

            if (numGames == 0)
                throw new InvalidDataException("P2M archive reports zero games.");
            // If cheatCount2 != numCheats, we just keep going – some files may be sloppy.

            // Game name (UTF-16LE) terminated by: u16 0; u32 TERM_GAME_NAME
            string gameName = ReadWideStringWithTag(userData, ref pos, TERM_GAME_NAME);
            result.GameName = gameName ?? string.Empty;

            // Cheats:
            //   for each cheat:
            //     u16 lineCount;
            //     UTF-16LE name;
            //     u16 0;
            //     u32 TERM_CODE_NAME;
            //     lineCount * (u32 addr, u32 val, u32 TERM_CODE_LINE)
            for (uint i = 0; i < numCheats; i++)
            {
                if (pos + 2 > end)
                    throw new InvalidDataException("Truncated cheat header in user.dat.");

                ushort lineCount = ReadUInt16LE(userData, ref pos);

                string cheatName = ReadWideStringWithTag(userData, ref pos, TERM_CODE_NAME);

                var cheat = new cheat_t
                {
                    id      = 0,
                    name    = cheatName ?? string.Empty,
                    comment = string.Empty,
                    state   = 0 // enabled
                };

                for (int line = 0; line < lineCount; line++)
                {
                    if (pos + 12 > end)
                        throw new InvalidDataException("Truncated code line in user.dat.");

                    uint addr = ReadUInt32LE(userData, ref pos);
                    uint val  = ReadUInt32LE(userData, ref pos);
                    uint term = ReadUInt32LE(userData, ref pos);

                    if (term != TERM_CODE_LINE)
                        throw new InvalidDataException("Unexpected record type in user.dat (expected code line).");

                    cheat.code.Add(addr);
                    cheat.code.Add(val);
                }

                result.Cheats.Add(cheat);
            }

            // Optional footer: TERM_FILE + "Cheat File"
            if (pos + 4 <= end)
            {
                uint termFile = ReadUInt32LE(userData, ref pos);
                if (termFile != TERM_FILE)
                {
                    // Some variants may omit / change this; not fatal.
                    pos -= 4;
                }
            }

            return result;
        }

        private static uint ReadUInt32LE(byte[] buffer, ref int offset)
        {
            if (offset + 4 > buffer.Length)
                throw new InvalidDataException("Unexpected end of buffer while reading UInt32.");

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
                throw new InvalidDataException("Unexpected end of buffer while reading UInt16.");

            ushort value = (ushort)(buffer[offset] | (buffer[offset + 1] << 8));
            offset += 2;
            return value;
        }

private static string ReadFixedAscii(byte[] buf, ref int offset, int length)
{
    int end = offset + length;
    if (end > buf.Length)
        throw new InvalidDataException("Truncated fixed-length ASCII field in P2M.");

    int i = offset;
    while (i < end && buf[i] != 0)
        i++;

    string s = Encoding.ASCII.GetString(buf, offset, i - offset);
    offset = end;
    return s;
}
        /// <summary>
/// Read a UTF-16LE string terminated by a 0 wide-char, then
/// optionally consume a 32-bit tag. The tag value is not enforced;
/// it is read and discarded if present to keep the stream aligned.
/// </summary>
private static string ReadWideStringWithTag(byte[] buffer, ref int offset, uint terminatorTag)
{
    int len = buffer.Length;
    var sb  = new StringBuilder();

    // Read UTF-16 chars until we hit 0x0000.
    while (true)
    {
        if (offset + 2 > len)
            throw new InvalidDataException("Unterminated UTF-16 string in P2M user.dat.");

        ushort ch = (ushort)(buffer[offset] | (buffer[offset + 1] << 8));
        offset += 2;

        if (ch == 0)
            break;

        sb.Append((char)ch);
    }

    // Optionally consume a following u32 tag (TERM_GAME_NAME / TERM_CODE_NAME).
    if (offset + 4 <= len)
    {
        uint tag = ReadUInt32LE(buffer, ref offset);
        // If tag != terminatorTag, we just ignore it – some P2M variants
        // may use different tag values or omit them entirely.
    }

    return sb.ToString();
}
    }
}
