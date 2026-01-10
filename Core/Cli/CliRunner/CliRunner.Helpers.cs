using System;

namespace OmniconvertCS.Cli
{
    internal static partial class CliRunner
    {
        private static string MakeAnalysisRelPath(string relPath)
        {
            // Preserve folder structure, but replace ".txt" with ".analysis.txt" when possible.
            if (relPath.EndsWith(".txt", StringComparison.OrdinalIgnoreCase))
                return relPath.Substring(0, relPath.Length - 4) + ".analysis.txt";
            return relPath + ".analysis.txt";
        }

        private static string ShortLabel(string? label)
        {
            if (string.IsNullOrWhiteSpace(label))
                return "(no label)";
            string s = label.Trim();
            if (s.Length > 80) s = s.Substring(0, 77) + "...";
            return s;
        }

        private static ConvertCore.Crypt GetDeclaredCrypt(CheatBlock b)
        {
            ConvertCore.Crypt inCrypt = ConvertCore.Crypt.CRYPT_RAW;
            if (b.Label != null && InlineCryptParser.TryGetCryptFromLabel(b.Label, out var inlineCrypt))
                inCrypt = inlineCrypt;
            else if (b.Label != null && b.Label.IndexOf("CRYPT_", StringComparison.OrdinalIgnoreCase) >= 0)
                inCrypt = ConvertCore.Crypt.CRYPT_RAW; // unknown tag -> default to RAW
            return inCrypt;
        }

        private static cheat_t CloneCheat(cheat_t src)
        {
            var c = Cheat.cheatInit();
            c.name = src.name;
            c.comment = src.comment;
            c.id = src.id;

            for (int i = 0; i < src.codecnt; i++)
                c.code.Add(src.code[i]);

            c.state = src.state;

            return c;
        }
    }
}
