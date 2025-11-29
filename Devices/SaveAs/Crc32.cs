using System;

namespace OmniconvertCS
{
    /// <summary>
    /// C# port of crc32.c / crc32.h.
    /// Provides crc32(const u8 *buffer, u32 size, u32 crc) equivalent.
    /// </summary>
    internal static class Crc32
    {
        // Standard CRC-32 polynomial used by the original code.
        private const uint Poly = 0xEDB88320u;

        private static readonly uint[] Table = CreateTable();

        private static uint[] CreateTable()
        {
            var table = new uint[256];
            for (uint i = 0; i < table.Length; i++)
            {
                uint c = i;
                for (int j = 0; j < 8; j++)
                {
                    if ((c & 1) != 0)
                        c = Poly ^ (c >> 1);
                    else
                        c >>= 1;
                }

                table[i] = c;
            }

            return table;
        }

        /// <summary>
        /// Equivalent to:
        ///   u32 crc32(const u8 *buffer, u32 size, u32 crc);
        /// Caller passes the initial CRC (usually 0xFFFFFFFF), and we do the
        /// same final XOR with 0xFFFFFFFF as the C version.
        /// </summary>
        public static uint Compute(ReadOnlySpan<byte> buffer, uint crc)
        {
            for (int i = 0; i < buffer.Length; i++)
            {
                byte b = buffer[i];
                crc = Table[(crc & 0xFFu) ^ b] ^ (crc >> 8);
            }

            // return crc ^= 0xFFFFFFFF;
            return crc ^ 0xFFFFFFFFu;
        }
    }
}
