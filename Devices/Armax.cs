using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Diagnostics;

namespace OmniconvertCS
{
    /// <summary>
    /// Partial C# port of armax.c (Action Replay MAX helpers).
    ///
    /// This file intentionally focuses on the text/format helpers that are
    /// easy to express safely in C#.  The heavy crypto routines (seed
    /// generation, bit shuffling, DES tables, disc hash, etc.) are left as
    /// TODOs so they can be filled in incrementally while keeping the public
    /// surface similar to the original C code.
    /// </summary>
        public static class Armax
    {
		// === Debug / trace helpers for ARMAX crypto ===

[Conditional("DEBUG")]
private static void Trace(string message)
{
    Debug.WriteLine(message);
}

[Conditional("DEBUG")]
private static void Trace(string format, params object[] args)
{
    Debug.WriteLine(string.Format(format, args));
}

[Conditional("DEBUG")]
private static unsafe void DumpWords(string label, uint* pCodes, int count)
{
    var sb = new StringBuilder();
    sb.AppendLine(label);
    for (int i = 0; i < count; i += 2)
    {
        if (i + 1 < count)
            sb.AppendFormat("  [{0:D2}] {1:X8} {2:X8}\r\n",
                i / 2, pCodes[i], pCodes[i + 1]);
        else
            sb.AppendFormat("  [{0:D2}] {1:X8}\r\n",
                i / 2, pCodes[i]);
    }
    Debug.WriteLine(sb.ToString());
}

        // Build ARMAX seeds once at startup (1:1 with buildseeds()
        // being called from WM_INITDIALOG in the original C code).
        static Armax()
        {
            unsafe
            {
                buildseeds();
            }
        }

        /// <summary>
        /// Maximum number of characters in a formatted ARMAX code block.
        /// Mirrors NUM_CHARS_ARM_CODE in the C source.
        /// </summary>
        private const int NUM_CHARS_ARM_CODE = 15;

        // The original C code uses a filter string:
        //   const char filter[33] = "0123456789ABCDEFGHJKMNPQRTUVWXYZ";
        // We keep the same alphabet for validation.
        private const string FilterAlphabet = "0123456789ABCDEFGHJKMNPQRTUVWXYZ";

// Number of bytes from the ELF used to compute the disc CRC.
        // Matches ELF_CRC_SIZE in the original armax.c (256 KB).
        private const int ELF_CRC_SIZE = 256 * 1024;

        /// <summary>
        /// Map a single ARMAX alphabet character to its 0‑based index.
        /// Equivalent to GETVAL(filter, ch) in the original C code.
        /// </summary>
        private static int GetVal(char ch)
        {
            return FilterAlphabet.IndexOf(char.ToUpperInvariant(ch));
        }

        /// <summary>
        /// Expansion sizes used by armReadVerifier() (mirrors exp_size[] in armax.c).
        /// Indices: 0..7 => { 6, 10, 12, 19, 19, 8, 7, 32 }.
        /// </summary>
        private static readonly byte[] ExpSize = new byte[] { 6, 10, 12, 19, 19, 8, 7, 32 };


        /// <summary>
        /// Equivalent to MaxRemoveDashes() in armax.c.
        ///
        /// Given an ARMAX code string, this:
        ///   - Removes '-' and whitespace.
        ///   - Converts to upper‑case.
        ///   - Stops once NUM_CHARS_ARM_CODE characters have been copied.
        /// The original C routine wrote into a char buffer and returned the
        /// number of characters copied.  Here we return the cleaned string
        /// and expose the length via out parameter.
        /// </summary>
        /// <param name="src">Source ARMAX code string.</param>
        /// <param name="length">Number of valid characters copied.</param>
        /// <returns>Cleaned, dash‑free, upper‑case code.</returns>
        public static string MaxRemoveDashes(string src, out int length)
        {
            if (src == null)
            {
                length = 0;
                return string.Empty;
            }

            var sb = new StringBuilder(src.Length);

            foreach (char c in src)
            {
                if (c == '-' || char.IsWhiteSpace(c))
                    continue;

                // ARMAX codes are uppercase.
                char u = char.ToUpperInvariant(c);
                sb.Append(u);

                if (sb.Length >= NUM_CHARS_ARM_CODE)
                    break;
            }

            length = sb.Length;
            return sb.ToString();
        }

                /// <summary>
        /// Simple validation helper that mirrors IsArMaxStr() from armax.c.
        ///
        /// The original function expects a 15-character string of the form
        /// "XXXXX-XXXXX-XXXXX":
        ///   - Total length 15
        ///   - '-' at positions 4 and 9
        ///   - All other characters from the ARMAX alphabet.
        /// </summary>
        public static bool IsArMaxStr(string code)
        {
            if (string.IsNullOrWhiteSpace(code))
                return false;

            string s = code.Trim();

            // Must be exactly 15 chars: "XXXXX-XXXXX-XXXXX"
            if (s.Length != NUM_CHARS_ARM_CODE)
                return false;

            for (int i = 0; i < NUM_CHARS_ARM_CODE; i++)
            {
                char ch = char.ToUpperInvariant(s[i]);

                // Positions 4 and 9 must be '-'
                if (i == 4 || i == 9)
                {
                    if (ch != '-')
                        return false;
                    continue;
                }

                // All other positions must be in the ARMAX alphabet
                if (FilterAlphabet.IndexOf(ch) < 0)
                    return false;
            }

            return true;
        }

        /// <summary>
        /// Convert a human readable ARMAX text code (e.g. "D8B9-Y7HK-0ZKU0")
        /// into the two 32-bit words used internally by the ARMAX crypto.
        /// Returns true on success, false on parse/validation error.
        /// </summary>
        public static bool TryParseEncryptedLine(string code, out uint word0, out uint word1)
        {
            word0 = 0;
            word1 = 0;

            if (!IsArMaxStr(code))
                return false;

            // Strip dashes / whitespace and upper-case, like MaxRemoveDashes in C
            string cleaned = MaxRemoveDashes(code, out int len);
            if (len <= 0)
                return false;

            // One row of up to 14 characters, like char alpha[1][14] in the C code
            char[,] alpha = new char[1, 14];
            for (int i = 0; i < len && i < 14; i++)
                alpha[0, i] = cleaned[i];

            uint[] dst = new uint[2];

            unsafe
            {
                fixed (uint* pDst = dst)
                {
                    // alphatobin returns a parity error flag, but the original
                    // DecodeText() ignores it, so we do the same.
                    _ = alphatobin(alpha, pDst, 1);
                }
            }

            word0 = dst[0];
            word1 = dst[1];
            return true;
        }

