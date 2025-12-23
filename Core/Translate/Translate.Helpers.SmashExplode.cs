using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {
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
    }
}

