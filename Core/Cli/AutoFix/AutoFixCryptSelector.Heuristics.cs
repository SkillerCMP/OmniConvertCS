using System;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
private static bool InputAddr28InStdRange(CheatBlock block, out uint addr28, out string rule)
{
    addr28 = 0;
    rule = "input.none";
    try
    {
        // If the input is ARMAX encrypted text (e.g. PNDF-M5ZV-ACXTE), the
        // words in CheatInput are *ciphertext*, not an address/value pair.
        // Do not apply RAW-style addr28 heuristics to such inputs.
        if (block.HasArmaxEncryptedTextLine)
        {
            rule = "input.armax.encrypted";
            return false;
        }

        if (block.CheatInput == null || block.CheatInput.code == null || block.CheatInput.codecnt <= 0)
            return false;
        if ((block.CheatInput.codecnt & 1) != 0)
            return false;

        int lineCount = block.CheatInput.codecnt / 2;

        // Mirror RawPlausibility's "first checkable line" selection: skip CB7 master prologue.
        for (int li = 0; li < lineCount; li++)
        {
            uint w1 = block.CheatInput.code[li * 2];
            uint w2 = block.CheatInput.code[li * 2 + 1];

            if (IsCb7MasterPrologueLineLocal(w1))
                continue;

            uint type = (w1 >> 28) & 0xFu;
            uint w1hi16 = w1 & 0xFFFF0000u;

            uint addressWord = w1;
            rule = "input.w1.addr28";

            if (w1hi16 == 0x30000000u || w1hi16 == 0x30100000u ||
                w1hi16 == 0x30200000u || w1hi16 == 0x30300000u)
            {
                addressWord = w2;
                rule = "input.3-type.w2.addr28";
            }
            else if (w1hi16 == 0x30400000u || w1hi16 == 0x30500000u)
            {
                addressWord = w2;
                rule = "input.3040/3050.w2.addr28";
            }
            else if (type == 0xEu)
            {
                addressWord = w2;
                rule = "input.E-type.w2.addr28";
            }

            addr28 = addressWord & 0x0FFFFFFFu;
            return addr28 <= 0x01FFFFFFu;
        }

        return false;
    }
    catch
    {
        return false;
    }
}

// Local copy of RawPlausibility's CB7 master prologue test.
private static bool IsCb7MasterPrologueLineLocal(uint w1)
{
    // FE?????? ????????  OR  000FFFFE/000FFFFF ????????
    if ((w1 & 0xFF000000u) == 0xFE000000u)
        return true;

    uint addr28 = (w1 & 0x0FFFFFFFu);
    return (addr28 == 0x000FFFFEu) || (addr28 == 0x000FFFFFu);
}



    }
}