        /// <summary>
        /// Convert a pair of encrypted ARMAX words back into a formatted text
        /// code ("XXXXX-XXXXX-XXXXX") using bintoalpha(), just like the C code.
        /// </summary>
        public static string FormatEncryptedLine(uint word0, uint word1)
        {
            uint[] src = new uint[2] { word0, word1 };
            char[] buf = new char[16]; // 15 chars + terminator

            unsafe
            {
                fixed (char* pBuf = buf)
                fixed (uint* pSrc = src)
                {
                    bintoalpha(pBuf, pSrc, 0);
                }
            }

            int len = Array.IndexOf(buf, '\0');
            if (len < 0)
                len = buf.Length;

            return new string(buf, 0, len);
        }

        
        #region ARMAX crypto and batch helpers

        // ===== ARMAX S-boxes, CRC tables, and generation tables (direct port from armax.c) =====

        private static readonly uint[] table0 = new uint[] { //80034D98
        	0x01010400, 0x00000000, 0x00010000, 0x01010404, 0x01010004, 0x00010404, 0x00000004, 0x00010000,
        	0x00000400, 0x01010400, 0x01010404, 0x00000400, 0x01000404, 0x01010004, 0x01000000, 0x00000004,
        	0x00000404, 0x01000400, 0x01000400, 0x00010400, 0x00010400, 0x01010000, 0x01010000, 0x01000404,
        	0x00010004, 0x01000004, 0x01000004, 0x00010004, 0x00000000, 0x00000404, 0x00010404, 0x01000000,
        	0x00010000, 0x01010404, 0x00000004, 0x01010000, 0x01010400, 0x01000000, 0x01000000, 0x00000400,
        	0x01010004, 0x00010000, 0x00010400, 0x01000004, 0x00000400, 0x00000004, 0x01000404, 0x00010404,
        	0x01010404, 0x00010004, 0x01010000, 0x01000404, 0x01000004, 0x00000404, 0x00010404, 0x01010400,
        	0x00000404, 0x01000400, 0x01000400, 0x00000000, 0x00010004, 0x00010400, 0x00000000, 0x01010004,
        };

        private static readonly uint[] table1 = new uint[] { //80034E98
        	0x80108020, 0x80008000, 0x00008000, 0x00108020, 0x00100000, 0x00000020, 0x80100020, 0x80008020,
        	0x80000020, 0x80108020, 0x80108000, 0x80000000, 0x80008000, 0x00100000, 0x00000020, 0x80100020,
        	0x00108000, 0x00100020, 0x80008020, 0x00000000, 0x80000000, 0x00008000, 0x00108020, 0x80100000,
        	0x00100020, 0x80000020, 0x00000000, 0x00108000, 0x00008020, 0x80108000, 0x80100000, 0x00008020,
        	0x00000000, 0x00108020, 0x80100020, 0x00100000, 0x80008020, 0x80100000, 0x80108000, 0x00008000,
        	0x80100000, 0x80008000, 0x00000020, 0x80108020, 0x00108020, 0x00000020, 0x00008000, 0x80000000,
        	0x00008020, 0x80108000, 0x00100000, 0x80000020, 0x00100020, 0x80008020, 0x80000020, 0x00100020,
        	0x00108000, 0x00000000, 0x80008000, 0x00008020, 0x80000000, 0x80100020, 0x80108020, 0x00108000,
        };

        private static readonly uint[] table2 = new uint[] { //80034F98
        	0x00000208, 0x08020200, 0x00000000, 0x08020008, 0x08000200, 0x00000000, 0x00020208, 0x08000200,
        	0x00020008, 0x08000008, 0x08000008, 0x00020000, 0x08020208, 0x00020008, 0x08020000, 0x00000208,
        	0x08000000, 0x00000008, 0x08020200, 0x00000200, 0x00020200, 0x08020000, 0x08020008, 0x00020208,
        	0x08000208, 0x00020200, 0x00020000, 0x08000208, 0x00000008, 0x08020208, 0x00000200, 0x08000000,
        	0x08020200, 0x08000000, 0x00020008, 0x00000208, 0x00020000, 0x08020200, 0x08000200, 0x00000000,
        	0x00000200, 0x00020008, 0x08020208, 0x08000200, 0x08000008, 0x00000200, 0x00000000, 0x08020008,
        	0x08000208, 0x00020000, 0x08000000, 0x08020208, 0x00000008, 0x00020208, 0x00020200, 0x08000008,
        	0x08020000, 0x08000208, 0x00000208, 0x08020000, 0x00020208, 0x00000008, 0x08020008, 0x00020200,
        };

        private static readonly uint[] table3 = new uint[] { //80035098
        	0x00802001, 0x00002081, 0x00002081, 0x00000080, 0x00802080, 0x00800081, 0x00800001, 0x00002001,
        	0x00000000, 0x00802000, 0x00802000, 0x00802081, 0x00000081, 0x00000000, 0x00800080, 0x00800001,
        	0x00000001, 0x00002000, 0x00800000, 0x00802001, 0x00000080, 0x00800000, 0x00002001, 0x00002080,
        	0x00800081, 0x00000001, 0x00002080, 0x00800080, 0x00002000, 0x00802080, 0x00802081, 0x00000081,
        	0x00800080, 0x00800001, 0x00802000, 0x00802081, 0x00000081, 0x00000000, 0x00000000, 0x00802000,
        	0x00002080, 0x00800080, 0x00800081, 0x00000001, 0x00802001, 0x00002081, 0x00002081, 0x00000080,
        	0x00802081, 0x00000081, 0x00000001, 0x00002000, 0x00800001, 0x00002001, 0x00802080, 0x00800081,
        	0x00002001, 0x00002080, 0x00800000, 0x00802001, 0x00000080, 0x00800000, 0x00002000, 0x00802080,
        };

        private static readonly uint[] table4 = new uint[] { //80035198
        	0x00000100, 0x02080100, 0x02080000, 0x42000100, 0x00080000, 0x00000100, 0x40000000, 0x02080000,
        	0x40080100, 0x00080000, 0x02000100, 0x40080100, 0x42000100, 0x42080000, 0x00080100, 0x40000000,
        	0x02000000, 0x40080000, 0x40080000, 0x00000000, 0x40000100, 0x42080100, 0x42080100, 0x02000100,
        	0x42080000, 0x40000100, 0x00000000, 0x42000000, 0x02080100, 0x02000000, 0x42000000, 0x00080100,
        	0x00080000, 0x42000100, 0x00000100, 0x02000000, 0x40000000, 0x02080000, 0x42000100, 0x40080100,
        	0x02000100, 0x40000000, 0x42080000, 0x02080100, 0x40080100, 0x00000100, 0x02000000, 0x42080000,
        	0x42080100, 0x00080100, 0x42000000, 0x42080100, 0x02080000, 0x00000000, 0x40080000, 0x42000000,
        	0x00080100, 0x02000100, 0x40000100, 0x00080000, 0x00000000, 0x40080000, 0x02080100, 0x40000100,
        };

