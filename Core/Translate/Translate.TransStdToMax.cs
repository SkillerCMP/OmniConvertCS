using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {
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
    }
}

