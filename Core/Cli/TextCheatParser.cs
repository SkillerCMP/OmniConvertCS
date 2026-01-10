using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal sealed class WildcardMask
    {
        public int WordIndex { get; init; } // index of ADDR word in cheat.code (VALUE is WordIndex+1)
        public string? ValMask { get; init; } // 8-char overlay mask (e.g. "????\0\0\0\0") style, '?' used
    }

    internal sealed class CheatBlock
    {
        public string? Label { get; init; }
        public cheat_t CheatInput { get; init; } = null!;
        public List<WildcardMask> Wildcards { get; } = new();

        // True if we parsed any ARMAX encrypted text lines (e.g. "PNDF-M5ZV-ACXTE") for this block.
        // This is used to suppress RAW-style "addr28 <= 0x01FFFFFF" heuristics on the *input* side.
        public bool HasArmaxEncryptedTextLine { get; set; }
    
        public List<string> OriginalLines { get; } = new();
}

    internal static class TextCheatParser
    {
        public static List<CheatBlock> ParseFile(string path)
        {
            // Many community TXT files are not strict UTF-8. Read with a non-throwing UTF-8 decoder
            // so folder runs don't abort on a single legacy-encoded file.
            string text = File.ReadAllText(path, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: false));
            return ParseText(text);
        }

        public static List<CheatBlock> ParseText(string inputText)
        {
            var blocks = new List<CheatBlock>();
            CheatBlock? current = null;

            void StartNewBlock(string? label, string? rawLabelLine)
            {
                current = new CheatBlock
                {
                    Label = label,
                    CheatInput = Cheat.cheatInit()
                };
                blocks.Add(current);
            
                if (!string.IsNullOrEmpty(rawLabelLine))
                    current.OriginalLines.Add(rawLabelLine);
}

            CheatBlock EnsureCurrentBlock()
            {
                if (current == null)
                {
                    StartNewBlock(null, null);
                }
                return current!;
            }

            string[] lines = inputText.Replace("\r\n", "\n").Split('\n');
            foreach (var rawLine in lines)
            {
                string raw = rawLine.TrimEnd();
                string line = raw.Trim();
                if (string.IsNullOrEmpty(line))
                    continue;

                if (line.StartsWith("//") || line.StartsWith("#") || line.StartsWith(";"))
                    continue;

                // Tokenize on whitespace
                string[] parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length == 0)
                    continue;

                // ARMAX encrypted line: last token is XXXXX-XXXXX-XXXXX
                string lastToken = parts[parts.Length - 1];
                if (Armax.IsArMaxStr(lastToken) && Armax.TryParseEncryptedLine(lastToken, out uint w0, out uint w1))
                {
                    var blk = EnsureCurrentBlock();
                    Cheat.cheatAppendOctet(blk.CheatInput, w0);
                    Cheat.cheatAppendOctet(blk.CheatInput, w1);
                    blk.HasArmaxEncryptedTextLine = true;

                    // Keep original source line for manual/analysis output
                    blk.OriginalLines.Add(raw);
                    continue;
                }

                // Standard hex line: ADDR VALUE (ADDR can be 6-8 hex digits, VALUE can include wildcards like ??)
                if (parts.Length >= 2 && TryParseHexAddressToken(parts[0], out uint addrWord))
                {
                    string valToken = parts[1];

                    SanitizeHexWithMask(valToken, out string valHex, out string? valMask);

                    if (uint.TryParse(valHex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _))
                    {
                        var blk = EnsureCurrentBlock();
                        int baseIndex = blk.CheatInput.codecnt; // index of addr word

                        string addrHex = addrWord.ToString("X8", CultureInfo.InvariantCulture);
                        Cheat.cheatAppendCodeFromText(blk.CheatInput, addrHex, valHex);

                        // Keep original source line for manual/analysis output
                        blk.OriginalLines.Add(raw);

                        if (!string.IsNullOrEmpty(valMask))
                        {
                            blk.Wildcards.Add(new WildcardMask
                            {
                                WordIndex = baseIndex,
                                ValMask = valMask
                            });
                        }
                        continue;
                    }
                }

                // Otherwise treat as label/group/name line
                StartNewBlock(line, raw);
            }

            return blocks;
        }

        private static bool TryParseHexAddressToken(string token, out uint addrWord)
        {
            addrWord = 0;
            if (string.IsNullOrWhiteSpace(token)) return false;

            string t = token.Trim();

            // Strip trailing punctuation/comments from token
            int commentIdx = t.IndexOfAny(new[] { ' ', '\t', '#', ';', '/' });
            if (commentIdx >= 0) t = t.Substring(0, commentIdx);

            // Allow 6-8 hex digits
            if (t.Length < 6 || t.Length > 8) return false;

            return uint.TryParse(t, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out addrWord);
        }

        private static void SanitizeHexWithMask(string token, out string sanitizedHex, out string? maskOrNull)
        {
            // Ensure 8 chars, treat any non-hex as '0' and record original char in mask.
            // We only use '?' in practice, but keep it generic.
            char[] hexChars = new char[8];
            char[] maskChars = new char[8];
            bool hasNonHex = false;

            string t = token.Trim();
            if (t.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                t = t.Substring(2);

            // left pad with zeros if shorter
            if (t.Length < 8)
                t = t.PadLeft(8, '0');
            else if (t.Length > 8)
                t = t.Substring(0, 8);

            for (int i = 0; i < 8; i++)
            {
                char c = t[i];
                bool isHex =
                    (c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F');

                if (isHex)
                {
                    hexChars[i] = char.ToUpperInvariant(c);
                    maskChars[i] = '\0';
                }
                else
                {
                    hexChars[i] = '0';
                    maskChars[i] = c; // keep '?' or other marker
                    hasNonHex = true;
                }
            }

            sanitizedHex = new string(hexChars);
            maskOrNull = hasNonHex ? new string(maskChars) : null;
        }
    }
}