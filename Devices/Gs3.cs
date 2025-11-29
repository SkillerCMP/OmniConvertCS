
using System;

namespace OmniconvertCS
{
    /// <summary>
    /// GameShark Version 3+ Crypt, CRC, and Utility routines (C# port of gs3.c).
    /// </summary>
    public static class Gs3
    {
        // mask for the msb/sign bit
        private const uint SMASK = 0x80000000u;

        // mask for the lower bits of a word.
        private const uint LMASK = 0x7FFFFFFFu;

        // Size of the state vector
        private const int MT_STATE_SIZE = 624;

        // Magical Mersenne masks
        private const uint MT_MASK_B = 0x9D2C5680u;
        private const uint MT_MASK_C = 0xEFC60000u;

        // Offset for recurrence relationship
        private const int MT_REC_OFFSET = 397;
        private const int MT_BREAK_OFFSET = 227;

        private const uint MT_MULTIPLIER = 69069u;

        private enum CryptIndex
        {
            GS3_CRYPT_X1 = 0,
            GS3_CRYPT_2 = 1,
            GS3_CRYPT_1 = 2,
            GS3_CRYPT_3 = 3,
            GS3_CRYPT_4 = 3,
            GS3_CRYPT_X2 = 3
        }

        // maximum number of crypt tables required for encryption
        private const int GS3_NUM_TABLES = 4;

        private const uint DECRYPT_MASK = 0xF1FFFFFFu;

        private static uint BYTES_TO_WORD(uint a, uint b, uint c, uint d)
        {
            return (a << 24) | (b << 16) | (c << 8) | d;
        }

        private static uint BYTES_TO_HALF(uint a, uint b)
        {
            return (a << 8) | b;
        }

        private static uint GETBYTE(uint word, int num)
        {
            return (word >> ((num - 1) << 3)) & 0xFFu;
        }

        private struct CryptTable
        {
            public int size;
            public byte[] table;

            public CryptTable(int sz)
            {
                size = sz;
                table = new byte[0x100];
            }
        }

        private static int passcount = MT_STATE_SIZE;
        private static readonly uint[] gs3_mtStateTbl_code = new uint[MT_STATE_SIZE];
        private static readonly uint[] gs3_mtStateTbl_p2m = new uint[MT_STATE_SIZE];
        private static readonly uint[] gs3_decision_matrix = { 0u, 0x9908B0DFu };
        private static readonly uint[] gs3_hseeds = { 0x6C27u, 0x1D38u, 0x7FE1u };

        private static readonly CryptTable[] gs3_encrypt_seeds =
        {
            new CryptTable(0x40),
            new CryptTable(0x39),
            new CryptTable(0x19),
            new CryptTable(0x100)
        };

        private static readonly CryptTable[] gs3_decrypt_seeds =
        {
            new CryptTable(0x40),
            new CryptTable(0x39),
            new CryptTable(0x19),
            new CryptTable(0x100)
        };

        private static byte gs3_init;
        private static byte gs3_linetwo;
        private static byte gs3_xkey; // key for second line of two-line code types

        // CCITT CRC and version-5 flag
        private static readonly ushort[] ccitt_table = new ushort[256];
        private static ushort ccitt_poly = 0x1021;
        private static byte gs3Version5;

        // Fire's CRC32 variation
        private const uint GS3_POLY = 0x04C11DB7u;
        private static readonly uint[] gs3_crc32tab = new uint[256];

        // === Version-5 helpers ===

        public static void SetVersion5(byte flag)
        {
            gs3Version5 = flag;
        }

        public static byte IsVersion5()
        {
            return gs3Version5;
        }

        private static bool IsVersion5Bool => gs3Version5 != 0;

        // === CCITT CRC ===

        private static void CcittBuildSeeds()
{
    short tmp;
    for (int i = 0; i < 256; i++)
    {
        // C: tmp = i << 8;
        tmp = (short)((i << 8) & 0xFFFF);

        for (int j = 0; j < 8; j++)
        {
            if ((tmp & 0x8000) != 0)
            {
                // C: tmp = ((tmp << 1) ^ ccitt_poly) & 0xFFFF;
                tmp = (short)(((tmp << 1) ^ ccitt_poly) & 0xFFFF);
            }
            else
            {
                // C: tmp <<= 1; (implicitly 16-bit)
                tmp = (short)((tmp << 1) & 0xFFFF);
            }
        }

        ccitt_table[i] = (ushort)(tmp & 0xFFFF);
    }
}

