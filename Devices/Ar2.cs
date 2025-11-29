using System;
using System.Collections.Generic;

namespace OmniconvertCS
{
    /// <summary>
    /// C# port of ar2.c – Action Replay 1 & 2 encryption routines.
    /// This is intentionally very close to the original logic, but uses
    /// managed types and collections.
    /// </summary>
    public static class Ar2
{
    // AR1 seed from ar2.h
    public const uint AR1_SEED     = 0x05100518;
    public const uint AR2_KEY_ADDR = 0xDEADFACE;

    // Global 4-byte seed used by the AR1/2 encryption.
    private static readonly byte[] g_seed = new byte[4];

    // Static constructor: mirror the C default (ar2seeds = AR1_SEED; ar2SetSeed(ar2seeds);)
    static Ar2()
    {
        Ar2SetSeed(AR1_SEED);
    }

        // const u8 tbl_[4][32] – seed tables from the original C code.
        private static readonly byte[,] tbl_ = new byte[4, 32]
        {
            { 0x00, 0x1F, 0x9B, 0x69, 0xA5, 0x80, 0x90, 0xB2, 0xD7, 0x44, 0xEC, 0x75, 0x3B, 0x62, 0x0C, 0xA3, 0xA6, 0xE4, 0x1F, 0x4C, 0x05, 0xE4, 0x44, 0x6E, 0xD9, 0x5B, 0x34, 0xE6, 0x08, 0x31, 0x91, 0x72 },
            { 0x00, 0xAE, 0xF3, 0x7B, 0x12, 0xC9, 0x83, 0xF0, 0xA9, 0x57, 0x50, 0x08, 0x04, 0x81, 0x02, 0x21, 0x96, 0x09, 0x0F, 0x90, 0xC3, 0x62, 0x27, 0x21, 0x3B, 0x22, 0x4E, 0x88, 0xF5, 0xC5, 0x75, 0x91 },
            { 0x00, 0xE3, 0xA2, 0x45, 0x40, 0xE0, 0x09, 0xEA, 0x42, 0x65, 0x1C, 0xC1, 0xEB, 0xB0, 0x69, 0x14, 0x01, 0xD2, 0x8E, 0xFB, 0xFA, 0x86, 0x09, 0x95, 0x1B, 0x61, 0x14, 0x0E, 0x99, 0x21, 0xEC, 0x40 },
            { 0x00, 0x25, 0x6D, 0x4F, 0xC5, 0xCA, 0x04, 0x39, 0x3A, 0x7D, 0x0D, 0xF1, 0x43, 0x05, 0x71, 0x66, 0x82, 0x31, 0x21, 0xD8, 0xFE, 0x4D, 0xC2, 0xC8, 0xCC, 0x09, 0xA0, 0x06, 0x49, 0xD5, 0xF1, 0x83 }
        };

        private static byte NibbleFlip(byte b)
        {
            unchecked
            {
                return (byte)(((b << 4) | (b >> 4)) & 0xFF);
            }
        }

