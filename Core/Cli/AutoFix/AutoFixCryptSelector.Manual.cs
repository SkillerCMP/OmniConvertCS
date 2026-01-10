using System;
using System.Collections.Generic;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
        private sealed class ManualAction
        {
            public static ManualAction ContinueAuto { get; } = new ManualAction();
            public static ManualAction SkipCheat { get; } = new ManualAction();
            public static ManualAction Quit { get; } = new ManualAction();
            public static ManualAction DeleteCheat { get; } = new ManualAction();
            public ConvertCore.Crypt? OverrideCrypt { get; init; }
            public bool ForceAccept { get; init; }
            public uint? Ar2Key { get; init; }
        }

        private static ManualAction PromptManualOnFail(CheatBlock block,
                                                       ConvertCore.Crypt outCrypt,
                                                       ConvertCore.Crypt attemptedCrypt,
                                                       int ret,
                                                       RawPlausibilityResult? plaus,
                                                       cheat_t? trial)
        {
            CliUi.PrintManualAttemptFailedHeader(block, outCrypt, attemptedCrypt, ret, plaus);


            // Show original source block before the converted attempt output
            CliUi.PrintOriginalBlockIfAny(block);
            // Print the converted output (only if the convert step succeeded)
            if (ret == 0 && trial != null && trial.codecnt > 0)
            {
                try
                {
                    var txt = TextCheatWriter.WriteToText(new List<(CheatBlock Block, cheat_t CheatConverted)>
                    {
                        (block, trial)
                    }, outCrypt);
                    CliUi.PrintConvertedOutputAttempt(txt);
                }
                catch
                {
                    // ignore
                }
            }

            CliUi.PrintMenu_PickNextCrypt_Fail(s_manualMenu);
            string? input = CliUi.PromptSelection();
            if (input == null)
                return ManualAction.ContinueAuto;
            input = input.Trim();

            if (input.Length == 0)
                return ManualAction.ContinueAuto;

            if (string.Equals(input, "S", StringComparison.OrdinalIgnoreCase))
                return ManualAction.SkipCheat;

            if (string.Equals(input, "Q", StringComparison.OrdinalIgnoreCase))
                return ManualAction.Quit;

            if (string.Equals(input, "D", StringComparison.OrdinalIgnoreCase))
                return ManualAction.DeleteCheat;

            
var parts = input.Split(new[] { ' ', '	' }, StringSplitOptions.RemoveEmptyEntries);
if (parts.Length >= 1 && int.TryParse(parts[0], out int n) && n >= 1 && n <= s_manualMenu.Length)
{
    ParseManualFlags(parts, 1, out bool force, out uint? ar2Key);

    return new ManualAction { OverrideCrypt = s_manualMenu[n - 1], ForceAccept = force, Ar2Key = ar2Key };
}

CliUi.PrintInvalidSelectionContinuingAuto();
            return ManualAction.ContinueAuto;
        }





private static ManualAction PromptManualConfirmOnPass(CheatBlock block,
                                                      ConvertCore.Crypt outCrypt,
                                                      ConvertCore.Crypt selectedCrypt,
                                                      RawPlausibilityResult? plaus,
                                                      cheat_t? converted,
                                                      uint inputAddr28,
                                                      string inputRule)
{
    CliUi.PrintManualConfirmHeader(block, outCrypt, selectedCrypt, inputAddr28, inputRule, plaus);

    // Show original source block before the converted output
    CliUi.PrintOriginalBlockIfAny(block);

    if (converted != null)
    {
        // Converted output (current selection)
        try
        {
            var txt = TextCheatWriter.WriteToText(new List<(CheatBlock Block, cheat_t CheatConverted)>
            {
                (block, converted)
            }, outCrypt);
            CliUi.PrintConvertedOutputCurrentSelection(txt);
        }
        catch
        {
            // ignore
        }
    }

    CliUi.PrintMenu_PickNextCrypt_Confirm(s_manualMenu);

            string? input = CliUi.PromptSelection();
    if (input == null)
        return ManualAction.ContinueAuto;

    input = input.Trim();
    if (input.Length == 0)
        return ManualAction.ContinueAuto;

    if (string.Equals(input, "Q", StringComparison.OrdinalIgnoreCase))
        return ManualAction.Quit;

    if (string.Equals(input, "D", StringComparison.OrdinalIgnoreCase))
        return ManualAction.DeleteCheat;

    // Allow "N -y" and "N -k<hex>" on confirm too
var parts = input.Split(new[] { ' ', '	' }, StringSplitOptions.RemoveEmptyEntries);
if (parts.Length >= 1 && int.TryParse(parts[0], out int n))
{
    ParseManualFlags(parts, 1, out bool force, out uint? ar2Key);
    if (n >= 1 && n <= s_manualMenu.Length)
        return new ManualAction { OverrideCrypt = s_manualMenu[n - 1], ForceAccept = force, Ar2Key = ar2Key };
}

CliUi.PrintInvalidSelectionAcceptingCurrent();
return ManualAction.ContinueAuto;
}

    }
}