        private static readonly uint[] table5 = new uint[] { //80035298
        	0x20000010, 0x20400000, 0x00004000, 0x20404010, 0x20400000, 0x00000010, 0x20404010, 0x00400000,
        	0x20004000, 0x00404010, 0x00400000, 0x20000010, 0x00400010, 0x20004000, 0x20000000, 0x00004010,
        	0x00000000, 0x00400010, 0x20004010, 0x00004000, 0x00404000, 0x20004010, 0x00000010, 0x20400010,
        	0x20400010, 0x00000000, 0x00404010, 0x20404000, 0x00004010, 0x00404000, 0x20404000, 0x20000000,
        	0x20004000, 0x00000010, 0x20400010, 0x00404000, 0x20404010, 0x00400000, 0x00004010, 0x20000010,
        	0x00400000, 0x20004000, 0x20000000, 0x00004010, 0x20000010, 0x20404010, 0x00404000, 0x20400000,
        	0x00404010, 0x20404000, 0x00000000, 0x20400010, 0x00000010, 0x00004000, 0x20400000, 0x00404010,
        	0x00004000, 0x00400010, 0x20004010, 0x00000000, 0x20404000, 0x20000000, 0x00400010, 0x20004010,
        };

        private static readonly uint[] table6 = new uint[] { //80035398
        	0x00200000, 0x04200002, 0x04000802, 0x00000000, 0x00000800, 0x04000802, 0x00200802, 0x04200800,
        	0x04200802, 0x00200000, 0x00000000, 0x04000002, 0x00000002, 0x04000000, 0x04200002, 0x00000802,
        	0x04000800, 0x00200802, 0x00200002, 0x04000800, 0x04000002, 0x04200000, 0x04200800, 0x00200002,
        	0x04200000, 0x00000800, 0x00000802, 0x04200802, 0x00200800, 0x00000002, 0x04000000, 0x00200800,
        	0x04000000, 0x00200800, 0x00200000, 0x04000802, 0x04000802, 0x04200002, 0x04200002, 0x00000002,
        	0x00200002, 0x04000000, 0x04000800, 0x00200000, 0x04200800, 0x00000802, 0x00200802, 0x04200800,
        	0x00000802, 0x04000002, 0x04200802, 0x04200000, 0x00200800, 0x00000000, 0x00000002, 0x04200802,
        	0x00000000, 0x00200802, 0x04200000, 0x00000800, 0x04000002, 0x04000800, 0x00000800, 0x00200002,
        };

        private static readonly uint[] table7 = new uint[] { //80035498
        	0x10001040, 0x00001000, 0x00040000, 0x10041040, 0x10000000, 0x10001040, 0x00000040, 0x10000000,
        	0x00040040, 0x10040000, 0x10041040, 0x00041000, 0x10041000, 0x00041040, 0x00001000, 0x00000040,
        	0x10040000, 0x10000040, 0x10001000, 0x00001040, 0x00041000, 0x00040040, 0x10040040, 0x10041000,
        	0x00001040, 0x00000000, 0x00000000, 0x10040040, 0x10000040, 0x10001000, 0x00041040, 0x00040000,
        	0x00041040, 0x00040000, 0x10041000, 0x00001000, 0x00000040, 0x10040040, 0x00001000, 0x00041040,
        	0x10001000, 0x00000040, 0x10000040, 0x10040000, 0x10040040, 0x10000000, 0x00040000, 0x10001040,
        	0x00000000, 0x10041040, 0x00040040, 0x10000040, 0x10040000, 0x10001000, 0x10001040, 0x00000000,
        	0x10041040, 0x00041000, 0x00041000, 0x00001040, 0x00001040, 0x00040040, 0x10000000, 0x10041000,
        };

        private static readonly ushort[] crctable0 = new ushort[] { //80035630
        	0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
        	0x8408, 0x9489, 0xA50A, 0xB58B, 0xC60C, 0xD68D, 0xE70E, 0xF78F,
        };

        private static readonly ushort[] crctable1 = new ushort[] { //80035650
        	0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
        	0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
        };

        private static readonly byte[] bitstringlen = new byte[] {
        	0x06, 0x0A, 0x0C, 0x13, 0x13, 0x08, 0x07, 0x20,
        };

        private static readonly byte[] gentable0 = new byte[] { //80035598
        	0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09, 0x01,
        	0x3A, 0x32, 0x2A, 0x22, 0x1A, 0x12, 0x0A, 0x02,
        	0x3B, 0x33, 0x2B, 0x23, 0x1B, 0x13, 0x0B, 0x03,
        	0x3C, 0x34, 0x2C, 0x24, 0x3F, 0x37, 0x2F, 0x27,
        	0x1F, 0x17, 0x0F, 0x07, 0x3E, 0x36, 0x2E, 0x26,
        	0x1E, 0x16, 0x0E, 0x06, 0x3D, 0x35, 0x2D, 0x25,
        	0x1D, 0x15, 0x0D, 0x05, 0x1C, 0x14, 0x0C, 0x04,
        };

        private static readonly byte[] gentable1 = new byte[] { //80035610
        	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
        };

        private static readonly byte[] gentable2 = new byte[] { //800355D0
        	0x01, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
        	0x0F, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1C,
        };

        private static readonly byte[] gentable3 = new byte[] { //800355E0
        	0x0E, 0x11, 0x0B, 0x18, 0x01, 0x05, 0x03, 0x1C,
        	0x0F, 0x06, 0x15, 0x0A, 0x17, 0x13, 0x0C, 0x04,
        	0x1A, 0x08, 0x10, 0x07, 0x1B, 0x14, 0x0D, 0x02,
        	0x29, 0x34, 0x1F, 0x25, 0x2F, 0x37, 0x1E, 0x28,
        	0x33, 0x2D, 0x21, 0x30, 0x2C, 0x31, 0x27, 0x38,
        	0x22, 0x35, 0x2E, 0x2A, 0x32, 0x24, 0x1D, 0x20,
        };

        private static readonly byte[] gensubtable = new byte[] { //00248668
        	0x1D, 0x2E, 0x7A, 0x85, 0x3F, 0xAB, 0xD9, 0x46,
        };

        private static readonly byte[] exp_size = new byte[] { 6, 10, 12, 19, 19, 8, 7, 32 };

        private static readonly uint[] genseeds = new uint[0x20];

        private static uint g_gameid;
        private static uint g_region;
		
