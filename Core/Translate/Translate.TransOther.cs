using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {

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
    }
}

