using System;
using System.Globalization;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
        private static bool TryParseHexUInt(string s, out uint value)
        {
            value = 0;
            if (string.IsNullOrWhiteSpace(s)) return false;

            s = s.Trim();
            if (s.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                s = s.Substring(2);

            return uint.TryParse(s, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value);
        }

        private static void ParseManualFlags(string[] parts, int startIndex, out bool forceAccept, out uint? ar2Key)
        {
            forceAccept = false;
            ar2Key = null;

            for (int pi = startIndex; pi < parts.Length; pi++)
            {
                string tok = parts[pi].Trim();

                if (string.Equals(tok, "-y", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(tok, "y", StringComparison.OrdinalIgnoreCase))
                {
                    forceAccept = true;
                    continue;
                }

                if (tok.StartsWith("-k", StringComparison.OrdinalIgnoreCase))
                {
                    string hex = tok.Length > 2 ? tok.Substring(2) : string.Empty;

                    if (string.IsNullOrWhiteSpace(hex) && (pi + 1) < parts.Length)
                        hex = parts[++pi];

                    if (TryParseHexUInt(hex, out uint k))
                        ar2Key = k;

                    continue;
                }
            }
        }

        private static ManualAction PromptManualPickInputCrypt(CheatBlock block,
                                                              ConvertCore.Crypt outCrypt,
                                                              ConvertCore.Crypt declaredCrypt)
        {
            CliUi.PrintManualSelectInputHeader(block, outCrypt, declaredCrypt);

            CliUi.PrintOriginalBlockIfAny(block);

            CliUi.PrintMenu_SelectInputCrypt(s_manualMenu);
            string? input = CliUi.PromptSelection();
            if (input == null)
                return new ManualAction { OverrideCrypt = declaredCrypt };

            input = input.Trim();
            if (input.Length == 0)
                return new ManualAction { OverrideCrypt = declaredCrypt };

            if (string.Equals(input, "Q", StringComparison.OrdinalIgnoreCase))
                return ManualAction.Quit;

            if (string.Equals(input, "D", StringComparison.OrdinalIgnoreCase))
                return ManualAction.DeleteCheat;

            var parts = input.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length >= 1 && int.TryParse(parts[0], out int n) && n >= 1 && n <= s_manualMenu.Length)
            {
                ParseManualFlags(parts, 1, out bool force, out uint? ar2Key);
                return new ManualAction { OverrideCrypt = s_manualMenu[n - 1], ForceAccept = force, Ar2Key = ar2Key };
            }

            CliUi.PrintInvalidSelectionUsingDeclared();
            return new ManualAction { OverrideCrypt = declaredCrypt };
        }
    }
}
