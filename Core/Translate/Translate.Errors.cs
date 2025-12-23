using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {
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
    }
}