		        /// <summary>
        /// Last game ID parsed from an ARMAX header during decrypt.
        /// Raw 13-bit game ID (0..0x1FFF) from the verifier.
        /// </summary>
        public static uint LastGameId => g_gameid;

        /// <summary>
        /// Last region parsed from an ARMAX header during decrypt.
        /// 0 = USA, 1 = PAL, 2 = Japan (matches ConvertCore.g_region).
        /// </summary>
        public static uint LastRegion => g_region;
		
        // Flags used for folder/organizer and disc-hash entries (from cheat.c / armax.h)
        private const uint EXPANSION_DATA_FOLDER = 0x0800u;
        private const uint FLAGS_FOLDER          = 0x5u << 20;
        private const uint FLAGS_FOLDER_MEMBER   = 0x4u << 20;
        private const uint FLAGS_DISC_HASH       = 0x7u << 20;



        // Simple PRNG used in armMakeVerifier (replacement for C's rand()).
        private static readonly Random _rng = new Random();

        private static int Rand()
        {
            lock (_rng)
            {
                return _rng.Next();
            }
        }


        // ===== ARMAX crypto core routines (ported from armax.c) =====

        private static unsafe uint byteswap(uint val) { //800107E0
        	return ((val << 24) | ((val << 8) & 0x00FF0000) | ((val >> 8) & 0x0000FF00) | (val >> 24));
        }


        private static unsafe uint rotate_left(uint value, byte rotate) { //8001054C
        	return ((value << rotate) | (value >> (0x20 - rotate)));
        }


        private static unsafe uint rotate_right(uint value, byte rotate) { //80010554
        	return ((value >> rotate) | (value << (0x20 - rotate)));
        }


        private static unsafe void scramble1(uint *addr, uint *val) {
        	uint tmp;
        
        	*addr = rotate_left(*addr,4);
        
        	tmp = ((*addr^*val)&0xF0F0F0F0);
        	*val ^= tmp;
        	*addr = rotate_right((*addr^tmp),0x14);
        
        	tmp = ((*addr^*val)&0xFFFF0000);
        	*val ^= tmp;
        	*addr = rotate_right((*addr^tmp),0x12);
        
        	tmp = ((*addr^*val)&0x33333333);
        	*val ^= tmp;
        	*addr = rotate_right((*addr^tmp),6);
        
        	tmp = ((*addr^*val)&0x00FF00FF);
        	*val ^= tmp;
        	*addr = rotate_left((*addr^tmp),9);
        
        	tmp = ((*addr^*val)&0xAAAAAAAA);
        	*val = rotate_left((*val^tmp),1);
        	*addr ^= tmp;
        }


        private static unsafe void scramble2(uint *addr, uint *val) {
        	uint tmp;
        
        	*addr = rotate_right(*addr,1);
        
        	tmp = ((*addr^*val)&0xAAAAAAAA);
        	*addr ^= tmp;
        	*val = rotate_right((*val^tmp),9);
        
        	tmp = ((*addr^*val)&0x00FF00FF);
        	*addr ^= tmp;
        	*val = rotate_left((*val^tmp),6);
        
        	tmp = ((*addr^*val)&0x33333333);
        	*addr ^= tmp;
        	*val = rotate_left((*val^tmp),0x12);
        
        	tmp = ((*addr^*val)&0xFFFF0000);
        	*addr ^= tmp;
        	*val = rotate_left((*val^tmp),0x14);
        
        	tmp = ((*addr^*val)&0xF0F0F0F0);
        	*addr ^= tmp;
        	*val = rotate_right((*val^tmp),4);
        }


        private static unsafe void unscramble1(uint *addr, uint *val) {
        	uint tmp;
        
        	*val = rotate_left(*val,4);
        
        	tmp = ((*addr^*val)&0xF0F0F0F0);
        	*addr ^= tmp;
        	*val = rotate_right((*val^tmp),0x14);
        
        	tmp = ((*addr^*val)&0xFFFF0000);
        	*addr ^= tmp;
        	*val = rotate_right((*val^tmp),0x12);
        
        	tmp = ((*addr^*val)&0x33333333);
        	*addr ^= tmp;
        	*val = rotate_right((*val^tmp),6);
        
        	tmp = ((*addr^*val)&0x00FF00FF);
        	*addr ^= tmp;
        	*val = rotate_left((*val^tmp),9);
        
        	tmp = ((*addr^*val)&0xAAAAAAAA);
        	*addr = rotate_left((*addr^tmp),1);
        	*val ^= tmp;
        }


        private static unsafe void unscramble2(uint *addr, uint *val) {
        	uint tmp;
        
        	*val = rotate_right(*val,1);
        
        	tmp = ((*addr^*val)&0xAAAAAAAA);
        	*val ^= tmp;
        	*addr = rotate_right((*addr^tmp),9);
        
        	tmp = ((*addr^*val)&0x00FF00FF);
        	*val ^= tmp;
        	*addr = rotate_left((*addr^tmp),6);
        
        	tmp = ((*addr^*val)&0x33333333);
        	*val ^= tmp;
        	*addr = rotate_left((*addr^tmp),0x12);
        
        	tmp = ((*addr^*val)&0xFFFF0000);
        	*val ^= tmp;
        	*addr = rotate_left((*addr^tmp),0x14);
        
        	tmp = ((*addr^*val)&0xF0F0F0F0);
        	*val ^= tmp;
        	*addr = rotate_right((*addr^tmp),4);
        }


        private static unsafe void getcode(uint *src, uint *addr, uint *val) { //80010800
        	*addr = byteswap(src[0]);
        	*val = byteswap(src[1]);
        }


        private static unsafe void setcode(uint *dst, uint addr, uint val) { //80010868
        	dst[0] = byteswap(addr);
        	dst[1] = byteswap(val);
        }


        
        /// <summary>
        /// Lightweight bit cursor used to walk ARMAX verifier/header bitstreams.
        /// This replaces the original C code's 32-bit ctrl[4] pointer/offset
        /// array, which is not safe on 64-bit runtimes.
        /// </summary>
        private unsafe struct BitCursor
        {
            public uint* Base;   // pointer to first u32 in the buffer
            public uint  Index;  // current 32-bit word index
            public uint  Bit;    // current bit offset within the word (0..31)
            public uint  Size;   // total number of 32-bit words in the buffer
        }

