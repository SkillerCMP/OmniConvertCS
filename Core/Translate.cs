
using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    /// <summary>
    /// C# port of the "standard code format" (SCF) translation helpers from translate.c.
    ///
    /// NOTE: Only the high-level batch dispatcher (TransBatchTranslate) and error
    /// helpers are wired at the moment.  The opcode-level translation routines
    /// (TransOther / TransStdToMax / TransMaxToStd) are stubbed and still need a
    /// full mechanical port from the original C if you want full device-to-device
    /// conversion identical to Omniconvert.
    /// </summary>
    internal static class Translate
    {
        
        [Conditional("DEBUG")]
        private static void Trace(string message)
            => Debug.WriteLine(message);

// --- Standard-format command IDs (upper nibble of the address) ---

        private const byte STD_WRITE_BYTE         = 0; // 8-bit write
        private const byte STD_WRITE_HALF         = 1; // 16-bit write
        private const byte STD_WRITE_WORD         = 2; // 32-bit write
        private const byte STD_INCREMENT_WRITE    = 3; // read-increment-write
        private const byte STD_WRITE_WORD_MULTI   = 4; // multi-address write
        private const byte STD_COPY_BYTES         = 5; // copy bytes
        private const byte STD_POINTER_WRITE      = 6; // pointer-based write
        private const byte STD_BITWISE_OPERATE    = 7; // bitwise op (CB7+ only)
        private const byte GS5_VERIFIER           = STD_BITWISE_OPERATE;
        private const byte STD_ML_TEST            = 8; // master-level test
        private const byte AR2_COND_HOOK          = STD_ML_TEST; // AR2 uses STD_ML_TEST as conditional hook
        private const byte STD_COND_HOOK          = 9; // conditional hook
        private const byte STD_ML_WRITE_WORD      = 0xA; // master-level write
        private const byte STD_TIMER              = 0xB; // timer / loop
        private const byte STD_TEST_ALL           = 0xC; // execute all remaining
        private const byte STD_TEST_SINGLE        = 0xD; // execute next code
        private const byte STD_TEST_MULTI         = 0xE; // execute next N codes
        private const byte STD_UNCOND_HOOK        = 0xF; // unconditional hook




        // --- Write / test sizes ---

        private const byte SIZE_BYTE    = 0;
        private const byte SIZE_HALF    = 1;
        private const byte SIZE_WORD    = 2;
        private const byte SIZE_SPECIAL = 3;

        // --- ARMAX command categories / sub‑types ---

        private const byte ARM_WRITE            = 0;
        private const byte ARM_EQUAL            = 1;
        private const byte ARM_NOT_EQUAL        = 2;
        private const byte ARM_LESS_SIGNED      = 3;
        private const byte ARM_GREATER_SIGNED   = 4;
        private const byte ARM_LESS_UNSIGNED    = 5;
        private const byte ARM_GREATER_UNSIGNED = 6;
        private const byte ARM_AND              = 7;

        // "write" sub‑types
        private const byte ARM_DIRECT   = 0;
        private const byte ARM_POINTER  = 1;
        private const byte ARM_INCREMENT= 2;
        private const byte ARM_HOOK     = 3;

        // test sub‑types
        private const byte ARM_SKIP_1      = 0;
        private const byte ARM_SKIP_2      = 1;
        private const byte ARM_SKIP_N      = 2;
        private const byte ARM_SKIP_ALL_INV= 3;

        // --- ARMAX helper constants / masks ---

        private const uint ARM_CMD_SPECIAL    = 0;
        private const uint ARM_CMD_RESUME     = 0x40000000;
        private const int  ARM_MAX_INCREMENT  = 127;
        private const uint ARM_ENABLE_STD     = 0x0003FF00;
        private const uint ARM_ENABLE_INT     = 0x0002FF01;
        private const uint ARM_ENABLE_THREAD  = 0x0001FF01; // not normally used

        private const uint MASK_ADDR      = 0x01FFFFFF;
        private const uint MASK_LOHALF    = 0x0000FFFF;
        private const uint MASK_HIHALF    = 0xFFFF0000;
        private const uint MASK_LOBYTE32  = 0x000000FF;
        private const uint MASK_HIBYTE32  = 0xFF000000;

        private static uint ADDR(uint a)        => a & MASK_ADDR;
        private static uint LOHALF(uint a)      => a & MASK_LOHALF;
        private static uint HIHALF(uint a)      => (a >> 16) & MASK_LOHALF;
        private static uint LOBYTE32(uint a)    => a & MASK_LOBYTE32;
        private static uint HIBYTE32(uint a)    => (a >> 24) & MASK_LOBYTE32;

        private static byte  GET_STD_CMD(uint a)     => (byte)((a >> 28) & 0xF);
        private static byte  GET_ARM_CMD(uint a)     => (byte)((a >> 24) & 0xFE);
        private static byte  GET_ARM_TYPE(uint a)    => (byte)((a >> 27) & 7);
        private static byte  GET_ARM_SUBTYPE(uint a) => (byte)((a >> 30) & 3);
        private static byte  GET_ARM_SIZE(uint a)    => (byte)((a >> 25) & 3);

        private static uint MAKE_ARM_CMD(byte type, byte subtype, byte size)
            => (uint)((subtype << 30) | (type << 27) | (size << 25));

        private static uint MAKE_STD_CMD(byte cmd)
            => (uint)(cmd << 28);

        private static uint MAKEWORD32(uint val, byte size)
            => size == SIZE_BYTE
                   ? (val << 24) | (val << 16) | (val << 8) | val
                   : (val << 16) | val;

        private static uint MAKEHALF(uint val, byte size)
            => size == SIZE_BYTE ? ((val << 8) | val) & 0xFFFFu : val & 0xFFFFu;

        // Size tables and helpers taken from translate.c
        
        /// <summary>
        /// 1:1 port of TransSmash(cheat_t *dest, u8 size, u32 address, u32 value, u32 fillcount).
        /// Packs a fill operation into standard-format commands, using multi-write
        /// and alignment rules identical to the C implementation.
        /// </summary>
        private static void TransSmash(cheat_t dest, byte size, uint address, uint value, uint fillcount)
        {
            uint bytes = fillcount << size;

            if (fillcount == 1)
            {
                Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(size) | address);
                Cheat.cheatAppendOctet(dest, value);
                return;
            }

            while (bytes != 0)
            {
                if ((address % 4) == 0)
                {
                    if (bytes > 12)
                    {
                        Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_WRITE_WORD_MULTI) | address);
                        uint count = (bytes - (bytes % 4)) / 4;
                        Cheat.cheatAppendOctet(dest, (count << 16) | 1u);
                        Cheat.cheatAppendOctet(dest, MAKEWORD32(value, size));
                        Cheat.cheatAppendOctet(dest, 0);
                        address += count * 4;
                        bytes   -= count * 4;
                    }
                    else if (bytes >= 4)
                    {
                        Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_WORD) | address);
                        Cheat.cheatAppendOctet(dest, MAKEWORD32(value, size));
                        address += 4;
                        bytes   -= 4;
                    }
                    else if (bytes >= 2)
                    {
                        Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_HALF) | address);
                        Cheat.cheatAppendOctet(dest, MAKEHALF(value, size));
                        address += 2;
                        bytes   -= 2;
                    }
                    else
                    {
                        Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_BYTE) | address);
                        Cheat.cheatAppendOctet(dest, value);
                        address += 1;
                        bytes   -= 1;
                    }
                }
                else if ((address % 2) == 0)
                {
                    if (bytes >= 2)
                    {
                        Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_HALF) | address);
                        Cheat.cheatAppendOctet(dest, MAKEHALF(value, size));
                        address += 2;
                        bytes   -= 2;
                    }
                    else
                    {
                        Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_BYTE) | address);
                        Cheat.cheatAppendOctet(dest, value);
                        address += 1;
                        bytes   -= 1;
                    }
                }
                else
                {
                    Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(SIZE_BYTE) | address);
                    Cheat.cheatAppendOctet(dest, value);
                    address += 1;
                    bytes   -= 1;
                }
            }
        }

        /// <summary>
        /// 1:1 port of TransExplode(cheat_t *dest, u8 size, u32 address, u32 value,
        /// u32 fillcount, u8 skip, u8 increment).
        /// Handles exploded fill with optional skip/increment; collapses to TransSmash
        /// when skip==1 and increment==0.
        /// </summary>
        private static void TransExplode(cheat_t dest, byte size, uint address, uint value,
                                         uint fillcount, byte skip, byte increment)
        {
            uint bytes = fillcount << size;

            if (skip == 1 && increment == 0)
            {
                TransSmash(dest, size, address, value, fillcount);
                return;
            }

            if (fillcount == 1)
            {
                Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(size) | address);
                Cheat.cheatAppendOctet(dest, value);
                return;
            }

            while (fillcount-- != 0)
            {
                Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(size) | address);
                Cheat.cheatAppendOctet(dest, value);
                address += (uint)(1 << size) * skip;
                value   += increment;
            }
        }