        /// <summary>
        /// Direct port of u32 ar2decrypt(u32 code, u8 type, u8 seed).
        /// </summary>
        public static uint Ar2Decrypt(uint code, byte type, byte seed)
{
    unchecked
    {
        // if (type == 7) { if (seed & 1) type = 1; else return ~code; }
        if (type == 7)
        {
            if ((seed & 1) != 0)
                type = 1;
            else
                return ~code;
        }

        // C: u8 tmp[4]; *(u32*)tmp = code;  (little-endian)
        byte[] tmp = new byte[4];
        tmp[0] = (byte)(code & 0xFF);
        tmp[1] = (byte)((code >> 8) & 0xFF);
        tmp[2] = (byte)((code >> 16) & 0xFF);
        tmp[3] = (byte)((code >> 24) & 0xFF);

        int s = seed & 0x1F; // safe index into tbl_[*, 0..31]

        switch (type)
        {
            case 0:
                // tmp[3] ^= tbl_[0][seed]; tmp[2] ^= tbl_[1][seed];
                // tmp[1] ^= tbl_[2][seed]; tmp[0] ^= tbl_[3][seed];
                tmp[3] = (byte)(tmp[3] ^ tbl_[0, s]);
                tmp[2] = (byte)(tmp[2] ^ tbl_[1, s]);
                tmp[1] = (byte)(tmp[1] ^ tbl_[2, s]);
                tmp[0] = (byte)(tmp[0] ^ tbl_[3, s]);
                break;

            case 1:
                // tmp[3] = nibble_flip(tmp[3]) ^ tbl_[0][seed];
                // tmp[2] = nibble_flip(tmp[2]) ^ tbl_[2][seed];
                // tmp[1] = nibble_flip(tmp[1]) ^ tbl_[3][seed];
                // tmp[0] = nibble_flip(tmp[0]) ^ tbl_[1][seed];
                tmp[3] = (byte)(NibbleFlip(tmp[3]) ^ tbl_[0, s]);
                tmp[2] = (byte)(NibbleFlip(tmp[2]) ^ tbl_[2, s]);
                tmp[1] = (byte)(NibbleFlip(tmp[1]) ^ tbl_[3, s]);
                tmp[0] = (byte)(NibbleFlip(tmp[0]) ^ tbl_[1, s]);
                break;

            case 2:
                // tmp[3] += tbl_[0][seed]; ... etc.
                tmp[3] = (byte)(tmp[3] + tbl_[0, s]);
                tmp[2] = (byte)(tmp[2] + tbl_[1, s]);
                tmp[1] = (byte)(tmp[1] + tbl_[2, s]);
                tmp[0] = (byte)(tmp[0] + tbl_[3, s]);
                break;

            case 3:
                // tmp[3] -= tbl_[3][seed]; ... etc.
                tmp[3] = (byte)(tmp[3] - tbl_[3, s]);
                tmp[2] = (byte)(tmp[2] - tbl_[2, s]);
                tmp[1] = (byte)(tmp[1] - tbl_[1, s]);
                tmp[0] = (byte)(tmp[0] - tbl_[0, s]);
                break;

            case 4:
                // tmp[3] = (tmp[3] ^ tbl_[0][seed]) + tbl_[0][seed]; etc.
                tmp[3] = (byte)(((byte)(tmp[3] ^ tbl_[0, s])) + tbl_[0, s]);
                tmp[2] = (byte)(((byte)(tmp[2] ^ tbl_[3, s])) + tbl_[3, s]);
                tmp[1] = (byte)(((byte)(tmp[1] ^ tbl_[1, s])) + tbl_[1, s]);
                tmp[0] = (byte)(((byte)(tmp[0] ^ tbl_[2, s])) + tbl_[2, s]);
                break;

            case 5:
                // tmp[3] = (tmp[3] - tbl_[1][seed]) ^ tbl_[0][seed]; etc.
                tmp[3] = (byte)(((byte)(tmp[3] - tbl_[1, s])) ^ tbl_[0, s]);
                tmp[2] = (byte)(((byte)(tmp[2] - tbl_[2, s])) ^ tbl_[1, s]);
                tmp[1] = (byte)(((byte)(tmp[1] - tbl_[3, s])) ^ tbl_[2, s]);
                tmp[0] = (byte)(((byte)(tmp[0] - tbl_[0, s])) ^ tbl_[3, s]);
                break;

            case 6:
                // tmp[3] += tbl_[0][seed];
                // tmp[2] -= tbl_[1][(seed + 1) & 0x1F];
                // tmp[1] += tbl_[2][(seed + 2) & 0x1F];
                // tmp[0] -= tbl_[3][(seed + 3) & 0x1F];
                tmp[3] = (byte)(tmp[3] + tbl_[0, s]);
                tmp[2] = (byte)(tmp[2] - tbl_[1, (s + 1) & 0x1F]);
                tmp[1] = (byte)(tmp[1] + tbl_[2, (s + 2) & 0x1F]);
                tmp[0] = (byte)(tmp[0] - tbl_[3, (s + 3) & 0x1F]);
                break;
        }

        // return *(u32*)tmp;  (little-endian)
        return (uint)(tmp[0]
                     | (tmp[1] << 8)
                     | (tmp[2] << 16)
                     | (tmp[3] << 24));
    }
}