        public static ushort CcittCrc(byte[] bytes, int bytecount)
        {
            if (bytecount == 0 || bytes == null)
                return 0;

            ushort crc = 0;
            for (int j = 0; j < bytecount; j++)
            {
                crc = (ushort)(((crc << 8) ^ ccitt_table[((crc >> 8) ^ (0xFF & bytes[j])) & 0xFF]) & 0xFFFF);
            }
            return crc;
        }

        public static ushort GenCrc(uint[] code, int count)
{
    if (code == null || count <= 0)
        return 0;

    var swap = new uint[count];
    for (int i = 0; i < count; i++)
    {
        swap[i] = Common.swapbytes(code[i]);
    }

    // Emulate the C behaviour: bytes = (u8*)swap; on a little-endian machine
    // i.e. little-endian view of the swapped 32-bit words.
    byte[] bytes = new byte[count * 4];
    for (int i = 0; i < count; i++)
    {
        uint v = swap[i];
        bytes[(i * 4) + 0] = (byte)(v & 0xFF);
        bytes[(i * 4) + 1] = (byte)((v >> 8) & 0xFF);
        bytes[(i * 4) + 2] = (byte)((v >> 16) & 0xFF);
        bytes[(i * 4) + 3] = (byte)((v >> 24) & 0xFF);
    }

    return CcittCrc(bytes, bytes.Length);
}


        // === CRC32 (Fire variation) ===

        public static void GenCrc32Tab()
        {
            uint crc;
            for (uint i = 0; i < 256; i++)
            {
                crc = i << 24;
                for (int j = 0; j < 8; j++)
                {
                    crc = ((crc & SMASK) == 0) ? (crc << 1) : ((crc << 1) ^ GS3_POLY);
                }
                gs3_crc32tab[i] = crc;
            }
        }

        public static uint Crc32(byte[] data, uint size)
        {
            if (data == null || size == 0 || size == 0xFFFFFFFFu)
                return 0;

            uint crc = 0;
            for (uint i = 0; i < size; i++)
            {
                crc = (crc << 8) ^ gs3_crc32tab[((crc >> 24) ^ data[i]) & 0xFF];
            }
            return crc;
        }

        public static uint GenCrc32(byte[] data, uint size)
        {
            GenCrc32Tab();
            return Crc32(data, size);
        }

        // === Mersenne Twister-like state ===

        public static void InitMtStateTbl(uint[] mtStateTbl, uint seed)
        {
            if (mtStateTbl == null || mtStateTbl.Length < MT_STATE_SIZE)
                throw new ArgumentException("mtStateTbl must have length >= MT_STATE_SIZE");

            uint wseed = seed;
            uint mask = 0xFFFF0000u;
            for (int i = 0; i < MT_STATE_SIZE; i++)
            {
                uint ms = wseed & mask;
                wseed = wseed * MT_MULTIPLIER + 1;
                uint ls = wseed & mask;
                mtStateTbl[i] = (ls >> 16) | ms;
                wseed = wseed * MT_MULTIPLIER + 1;
            }
            passcount = MT_STATE_SIZE;
        }