        /// <summary>
        /// 1:1 logical port of:
        ///   u8 getbitstring(u32 *ctrl, u32 *out, u8 len)
        /// from armax.c, implemented in terms of <see cref="BitCursor"/>.
        /// Returns 1 on success and 0 if the cursor would run past the end of
        /// the buffer.
        /// </summary>
        private static unsafe byte getbitstring(BitCursor* ctrl, uint* output, byte len)
        {
            if (ctrl == null) throw new ArgumentNullException(nameof(ctrl));
            if (output == null) throw new ArgumentNullException(nameof(output));

            uint* ptr = ctrl->Base + ctrl->Index;
            *output = 0;

            while (len-- > 0)
            {
                if (ctrl->Bit > 0x1F)
                {
                    ctrl->Bit = 0;
                    ctrl->Index++;
                    ptr = ctrl->Base + ctrl->Index;
                }

                if (ctrl->Index >= ctrl->Size)
                    return 0;

                *output = (*output << 1) | ((*ptr >> (int)(0x1F - ctrl->Bit)) & 1u);
                ctrl->Bit++;
            }

            return 1;
        }

        /// <summary>
        /// Convert ARMAX human‑readable blocks into their packed 32‑bit form.
        ///
        /// This is a direct C# port of:
        ///   u8 alphatobin(char alpha[][14], u32 *dst, int size)
        /// from armax.c.  The <paramref name="alpha"/> parameter is modelled as
        /// a 2‑D array [row, column] where each row is a 14‑character block.
        /// </summary>
        private static unsafe byte alphatobin(char[,] alpha, uint* dst, int size)
        {
            if (alpha == null) throw new ArgumentNullException(nameof(alpha));

            int i, j = 0, k;
            uint[] bin = new uint[2];
            byte ret = 0;
            byte parity;

            while (size > 0)
            {
                // First 32‑bit word
                bin[0] = 0;
                for (i = 0; i < 6; i++)
                {
                    char ch = alpha[j >> 1, i];
                    bin[0] |= (uint)(GetVal(ch) << (((5 - i) * 5) + 2));
                }
                bin[0] |= (uint)(GetVal(alpha[j >> 1, 6]) >> 3);
                dst[j++] = bin[0];

                // Second 32‑bit word
                bin[1] = 0;
                for (i = 0; i < 6; i++)
                {
                    char ch = alpha[j >> 1, i + 6];
                    bin[1] |= (uint)(GetVal(ch) << (((5 - i) * 5) + 4));
                }
                bin[1] |= (uint)(GetVal(alpha[j >> 1, 12]) >> 1);
                dst[j++] = bin[1];

                // Verify parity bit
                k = 0;
                parity = 0;
                for (i = 0; i < 64; i++)
                {
                    if (i == 32)
                        k++;
                    parity ^= (byte)(bin[k] >> (i - (k << 5)));
                }

                if (((parity & 1) != (GetVal(alpha[(j - 2) >> 1, 12]) & 1)))
                    ret = 1;

                size--;
            }

            return ret;
        }


        /// <summary>
        /// Convert packed 32‑bit ARMAX words back into the human‑readable
        /// 13‑character + parity form.
        ///
        /// Direct port of:
        ///   void bintoalpha(char *dst, u32 *src, int index)
        /// from armax.c.
        /// </summary>
        private static unsafe void bintoalpha(char* dst, uint* src, int index)
        {
            int i, j = 0, k;
            byte parity = 0;

            if (dst == null) throw new ArgumentNullException(nameof(dst));
            if (src == null) throw new ArgumentNullException(nameof(src));

            // Convert hex directly to alphanumeric equivalent
            for (i = 0; i < 6; i++)
            {
                if (i == 4)
                {
                    dst[i + j] = '-';
                    j++;
                }

                int fIndex = (int)((src[index] >> (((5 - i) * 5) + 2)) & 0x1F);
                dst[i + j] = FilterAlphabet[fIndex];
            }

            int idxMid = (int)(((src[index] << 3) | (src[index + 1] >> 29)) & 0x1F);
            dst[i + j] = FilterAlphabet[idxMid];
            j += 6;

            for (i = 1; i < 6; i++)
            {
                if (i == 2)
                {
                    dst[i + j] = '-';
                    j++;
                }

                int fIndex = (int)((src[index + 1] >> (((5 - i) * 5) + 4)) & 0x1F);
                dst[i + j] = FilterAlphabet[fIndex];
            }
            j += 6;

            // Parity bit
            k = 0;
            for (i = 0; i < 64; i++)
            {
                if (i == 32)
                    k++;
                parity ^= (byte)(src[index + k] >> (i - (k << 5)));
            }

            int lastIndex = (int)(((src[index + 1] << 1) & 0x1E) | (parity & 1));
            dst[j] = FilterAlphabet[lastIndex];
            dst[j + 1] = '\0';
        }


        private static unsafe ushort gencrc16(uint *codes, ushort size) { //80010AE8
        	ushort ret=0;
        	byte tmp=0,tmp2;
        	int i;
        
        	if (size > 0) {
        		while (tmp < size) {
        			for (i = 0; i < 4; i++) {
        				tmp2 = (byte)(((codes[tmp] >> (i << 3)) ^ ret) & 0xFF);
        				ret = (ushort)(((crctable0[(tmp2 >> 4) & 0x0F] ^ crctable1[tmp2 & 0x0F]) ^ (ret >> 8)) & 0xFFFF);
        			}
        			tmp++;
        		}
        	}
        	return ret;
        }


        /// <summary>
        /// Generate the 32‑entry ARMAX seed table from the 16‑byte seed table.
        ///
        /// C original:
        ///   void generateseeds(u32 *seeds, const u8 *seedtable, u8 doreverse)
        /// </summary>
        private static unsafe void generateseeds(uint* seeds, byte* seedtable, byte doreverse)
        {
            if (seeds == null) throw new ArgumentNullException(nameof(seeds));
            if (seedtable == null) throw new ArgumentNullException(nameof(seedtable));

            int i, j;
            uint tmp3;
            byte[] array0 = new byte[0x38];
            byte[] array1 = new byte[0x38];
            byte[] array2 = new byte[0x08];
            byte tmp, tmp2;

            i = 0;
            while (i < 0x38)
            {
                tmp = (byte)(gentable0[i] - 1);
                array0[i++] = (byte)(((uint)(0 - (seedtable[tmp >> 3] & gentable1[tmp & 7])) >> 31));
            }

            i = 0;
            while (i < 0x10)
            {
                // memset(array2, 0, 8);
                for (int z = 0; z < 8; z++)
                    array2[z] = 0;

                tmp2 = gentable2[i];

                for (j = 0; j < 0x38; j++)
                {
                    tmp = (byte)(tmp2 + j);

                    if (j > 0x1B)
                    {
                        if (tmp > 0x37) tmp -= 0x1C;
                    }
                    else if (tmp > 0x1B)
                    {
                        tmp -= 0x1C;
                    }

                    array1[j] = array0[tmp];
                }

                for (j = 0; j < 0x30; j++)
                {
                    if (array1[gentable3[j] - 1] == 0)
                        continue;

                    tmp = (byte)(((j * 0x2AAB) >> 16) - (j >> 0x1F));
                    array2[tmp] |= (byte)(gentable1[j - (tmp * 6)] >> 2);
                }

                seeds[i << 1] = (uint)((array2[0] << 24) | (array2[2] << 16) | (array2[4] << 8) | array2[6]);
                seeds[(i << 1) + 1] = (uint)((array2[1] << 24) | (array2[3] << 16) | (array2[5] << 8) | array2[7]);
                i++;
            }

            if (doreverse == 0)
            {
                j = 0x1F;
                for (i = 0; i < 16; i += 2)
                {
                    tmp3 = seeds[i];
                    seeds[i] = seeds[j - 1];
                    seeds[j - 1] = tmp3;

                    tmp3 = seeds[i + 1];
                    seeds[i + 1] = seeds[j];
                    seeds[j] = tmp3;
                    j -= 2;
                }
            }
        }