        /// <summary>
        /// Direct port of u32 ar2encrypt(u32 code, u8 type, u8 seed).
        /// </summary>
        public static uint Ar2Encrypt(uint code, byte type, byte seed)
{
    unchecked
    {
        if (type == 7)
        {
            if ((seed & 1) != 0)
                type = 1;
            else
                return ~code;
        }

        byte[] tmp = new byte[4];
        tmp[0] = (byte)(code & 0xFF);
        tmp[1] = (byte)((code >> 8) & 0xFF);
        tmp[2] = (byte)((code >> 16) & 0xFF);
        tmp[3] = (byte)((code >> 24) & 0xFF);

        int s = seed & 0x1F;

        switch (type)
        {
            case 0:
                // same as decrypt case 0
                tmp[3] = (byte)(tmp[3] ^ tbl_[0, s]);
                tmp[2] = (byte)(tmp[2] ^ tbl_[1, s]);
                tmp[1] = (byte)(tmp[1] ^ tbl_[2, s]);
                tmp[0] = (byte)(tmp[0] ^ tbl_[3, s]);
                break;

            case 1:
                // tmp[3] = nibble_flip(tmp[3] ^ tbl_[0][seed]); etc.
                tmp[3] = NibbleFlip((byte)(tmp[3] ^ tbl_[0, s]));
                tmp[2] = NibbleFlip((byte)(tmp[2] ^ tbl_[2, s]));
                tmp[1] = NibbleFlip((byte)(tmp[1] ^ tbl_[3, s]));
                tmp[0] = NibbleFlip((byte)(tmp[0] ^ tbl_[1, s]));
                break;

            case 2:
                // tmp[3] -= tbl_[0][seed]; ... etc.
                tmp[3] = (byte)(tmp[3] - tbl_[0, s]);
                tmp[2] = (byte)(tmp[2] - tbl_[1, s]);
                tmp[1] = (byte)(tmp[1] - tbl_[2, s]);
                tmp[0] = (byte)(tmp[0] - tbl_[3, s]);
                break;

            case 3:
                // tmp[3] += tbl_[3][seed]; ... etc.
                tmp[3] = (byte)(tmp[3] + tbl_[3, s]);
                tmp[2] = (byte)(tmp[2] + tbl_[2, s]);
                tmp[1] = (byte)(tmp[1] + tbl_[1, s]);
                tmp[0] = (byte)(tmp[0] + tbl_[0, s]);
                break;

            case 4:
                // tmp[3] = (tmp[3] - tbl_[0][seed]) ^ tbl_[0][seed]; etc.
                tmp[3] = (byte)(((byte)(tmp[3] - tbl_[0, s])) ^ tbl_[0, s]);
                tmp[2] = (byte)(((byte)(tmp[2] - tbl_[3, s])) ^ tbl_[3, s]);
                tmp[1] = (byte)(((byte)(tmp[1] - tbl_[1, s])) ^ tbl_[1, s]);
                tmp[0] = (byte)(((byte)(tmp[0] - tbl_[2, s])) ^ tbl_[2, s]);
                break;

            case 5:
                // tmp[3] = (tmp[3] ^ tbl_[0][seed]) + tbl_[1][seed]; etc.
                tmp[3] = (byte)(((byte)(tmp[3] ^ tbl_[0, s])) + tbl_[1, s]);
                tmp[2] = (byte)(((byte)(tmp[2] ^ tbl_[1, s])) + tbl_[2, s]);
                tmp[1] = (byte)(((byte)(tmp[1] ^ tbl_[2, s])) + tbl_[3, s]);
                tmp[0] = (byte)(((byte)(tmp[0] ^ tbl_[3, s])) + tbl_[0, s]);
                break;

            case 6:
                // tmp[3] -= tbl_[0][seed];
                // tmp[2] += tbl_[1][(seed + 1) & 0x1F];
                // tmp[1] -= tbl_[2][(seed + 2) & 0x1F];
                // tmp[0] += tbl_[3][(seed + 3) & 0x1F];
                tmp[3] = (byte)(tmp[3] - tbl_[0, s]);
                tmp[2] = (byte)(tmp[2] + tbl_[1, (s + 1) & 0x1F]);
                tmp[1] = (byte)(tmp[1] - tbl_[2, (s + 2) & 0x1F]);
                tmp[0] = (byte)(tmp[0] + tbl_[3, (s + 3) & 0x1F]);
                break;
        }

        return (uint)(tmp[0]
                     | (tmp[1] << 8)
                     | (tmp[2] << 16)
                     | (tmp[3] << 24));
    }
}
        /// <summary>
        /// Port of void ar2SetSeed(u32 key).
        /// Stores key bytes into g_seed after a byte swap.
        /// </summary>
        public static void Ar2SetSeed(uint key)
{
    unchecked
    {
        // C: key = swapbytes(key); memcpy(g_seed, &key, 4);
        key = Common.swapbytes(key);

        // LSB-first, like memcpy() on a little-endian machine
        g_seed[0] = (byte)(key & 0xFF);
        g_seed[1] = (byte)((key >> 8) & 0xFF);
        g_seed[2] = (byte)((key >> 16) & 0xFF);
        g_seed[3] = (byte)((key >> 24) & 0xFF);
    }
}

public static uint Ar2GetSeed()
{
    unchecked
    {
        // C: memcpy(&key, g_seed, 4); key = swapbytes(key);
        uint key = (uint)(g_seed[0]
                        | (g_seed[1] << 8)
                        | (g_seed[2] << 16)
                        | (g_seed[3] << 24));
        return Common.swapbytes(key);
    }
}
        /// <summary>
        /// Port of void ar2BatchDecryptArr(u32 *code, u32 *size).
        /// </summary>
        public static void Ar2BatchDecryptArr(uint[] code, ref int size)
        {
            if (code is null) throw new ArgumentNullException(nameof(code));
            if (size < 0 || size > code.Length) throw new ArgumentOutOfRangeException(nameof(size));

            unchecked
            {
                int i = 0;
                while (i < size)
                {
                    int i1 = i + 1;
                    if (i1 >= size) break;

                    code[i]  = Ar2Decrypt(code[i],  g_seed[0], g_seed[1]);
                    code[i1] = Ar2Decrypt(code[i1], g_seed[2], g_seed[3]);

                    if (code[i] == 0xDEADFACEu)
                    {
                        Ar2SetSeed(code[i1]);
                        int j = i + 2;
                        if (j < size)
                        {
                            for (; j < size; j++)
                            {
                                code[j - 2] = code[j];
                            }
                        }

                        i -= 2;
                        size -= 2;
                        if (i < 0) i = 0;
                        continue;
                    }

                    i += 2;
                }
            }
        }

