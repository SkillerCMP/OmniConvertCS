using System;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
        private static uint SeedFromAr2Key(uint key)
        {
            // Same logic as the GUI Ar2KeyForm:
            //   ar2seeds = ar2decrypt(key, tseed[1], tseed[0]) where tseed comes from AR1_SEED.
            uint ar1seed = Ar2.AR1_SEED;
            byte[] tseed = BitConverter.GetBytes(ar1seed);
            return Ar2.Ar2Decrypt(key, tseed[1], tseed[0]);
        }

        private static cheat_t CloneCheat(cheat_t src)
        {
            var c = Cheat.cheatInit();
            c.name = src.name;
            c.comment = src.comment;

            // Shallow-copy other relevant fields
            c.id = src.id;

            // Copy code words
            for (int i = 0; i < src.codecnt; i++)
                c.code.Add(src.code[i]);

            // Copy simple metadata if present
            c.state = src.state;

            return c;
        }


private static bool IsRawFamilyCrypt(ConvertCore.Crypt c) =>
    c == ConvertCore.Crypt.CRYPT_RAW ||
    c == ConvertCore.Crypt.CRYPT_ARAW ||
    c == ConvertCore.Crypt.CRYPT_CRAW ||
    c == ConvertCore.Crypt.CRYPT_GRAW ||
    c == ConvertCore.Crypt.CRYPT_MAXRAW;

    }
}
