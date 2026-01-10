using System;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace OmniconvertCS.Cli
{
    internal static class E001ToDFixer
    {
        // Matches:
        //   $E0010060 00293580
        // or without '$'
        //   E001BDFF 0103AB02
        //
        // NOTE: second word MUST start with '0' (0aaaaaaa).
        private static readonly Regex Rx = new Regex(
            @"^(?<dol>\$)?E001(?<vvvv>[0-9A-Fa-f]{4})\s+(?<addr0>0[0-9A-Fa-f]{7})\s*$",
            RegexOptions.Compiled);

        public static string RewriteE001ToD(string text)
        {
            if (string.IsNullOrEmpty(text))
                return text ?? string.Empty;

            var sb = new StringBuilder(text.Length + 64);

            // Keep original line endings as much as possible: split on \n but preserve \r if present.
            string[] lines = text.Split('\n');
            for (int i = 0; i < lines.Length; i++)
            {
                string line = lines[i];
                string lineNoNl = line;
                string nl = "\n";
                if (i == lines.Length - 1 && !text.EndsWith("\n", StringComparison.Ordinal))
                    nl = string.Empty;

                // Preserve CR if present (because split kept it on the line)
                string trailing = string.Empty;
                if (lineNoNl.EndsWith("\r", StringComparison.Ordinal))
                {
                    trailing = "\r";
                    lineNoNl = lineNoNl[..^1];
                }

                string rewritten = RewriteLine(lineNoNl);
                sb.Append(rewritten);
                sb.Append(trailing);
                sb.Append(nl);
            }

            return sb.ToString();
        }

        private static string RewriteLine(string line)
        {
            var m = Rx.Match(line);
            if (!m.Success)
                return line;

            // Extract
            string dol = m.Groups["dol"].Success ? "$" : "";
            string vvvv = m.Groups["vvvv"].Value.ToUpperInvariant();
            string addr0 = m.Groups["addr0"].Value.ToUpperInvariant(); // 0xxxxxxx

            // addr28 range check (0..0x1FFFFFF)
            uint addrWord = uint.Parse(addr0, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
            uint addr28 = addrWord & 0x0FFFFFFFu;
            if (addr28 > 0x01FFFFFFu)
                return line;

            // Build Daaaaaaa 0000vvvv
            uint dWord = 0xD0000000u | addr28;
            string dWordHex = dWord.ToString("X8", CultureInfo.InvariantCulture);
            string valHex = "0000" + vvvv;

            return $"{dol}{dWordHex} {valHex}";
        }
    }
}
