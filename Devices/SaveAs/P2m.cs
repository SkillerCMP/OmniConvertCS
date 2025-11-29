using System;
using System.IO;

namespace OmniconvertCS
{
    /// <summary>
    /// C# port of p2m.c / p2m.h (GameShark v5+ .p2m file support).
    /// This file currently contains only constants and struct shapes
    /// from the original C code. Logic will be added in later chunks.
    /// </summary>
    internal static class P2m
    {
        // "P2MS"
        private const uint P2M_FILE_ID = 0x534D3250u;

        // "P2V2"
        private const uint P2M_FILE_VERSION = 0x32563250u;

        // The total size of the file in the header is less the size of the
        // fileid and filesize variables. The remaining 4 bytes difference is
        // probably the null padding/terminator in the footer.
        private const int P2M_SIZE_EXCLUDE = 12;

        private const int P2M_NAME_SIZE  = 0x38;
        private const int P2M_FNAME_SIZE = 0x20;
        private const int P2M_NUM_FILES  = 2;
		private const int P2M_FD_SERIALIZED_SIZE = 0x80; // sizeof(p2mfd_t) = 128 bytes
		private const int P2M_HEADER_SIZE   = 0x48; // sizeof(p2mheader_t)  = 72 bytes
		private const int P2M_ARCSTAT_SIZE  = 8;    // sizeof(p2marcstat_t) = 8 bytes
		
        // Regions
        private const int NUM_REGIONS = 3;
        private const int REG_USA     = 0;
        private const int REG_EUR     = 1;
        private const int REG_JAP     = 2;

        // File attributes
        private const uint ATTR_DIR  = 0x00008427u;
        private const uint ATTR_FILE = 0x00008417u;

        // Color switches for name strings (used as "{N" prefixes).
        private const int COLOR_DEFAULT  = 0;
        private const int COLOR_BLUE     = 1;
        private const int COLOR_RED      = 2;
        private const int COLOR_GREEN    = 3;
        private const int COLOR_YELLOW   = 4;
        private const int COLOR_PURPLE   = 5;
        private const int COLOR_SKY_BLUE = 6;
        private const int COLOR_WHITE    = 7;
        private const int COLOR_GREY     = 8;

        private const char GS3_COLOR_IND   = '{';
        private const int  NUM_COLORS      = 9;
        private const int  GS_NAME_MAX_SIZE = 99;
		
		

        // ====================================================================
        // Structs lifted from p2m.h
        // ====================================================================

        /// <summary>
        /// Managed equivalent of p2mdate_t (64-bit date/time structure).
        /// </summary>
        private struct p2mdate_t
        {
            public byte   reserved; // 0
            public byte   second;
            public byte   minute;
            public byte   hour;
            public byte   day;
            public byte   month;
            public ushort year;
        }

        /// <summary>
        /// P2M file header (p2mheader_t).
        /// </summary>
        private struct p2mheader_t
        {
            public uint   fileid;
            public uint   filesize;
            // name[P2M_NAME_SIZE] in C; we keep this as a managed string and
            // will truncate/pad to P2M_NAME_SIZE when serializing.
            public string name;
            public uint   fileversion;
            // Always contains 0xA in samples; probably LF at tail of version.
            public uint   unknown;
        }

        /// <summary>
        /// Archive stats (p2marcstat_t).
        /// </summary>
        private struct p2marcstat_t
        {
            public uint numfiles;
            public uint size;
        }

        /// <summary>
        /// P2M file descriptor (p2mfd_t).
        /// </summary>
        private struct p2mfd_t
        {
            public p2mdate_t created;
            public p2mdate_t modified;

            // 0 in all known samples.
            public uint unknown1;

            // 0x8427 for directories, 0x8417 for files.
            public uint attributes;

            // Two unknown u32 values; always 0.
            public uint unknown2_0;
            public uint unknown2_1;

            // name[P2M_FNAME_SIZE] in C; stored as managed string here.
            public string name;

            // Offset relative to end of file header.
            public uint offset;

            // Size in bytes of the file.
            public uint size;

            // May not be a CRC, but isn't checked either way in the original.
            public uint crc;