        public static uint GetMtNum(uint[] mtStateTbl)
        {
            if (mtStateTbl == null || mtStateTbl.Length < MT_STATE_SIZE)
                throw new ArgumentException("mtStateTbl must have length >= MT_STATE_SIZE");

            uint tmp;
            int off = MT_REC_OFFSET;

            if (passcount >= MT_STATE_SIZE)
            {
                if (passcount == MT_STATE_SIZE + 1)
                {
                    InitMtStateTbl(mtStateTbl, 0x1105);
                }

                // Twist
                for (int i = 0; i < MT_STATE_SIZE - 1; i++)
                {
                    if (i == MT_BREAK_OFFSET)
                        off = -MT_BREAK_OFFSET;

                    tmp = (mtStateTbl[i] & SMASK) | (mtStateTbl[i + 1] & LMASK);
                    mtStateTbl[i] = (tmp >> 1) ^ mtStateTbl[i + off] ^ gs3_decision_matrix[tmp & 1];
                }

                tmp = (mtStateTbl[0] & LMASK) | (mtStateTbl[MT_STATE_SIZE - 1] & SMASK);
                mtStateTbl[MT_STATE_SIZE - 1] =
                    (tmp >> 1) ^ mtStateTbl[MT_REC_OFFSET - 1] ^ gs3_decision_matrix[tmp & 1];

                passcount = 0;
            }

            tmp = mtStateTbl[passcount];
            tmp ^= tmp >> 11;
            tmp ^= (tmp << 7) & MT_MASK_B;
            tmp ^= (tmp << 15) & MT_MASK_C;
            tmp ^= tmp >> 18;

            passcount++;
            return tmp;
        }

        // === Seed table helpers ===

        public static void BuildByteSeedTbl(byte[] tbl, uint seed, int size)
        {
            if (tbl == null)
                throw new ArgumentNullException(nameof(tbl));

            if (size <= 0)
            {
                InitMtStateTbl(gs3_mtStateTbl_code, seed);
                return;
            }

            for (int i = 0; i < size; i++)
                tbl[i] = (byte)i;

            InitMtStateTbl(gs3_mtStateTbl_code, seed);

            int passes = size << 1;
            while (passes-- > 0)
            {
                int idx1 = (int)(GetMtNum(gs3_mtStateTbl_code) % (uint)size);
                int idx2 = (int)(GetMtNum(gs3_mtStateTbl_code) % (uint)size);

                byte tmp = tbl[idx1];
                tbl[idx1] = tbl[idx2];
                tbl[idx2] = tmp;
            }
        }

        public static void ReverseSeeds(byte[] dest, byte[] src, int size)
        {
            if (dest == null) throw new ArgumentNullException(nameof(dest));
            if (src == null) throw new ArgumentNullException(nameof(src));

            for (int i = 0; i < size; i++)
            {
                dest[src[i]] = (byte)i;
            }
        }

        public static uint BuildSeeds(uint seed1, uint seed2, uint seed3)
        {
            for (int i = 0; i < GS3_NUM_TABLES; i++)
            {
                int shift = 5 - i;
                if (shift < 3)
                    shift = 0;

                uint s1Shifted = seed1 >> shift;
                BuildByteSeedTbl(gs3_encrypt_seeds[i].table, s1Shifted, gs3_encrypt_seeds[i].size);
                ReverseSeeds(gs3_decrypt_seeds[i].table, gs3_encrypt_seeds[i].table, gs3_encrypt_seeds[i].size);
            }

            uint ret = (uint)(((((seed1 >> 8) | 1) + seed2 - seed3)
                               - (((seed2 >> 8) & 0xFF) >> 2))
                               + (seed1 & 0xFC) + (seed3 >> 8) + 0xFF9A);
            return ret & 0xFF;
        }

        // === Initialization and seed update ===

        public static void Init()
        {
            if (gs3_init == 0)
            {
                CcittBuildSeeds();
            }
            BuildSeeds(gs3_hseeds[0], gs3_hseeds[1], gs3_hseeds[2]);
            gs3_init = 1;
        }

        public static int Update(uint[] seeds)
        {
            if (seeds == null || seeds.Length < 2)
                return -1;

            uint s0 = seeds[0] & 0xFFFF;
            uint s1 = (seeds[0] >> 16) & 0xFFFF;
            uint s2 = seeds[1] & 0xFFFF;

            uint newHigh = BuildSeeds(s0, s1, s2);
            seeds[0] = (seeds[0] & 0xFF00FFFFu) | (newHigh << 16);
            return 0;
        }

        // === Line decrypt ===