        private static unsafe void buildseeds() { //80010BAC
        	fixed (uint* pSeeds = genseeds)
        	fixed (byte* pTable = gensubtable)
        	{
        		generateseeds(pSeeds, pTable, 0);
        	}
        }


        private static unsafe void encryptcode(uint *seeds, uint *code) {
        	uint addr,val;
        	uint tmp,tmp2;
        	int i=31;
        
        	getcode(code,&val,&addr);
        	scramble1(&addr,&val);
        	while (i >= 0) {
        		tmp2 = (addr^seeds[i--]);
        		tmp = (rotate_right(addr,4)^seeds[i--]);
        		val ^= (table6[(int)(tmp&0x3F)]^table4[(int)((tmp>>8)&0x3F)]^table2[(int)((tmp>>16)&0x3F)]^table0[(int)((tmp>>24)&0x3F)]^table7[(int)(tmp2&0x3F)]^table5[(int)((tmp2>>8)&0x3F)]^table3[(int)((tmp2>>16)&0x3F)]^table1[(int)((tmp2>>24)&0x3F)]);
        
        		tmp2 = (val^seeds[i--]);
        		tmp = (rotate_right(val,4)^seeds[i--]);
        		addr ^= (table6[(int)(tmp&0x3F)]^table4[(int)((tmp>>8)&0x3F)]^table2[(int)((tmp>>16)&0x3F)]^table0[(int)((tmp>>24)&0x3F)]^table7[(int)(tmp2&0x3F)]^table5[(int)((tmp2>>8)&0x3F)]^table3[(int)((tmp2>>16)&0x3F)]^table1[(int)((tmp2>>24)&0x3F)]);
        	}
        	scramble2(&addr,&val);
        	setcode(code,addr,val);
        }


        private static unsafe void decryptcode(uint *seeds, uint *code) {
        	uint addr,val;
        	uint tmp,tmp2;
        	int i=0;
        
        	getcode(code,&addr,&val);
        	unscramble1(&addr,&val);
        	while (i < 32) {
        		tmp = (rotate_right(val,4)^seeds[i++]);
        		tmp2 = (val^seeds[i++]);
        		addr ^= (table6[(int)(tmp&0x3F)]^table4[(int)((tmp>>8)&0x3F)]^table2[(int)((tmp>>16)&0x3F)]^table0[(int)((tmp>>24)&0x3F)]^table7[(int)(tmp2&0x3F)]^table5[(int)((tmp2>>8)&0x3F)]^table3[(int)((tmp2>>16)&0x3F)]^table1[(int)((tmp2>>24)&0x3F)]);
        
        		tmp = (rotate_right(addr,4)^seeds[i++]);
        		tmp2 = (addr^seeds[i++]);
        		val ^= (table6[(int)(tmp&0x3F)]^table4[(int)((tmp>>8)&0x3F)]^table2[(int)((tmp>>16)&0x3F)]^table0[(int)((tmp>>24)&0x3F)]^table7[(int)(tmp2&0x3F)]^table5[(int)((tmp2>>8)&0x3F)]^table3[(int)((tmp2>>16)&0x3F)]^table1[(int)((tmp2>>24)&0x3F)]);
        	}
        	unscramble2(&addr,&val);
        	setcode(code,val,addr);
        }


        
        private static unsafe byte batchencrypt(uint* codes, ushort size)
        {
            uint tmp;
            uint* ptr = codes;

            if ((codes[0] >> 28) != 0)
                return 0;

            codes[0] |= ((uint)verifycode(codes, size) << 28);

            tmp = (uint)(size >> 1);
            fixed (uint* pSeeds = genseeds)
            {
                while (tmp-- != 0)
                {
                    encryptcode(pSeeds, ptr);
                    ptr += 2;
                }
            }

            return 1;
        }
        /// <summary>
        /// 1:1 port of batchdecrypt(u32 *codes, u16 size) from armax.c.
        /// Decrypts the ARMAX blocks in-place and validates the CRC nibble in
        /// the high bits of the first word.  Returns 1 on success and 0 on
        /// CRC failure.
        /// </summary>
        private static unsafe byte batchdecrypt(uint* codes, ushort size)
        {
            if (codes == null) throw new ArgumentNullException(nameof(codes));

            // Decrypt all 64-bit blocks with the global ARMAX seeds.
            uint blocks = (uint)(size >> 1);
            uint* ptr = codes;
            fixed (uint* pSeeds = genseeds)
            {
                while (blocks-- > 0)
                {
                    decryptcode(pSeeds, ptr);
                    ptr += 2;
                }
            }

            // Read header fields via BitCursor instead of the original
            // ctrl[4] pointer hack.
            uint[] tmparray2 = new uint[8];

            BitCursor cursor;
            cursor.Base  = codes;
            cursor.Index = 0;
            cursor.Bit   = 4;      // skip CRC nibble (4 bits)
            cursor.Size  = size;   // number of u32s

            fixed (uint* pTmp2 = tmparray2)
            {
                getbitstring(&cursor, pTmp2 + 1, 13); // game id
                getbitstring(&cursor, pTmp2 + 2, 19); // code id
                getbitstring(&cursor, pTmp2 + 3, 1);  // master code
                getbitstring(&cursor, pTmp2 + 4, 1);  // unknown
                getbitstring(&cursor, pTmp2 + 5, 2);  // region
            }

            // Grab gameid and region from the last decrypted code.
            g_gameid = tmparray2[1];
            g_region = tmparray2[5];

            // CRC nibble verification (1:1 with original C).
            uint tmp = codes[0];
            codes[0] &= 0x0FFFFFFF;

            if ((tmp >> 28) != verifycode(codes, size))
                return 0;

            return 1;
        }

private static unsafe byte verifycode(uint *codes, ushort size) { //80010B70
        	ushort tmp;
        
        	tmp = gencrc16(codes,size);
        	return (byte)(((tmp >> 12) ^ (tmp >> 8) ^ (tmp >> 4) ^ tmp) & 0x0F);
        }