private static readonly byte[] ARSize   = { SIZE_BYTE, SIZE_BYTE, SIZE_BYTE, SIZE_HALF, SIZE_HALF, SIZE_WORD, SIZE_WORD };
        private static readonly byte[] STDSize  = { SIZE_BYTE, SIZE_BYTE, SIZE_HALF, SIZE_HALF, SIZE_WORD, SIZE_WORD, SIZE_WORD };
        private static readonly byte[] STDCompToArm = { ARM_EQUAL, ARM_NOT_EQUAL, ARM_LESS_UNSIGNED, ARM_GREATER_UNSIGNED, ARM_AND, ARM_AND };
        private static readonly uint[] valmask  = { 0xFFu, 0xFFFFu, 0xFFFFFFFFu };

        // --- Error codes / messages ---

        private const int NUM_ERROR_TEXT = 15;

        internal const int ERR_NONE             = 0;
        internal const int ERR_VALUE_INCREMENT  = 1;
        internal const int ERR_COPYBYTES        = 2;
        internal const int ERR_EXCESS_OFFSETS   = 3;
        internal const int ERR_OFFSET_VALUE_32  = 4;
        internal const int ERR_OFFSET_VALUE_16  = 5;
        internal const int ERR_OFFSET_VALUE_8   = 6;
        internal const int ERR_BITWISE          = 7;
        internal const int ERR_TIMER            = 8;
        internal const int ERR_ALL_OFF          = 9;
        internal const int ERR_TEST_TYPE        = 10;
        internal const int ERR_SINGLE_LINE_FILL = 11;
        internal const int ERR_PTR_SIZE         = 12;
        internal const int ERR_TEST_TYPE_2      = 13;
        internal const int ERR_TEST_SIZE        = 14;
        internal const int ERR_MA_WRITE_SIZE    = 15;
        internal const int ERR_INVALID_CODE     = unchecked((int)0xFFFF);

        // Text table mirrors transErrorText[] from the C code.
        // Indexing is 1‑based in the original (error 1 -> entry 0, etc).
        private static readonly string[] transErrorText = new[]
        {
            "Code contains a value increment\nlarger than allowed by target device.",                 //1
            "Target device does not support\nthe copy bytes command.",                               //2
            "Target device does not support\npointer writes with more than one offset.",             //3
            "Target device does not allow\n32-bit pointer writes with offsets.",                     //4
            "Target device does not allow\noffsets > 0xFFFF on 16-bit pointers.",                    //5
            "Target device does not allow\noffsets > 0xFFFFFF on 8-bit pointers.",                   //6
            "Target device does not support\nbitwise operations (type 7).",                          //7
            "Target device does not support\ntimer codes.",                                          //8
            "Target device does not support\nthe all-off test command.",                             //9
            "Target device does not support\nthe comparison specified by test command.",             //10
            "Single line fill codes not yet\nhandled by Omniconvert.",                              //11
            "Target device does not support\nsize indicated in pointer write.",                      //12
            "Target device does not support\nthe comparison specified by test command.",             //13
            "Target device does not support\nthe size specified by test command.",                   //14
            "Target device does not support\nthe size specified by multi-address write."             //15
        };

        private const string transErrorOctets =
            "Code contains an invalid\nnumber of 8-digit values,\n" +
            "or general crypt error.  Check your input.";

        // In C this is a u8 flag; keep it byte‑sized here as well.
        private static byte err_suppress = 0;

        internal static void TransSetErrorSuppress(byte val) => err_suppress = val;

        internal static void TransToggleErrorSuppress() => err_suppress ^= 1;

        internal static string TransGetErrorText(int idx)
        {
            var tmpIndex = idx - 1;
            if (tmpIndex < 0 || tmpIndex >= NUM_ERROR_TEXT)
                return transErrorOctets;

            return transErrorText[tmpIndex];
        }

        /// <summary>
        /// High‑level dispatcher equivalent to int transBatchTranslate(cheat_t *src).
        /// It takes the current global g_indevice / g_outdevice and calls the
        /// appropriate per‑opcode translator.
        ///
        /// The translated octets are written into a temporary cheat_t and then
        /// copied back into <paramref name="src"/>.
        /// </summary>
        internal static int TransBatchTranslate(cheat_t src)
        {
            if (src == null) throw new ArgumentNullException(nameof(src));

            // When devices are the same the original simply returns success.
            if (g_indevice == g_outdevice)
                return 0;

            // Destination container that accumulates translated octets.
            var dest = Cheat.cheatInit();
            int ret = 0;

            if (g_indevice != Device.DEV_ARMAX)
            {
                if (g_outdevice == Device.DEV_ARMAX)
                {
                    // Standard-format -> ARMAX
                    for (int i = 0; i < src.codecnt;)
                    {
                        ret = TransStdToMax(dest, src, ref i);
                        if (err_suppress == 0 && ret != 0)
                            break;
                    }
                }
                else
                {
                    // Standard-format -> other non-ARMAX device
                    for (int i = 0; i < src.codecnt;)
                    {
                        ret = TransOther(dest, src, ref i);
                        if (err_suppress == 0 && ret != 0)
                            break;
                    }
                }
            }
            else
            {
                // ARMAX -> standard-format
                for (int i = 0; i < src.codecnt;)
                {
                    ret = TransMaxToStd(dest, src, ref i);
                    if (err_suppress == 0 && ret != 0)
                        break;
                }
            }

            // Copy translated octets back into the source cheat.
            src.code.Clear();
            src.code.AddRange(dest.code);

            return ret;
        }

        // --------------------------------------------------------------------
        //  Opcode-level translators (SCF core)
        //
        //  These are still TODO.  The C equivalents live in translate.c as:
        //      int transOther(cheat_t *dest, cheat_t *src, int *idx);
        //      static int TransStdToMax(cheat_t *dest, cheat_t *src, int *idx);
        //      static int TransMaxToStd(cheat_t *dest, cheat_t *src, int *idx);
        //
        //  For now they behave as pass‑throughs so that the pipeline is wired
        //  but you will not get real cross‑device opcode conversion yet.
        // --------------------------------------------------------------------

                private static int TransOther(cheat_t dest, cheat_t src, ref int idx)
        {
            if (dest == null) throw new ArgumentNullException(nameof(dest));
            if (src == null) throw new ArgumentNullException(nameof(src));

            if (idx < 0 || idx >= src.codecnt)
                return ERR_INVALID_CODE;

            byte cmd = (byte)((src.code[idx] >> 28) & 0xF);
            byte size;
            uint addr, val;

            switch (cmd) {
                    case STD_WRITE_BYTE:
                    case STD_WRITE_HALF:
                    case STD_WRITE_WORD:
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        idx+=2;
                        break;
                    case STD_INCREMENT_WRITE:
                        addr = src.code[idx];
                        if (g_indevice == Device.DEV_AR2 || g_indevice == Device.DEV_AR1) {
                            if (g_outdevice == Device.DEV_CB || g_outdevice == Device.DEV_GS3) addr -= 0x10000;  //CodeBreaker/GS use zero-based width indicators
                            size = (byte)(((addr >> 20) & 7u) - 1u);
                        }
                        else {
                            size = (byte)((addr >> 20) & 7u);
                            if (g_outdevice == Device.DEV_AR2 || g_outdevice == Device.DEV_AR1) addr += 0x10000; //AR1/AR2 use one-based width indicators
                        }
                        Cheat.cheatAppendOctet(dest, addr);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        if (size > 3) {
                            if (idx + 2 >= src.codecnt) return ERR_INVALID_CODE;
                            Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                            Cheat.cheatAppendOctet(dest, src.code[idx + 3]);
                            idx+=2;
                        }
                        idx+=2;
                        break;
                    case STD_WRITE_WORD_MULTI:
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        if (idx + 2 >= src.codecnt) return ERR_INVALID_CODE;
                        if ((g_outdevice == Device.DEV_AR2 || g_outdevice == Device.DEV_AR1) && src.code[idx + 3] > 0) return ERR_VALUE_INCREMENT;
                        Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 3]);
                        idx+=4;
                        break;
                    case STD_COPY_BYTES:
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        if (idx + 2 >= src.codecnt) return ERR_INVALID_CODE;
                        Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 3]);
                        idx+=4;
                        break;
                    case STD_POINTER_WRITE:
                        if (idx + 2 >= src.codecnt) return ERR_INVALID_CODE;
                        /* Attempt to translate pointer writes.
                        so far as I know, GS3 stole (and subsequently wrecked) their format from the old AR
                        The second nibble is a size parameter, and the second word is a "load offset" that is added
                        to the address in the code (to accomodate addresses > 0xFFFFFF).  However, on the GS3, this infringes
                        upon their use of 3 uppermost bits of that nibble as a decryption key*/
                        if (g_outdevice == Device.DEV_STD || g_indevice == Device.DEV_STD || (g_outdevice != Device.DEV_CB && g_indevice != Device.DEV_CB)) {
                            Cheat.cheatAppendOctet(dest, src.code[idx]);
                            Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                            Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                            Cheat.cheatAppendOctet(dest, src.code[idx + 3]);
                            idx += 4;
                            return ERR_NONE;
                        }
                        if (g_indevice == Device.DEV_CB) {
                            if (LOHALF(src.code[idx + 2]) > 1) return ERR_EXCESS_OFFSETS;
                            addr = ADDR(src.code[idx]);
                            uint offset = addr & 0x1000000;
                            size = (byte)HIHALF(src.code[idx + 2]);
                            if (size == 0) return ERR_PTR_SIZE;
                            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_POINTER_WRITE) | ((uint)size << 24) | (addr & 0xFFFFFF));
                            Cheat.cheatAppendOctet(dest, offset);
                            Cheat.cheatAppendOctet(dest, src.code[idx + 3]);
                            Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        } else {
                            size = (byte)((src.code[idx] >> 24) & 0x3u);
                            if (size == 0 || size == 3) size = 1;
                            Cheat.cheatAppendOctet(dest, (src.code[idx] & 0xF0FFFFFF) + src.code[idx + 1]); //build the proper address
                            Cheat.cheatAppendOctet(dest, src.code[idx + 3]);				   //move value to third word
                            Cheat.cheatAppendOctet(dest, 1u | ((uint)size << 16));				   //ensure count is one and or in the size
                            Cheat.cheatAppendOctet(dest, src.code[idx + 2]);				   //final word is offset
                        }
                        idx+=4;
                        break;
                    case STD_BITWISE_OPERATE:		//GS5_VERIFIER | Neither GS nor AR do the bitwise ops, and GS5 uses them as verifiers.
                        if (g_indevice == Device.DEV_CB || g_indevice == Device.DEV_STD) {
                            if (g_outdevice == Device.DEV_CB || g_outdevice == Device.DEV_STD) {
                                Cheat.cheatAppendOctet(dest, src.code[idx]);
                                Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                            } else {
                                return ERR_BITWISE;
                            }
                        }
                        idx+=2;
                        break;
                    case STD_ML_TEST:			//Also AR2_COND_HOOK
                        if (g_indevice == Device.DEV_AR2 || g_indevice == Device.DEV_AR1) {
                            if (idx + 2 >= src.codecnt) return ERR_INVALID_CODE;
                            if (g_outdevice == Device.DEV_STD || g_outdevice == Device.DEV_AR1 || g_outdevice == Device.DEV_AR2) {
                                Cheat.cheatAppendOctet(dest, src.code[idx]);
                                Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                                Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                                Cheat.cheatAppendOctet(dest, src.code[idx + 3]);
                                idx+=4;
                            } else {
                                Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_COND_HOOK) | ADDR(src.code[idx + 1]));
                                Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                                idx+=4;
                            }
                        } else if (g_indevice == Device.DEV_CB || g_indevice == Device.DEV_GS3) {	//STD_ML_TEST
                            if (g_outdevice == Device.DEV_STD) {
                                Cheat.cheatAppendOctet(dest, src.code[idx]);
                                Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                                idx+=2;
                            } else {
                                return ERR_TEST_TYPE;
                            }
                        } else { //if (g_indevice == Device.DEV_STD)
                            if (idx + 2 >= src.codecnt) return ERR_INVALID_CODE;
                            if ((src.code[idx + 2] >> 28) == 0) {		//this is either an AR hook code, or an error
                                // TODO: [oddity] Should the following actually be checking g_outdevice & g_indevice?
                                if (g_outdevice == Device.DEV_AR2 || g_outdevice == Device.DEV_AR2) {
                                    Cheat.cheatAppendOctet(dest, src.code[idx]);
                                    Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                                    Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                                    Cheat.cheatAppendOctet(dest, src.code[idx + 3]);
                                    idx+=4;
                                } else {
                                    Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_COND_HOOK) | ADDR(src.code[idx + 1]));
                                    Cheat.cheatAppendOctet(dest, src.code[idx + 2]);
                                    idx+=4;
                                }
                            } else {
                                // TODO: [oddity] Should the following actually be checking g_outdevice & g_indevice?
                                if (g_outdevice == Device.DEV_AR2 || g_outdevice == Device.DEV_AR2) {
                                    return ERR_TEST_TYPE;
                                } else {
                                    Cheat.cheatAppendOctet(dest, src.code[idx]);
                                    Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                                    idx+=2;
                                }
                            }
                        }
                        break;
                    case STD_COND_HOOK:
                        if (g_outdevice == Device.DEV_AR1 || g_outdevice == Device.DEV_AR2) {
                            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(AR2_COND_HOOK) | (ADDR(src.code[idx]) + 1));
                            Cheat.cheatAppendOctet(dest, ADDR(src.code[idx]));
                            Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                            Cheat.cheatAppendOctet(dest, 0);
                        } else {
                            Cheat.cheatAppendOctet(dest, src.code[idx]);
                            Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        }
                        idx+=2;
                        break;
                    case STD_ML_WRITE_WORD:
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        idx+=2;
                        break;
                    case STD_TIMER:					//I've seen this code never.  Just assume the value is a standard count.
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        idx+=2;
                        break;
                    case STD_TEST_ALL:
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        idx+=2;
                        break;
                    case STD_TEST_SINGLE:
                        if (g_indevice == Device.DEV_CB && (g_outdevice != Device.DEV_STD) && (((src.code[idx + 1] >> 16) & 0xF)  > 0)) return ERR_TEST_SIZE;
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        idx+=2;
                        break;
                    case STD_TEST_MULTI:
                        if (g_indevice == Device.DEV_CB && (g_outdevice != Device.DEV_STD) && (((src.code[idx] >> 24) & 0xF) > 0)) return ERR_TEST_SIZE;
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        idx+=2;
                        break;
                    case STD_UNCOND_HOOK:				//difficult to make determinations here.  Assume it's correct.
                        Cheat.cheatAppendOctet(dest, src.code[idx]);
                        Cheat.cheatAppendOctet(dest, src.code[idx + 1]);
                        idx+=2;
                        break;
                }
            return ERR_NONE;
        }
