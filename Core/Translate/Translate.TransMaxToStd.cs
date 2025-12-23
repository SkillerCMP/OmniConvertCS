using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {

        private static int TransMaxToStd(cheat_t dest, cheat_t src, ref int idx)
        {
            int i, cnt, trans, tmp, ret;
            byte command, size, type, subtype, type2, subtype2;
            uint address, increment, offset, value, count, skip;
            var code = src.code;
            
            trans = cnt = ret = 0;
            if(code[idx] == ARM_CMD_SPECIAL) {
				Trace($"[TransMaxToStd] SPECIAL idx={idx} w1=0x{code[idx+1]:X8} w2=0x{code[idx+2]:X8} w3=0x{code[idx+3]:X8} hibyte(w1)=0x{HIBYTE32(code[idx+1]):X2}");
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
            Trace($"  EXPLODE size={GET_ARM_SIZE(code[idx+1])} addr=0x{ADDR(code[idx+1]):X8} " +
      $"fill={HIHALF(code[idx+3])} skip={(byte)LOHALF(code[idx+3])} inc={(byte)HIBYTE32(code[idx+3])} " +
      $"value=0x{code[idx+2]:X8}");
			TransExplode(dest, GET_ARM_SIZE(code[idx + 1]), ADDR(code[idx + 1]), code[idx + 2],
             HIHALF(code[idx + 3]),          // fillcount (was wrong)
             (byte)LOHALF(code[idx + 3]),    // skip (was wrong)
             (byte)HIBYTE32(code[idx + 3])); // increment
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