        /// <summary>
        /// Parse the ARMAX verifier structure and return the number of
        /// 64‑bit lines it occupies, or -1 on error.
        ///
        /// C original:
        ///   s16 armReadVerifier(u32 *code, u32 size)
        /// </summary>
        
        /// <summary>
        /// 1:1 port of armReadVerifier(u32 *code, u32 size) from armax.c,
        /// rewritten to use <see cref="BitCursor"/> instead of a 32-bit
        /// pointer/offset array.  Returns the number of 64-bit verifier
        /// lines, or -1 on failure.
        /// </summary>
        private static unsafe short armReadVerifier(uint* code, uint size)
        {
            if (code == null) throw new ArgumentNullException(nameof(code));

            uint term = 0, exp_size_ind = 0, lines = 1;
            byte ret;
            int bits = 0;

            // Set up initial cursor:
            //   - start at word index 1 (skip first u32)
            //   - bit offset 8 (skip first 8 bits in that word)
            BitCursor cursor;
            cursor.Base  = code;
            cursor.Index = 1;
            cursor.Bit   = 8;
            cursor.Size  = size;

            // See if any used bits remain.
            ret = getbitstring(&cursor, &term, 1);   // verifier terminator
            if (ret == 0)
                return -1;
            bits++;

            while (term == 0)
            {
                // Read expansion-size index (3 bits).
                ret = getbitstring(&cursor, &exp_size_ind, 3);
                if (ret == 0)
                    return -1;
                bits += 3;

                byte len = ExpSize[exp_size_ind & 7];    // mask for safety

                // Retrieve expansion data (we don't actually need to inspect it).
                if ((ret = getbitstring(&cursor, &term, len)) != 0)
                {
                    // Get next terminator value.
                    ret = getbitstring(&cursor, &term, 1);
                }

                if (ret == 0)
                    return -1;

                bits += len + 1;
            }

            // There are 24 bits available on the first line for
            // [term | size | expansion data].
            bits -= 24;
            while (bits > 0)
            {
                lines++;
                bits -= 64;  // size of a line in bits
            }

            return (short)lines;
        }

private static unsafe void armMakeVerifier(uint* code, uint num, uint* ver, uint gameid, byte region)
        {
            int j;
            byte mcode = 0;

            for (j = 0; j < num; j += 2)
            {
                if ((code[j] >> 25) == 0x62)
                    mcode = 1;
            }

            ver[0] = (gameid << 15) | (uint)(Rand() % 0x8000);
            ver[1] = ((uint)(Rand() % 0x10) << 28)
                   | 0x00800000u
                   | ((uint)mcode << 27)
                   | ((uint)region << 24);
        }

        /// <summary>
        /// 1:1 port of armBatchDecryptFull(cheat_t *cheat, u32 ar2key).
        /// Returns 0 on success, 1 on failure (same convention as original C).
        /// </summary>
        public static byte ArmBatchDecryptFull(cheat_t cheat, uint ar2key)
{
    if (cheat == null) throw new ArgumentNullException(nameof(cheat));
    if (cheat.code == null || cheat.codecnt <= 0)
        return 1;

    int size = cheat.codecnt;
    var codes = cheat.code.ToArray();

    Trace("=== Armax.ArmBatchDecryptFull ===");
    Trace("  ar2key = 0x{0:X8}, codecnt = {1}", ar2key, size);

    byte ret;
    unsafe
    {
        fixed (uint* pCodes = codes)
        {
            // Initial words as they came from the UI (after alphatobin)
            DumpWords("  initial pCodes:", pCodes, size);

            // 1) ARMAX block decrypt (text → internal 32-bit words)
            ret = batchdecrypt(pCodes, (ushort)size);
            Trace("  batchdecrypt returned {0}", ret);
            if (ret == 0)
            {
                Trace("  batchdecrypt failed, aborting.");
                return 1;
            }

            DumpWords("  after batchdecrypt:", pCodes, size);

            // 2) Parse verifier header
            short lines = armReadVerifier(pCodes, (uint)size);
            Trace("  armReadVerifier -> lines = {0}", lines);
            if (lines == -1)
            {
                Trace("  armReadVerifier returned -1, aborting.");
                return 1;
            }

            uint uLines = (uint)lines;
            uint uSize = (uint)size - (uLines << 1);
            Trace("  uLines = {0}, uSize = {1}", uLines, uSize);

            // 3) AR2 decrypt block (game codes only)
            if (uSize > 0)
            {
                uint* codePtr = pCodes + (uLines << 1);

                DumpWords("  AR2 block before byteswap:", codePtr, (int)uSize);

                // ARMAX stores these big-endian; we byteswap to host order
                for (uint i = 0; i < uSize; i++)
                    codePtr[i] = byteswap(codePtr[i]);

                DumpWords("  AR2 block after byteswap:", codePtr, (int)uSize);

                int intSize = (int)uSize;
                var segment = new uint[intSize];
                for (int i = 0; i < intSize; i++)
                    segment[i] = codePtr[i];

                Trace("  calling Ar2BatchDecryptArr(size = {0})", intSize);
                Ar2.Ar2SetSeed(ar2key);
                Ar2.Ar2BatchDecryptArr(segment, ref intSize);
                Trace("  Ar2BatchDecryptArr returned, new size = {0}", intSize);

                for (int i = 0; i < intSize; i++)
                    codePtr[i] = segment[i];

                DumpWords("  after Ar2BatchDecryptArr:", codePtr, intSize);
            }

            // Final view of the entire buffer (header + decrypted codes)
            DumpWords("  final pCodes:", pCodes, size);
        }
    }

    cheat.code.Clear();
    cheat.code.AddRange(codes);

    Trace("=== Armax.ArmBatchDecryptFull END ===");
    return 0;
}


        /// <summary>
        /// 1:1 port of armBatchEncryptFull(cheat_t *cheat, u32 ar2key).
        /// Returns 0 on success, 1 on failure.
        /// </summary>
        public static byte ArmBatchEncryptFull(cheat_t cheat, uint ar2key)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            if (cheat.code == null || cheat.codecnt <= 0)
                return 1;

            int size = cheat.codecnt;
            var codes = cheat.code.ToArray();