        /// <summary>
        /// Port of void ar2BatchDecrypt(cheat_t *cheat).
        /// </summary>
        public static void Ar2BatchDecrypt(cheat_t cheat)
        {
            if (cheat is null) throw new ArgumentNullException(nameof(cheat));

            unchecked
            {
                int i = 0;
                while (i + 1 < cheat.code.Count)
                {
                    cheat.code[i]     = Ar2Decrypt(cheat.code[i],     g_seed[0], g_seed[1]);
                    cheat.code[i + 1] = Ar2Decrypt(cheat.code[i + 1], g_seed[2], g_seed[3]);

                    if (cheat.code[i] == 0xDEADFACEu)
                    {
                        Ar2SetSeed(cheat.code[i + 1]);
                        // Remove the two octets starting at i+1
                        Cheat.cheatRemoveOctets(cheat, i + 1, 2);
                        i -= 2;
                        if (i < 0) i = 0;
                        continue;
                    }

                    i += 2;
                }
            }
        }

        /// <summary>
        /// Port of void ar2BatchEncryptArr(u32 *code, u32 *size).
        /// </summary>
        public static void Ar2BatchEncryptArr(uint[] code, int size)
        {
            if (code is null) throw new ArgumentNullException(nameof(code));
            if (size < 0 || size > code.Length) throw new ArgumentOutOfRangeException(nameof(size));

            unchecked
            {
                uint seed = 0xFFFFFFFFu;
                for (int i = 0; i + 1 < size; i += 2)
                {
                    if (code[i] == 0xDEADFACEu)
                        seed = code[i + 1];

                    code[i]     = Ar2Encrypt(code[i],     g_seed[0], g_seed[1]);
                    code[i + 1] = Ar2Encrypt(code[i + 1], g_seed[2], g_seed[3]);

                    if (seed != 0xFFFFFFFFu)
                        Ar2SetSeed(seed);
                }
            }
        }

