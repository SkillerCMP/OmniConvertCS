using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static class TextCheatWriter
    {
        public static string WriteToText(List<(CheatBlock Block, cheat_t CheatConverted)> converted,
                                         ConvertCore.Crypt outCrypt)
        {
            var sb = new StringBuilder();

            bool outputAsArmaxEncrypted = (outCrypt == ConvertCore.Crypt.CRYPT_ARMAX);

            foreach (var item in converted)
            {
                string? header = item.Block.Label;
                if (string.IsNullOrWhiteSpace(header))
                {
                    // fallback
                    header = !string.IsNullOrWhiteSpace(item.CheatConverted.name) ? item.CheatConverted.name : "Unnamed Cheat";
                }

                string effectiveHeader = header;

                // Only update crypt tag if a CRYPT_ tag exists in the header already.
                if (header.IndexOf("CRYPT_", StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    effectiveHeader = InlineCryptParser.UpdateCryptTagForOutput(effectiveHeader, outCrypt);
                }

                if (!string.IsNullOrWhiteSpace(effectiveHeader))
                    sb.AppendLine(effectiveHeader);

                // Group-only (no codes)
                if (item.CheatConverted.codecnt == 0)
                    continue;

                if (outputAsArmaxEncrypted)
                {
                    for (int i = 0; i + 1 < item.CheatConverted.codecnt; i += 2)
                    {
                        string armCode = Armax.FormatEncryptedLine(item.CheatConverted.code[i], item.CheatConverted.code[i + 1]);
                        sb.AppendLine(armCode);
                    }
                }
                else
                {
                    // Build quick lookup for wildcard masks by word index
                    Dictionary<int, string?> masksByIndex = new Dictionary<int, string?>();
                    foreach (var m in item.Block.Wildcards)
                    {
                        masksByIndex[m.WordIndex] = m.ValMask;
                    }

                    for (int i = 0; i < item.CheatConverted.codecnt; i += 2)
                    {
                        uint addr = item.CheatConverted.code[i];
                        uint val = (i + 1 < item.CheatConverted.codecnt) ? item.CheatConverted.code[i + 1] : 0u;

                        string addrText = addr.ToString("X8", CultureInfo.InvariantCulture);
                        string valText = val.ToString("X8", CultureInfo.InvariantCulture);

                        if (masksByIndex.TryGetValue(i, out var mask) && !string.IsNullOrEmpty(mask))
                        {
                            valText = ApplyMaskToHex(valText, mask);
                        }

                        sb.Append(addrText).Append(' ').AppendLine(valText);
                    }
                }
            }

            return sb.ToString();
        }

        private static string ApplyMaskToHex(string hex8, string? mask)
        {
            if (string.IsNullOrEmpty(hex8) || string.IsNullOrEmpty(mask))
                return hex8;

            if (hex8.Length != 8 || mask.Length != 8)
                return hex8;

            char[] chars = hex8.ToCharArray();
            for (int i = 0; i < 8; i++)
            {
                char m = mask[i];
                if (m != '\0')
                    chars[i] = m;
            }
            return new string(chars);
        }
    }
}