private static int TransStdToMax(cheat_t dest, cheat_t src, ref int idx)
        {
            int i, cnt, trans, tmp, ret;
            byte command, size;
            uint address, increment, offset, value, count, skip;
            var code = src.code;
            
            cnt = trans = ret = 0;
            command = GET_STD_CMD(code[idx]);
            switch(command) {
            case STD_WRITE_BYTE:
            case STD_WRITE_HALF:
            case STD_WRITE_WORD:
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_DIRECT, command) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, code[idx + 1]);
            idx+=2;
            break;
            case STD_INCREMENT_WRITE: //Also decrement
            tmp        = (int)((code[idx] >> 20) & 0x7u);
            if(g_indevice == Device.DEV_AR1 || g_indevice == Device.DEV_AR2) {
            size = ARSize[tmp];
            tmp = ((tmp & 1) != 0) ? 0 : 1;        //flag increment/decrement
            }
            else {
            size = STDSize[tmp];
            tmp = ((tmp & 1) != 0) ? 1 : 0;        //flag increment/decrement
            }
            if(size == SIZE_WORD) {
            trans=4;
            if(idx + 4 >= src.codecnt) {
            ret = ERR_INVALID_CODE;        //too few octets
            break;
            }
            value = code[idx + 2];
            }
            else {
            value = LOHALF(code[idx]);
            trans=2;
            }
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_INCREMENT, size) | ADDR(code[idx + 1]));
            Cheat.cheatAppendOctet(dest, (tmp != 0) ? (~value) + 1 : value);
            idx+=trans;
            break;
            case STD_WRITE_WORD_MULTI:
            if(idx+4 > src.codecnt) {
            ret = ERR_INVALID_CODE;        //too few octets
            break;
            }
            increment     = code[idx + 3];
            if(increment > ARM_MAX_INCREMENT) {
            ret = ERR_VALUE_INCREMENT;    //unsupported value increment size
            break;
            }
            Cheat.cheatAppendOctet(dest, ARM_CMD_SPECIAL);
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, 2, SIZE_WORD) | ADDR(code[idx + 0]));
            Cheat.cheatAppendOctet(dest, code[idx + 2]);
            Cheat.cheatAppendOctet(dest, ((increment & MASK_LOBYTE32) << 24) | (HIHALF(code[idx + 1]) << 16) | LOHALF(code[idx + 1]));
            idx+=4;
            break;
            case STD_COPY_BYTES:
            idx+=4;
            ret = ERR_COPYBYTES;                    //ARM does not support copy bytes.
            break;
            case STD_POINTER_WRITE:
            if(idx + 4 > src.codecnt) {
            ret = ERR_INVALID_CODE;        //too few octets
            break;
            }
            if(g_indevice == Device.DEV_CB) {
            size     = (byte)(HIHALF(code[idx + 2]) & 0x3u);
            address = ADDR(code[idx]);
            value     = code[idx + 1] & valmask[size];
            count     = LOHALF(code[idx + 2]);
            offset    = code[idx + 3];
            } else {
            size     = (byte)(HIBYTE32(code[idx]) & 0xFu);
            if(size < 1 && g_indevice == Device.DEV_GS3) size = 1;  //GS3 does not do 8-bit writes (or 32 really).
            address = (code[idx] & 0x00FFFFFFu) + code[idx + 1];
            offset     = code[idx + 2];
            value     = code[idx + 3] & valmask[size];
            count     = 0;
            }
            idx+=4;
            if(count > 1) {                        //no support for multiple levels of indirection
            idx += (int)((count >> 1) << 1);
            ret = ERR_EXCESS_OFFSETS;
            break;
            }
            if(size == SIZE_WORD && offset > 0) {            //32-bit write does not allow offset
            ret = ERR_OFFSET_VALUE_32;
            break;
            }
            if(size == SIZE_HALF && offset > 0xFFFF) {    //16-bit write has maximum offset 65535
            ret = ERR_OFFSET_VALUE_16;
            break;
            }
            if(size == SIZE_BYTE && offset > 0xFFFFFF) {        //8 -bit write has maximum offset 16,777,215
            ret = ERR_OFFSET_VALUE_8;
            break;
            }
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_POINTER, size) | address);
            Cheat.cheatAppendOctet(dest, (offset << 16) | value);
            break;
            case STD_BITWISE_OPERATE:                     //CB Only!
            idx+=2;
            ret = ERR_BITWISE;
            break;
            case STD_ML_TEST: {
            if(g_indevice == Device.DEV_AR1 || g_indevice == Device.DEV_AR2) {
            if(idx + 4 > src.codecnt) {
            return ERR_INVALID_CODE;
            }
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_1, SIZE_WORD) | ADDR(code[idx + 1]));
            Cheat.cheatAppendOctet(dest, code[idx + 2]);
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_HOOK, SIZE_WORD) | ADDR(code[idx + 1]));
            Cheat.cheatAppendOctet(dest, ARM_ENABLE_STD);
            idx+=4;
            } else {
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_1, SIZE_HALF) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, LOHALF(code[idx + 1]));
            int hold = dest.codecnt - 2;
            skip        = HIHALF(code[idx + 1]) << 1;
            idx+=2;
            skip += (uint)idx;
            int lines = 0;
            while(idx < src.codecnt && idx < skip) {
            ret = TransStdToMax(dest, src, ref idx);
            if (ret != 0) break;
            lines++;
            }
            if (ret == 0 && lines > 1) {
            if(lines == 2)
            dest.code[hold] = MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_2, SIZE_HALF) | ADDR(dest.code[hold]);
            else {
            dest.code[hold] = MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_N, SIZE_HALF) | ADDR(dest.code[hold]);
            Cheat.cheatAppendOctet(dest, ARM_CMD_SPECIAL);
            Cheat.cheatAppendOctet(dest, ARM_CMD_RESUME);
            }
            }
            }
            break;
            }
            case STD_COND_HOOK:
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_EQUAL, ARM_SKIP_1, SIZE_WORD) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, code[idx + 1]);
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_HOOK, SIZE_WORD) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, ARM_ENABLE_STD);
            idx+=2;
            break;
            case STD_ML_WRITE_WORD:
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_DIRECT, SIZE_WORD) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, code[idx + 1]);
            idx+=2;
            break;
            case STD_TIMER:                //unsupported
            idx+=2;
            ret = ERR_TIMER;
            break;
            case STD_TEST_ALL:            //unsupported
            idx+=2;
            ret = ERR_ALL_OFF;
            break;
            case STD_TEST_SINGLE:
            tmp = (int)((code[idx + 1] >> 20) & 0x7u);
            if(tmp > 5 || tmp == 4) {
            ret = ERR_TEST_TYPE;
            break;
            }
            size = (byte)(g_indevice == Device.DEV_CB ? ((HIHALF(code[idx + 1]) & 0x1u) ^ 1u) : SIZE_HALF);
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(STDCompToArm[tmp], ARM_SKIP_1, size) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, LOHALF(code[idx + 1]));
            idx+=2;
            break;
            case STD_TEST_MULTI:
            tmp = (int)((code[idx + 1] >> 28) & 0x7u);
            if(tmp > 5 || tmp == 4) {
            ret = ERR_TEST_TYPE;
            break;
            }
            size = (byte)(g_indevice == Device.DEV_CB ? ((HIBYTE32(code[idx]) & 0x1u) ^ 1u) : SIZE_HALF);
            skip = (g_indevice == Device.DEV_CB) ? HIHALF(code[idx]) & 0xFF : HIHALF(code[idx]) & 0xFFF;
            count = (skip <= 2) ? skip-1 : ARM_SKIP_N;
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(STDCompToArm[tmp], (byte)count, size) | ADDR(code[idx+1]));
            Cheat.cheatAppendOctet(dest, LOHALF(code[idx]));
            idx+=2;
            if(skip > 2) {
            skip = (skip << 1) + (uint)idx;
            while(idx < src.codecnt && idx < skip) {
            ret = TransStdToMax(dest, src, ref idx);
            if (ret != 0) break;
            }
            if (ret == 0) {
            Cheat.cheatAppendOctet(dest, ARM_CMD_SPECIAL);
            Cheat.cheatAppendOctet(dest, ARM_CMD_RESUME);
            }
            }
            break;
            case STD_UNCOND_HOOK:
            address = ADDR(code[idx]);
            value = ARM_ENABLE_STD;
            if(address == 0x100008 || address == 0x100000 || address == 0x200000 || address == 0x200008) {  //likely entry point values
            if(code[idx + 1] == 0x1FD || code[idx + 1] == 0xE)
            value = ARM_ENABLE_INT;
            else
            address = code[idx + 1] & 0x1FFFFFC;
            }
            Cheat.cheatAppendOctet(dest, MAKE_ARM_CMD(ARM_WRITE, ARM_HOOK, SIZE_WORD) | address);
            Cheat.cheatAppendOctet(dest, value);
            idx+=2;
            break;
            }
            return ret;
        }

        private static int TransMaxToStd(cheat_t dest, cheat_t src, ref int idx)
        {
            int i, cnt, trans, tmp, ret;
            byte command, size, type, subtype, type2, subtype2;
            uint address, increment, offset, value, count, skip;
            var code = src.code;
            
            trans = cnt = ret = 0;
            if(code[idx] == ARM_CMD_SPECIAL) {
            if(code[idx + 1] == ARM_CMD_RESUME) {
            idx+=2;
            return 0;
            }
            else if(HIBYTE32(code[idx + 1]) >= 0x84 && HIBYTE32(code[idx + 1]) <= 0x85) {
            if(idx+3 >= src.codecnt) {
            return ERR_INVALID_CODE;        //too few octets
            }
            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_WRITE_WORD_MULTI) | ADDR(code[idx + 1]));
            Cheat.cheatAppendOctet(dest, (LOBYTE32(HIHALF(code[idx + 3])) << 16) | LOHALF(code[idx + 3]));
            Cheat.cheatAppendOctet(dest, code[idx + 2]);
            Cheat.cheatAppendOctet(dest, HIBYTE32(code[idx + 3]));
            idx+=4;
            return 0;
            } else {
            if(idx + 3 >= src.codecnt) return ERR_INVALID_CODE;
            if(HIBYTE32(code[idx + 1]) >= 0x80 && HIBYTE32(code[idx + 1]) <= 0x83) {
            TransExplode(dest, GET_ARM_SIZE(code[idx + 1]), ADDR(code[idx + 1]), code[idx + 2],
                         LOHALF(code[idx + 3]), (byte)LOBYTE32(HIHALF(code[idx + 3])), (byte)HIBYTE32(code[idx + 3]));
            idx += 4;
            return 0;
            }
            idx += 2;
            return ERR_MA_WRITE_SIZE;
            }
            }
            
            command    = GET_ARM_CMD(code[idx]);
            type    = GET_ARM_TYPE(code[idx]);
            subtype = GET_ARM_SUBTYPE(code[idx]);
            size    = GET_ARM_SIZE(code[idx]);
            
            if(type == ARM_WRITE) {
            switch(subtype) {
            case ARM_DIRECT:
            if(size == SIZE_BYTE) {
            count = code[idx + 1] >> 8;
            value = LOBYTE32(code[idx + 1]);
            } else if(size == SIZE_HALF) {
            count = HIHALF(code[idx + 1]);
            value = LOHALF(code[idx + 1]);
            } else {
            count = 0;
            value = code[idx + 1];
            }
            if(count > 0) {
            TransSmash(dest, size, ADDR(code[idx]), value, count);
            }
            else {
            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(size) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, value);
            }
            idx+=2;
            break;
            case ARM_POINTER:
            if(size == SIZE_BYTE) {
            if(g_outdevice == Device.DEV_GS3) {
            ret = ERR_PTR_SIZE;            //unsupported size;
            break;
            }
            offset    = code[idx + 1] >> 8;
            value    = LOBYTE32(code[idx + 1]);
            } else if(size == SIZE_HALF) {
            offset    = HIHALF(code[idx + 1]);
            value    = LOHALF(code[idx + 1]);
            } else {
            offset    = 0;
            value    = code[idx + 1];
            }
            if(g_outdevice == Device.DEV_CB) {
            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_POINTER_WRITE) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, value);
            Cheat.cheatAppendOctet(dest, (uint)((size << 16) | 1)); //count;
            Cheat.cheatAppendOctet(dest, offset);
            } else {
            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_POINTER_WRITE) | (code[idx] & 0xFFFFFF));
            Cheat.cheatAppendOctet(dest, code[idx] & 0x1000000);
            Cheat.cheatAppendOctet(dest, offset);
            Cheat.cheatAppendOctet(dest, value);
            }
            idx+=2;
            break;
            case ARM_INCREMENT:
            tmp = (g_outdevice == Device.DEV_AR1 || g_outdevice == Device.DEV_AR2) ? (size << 1) + 1 : (size << 1);
            Cheat.cheatAppendOctet(dest, (size < SIZE_WORD) ?
                (MAKE_STD_CMD(STD_INCREMENT_WRITE) | ((uint)tmp << 20) | (code[idx + 1] & valmask[size])) :
                (MAKE_STD_CMD(STD_INCREMENT_WRITE) | ((uint)tmp << 20)));
            Cheat.cheatAppendOctet(dest, ADDR(code[idx]));
            idx+=2;
            if(size > SIZE_WORD) {
            Cheat.cheatAppendOctet(dest, code[idx + 1]);
            idx+=2;
            }
            break;
            case ARM_HOOK:
            if(idx > 0 && GET_ARM_TYPE(code[idx-2]) == ARM_EQUAL && GET_ARM_SUBTYPE(code[idx-2]) == ARM_SKIP_1
            && ADDR(code[idx - 2]) == ADDR(code[idx])) {
            dest.code[dest.codecnt - 2] = MAKE_STD_CMD(STD_COND_HOOK) | ADDR(code[idx]);
            }
            else {
            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_UNCOND_HOOK) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, ADDR(code[idx]) + 3);
            }
            idx+=2;
            break;
            }
            }
            else {  //test commands
            // ARMAX has signed and unsigned compares; STD RAW only supports unsigned.
            // Treat signed < / > as unsigned < / > for translation.
            if(type == ARM_LESS_SIGNED) type = ARM_LESS_UNSIGNED;
            else if(type == ARM_GREATER_SIGNED) type = ARM_GREATER_UNSIGNED;

            // AND tests are CodeBreaker-only in STD RAW.
            if(type == ARM_AND && g_outdevice != Device.DEV_CB) {
            ret = ERR_TEST_TYPE;
            }
if (size == SIZE_BYTE && g_outdevice != Device.DEV_CB)
            {
                uint a0 = code[idx];
                uint a1 = (idx + 1) < src.codecnt ? code[idx + 1] : 0;
                Trace($"[TransMaxToStd] ERR_TEST_SIZE: byte test not supported for out={g_outdevice} idx={idx} arm0={a0:X8} arm1={a1:X8} cmd=0x{command:X2} type={type} subtype={subtype} size={size}");
                ret = ERR_TEST_SIZE;
            }
            trans = 0;
            if(size == SIZE_WORD) {
            bool didCondHook = false;
            // Preserve the special ARMAX conditional-hook pattern (word == value, followed by a hook write).
            if(type == ARM_EQUAL && (idx + 2) < src.codecnt) {
            type2        = GET_ARM_TYPE(code[idx + 2]);
            subtype2    = GET_ARM_SUBTYPE(code[idx + 2]);
            if(type2 == ARM_WRITE && subtype2 == ARM_HOOK) {
            if(g_outdevice == Device.DEV_AR2) {
            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(AR2_COND_HOOK) | ADDR(code[idx]) + 1);
            Cheat.cheatAppendOctet(dest, ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, ADDR(code[idx + 1]));
            Cheat.cheatAppendOctet(dest, 0);
            } else {
            Cheat.cheatAppendOctet(dest, MAKE_STD_CMD(STD_COND_HOOK) | ADDR(code[idx]));
            Cheat.cheatAppendOctet(dest, code[idx + 1]);
            }
            trans = 1;
            idx+=4;
            didCondHook = true;
            }
            // Not a conditional-hook pattern; fall through to generic word-test splitting.
            }

            // General word-sized tests: split into two 16-bit (E0) tests (low then high) and nest counts.
            if (ret == 0 && didCondHook == false) {
            uint address32 = ADDR(code[idx]);
            uint value32   = code[idx + 1];
            uint loVal     = LOHALF(value32);
            uint hiVal     = HIHALF(value32);
            uint addrLo    = address32;
            uint addrHi    = address32 + 2;

            // Map ARM compare type -> STD compare nibble (0..5) like the generic path does.
            byte stdType = type;
            if(stdType == ARM_AND) stdType-=2;
            else if(stdType <= ARM_NOT_EQUAL) stdType--;
            else stdType-=3;

            // Determine how far the test body runs.
            uint bodyEnd;
            if(subtype == ARM_SKIP_1) bodyEnd = (uint)(idx + 2 + 2);
            else if(subtype == ARM_SKIP_2) bodyEnd = (uint)(idx + 2 + 4);
            else bodyEnd = (uint)src.codecnt;            // until SPECIAL/RESUME

            // Emit placeholders for two E0 tests (outer then inner).
            int tmpOuter = dest.codecnt;
            Cheat.cheatAppendOctet(dest, 0);
            Cheat.cheatAppendOctet(dest, addrLo | ((uint)stdType << 28));
            int tmpInner = dest.codecnt;
            Cheat.cheatAppendOctet(dest, 0);
            Cheat.cheatAppendOctet(dest, addrHi | ((uint)stdType << 28));

            idx += 2; // consume test header (addr/value)

            int bodyStart = dest.codecnt;
            while(idx < src.codecnt && idx < bodyEnd) {
            if(idx + 1 < src.codecnt && code[idx] == ARM_CMD_SPECIAL && code[idx + 1] == ARM_CMD_RESUME) {
            idx+=2;
            break;
            }
            ret = TransMaxToStd(dest, src, ref idx);
            if (ret != 0) break;
            }
            int bodyLines = (dest.codecnt - bodyStart) >> 1;

            // Outer test must cover the inner test line + the body.
            int outerLines = bodyLines + 1;
            int innerLines = bodyLines;

            // Patch the placeholders: word tests become 16-bit tests => size=0 (E0)
            dest.code[tmpOuter] = MAKE_STD_CMD(STD_TEST_MULTI) | ((uint)0 << 24) | ((uint)outerLines << 16) | loVal;
            dest.code[tmpInner] = MAKE_STD_CMD(STD_TEST_MULTI) | ((uint)0 << 24) | ((uint)innerLines << 16) | hiVal;

            trans = 1;
            }
            }

            if (ret == 0 && trans == 0) {
            address    = ADDR(code[idx]);
            value    = code[idx + 1] & valmask[size];
            
            if(type == ARM_AND) type-=2;
            else if(type <= ARM_NOT_EQUAL) type--;
            else type-=3;
            
            if(subtype == ARM_SKIP_1) skip = 2;
            else if(subtype == ARM_SKIP_2) skip = 4;
            else skip = (uint)src.codecnt;            //max
            
            //temporarily append the code
            int tmpout = dest.codecnt;
            size = (byte)(size == SIZE_HALF ? 0 : 1);
            Cheat.cheatAppendOctet(dest, 0);
            Cheat.cheatAppendOctet(dest, address | ((uint)type << 28));
            idx+=2;
            int tmpcnt = dest.codecnt;
            skip += (uint)idx;
            while(idx < src.codecnt && idx < skip) {
            if(idx + 1 < src.codecnt && code[idx] == ARM_CMD_SPECIAL && code[idx + 1] == ARM_CMD_RESUME) {
            idx+=2;
            break;
            }
            ret = TransMaxToStd(dest, src, ref idx);
            if (ret != 0) break;
            }
            tmpcnt = (dest.codecnt - tmpcnt) >> 1;
            //fix the code
            bool forceMulti = (g_outdevice == Device.DEV_STD || g_outdevice == Device.DEV_GS3 || g_outdevice == Device.DEV_CB);
            if(forceMulti || tmpcnt != 1) {
            dest.code[tmpout]    = MAKE_STD_CMD(STD_TEST_MULTI) | ((uint)size << 24) | ((uint)tmpcnt << 16) | value;
            } else {
            dest.code[tmpout]    = MAKE_STD_CMD(STD_TEST_SINGLE) | address;
            dest.code[tmpout+1]  = ((uint)type << 20) | ((uint)size << 16) | value;
            }
            }
            }
            return ret;
        }
    }
}