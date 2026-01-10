using System;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
        private static bool TryConvertCandidate(CheatBlock block,
                                                ConvertCore.Crypt inCrypt,
                                                ConvertCore.Crypt outCrypt,
                                                bool analyzeRawFamily,
                                                AnalyzeMode mode,
                                                     uint? manualAr2Key,
                                                out cheat_t? converted,
                                                out RawPlausibilityResult? plaus,
                                                out int retCode,
                                                out string note)
        {
            converted = null;
            plaus = null;
            retCode = 0;
            note = string.Empty;

            uint savedAr2Seeds = ConvertCore.ar2seeds;

            try
{
    // Manual AR2 key override (only try the user-provided key)
    if (inCrypt == ConvertCore.Crypt.CRYPT_AR2 && manualAr2Key.HasValue)
    {
        uint key = manualAr2Key.Value;
        ConvertCore.ar2seeds = SeedFromAr2Key(key);
        if (TryConvertOnce(block, inCrypt, outCrypt, analyzeRawFamily, mode, out converted, out plaus, out retCode))
        {
            note = $"Selected (manual AR2 key 0x{key:X8})";
            return true;
        }
        return false;
    }

    // If we're trying AR2 while analyzing RAW-family outputs, try common key variants first.

                if (inCrypt == ConvertCore.Crypt.CRYPT_AR2 && analyzeRawFamily &&
                    (outCrypt == ConvertCore.Crypt.CRYPT_RAW || outCrypt == ConvertCore.Crypt.CRYPT_CRAW || outCrypt == ConvertCore.Crypt.CRYPT_GRAW))
                {
                    // Attempt with current seed first.
                    if (TryConvertOnce(block, inCrypt, outCrypt, analyzeRawFamily, mode, out converted, out plaus, out retCode))
                    {
                        note = "Selected by RAW-family plausibility (AR2 current seed)";
                        return true;
                    }

                    foreach (uint key in s_commonAr2Keys)
                    {
                        ConvertCore.ar2seeds = SeedFromAr2Key(key);
                        if (TryConvertOnce(block, inCrypt, outCrypt, analyzeRawFamily, mode, out converted, out plaus, out retCode))
                        {
                            note = $"Selected by RAW-family plausibility (AR2 key 0x{key:X8})";
                            return true;
                        }
                    }

                    return false;
                }

                if (TryConvertOnce(block, inCrypt, outCrypt, analyzeRawFamily, mode, out converted, out plaus, out retCode))
                {
                    note = analyzeRawFamily ? "Selected by RAW-family first-line plausibility check" : "Selected (no analysis applied)";
                    return true;
                }

                return false;
            }
            finally
            {
                ConvertCore.ar2seeds = savedAr2Seeds;
            }
        }

        private static bool TryConvertOnce(CheatBlock block,
                                           ConvertCore.Crypt inCrypt,
                                           ConvertCore.Crypt outCrypt,
                                           bool analyzeRawFamily,
                                           AnalyzeMode mode,
                                           out cheat_t? trial,
                                           out RawPlausibilityResult? plaus,
                                           out int retCode)
        {
            trial = CloneCheat(block.CheatInput);
            plaus = null;
            retCode = 0;

            ConvertCore.g_incrypt = inCrypt;
            ConvertCore.g_indevice = InlineCryptParser.GetDeviceForCrypt(inCrypt);
            ConvertCore.g_outcrypt = outCrypt;
            ConvertCore.g_outdevice = InlineCryptParser.GetDeviceForCrypt(outCrypt);

            try
            {
                retCode = ConvertCore.ConvertCode(trial);
            }
            catch
            {
                retCode = -1;
                return false;
            }

            if (retCode != 0)
                return false;

            if (analyzeRawFamily)
            {
                plaus = RawPlausibility.ValidateFirstCheckableAddress(trial, outCrypt, mode);
                if (!plaus.IsValid)
                    return false;
            }

            return true;
        }


    }
}