            // Another unknown field; always 0.
            public uint unknown3;

            // Probably a longer file name or description; always 0.
            // desc[0x30] in C.
            public byte[] desc;
        }

        /// <summary>
        /// Header for user.dat (p2mudheader_t).
        /// </summary>
        private struct p2mudheader_t
        {
            public uint numgames;
            public uint numcheats;
            public uint numlines;
        }
/// <summary>
/// Write a 16-bit unsigned integer in little-endian order into the buffer.
/// </summary>
private static void WriteUInt16LE(byte[] buffer, ref int offset, ushort value)
{
    buffer[offset++] = (byte)(value & 0xFF);
    buffer[offset++] = (byte)((value >> 8) & 0xFF);
}

/// <summary>
/// Write a 32-bit unsigned integer in little-endian order into the buffer.
/// </summary>
private static void WriteUInt32LE(byte[] buffer, ref int offset, uint value)
{
    buffer[offset++] = (byte)(value & 0xFF);
    buffer[offset++] = (byte)((value >> 8) & 0xFF);
    buffer[offset++] = (byte)((value >> 16) & 0xFF);
    buffer[offset++] = (byte)((value >> 24) & 0xFF);
}

/// <summary>
/// Write a UTF-16 ("wchar_t") string without a terminating null.
/// Length is the number of characters to write (may be less than text.Length).
/// </summary>
private static void WriteStringW(byte[] buffer, ref int offset, string text, int length)
{
    if (text == null)
        text = string.Empty;

    if (length > text.Length)
        length = text.Length;

    for (int i = 0; i < length; i++)
    {
        char ch = text[i];
        buffer[offset++] = (byte)(ch & 0xFF);
        buffer[offset++] = (byte)((ch >> 8) & 0xFF);
    }
}
        // ====================================================================
        // Static data from p2m.c
        // ====================================================================

        // MT State Table roots for file encryption.
        private static readonly string[] rootid = new[]
        {
            "BLAZE_USA",
            "BLAZE_UK",
            "BLAZE_JAPAN"
        };

        // The embedded file name used inside the archive.
        private const string userfilename = "user.dat";

        // ASCII footer string at the end of the file.
        private const string filefooter = "Cheat File";

        // Terminators used for various record types.
        private const uint TERM_FILE      = 0x00000000u;
        private const uint TERM_GAME_NAME = 0x20000000u;
        private const uint TERM_CODE_NAME = 0x40000000u;
        private const uint TERM_CODE_LINE = 0x80000000u;