        /// <summary>
        /// Port of void ar2BatchEncrypt(cheat_t *cheat).
        /// </summary>
        public static void Ar2BatchEncrypt(cheat_t cheat)
        {
            if (cheat is null) throw new ArgumentNullException(nameof(cheat));

            unchecked
            {
                uint seed = 0xFFFFFFFFu;
                for (int i = 0; i + 1 < cheat.code.Count; i += 2)
                {
                    if (cheat.code[i] == 0xDEADFACEu)
                        seed = cheat.code[i + 1];

                    cheat.code[i]     = Ar2Encrypt(cheat.code[i],     g_seed[0], g_seed[1]);
                    cheat.code[i + 1] = Ar2Encrypt(cheat.code[i + 1], g_seed[2], g_seed[3]);

                    if (seed != 0xFFFFFFFFu)
                        Ar2SetSeed(seed);
                }
            }
        }

        /// <summary>
        /// Port of AR1 batch decrypt/encrypt helpers.
        /// </summary>
        public static void Ar1BatchDecrypt(cheat_t cheat)
        {
            if (cheat is null) throw new ArgumentNullException(nameof(cheat));

            unchecked
            {
                uint hold = Ar2GetSeed();
                Ar2SetSeed(AR1_SEED);

                for (int i = 0; i + 1 < cheat.code.Count; i += 2)
                {
                    cheat.code[i]     = Ar2Decrypt(cheat.code[i],     g_seed[0], g_seed[1]);
                    cheat.code[i + 1] = Ar2Decrypt(cheat.code[i + 1], g_seed[2], g_seed[3]);
                }

                Ar2SetSeed(hold);
            }
        }

        public static void Ar1BatchEncrypt(cheat_t cheat)
        {
            if (cheat is null) throw new ArgumentNullException(nameof(cheat));

            unchecked
            {
                uint hold = Ar2GetSeed();
                Ar2SetSeed(AR1_SEED);

                for (int i = 0; i + 1 < cheat.code.Count; i += 2)
                {
                    cheat.code[i]     = Ar2Encrypt(cheat.code[i],     g_seed[0], g_seed[1]);
                    cheat.code[i + 1] = Ar2Encrypt(cheat.code[i + 1], g_seed[2], g_seed[3]);
                }

                Ar2SetSeed(hold);
            }
        }

        /// <summary>
        /// Port of u8 ar2AddKeyCode(cheat_t *cheat).
        /// </summary>
        public static byte Ar2AddKeyCode(cheat_t cheat)
        {
            if (cheat is null) throw new ArgumentNullException(nameof(cheat));

            unchecked
            {
                int num = cheat.code.Count;
                byte ret = 0;

                for (int i = 0; i < num && ret == 0; i += 2)
                {
                    if (cheat.code[i] == AR2_KEY_ADDR)
                        return 1;

                    uint tmp = cheat.code[i] >> 28;
                    if (tmp == 0xFu || tmp == 0x8u)
                        ret = 1;
                }

                if (ret != 0)
                {
                    uint tmp = Ar2GetSeed();
                    Cheat.cheatPrependOctet(cheat, tmp);
                    Cheat.cheatPrependOctet(cheat, AR2_KEY_ADDR);
                    Ar2SetSeed(AR1_SEED);
                }

                return ret;
            }
        }
    }
}