        public static byte Decrypt(ref uint address, ref uint value)
        {
            unchecked
            {
                	uint command, tmp = 0, tmp2 = 0, mask = 0;
                	uint flag; uint commandKey; byte[] seedtbl;
                	int size, i = 0;
	
                	//First, check to see if this is the second line
                	//  of a two-line code type.
                	if (gs3_linetwo != 0) {
                	  switch(gs3_xkey) {
                		case 1:
                		case 2: {  //commandKey x1 - second line for commandKey 1 and 2.
                			seedtbl = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_X1].table;
                			size = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_X1].size;
                			for(i = 0; i < size; i++) {
                				flag =  i < 32 ?
                					((value ^ (gs3_hseeds[1] << 13)) >> i) & 1 :
                					(((address ^ gs3_hseeds[1] << 2)) >> (i - 32)) & 1;
                				if(flag > 0) {
                					if(seedtbl[i] < 32) {
                						tmp2 |= (1u << seedtbl[i]);
                					}
                					else {
                						tmp |= (1u << (seedtbl[i] - 32));
                					}
                				}
                			}
                			address = tmp ^ (gs3_hseeds[2] << 3);
                			value = tmp2 ^ gs3_hseeds[2];
                			gs3_linetwo = 0;
                			return 0;
                		}
                		case 3:
                		case 4: {  //commandKey x2 - second line for commandKey 3 and 4.
                			seedtbl = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_X2].table;
                			tmp = address ^ (gs3_hseeds[1] << 3);
                			address = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
                					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
                					^ (gs3_hseeds[2] << 16);
                			tmp = value ^ (gs3_hseeds[1] << 9);
                			value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
                					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
                					^ (gs3_hseeds[2] << 5);
                			gs3_linetwo = 0;
                			return 0;
                		}
                		default: {
                			gs3_linetwo = 0;
                			return 0;
                		}
                	  }
                	}
	
                	//Now do normal encryptions.
                	command = address & 0xFE000000;
                	commandKey = (address >> 25) & 0x7;
                	if(command >= 0x30000000 && command <= 0x6FFFFFFF) {
                		gs3_linetwo = 1;
                		gs3_xkey = (byte)commandKey;
                	}
	
                	if((command >> 28) == 7) return 0;  //do nothing on verifier lines.