        // Color prefixes applied to cheat names (e.g. "{5" for purple).
        private static readonly string[] color_prefix = new[]
        {
            "{0",
            "{1",
            "{2",
            "{3",
            "{4",
            "{5",
            "{6",
            "{7",
            "{8"
        };
		/// <summary>
/// Initialize a P2M file header (C: p2mInitHeader).
/// </summary>
private static void InitHeader(ref p2mheader_t header, string name)
{
    header.fileid      = P2M_FILE_ID;
    header.fileversion = P2M_FILE_VERSION;

    if (string.IsNullOrEmpty(name))
        name = string.Empty;

    // In C, the name is a fixed char[P2M_NAME_SIZE] with the last byte
    // forced to '\0'. We mimic that by truncating to (P2M_NAME_SIZE - 1).
    if (name.Length > P2M_NAME_SIZE - 1)
        header.name = name.Substring(0, P2M_NAME_SIZE - 1);
    else
        header.name = name;

    // Always 0xA in sample files (likely LF at the end of the version string).
    header.unknown = 0x0Au;
}

/// <summary>
/// Initialize a P2M file descriptor (C: p2mInitFd).
/// </summary>
/// <param name="now">Current time (equivalent to struct tm in the C code).</param>
/// <param name="name">File name to store in the descriptor.</param>
/// <param name="attributes">Attribute flags (ATTR_DIR / ATTR_FILE).</param>
private static p2mfd_t InitFd(DateTime now, string name, uint attributes)
{
    var fd = new p2mfd_t();

    // memset(fd, 0, sizeof(p2mfd_t));
    // .NET default for a struct is already zeroed, but we explicitly
    // set the fields that differ from zero in the original.
    fd.offset = 0xFFFFFFFFu; // -1 in the original C code
    fd.size   = 0xFFFFFFFFu; // -1 in the original C code

    // Created timestamp
    fd.created.second = (byte)now.Second;
    fd.created.minute = (byte)now.Minute;
    fd.created.hour   = (byte)now.Hour;
    fd.created.day    = (byte)now.Day;
    fd.created.month  = (byte)now.Month;
    fd.created.year   = (ushort)now.Year;

    // Name field: in C this is char[P2M_FNAME_SIZE] with strcpy (no bound
    // check, but our callers only use short literals). We store the full
    // string here and will clamp/pad during serialization.
    fd.name = name ?? string.Empty;

    fd.attributes = attributes;

    // desc[0x30] is always zeroed in samples; allocate so we can serialize it.
    fd.desc = new byte[0x30];

    return fd;
}
private static void WriteFixedAscii(byte[] buffer, ref int offset, string text, int fieldSize)
{
    if (text == null)
        text = string.Empty;

    // Truncate to fit in the fixed buffer.
    if (text.Length > fieldSize)
        text = text.Substring(0, fieldSize);

    int i = 0;
    for (; i < text.Length && i < fieldSize; i++)
    {
        char ch = text[i];
        buffer[offset++] = (byte)(ch & 0xFF); // ASCII
    }

    // Pad remaining bytes with 0.
    for (; i < fieldSize; i++)
    {
        buffer[offset++] = 0;
    }
}

/// <summary>
/// Serialize a p2mdate_t into the buffer (8 bytes).
/// </summary>
private static void WriteDate(byte[] buffer, ref int offset, p2mdate_t date)
{
    buffer[offset++] = date.reserved;
    buffer[offset++] = date.second;
    buffer[offset++] = date.minute;
    buffer[offset++] = date.hour;
    buffer[offset++] = date.day;
    buffer[offset++] = date.month;

    // year (u16 LE)
    buffer[offset++] = (byte)(date.year & 0xFF);
    buffer[offset++] = (byte)((date.year >> 8) & 0xFF);
}

/// <summary>
/// Serialize a p2mheader_t into the buffer. Must advance offset by
/// exactly P2M_HEADER_SIZE bytes (0x48).
/// </summary>
private static void WriteHeader(byte[] buffer, ref int offset, ref p2mheader_t header)
{
    WriteUInt32LE(buffer, ref offset, header.fileid);
    WriteUInt32LE(buffer, ref offset, header.filesize);

    // char name[P2M_NAME_SIZE];
    WriteFixedAscii(buffer, ref offset, header.name, P2M_NAME_SIZE);

    WriteUInt32LE(buffer, ref offset, header.fileversion);
    WriteUInt32LE(buffer, ref offset, header.unknown);
}

/// <summary>
/// Serialize a p2marcstat_t into the buffer (8 bytes).
/// </summary>
private static void WriteArcStat(byte[] buffer, ref int offset, ref p2marcstat_t arcStat)
{
    WriteUInt32LE(buffer, ref offset, arcStat.numfiles);
    WriteUInt32LE(buffer, ref offset, arcStat.size);
}

/// <summary>
/// Serialize a p2mfd_t into the buffer. Must advance offset by
/// exactly P2M_FD_SERIALIZED_SIZE bytes (0x80).
/// </summary>
private static void WriteFileDescriptor(byte[] buffer, ref int offset, ref p2mfd_t fd)
{
    // created + modified
    WriteDate(buffer, ref offset, fd.created);
    WriteDate(buffer, ref offset, fd.modified);

    // unknown1, attributes, unknown2[2]
    WriteUInt32LE(buffer, ref offset, fd.unknown1);
    WriteUInt32LE(buffer, ref offset, fd.attributes);
    WriteUInt32LE(buffer, ref offset, fd.unknown2_0);
    WriteUInt32LE(buffer, ref offset, fd.unknown2_1);

    // name[P2M_FNAME_SIZE]
    WriteFixedAscii(buffer, ref offset, fd.name, P2M_FNAME_SIZE);

    // offset, size, crc, unknown3
    WriteUInt32LE(buffer, ref offset, fd.offset);
    WriteUInt32LE(buffer, ref offset, fd.size);
    WriteUInt32LE(buffer, ref offset, fd.crc);
    WriteUInt32LE(buffer, ref offset, fd.unknown3);

    // desc[0x30]
    byte[] desc = fd.desc ?? Array.Empty<byte>();
    int maxDesc = 0x30;
    int i = 0;

    for (; i < desc.Length && i < maxDesc; i++)
    {
        buffer[offset++] = desc[i];
    }

    // Pad remainder with 0.
    for (; i < maxDesc; i++)
    {
        buffer[offset++] = 0;
    }
}

// <summary>
/// 1:1 scaffold for the original:
///   int p2mCreateFile(cheat_t* cheat, game_t *game, char *szFileName, u8 doheadings)
/// This C# version will ultimately mirror the native implementation and
/// write a GameShark v5+ .p2m save file. For now, it only declares the
/// variables and high-level structure; the detailed logic will be filled
/// in by later chunks.
///
/// Returns 0 on success, non-zero on failure, matching the C behaviour.
/// </summary>
public static int CreateFile(System.Collections.Generic.List<cheat_t> cheats,
                             game_t game,
                             string fileName,
                             bool doHeadings)
{
    if (cheats == null) throw new ArgumentNullException(nameof(cheats));
    if (game == null) throw new ArgumentNullException(nameof(game));
    if (fileName == null) throw new ArgumentNullException(nameof(fileName));

    // --- Locals ported from p2mCreateFile() ---
    byte[]? buffer = null;
    int bufferOffset = 0; // used for writing header/arcstat/FDs
    byte mcode = 0;

    uint numCheats = 0;
    uint numLines = 0;
    uint size = 0;
    uint fileSize = 0;
    uint i = 0;

    cheat_t? first = cheats.Count > 0 ? cheats[0] : null;

    p2mheader_t header = default;
    p2marcstat_t arcStat = default;
    p2mfd_t fdRoot = default;
    p2mfd_t fdUserData = default;
    p2mudheader_t udHeader = default;

    // C: time_t timestamp; struct tm *currtime;
    DateTime timestamp = DateTime.Now;
    DateTime currtime = timestamp;

    // =====================================================================
    // Init header / FDs / arcStat  (Chunk 5)
    // =====================================================================

    InitHeader(ref header, game.name);

    fdRoot = InitFd(currtime, rootid[REG_USA], ATTR_DIR);
    fdUserData = InitFd(currtime, userfilename, ATTR_FILE);

    arcStat.numfiles = P2M_NUM_FILES;
    arcStat.size = (uint)(P2M_FD_SERIALIZED_SIZE * P2M_NUM_FILES);

    // =====================================================================
    // Cheat scan, master-code checks, size accumulation (Chunk 6)
    // =====================================================================

    fdUserData.size = 0;

    // First pass: walk the List<cheat_t> instead of a linked list.
    for (int idx = 0; idx < cheats.Count; idx++)
    {
        cheat_t cheat = cheats[idx];

        // First entry must be the master code (if present at all)
        if (idx == 0)
        {
            if (cheat.flag[Cheat.FLAG_MCODE] == 0)
            {
                // No master code as the first entry -> handled later via mcode == 0
                break;
            }
            else
            {
                mcode++;

                // If the enable code name does not already start with the
                // GS3 color indicator, prepend the purple prefix "{5".
                if (!string.IsNullOrEmpty(cheat.name) &&
                    cheat.name[0] != GS3_COLOR_IND)
                {
                    uint textmax = (uint)(cheat.name.Length + color_prefix[COLOR_PURPLE].Length + 16);
                    Common.PrependText(ref cheat.name, color_prefix[COLOR_PURPLE], ref textmax);
                }
            }
        }
        // Any *other* cheat marked as master => second enable code = error
        else if (cheat.flag[Cheat.FLAG_MCODE] != 0)
        {
            mcode++;
            break;
        }

        // Only count cheats that are "on" (state == 0) and either:
        //  - we're including headings (doHeadings)
        //  - or they have at least one code line
        if (cheat.state == 0 && (doHeadings || cheat.codecnt > 0))
        {
            // If this cheat has zero code lines, it is treated as a
            // divider and gets a green color prefix "{3".
            if (cheat.codecnt == 0)
{
    // Only auto-green it if the user didn't already pick a color.
    if (!string.IsNullOrEmpty(cheat.name) &&
        cheat.name[0] != GS3_COLOR_IND)
    {
        uint textmax = (uint)((cheat.name?.Length ?? 0) + color_prefix[COLOR_GREEN].Length + 16);
        Common.PrependText(ref cheat.name, color_prefix[COLOR_GREEN], ref textmax);
    }
}

            int nameLen = cheat.name?.Length ?? 0;
            if (nameLen > GS_NAME_MAX_SIZE) nameLen = GS_NAME_MAX_SIZE;

            // strlen + term null + term (u32) + line count (u16)
            fdUserData.size += (uint)(
                  nameLen * sizeof(char)
                + sizeof(char)
                + sizeof(uint)
                + sizeof(ushort));

            // numlines = cheat->codecnt >> 1;
            uint lineCount = (uint)(cheat.codecnt >> 1);
            udHeader.numlines += lineCount;

            // include lines + per-line terminators:
            // sizeof(u32) * (codecnt + (codecnt >> 1))
            fdUserData.size += (uint)(
                sizeof(uint) * (cheat.codecnt + (cheat.codecnt >> 1)));

            udHeader.numcheats++;
            numCheats++;
            numLines += lineCount;
        }
    }

    if (mcode == 0)
    {
        Common.MsgBox(IntPtr.Zero, 0,
            "File cannot be created without a master code in the list.");
        return 1;
    }

    if (mcode > 1)
    {
        Common.MsgBox(IntPtr.Zero, 0,
            "File cannot be created with more than one enable code");
        return 1;
    }

    if (fdUserData.size == 0 || udHeader.numcheats < 1)
    {
        Common.MsgBox(IntPtr.Zero, 0,
            "After careful analysis, it seems no cheats are valid for GameShark.");
        return 1;
    }

    // =====================================================================
    // Game-name size + total filesize (Chunk 7)
    // =====================================================================

    int gameNameLen = game.name?.Length ?? 0;
    if (gameNameLen > GS_NAME_MAX_SIZE) gameNameLen = GS_NAME_MAX_SIZE;

    fdUserData.size += (uint)(
          (gameNameLen + 1) * sizeof(char)
        + sizeof(ushort)
        + sizeof(uint) * 5);

    size = (uint)gameNameLen;

    fileSize =
        arcStat.size
        + fdUserData.size
        + (uint)P2M_ARCSTAT_SIZE
        + (uint)P2M_HEADER_SIZE
        + (uint)filefooter.Length
        + sizeof(uint);

    // =====================================================================
    // Allocate buffer + write udHeader + game name (Chunk 8)
    // =====================================================================

    if (fileSize == 0 || fileSize > int.MaxValue)
    {
        Common.MsgBox(IntPtr.Zero, 0,
            "Unable to allocate output buffer for P2M file");
        return 1;
    }

    try
    {
        buffer = new byte[(int)fileSize];
    }
    catch (OutOfMemoryException)
    {
        Common.MsgBox(IntPtr.Zero, 0,
            "Unable to allocate output buffer for P2M file");
        return 1;
    }

    Array.Clear(buffer, 0, buffer.Length);

    int fdAreaSize = P2M_FD_SERIALIZED_SIZE * P2M_NUM_FILES;
    int userDataStartOffset = P2M_HEADER_SIZE + P2M_ARCSTAT_SIZE + fdAreaSize;
    int userdataOffset = userDataStartOffset;

    udHeader.numgames = 1;

    WriteUInt32LE(buffer, ref userdataOffset, udHeader.numgames);
    WriteUInt32LE(buffer, ref userdataOffset, udHeader.numcheats);
    WriteUInt32LE(buffer, ref userdataOffset, udHeader.numlines);

    WriteUInt16LE(buffer, ref userdataOffset, (ushort)udHeader.numcheats);

    string gameName = game.name ?? string.Empty;
    if (gameNameLen > gameName.Length) gameNameLen = gameName.Length;
    if (gameNameLen > 0)
    {
        gameName = gameName.Substring(0, gameNameLen);
        WriteStringW(buffer, ref userdataOffset, gameName, gameNameLen);
    }

    WriteUInt16LE(buffer, ref userdataOffset, 0);
    WriteUInt32LE(buffer, ref userdataOffset, TERM_GAME_NAME);

    // =====================================================================
    // Write cheats + footer into userdata (Chunk 9)
    // =====================================================================

    for (int idx = 0; idx < cheats.Count; idx++)
    {
        cheat_t cheat = cheats[idx];

        if (cheat.state == 0 && (doHeadings || cheat.codecnt > 0))
        {
            uint lineCount = (uint)(cheat.codecnt >> 1);

            WriteUInt16LE(buffer, ref userdataOffset, (ushort)lineCount);

            string cheatName = cheat.name ?? string.Empty;
            int cheatNameLen = cheatName.Length;
            if (cheatNameLen > GS_NAME_MAX_SIZE) cheatNameLen = GS_NAME_MAX_SIZE;
            if (cheatNameLen > cheatName.Length) cheatNameLen = cheatName.Length;
            if (cheatNameLen > 0)
            {
                cheatName = cheatName.Substring(0, cheatNameLen);
                WriteStringW(buffer, ref userdataOffset, cheatName, cheatNameLen);
            }

            WriteUInt16LE(buffer, ref userdataOffset, 0);
            WriteUInt32LE(buffer, ref userdataOffset, TERM_CODE_NAME);

            for (int ci = 0; ci < cheat.codecnt; ci += 2)
            {
                uint address = cheat.code[ci];
                uint value   = cheat.code[ci + 1];

                WriteUInt32LE(buffer, ref userdataOffset, address);
                WriteUInt32LE(buffer, ref userdataOffset, value);
                WriteUInt32LE(buffer, ref userdataOffset, TERM_CODE_LINE);
            }
        }
    }

    WriteUInt32LE(buffer, ref userdataOffset, TERM_FILE);

    for (int k = 0; k < filefooter.Length; k++)
    {
        buffer[userdataOffset++] = (byte)filefooter[k];
    }

    // =====================================================================
    // Encrypt user.dat, compute CRC, update header/FDs (Chunk 10)
    // =====================================================================

    if (fdUserData.size > 0)
    {
        uint userDataSize = fdUserData.size;
        int  userDataLen  = (int)userDataSize;

        byte[] userDataBuf = new byte[userDataLen];
        Buffer.BlockCopy(buffer, userDataStartOffset, userDataBuf, 0, userDataLen);

        Gs3.CryptFileData(userDataBuf, userDataSize);
        fdUserData.crc = Gs3.GenCrc32(userDataBuf, userDataSize);

        Buffer.BlockCopy(userDataBuf, 0, buffer, userDataStartOffset, userDataLen);
    }
    else
    {
        fdUserData.crc = 0;
    }

    fdUserData.offset = (uint)(arcStat.size + P2M_ARCSTAT_SIZE);
    header.filesize = fileSize - P2M_SIZE_EXCLUDE;
    arcStat.size = fdUserData.size;

    // =====================================================================
    // Serialize header, arcStat, and file descriptors (Chunk 11)
    // =====================================================================

    bufferOffset = 0;

    WriteHeader(buffer, ref bufferOffset, ref header);
    WriteArcStat(buffer, ref bufferOffset, ref arcStat);
    WriteFileDescriptor(buffer, ref bufferOffset, ref fdRoot);
    WriteFileDescriptor(buffer, ref bufferOffset, ref fdUserData);

    // =====================================================================
    // Final chunk: write buffer to disk, return status
    // =====================================================================

    try
    {
        System.IO.File.WriteAllBytes(fileName, buffer);
    }
    catch (Exception)
    {
        Common.MsgBox(IntPtr.Zero, 0,
            $"Unable to write P2M file \"{fileName}\".");
        return 1;
    }

    return 0;
}
    }
}
