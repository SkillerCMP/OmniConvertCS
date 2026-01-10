using System;
using System.Collections.Generic;
using System.Linq;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
        // Common AR2 key codes seen in the wild (can differ per cheat/source).
        private static readonly uint[] s_commonAr2Keys = new uint[]
        {
            0x1746EAADu,
            0x1645EBB3u,
            0x1853E59Eu,
        };

        private static readonly ConvertCore.Crypt[] s_manualMenu = new[]
        {
            ConvertCore.Crypt.CRYPT_GS3,
			ConvertCore.Crypt.CRYPT_AR1,
            ConvertCore.Crypt.CRYPT_AR2,
            ConvertCore.Crypt.CRYPT_ARMAX,
            ConvertCore.Crypt.CRYPT_CB,
            ConvertCore.Crypt.CRYPT_CB7_COMMON,
            ConvertCore.Crypt.CRYPT_GS5,
            ConvertCore.Crypt.CRYPT_MAXRAW,
            ConvertCore.Crypt.CRYPT_RAW,
            ConvertCore.Crypt.CRYPT_ARAW,
            ConvertCore.Crypt.CRYPT_CRAW,
            ConvertCore.Crypt.CRYPT_GRAW,
        };

        public static AutoFixResult ConvertWithAutoFix(CheatBlock block,
                                                       ConvertCore.Crypt outCrypt,
                                                       AnalyzeMode mode,
                                                       bool analyzeRawFamily,
                                                       bool manual,
                                                       bool manualAll = false)
        {
            if (block == null) throw new ArgumentNullException(nameof(block));

            manual = manual || manualAll;

            // Determine the "declared" input crypt
            ConvertCore.Crypt declaredCrypt = ConvertCore.Crypt.CRYPT_RAW;
            bool hasCryptTag = block.Label != null && block.Label.IndexOf("CRYPT_", StringComparison.OrdinalIgnoreCase) >= 0;
            if (block.Label != null && InlineCryptParser.TryGetCryptFromLabel(block.Label, out var inlineCrypt))
                declaredCrypt = inlineCrypt;
            else if (block.Label != null && hasCryptTag)
            {
                // Unknown tag -> InlineCryptParser returns false, but we still treat as RAW.
                declaredCrypt = ConvertCore.Crypt.CRYPT_RAW;
            }

            // Speed/QoL: If the declared crypt is already a RAW-family input, don't try to auto-fix.
            // Just convert as declared. (Except in full-manual mode.)
            if (!manualAll && (declaredCrypt == ConvertCore.Crypt.CRYPT_RAW ||
                declaredCrypt == ConvertCore.Crypt.CRYPT_CRAW ||
                declaredCrypt == ConvertCore.Crypt.CRYPT_GRAW))
            {
                cheat_t quick = CloneCheat(block.CheatInput);
                ConvertCore.g_incrypt = declaredCrypt;
                ConvertCore.g_indevice = InlineCryptParser.GetDeviceForCrypt(declaredCrypt);
                ConvertCore.g_outcrypt = outCrypt;
                ConvertCore.g_outdevice = InlineCryptParser.GetDeviceForCrypt(outCrypt);

                int retQuick;
                try { retQuick = ConvertCore.ConvertCode(quick); }
                catch
                {
                    retQuick = -1;
                }

                return new AutoFixResult
                {
                    Fixed = false,
                    UsedInputCrypt = declaredCrypt,
                    CheatConverted = quick,
                    Plausibility = (analyzeRawFamily && retQuick == 0) ? RawPlausibility.ValidateFirstCheckableAddress(quick, outCrypt, mode) : null,
                    Note = "Declared RAW-family input; no auto-fix attempted"
                };
            }

            
ConvertCore.Crypt? manualOverride = null;
bool forceAcceptNextAttempt = false;
uint? manualAr2KeyNextAttempt = null;

if (manualAll)
{
    var early = InitManualAllPick(block, outCrypt, declaredCrypt, out var picked, out forceAcceptNextAttempt, out manualAr2KeyNextAttempt);
    if (early != null) return early;
    manualOverride = picked;
}

// Candidate list: start with declared, then other known crypts
            var candidates = BuildCandidateList(declaredCrypt);

            var tried = new HashSet<ConvertCore.Crypt>();
            int autoPos = 0;

            while (true)
            {
                ConvertCore.Crypt inCrypt;
                bool forceAcceptThisAttempt = false;
                bool isManualAttempt = false;
                uint? manualAr2KeyThisAttempt = null;
                if (manualOverride.HasValue)
                {
                    inCrypt = manualOverride.Value;
                    isManualAttempt = true;
                    manualOverride = null;
                    forceAcceptThisAttempt = forceAcceptNextAttempt;
                    forceAcceptNextAttempt = false;
                    manualAr2KeyThisAttempt = manualAr2KeyNextAttempt;
                    manualAr2KeyNextAttempt = null;
                }
                else
                {
                    if (autoPos >= candidates.Count)
                        break;
                    inCrypt = candidates[autoPos++];
                }

                if (!isManualAttempt)
                {
                    if (tried.Contains(inCrypt))
                        continue;
                    tried.Add(inCrypt);
                }

                // Attempt conversion (with AR2-key variants when trying AR2)
                bool analyzeThisAttempt = analyzeRawFamily && !forceAcceptThisAttempt;

                cheat_t? trial = null;
                RawPlausibilityResult? plaus = null;
                int retCode = 0;
                string note = string.Empty;

                if (TryConvertCandidate(block, inCrypt, outCrypt, analyzeThisAttempt, mode, manualAr2KeyThisAttempt, out trial, out plaus, out retCode, out note))
                {
                    // If user forced acceptance (-y), still compute plausibility for reporting
                    // but do NOT block acceptance.
                    if (forceAcceptThisAttempt && analyzeRawFamily)
                    {
                        plaus = RawPlausibility.ValidateFirstCheckableAddress(trial!, outCrypt, mode);
                        note = string.IsNullOrWhiteSpace(note) ? "Manual -y bypass" : (note + " (Manual -y bypass)");
                    }

// If manual mode is enabled, and the ORIGINAL input already looks like a plausible RAW-family address
// (addr28 within 0..0x01FFFFFF), then even a "PASS" can be a false-positive.
// In that case, prompt the user to confirm/override before accepting the candidate.
if (manual && !forceAcceptThisAttempt && !IsRawFamilyCrypt(declaredCrypt) && !IsRawFamilyCrypt(inCrypt))
{

    if (InputAddr28InStdRange(block, out uint inAddr28, out string inRule))
    {
        var confirm = PromptManualConfirmOnPass(block, outCrypt, inCrypt, plaus, trial, inAddr28, inRule);
        if (confirm == ManualAction.Quit)
            throw new OperationCanceledException("Manual quit");
        if (confirm == ManualAction.DeleteCheat)
            return new AutoFixResult { Deleted = true, Fixed = false, UsedInputCrypt = declaredCrypt, CheatConverted = CloneCheat(block.CheatInput), Note = "Deleted by user (manual confirm)" };

        if (confirm.OverrideCrypt.HasValue)
        {
            manualOverride = confirm.OverrideCrypt.Value;
            forceAcceptNextAttempt = confirm.ForceAccept;
            continue;
        }
        // ContinueAuto => accept current selection
    }
}


                    return new AutoFixResult
                    {
                        Fixed = (inCrypt != declaredCrypt),
                        UsedInputCrypt = inCrypt,
                        CheatConverted = trial!,
                        Plausibility = plaus,
                        Note = note
                    };
                }
                // Manual prompt on failure (CLI-only). Lets you pick which crypt to try next.
                if (manual)
                {
                    var action = PromptManualOnFail(block, outCrypt, inCrypt, retCode, plaus, trial);
                    if (action == ManualAction.ContinueAuto)
                        continue;
                    if (action == ManualAction.SkipCheat)
                        break;
                    if (action == ManualAction.Quit)
                        throw new OperationCanceledException("Manual quit");
                    if (action == ManualAction.DeleteCheat)
                        return new AutoFixResult { Deleted = true, Fixed = false, UsedInputCrypt = declaredCrypt, CheatConverted = CloneCheat(block.CheatInput), Note = "Deleted by user (manual)" };
                    if (action.OverrideCrypt.HasValue)
                    {
                        manualOverride = action.OverrideCrypt.Value;
                        forceAcceptNextAttempt = action.ForceAccept;
                        manualAr2KeyNextAttempt = action.Ar2Key;
                        continue;
                    }
                }
            }

            // No candidate passed; fall back to declared crypt conversion (even if bad)
            cheat_t fallback = CloneCheat(block.CheatInput);
            ConvertCore.g_incrypt = declaredCrypt;
            ConvertCore.g_indevice = InlineCryptParser.GetDeviceForCrypt(declaredCrypt);

            ConvertCore.g_outcrypt = outCrypt;
            ConvertCore.g_outdevice = InlineCryptParser.GetDeviceForCrypt(outCrypt);

            int retFallback;
            try { retFallback = ConvertCore.ConvertCode(fallback); }
            catch { retFallback = -1; }
            RawPlausibilityResult? plausFallback = analyzeRawFamily ? RawPlausibility.ValidateFirstCheckableAddress(fallback, outCrypt, mode) : null;

            return new AutoFixResult
            {
                Fixed = false,
                UsedInputCrypt = declaredCrypt,
                CheatConverted = fallback,
                Plausibility = plausFallback,
                Note = retFallback == 0 ? "No candidate passed analysis; using declared crypt" : "Conversion failed for all candidates"
            };
        }


    }
}