                	switch(commandKey) {
                		case 0: //Key 0.  Raw.
                			break;
                		case 1: {  //Key 1.  This commandKey is used for codes built into GS/XP cheat discs.
                					//  You typically cannot see these codes.
                			seedtbl = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_1].table;
                			size = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_1].size;
                			tmp = (address & 0x01FFFFFF) ^ (gs3_hseeds[1] << 8);
                			mask = 0;
                			for(i = 0; i < size; i++) {
                				mask |= ((tmp & (1u << i)) >> i) << seedtbl[i];
                			}
                			address = ((mask ^ gs3_hseeds[2]) | command) & DECRYPT_MASK;
                			break;
                		}
                		case 2: {  //Key 2.  The original encryption used publicly.  Fell into disuse 
                					//around August, 2004.
                			seedtbl = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_2].table;
                			size = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_2].size;
                			address = (address & 0x01FFFFFF) ^ (gs3_hseeds[1] << 1);
                			tmp = tmp2 = 0;
                			for(i = 0; i < size; i++) {
                				flag = (i < 32) ? ((value  ^ (gs3_hseeds[1] << 16)) >> i) & 1 :
                					(address >> (i - 32)) & 1;
                				if(flag > 0) {
                					if(seedtbl[i] < 32) {
                						tmp2 |= (1u << seedtbl[i]);
                					}
                					else {
                						tmp |= (1u << (seedtbl[i] - 32));
                					}
                				}
                			}
                			address = ((tmp ^ (gs3_hseeds[2] << 8)) | command) & DECRYPT_MASK;
                			value = tmp2 ^ gs3_hseeds[2];
                			break;
                		}
                		case 3: {  //Key 3.  Unused publicly.
                			seedtbl = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_3].table;
                			tmp = address ^ (gs3_hseeds[1] << 8);
                			address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 1)], seedtbl[GETBYTE(tmp, 2)])
                				| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[2] << 4)) & DECRYPT_MASK;
                			break;
                		}
                		case 4: {  //Key 4.  Second encryption used publicly.
                			seedtbl = gs3_decrypt_seeds[(int)CryptIndex.GS3_CRYPT_4].table;
                			tmp = address ^ (gs3_hseeds[1] << 8);
                			address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
                					| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[2] << 4)) & DECRYPT_MASK;
                			tmp = value ^ (gs3_hseeds[1] << 9);
                			value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
                						seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
                						^ (gs3_hseeds[2] << 5);
                			break;
                		}
                		case 5:
                		case 6:	//Key 5 and Key 6 appear to be nonexistent routines.  Entering codes that use
                			//  commandKey 6 into GSv3 and XPv4 result in code entries that cannot be activated except
                			//  with much effort and luck.
                			break;
                		case 7: { //seed refresh
                			uint[] code = new uint[] { address, value };
                			Update(code);
                			break;
                		}
                		default:
                			break;
                	}
                	return 0;
            }
        }

        // === Line encrypt ===

        public static byte Encrypt(ref uint address, ref uint value, uint key)
        {
            unchecked
            {
                	uint command = address & 0xFE000000;
                	uint tmp = 0, tmp2 = 0, mask = 0;
                	uint flag; byte[] seedtbl;
                	int size, i = 0;
                	if (!(key > 0 && key < 8)) {
                		key = ((address) >> 25) & 0x7;
                	}

                	//First, check to see if this is the second line
                	//  of a two-line code type.
                	if (gs3_linetwo != 0) {
                	  switch(gs3_xkey) {
                		case 1:
                		case 2: { //key x1 - second line for key 1 and 2.
                			seedtbl	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_X1].table;
                			size	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_X1].size;
                			address = address ^ (gs3_hseeds[2] << 3);
                			value = value ^ gs3_hseeds[2];
                			for(i = 0; i < size; i++) {
                				flag = (i < 32) ? (value >> i) & 1 : (address >> (i - 32)) & 1;
                				if(flag > 0) {
                					if(seedtbl[i] < 32) {
                						tmp2 |= (1u << seedtbl[i]);
                					}
                					else {
                						tmp |= (1u << (seedtbl[i] - 32));
                					}
                				}
                			}
                			address = tmp ^ (gs3_hseeds[1] << 2);
                			value = tmp2 ^ (gs3_hseeds[1] << 13);
                			gs3_linetwo = 0;
                			return 0;
                		}
                		case 3:
                		case 4: {  //key x2 - second line for key 3 and 4.
                			seedtbl	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_X2].table;
                			tmp = address ^ (gs3_hseeds[2] << 16);
                			address = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
                					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
                					^ (gs3_hseeds[1] << 3);
                			tmp = value ^ (gs3_hseeds[2] << 5);
                			value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
                					seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
                					^ (gs3_hseeds[1] << 9);
                			gs3_linetwo = 0;
                			return 0;
                		}
                		default: {
                			gs3_linetwo = 0;
                			return 0;
                		}
                	  }
                	}

                	if(command >= 0x30000000 && command <= 0x6FFFFFFF) {
                		gs3_linetwo = 1;
                		gs3_xkey = (byte)key;
                	}

                	switch(key) {
                		case 0: //Raw
                			break;
                		case 1: {//Key 1.  This key is used for codes built into GS/XP cheat discs.
                					//  You typically cannot see these codes.
                			seedtbl	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_1].table;
                			size = gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_1].size;
                			tmp = (address & 0x01FFFFFF) ^ gs3_hseeds[2];
                			mask = 0;
                			for(i = 0; i < size; i++) {
                				mask |= ((tmp & (1u << i)) >> i) << seedtbl[i];
                			}
                			address =  ((mask  ^ (gs3_hseeds[1] << 8)))  | command;
                			break;
                		}

                		case 2: {  //Key 2.  The original encryption used publicly.  Fell into disuse
                			seedtbl	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_2].table;
                			size	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_2].size;
                			address = (address ^ (gs3_hseeds[2] << 8)) & 0x01FFFFFF;
                			value ^= gs3_hseeds[2];
                			tmp = tmp2 = 0;
                			for(i = 0; i < 0x39; i++) {
                				flag = (i < 32) ? (value >> i) & 1 : (address >> (i - 32)) & 1;
                				if(flag > 0) {
                					if(seedtbl[i] < 32) {
                						tmp2 |= (1u << seedtbl[i]);
                					}
                					else {
                						tmp |= (1u << (seedtbl[i] - 32));
                					}
                				}
                			}
                			address = (tmp ^ (gs3_hseeds[1] << 1)) | command;
                			value = tmp2 ^ (gs3_hseeds[1] << 16);
                			break;
                		}
                		case 3: {  //Key 3.  Unused publicly.
                			seedtbl	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_3].table;
                			tmp = (address & 0xF1FFFFFF) ^ (gs3_hseeds[2] << 4);
                			address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 1)], seedtbl[GETBYTE(tmp, 2)])
                					| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[1] << 8));
                			break;
                		}
                		case 4: {  //Key 4.  Second encryption used publicly.
                			seedtbl	= gs3_encrypt_seeds[(int)CryptIndex.GS3_CRYPT_4].table;
                			tmp = (address & 0xF1FFFFFF) ^ (gs3_hseeds[2] << 4);
                			address = ((BYTES_TO_HALF(seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)])
                					| (tmp & 0xFFFF0000)) ^ (gs3_hseeds[1] << 8));
                			tmp = value ^ (gs3_hseeds[2] << 5);
                			value = BYTES_TO_WORD(seedtbl[GETBYTE(tmp, 4)], seedtbl[GETBYTE(tmp, 3)],
                						seedtbl[GETBYTE(tmp, 2)], seedtbl[GETBYTE(tmp, 1)]) ^ (gs3_hseeds[1] << 9);
                			break;
                		}
                		case 5:
                		case 6:
                			break;
                		case 7: {//seed refresh.
                			uint[] code = new uint[] { address, value };
                			Update(code);
                			break;
                		}
                		default:
                			break;
                	}
                	//add key to the code
                	address |= (key << 25);
                	return 0;
            }
        }

        // === Batch and verifier helpers ===

        public static void BatchDecrypt(cheat_t cheat)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            int i = 0;
            while (i < cheat.codecnt)
            {
                if (((cheat.code[i] >> 28) == 7u) && gs3_linetwo == 0)
                {
                    Cheat.cheatRemoveOctets(cheat, i + 1, 2);
                }
                else
                {
                    uint addr = cheat.code[i];
                    uint val = cheat.code[i + 1];
                    Decrypt(ref addr, ref val);
                    cheat.code[i] = addr;
                    cheat.code[i + 1] = val;
                    i += 2;
                }
            }
        }

        public static void BatchEncrypt(cheat_t cheat, uint key)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));

            for (int i = 0; i + 1 < cheat.codecnt; i += 2)
            {
                uint addr = cheat.code[i];
                uint val = cheat.code[i + 1];
                Encrypt(ref addr, ref val, key);
                cheat.code[i] = addr;
                cheat.code[i + 1] = val;
            }
        }

        public static void CreateVerifier(out uint address, out uint value, uint[] codes, int size)
        {
            value = 0;
            if (codes == null || size <= 0)
            {
                address = 0;
                return;
            }

            ushort crc = GenCrc(codes, size);
            address = 0x76000000u | ((uint)crc << 4);
        }

        public static void AddVerifier(cheat_t cheat)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));

            uint address, value;
            CreateVerifier(out address, out value, cheat.code.ToArray(), cheat.codecnt);
            Cheat.cheatPrependOctet(cheat, value);
            Cheat.cheatPrependOctet(cheat, address);
        }

        public static void CryptFileData(byte[] data, uint size)
        {
            if (data == null) throw new ArgumentNullException(nameof(data));
            if (size > data.Length)
                size = (uint)data.Length;

            InitMtStateTbl(gs3_mtStateTbl_p2m, size);
            for (uint i = 0; i < size; i++)
            {
                data[i] ^= (byte)(GetMtNum(gs3_mtStateTbl_p2m) & 0xFF);
            }
        }
    }
}