            byte ret;
            unsafe
            {
                fixed (uint* pCodes = codes)
                {
                    short lines = armReadVerifier(pCodes, (uint)size);
                    if (lines == -1)
                        return 1;

                    uint uLines = (uint)lines;
                    uint uSize = (uint)size - (uLines << 1);
                    if (uSize > 0)
                    {
                        uint* codePtr = pCodes + (uLines << 1);
                        int intSize = (int)uSize;
                        var segment = new uint[intSize];

                        for (int i = 0; i < intSize; i++)
                            segment[i] = codePtr[i];

                        Ar2.Ar2SetSeed(ar2key);
                        Ar2.Ar2BatchEncryptArr(segment, intSize);

                        for (int i = 0; i < intSize; i++)
                            codePtr[i] = byteswap(segment[i]);
                    }

                    byte inner = batchencrypt(pCodes, (ushort)size);
                    // ret = !(batchencrypt(...));
                    ret = (byte)(inner == 0 ? 1 : 0);
                }
            }

            cheat.code.Clear();
            cheat.code.AddRange(codes);

            return ret;
        }

        /// <summary>
        /// Safe wrapper for armReadVerifier; returns number of verifier lines or -1 on error.
        /// </summary>
        public static short ArmReadVerifier(cheat_t cheat)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            if (cheat.code == null || cheat.codecnt <= 0)
                return -1;

            var codes = cheat.code.ToArray();
            short lines;

            unsafe
            {
                fixed (uint* pCodes = codes)
                {
                    lines = armReadVerifier(pCodes, (uint)cheat.codecnt);
                }
            }

            return lines;
        }

        /// <summary>
        /// Safe wrapper for armMakeVerifier; fills verifier[0..1] with the ARMAX header pair.
        /// </summary>
        public static void ArmMakeVerifier(cheat_t cheat, uint[] verifier, uint gameid, byte region)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            if (verifier == null || verifier.Length < 2)
                throw new ArgumentException("Verifier array must have length >= 2.", nameof(verifier));

            if (cheat.code == null || cheat.codecnt <= 0)
            {
                verifier[0] = verifier[1] = 0;
                return;
            }

            var codes = cheat.code.ToArray();

            unsafe
            {
                fixed (uint* pCodes = codes)
                fixed (uint* pVer = verifier)
                {
                    armMakeVerifier(pCodes, (uint)cheat.codecnt, pVer, gameid, region);
                }
            }
        }

        
        /// <summary>
        /// 1:1 port of armEnableExpansion(cheat_t *cheat).
        /// If the cheat has at least two octets and the second octet has the
        /// expansion bit set (0x00800000), toggle that bit off.
        /// </summary>
        public static void ArmEnableExpansion(cheat_t cheat)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            if (cheat.codecnt < 2)
                return;

            // if (cheat->code[1] & 0x00800000) cheat->code[1] ^= 0x00800000;
            if ((cheat.code[1] & 0x00800000u) != 0)
                cheat.code[1] ^= 0x00800000u;
        }
        /// <summary>
        /// Managed equivalent of armMakeDiscHash(u32 *hash, HWND hwnd, char drive).
        ///
        /// Using the drive letter passed, it:
        ///  - CRCs SYSTEM.CNF on that disc
        ///  - Parses the boot parameter "cdrom0:\...;1"
        ///  - CRCs the first 256 KB of the referenced ELF
        ///  - Returns crc_elf ^ crc_sys
        ///
        /// On any error, it logs a message via Common.MsgBox and returns 0.
        /// </summary>
        public static uint ComputeDiscHash(char driveLetter)
        {
            if (driveLetter == '\0')
                return 0;

            driveLetter = char.ToUpperInvariant(driveLetter);

            const string CdromMarker = "cdrom0:\\";
            string sysCnfPath = driveLetter + ":\\SYSTEM.CNF";

            byte[] sysBytes;
            try
            {
                sysBytes = File.ReadAllBytes(sysCnfPath);
            }
            catch
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Could not open SYSTEM.CNF file. Disc is probably not a PS2 game.");
                return 0;
            }

            // CRC SYSTEM.CNF (same initial value as the C code)
            uint crcSys = Crc32.Compute(sysBytes, 0xFFFFFFFFu);

            // Parse SYSTEM.CNF as ASCII text to locate cdrom0:\...;1
            string sysText = Encoding.ASCII.GetString(sysBytes);

            int idx = sysText.IndexOf(CdromMarker, StringComparison.OrdinalIgnoreCase);
            if (idx < 0)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Could not find boot parameter in SYSTEM.CNF file.");
                return 0;
            }

            int start = idx + CdromMarker.Length;
            int end = sysText.IndexOf(";1", start, StringComparison.OrdinalIgnoreCase);
            if (end < 0)
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Could not find end of boot parameter in SYSTEM.CNF file.");
                return 0;
            }

            string elfRelative = sysText.Substring(start, end - start).Trim();
            if (string.IsNullOrEmpty(elfRelative))
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Boot parameter in SYSTEM.CNF is empty or invalid.");
                return 0;
            }

            string elfPath = driveLetter + ":\\" + elfRelative;

            byte[] elfBytes;
            try
            {
                using (var fs = new FileStream(elfPath, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    long maxRead = Math.Min((long)ELF_CRC_SIZE, fs.Length);
                    int toRead = (int)maxRead;

                    elfBytes = new byte[toRead];
                    int read = fs.Read(elfBytes, 0, toRead);
                    if (read <= 0)
                    {
                        Common.MsgBox(IntPtr.Zero, 0,
                            "Could not read ELF file indicated by SYSTEM.CNF.");
                        return 0;
                    }

                    if (read != toRead)
                    {
                        Array.Resize(ref elfBytes, read);
                    }
                }
            }
            catch
            {
                Common.MsgBox(IntPtr.Zero, 0,
                    "Could not open ELF file indicated by SYSTEM.CNF");
                return 0;
            }

            uint crcElf = Crc32.Compute(elfBytes, 0xFFFFFFFFu);

            uint hash = crcElf ^ crcSys;
            return hash;
        }

        /// <summary>
        /// 1:1 port of armMakeFolder(cheat_t *cheat, u32 gameid, u8 region).
        /// Prepends an ARMAX folder header for the given game/region, using
        /// the same randomisation as the original C code.
        /// </summary>
        public static void ArmMakeFolder(cheat_t cheat, uint gameid, byte region)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));

            unchecked
            {
                // u32 tmp = ((rand() % 0x10) << 28) | (region << 24) | (FLAGS_FOLDER) | EXPANSION_DATA_FOLDER;
                uint tmp = ((uint)(Rand() % 0x10) << 28)
                         | ((uint)region << 24)
                         | FLAGS_FOLDER
                         | EXPANSION_DATA_FOLDER;

                Cheat.cheatPrependOctet(cheat, tmp);

                // tmp = (gameid << 15) | (rand() % 0x8000);
                tmp = (gameid << 15) | (uint)(Rand() % 0x8000);
                Cheat.cheatPrependOctet(cheat, tmp);
            }
        }
#endregion

    }
}
