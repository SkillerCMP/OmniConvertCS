using System;
using System.IO;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal enum AnalyzeMode
    {
        None = 0,
        Analyze = 1,
        AnalyzeFix = 2
    }

    internal sealed class CliOptions
    {
        public AnalyzeMode Mode { get; private set; }
        public ConvertCore.Crypt OutputCrypt { get; private set; }

        /// <summary>
        /// Manual mode (interactive). When used with -af, prompts on failures so you can pick the next crypt to try.
        /// </summary>
        public bool Manual { get; private set; }

        /// <summary>
        /// Manual mode (interactive), but prompts for EVERY cheat (not only failures).
        /// Set via -mc / --manual-all / --manual-check-all.
        /// </summary>
        public bool ManualAll { get; private set; }

        /// <summary>
        /// Post-process output: rewrite $E001vvvv 0aaaaaaa to $Daaaaaaa 0000vvvv (CRAW only).
        /// </summary>
        public bool FixE001ToD { get; private set; }

        /// <summary>Input path: either a .txt file, or a folder containing .txt files.</summary>
        public string InputPath { get; private set; } = string.Empty;

        public bool InputIsFile => File.Exists(InputPath);
        public bool InputIsDirectory => Directory.Exists(InputPath);

        public string OutputRoot { get; private set; } = Environment.CurrentDirectory;

        public string OutputFolderName => OutputCrypt.ToString(); // e.g. CRYPT_RAW
        public string OutputDir => Path.Combine(OutputRoot, OutputFolderName);
        public string AnalysisDir => Path.Combine(OutputDir, "Analysis");

        public static bool TryParse(string[] args, out CliOptions options, out string error)
        {
            options = new CliOptions();
            error = string.Empty;

            if (args == null || args.Length == 0)
            {
                error = "No arguments provided.";
                return false;
            }

            int i = 0;

            // Options
            while (i < args.Length && args[i].StartsWith("-", StringComparison.Ordinal))
            {
                string a = args[i].Trim();

                if (string.Equals(a, "-a", StringComparison.OrdinalIgnoreCase))
                {
                    options.Mode = AnalyzeMode.Analyze;
                }
                else if (string.Equals(a, "-af", StringComparison.OrdinalIgnoreCase))
                {
                    options.Mode = AnalyzeMode.AnalyzeFix;
                }
                else if (string.Equals(a, "-e", StringComparison.OrdinalIgnoreCase))
                {
                    options.FixE001ToD = true;
                }

                else if (string.Equals(a, "-o", StringComparison.OrdinalIgnoreCase) || string.Equals(a, "--out", StringComparison.OrdinalIgnoreCase))
                {
                    i++;
                    if (i >= args.Length)
                    {
                        error = "Missing value for -o/--out.";
                        return false;
                    }
                    options.OutputRoot = args[i];
                }
                else if (string.Equals(a, "-mc", StringComparison.OrdinalIgnoreCase) ||
                         string.Equals(a, "--manual-all", StringComparison.OrdinalIgnoreCase) ||
                         string.Equals(a, "--manual-check-all", StringComparison.OrdinalIgnoreCase))
                {
                    options.Manual = true;
                    options.ManualAll = true;
                }
                else if (string.Equals(a, "-m", StringComparison.OrdinalIgnoreCase) || string.Equals(a, "--manual", StringComparison.OrdinalIgnoreCase))
                {
                    options.Manual = true;
                }
                else if (string.Equals(a, "-h", StringComparison.OrdinalIgnoreCase) ||
                         string.Equals(a, "--help", StringComparison.OrdinalIgnoreCase) ||
                         string.Equals(a, "/?", StringComparison.OrdinalIgnoreCase))
                {
                    error = "help";
                    return false;
                }
                else
                {
                    error = $"Unknown option: {args[i]}";
                    return false;
                }

                i++;
            }

            // Required: output crypt, input path
            if (i >= args.Length)
            {
                error = "Missing <Output-Crypt>.";
                return false;
            }

            if (!TryParseCrypt(args[i], out var outCrypt))
            {
                error = $"Unknown output crypt: {args[i]}";
                return false;
            }
            options.OutputCrypt = outCrypt;
            i++;

            if (i >= args.Length)
            {
                error = "Missing <input.txt | input_folder> path.";
                return false;
            }

            options.InputPath = args[i];

            if (!File.Exists(options.InputPath) && !Directory.Exists(options.InputPath))
            {
                error = $"Input path not found: {options.InputPath}";
                return false;
            }

            return true;
        }

        private static bool TryParseCrypt(string token, out ConvertCore.Crypt crypt)
        {
            crypt = ConvertCore.Crypt.CRYPT_RAW;

            if (string.IsNullOrWhiteSpace(token))
                return false;

            string t = token.Trim();
            if (t.StartsWith("CRYPT_", StringComparison.OrdinalIgnoreCase))
                t = t.Substring(6);

            t = t.ToUpperInvariant();

            switch (t)
            {
                case "AR1": crypt = ConvertCore.Crypt.CRYPT_AR1; return true;
                case "AR2": crypt = ConvertCore.Crypt.CRYPT_AR2; return true;
                case "ARMAX": crypt = ConvertCore.Crypt.CRYPT_ARMAX; return true;

                case "CB": crypt = ConvertCore.Crypt.CRYPT_CB; return true;
                case "CB7_COMMON": crypt = ConvertCore.Crypt.CRYPT_CB7_COMMON; return true;

                case "GS3": crypt = ConvertCore.Crypt.CRYPT_GS3; return true;
                case "GS5": crypt = ConvertCore.Crypt.CRYPT_GS5; return true;

                case "MAXRAW": crypt = ConvertCore.Crypt.CRYPT_MAXRAW; return true;

                case "RAW": crypt = ConvertCore.Crypt.CRYPT_RAW; return true;
                case "ARAW": crypt = ConvertCore.Crypt.CRYPT_ARAW; return true;
                case "CRAW": crypt = ConvertCore.Crypt.CRYPT_CRAW; return true;
                case "GRAW": crypt = ConvertCore.Crypt.CRYPT_GRAW; return true;
            }

            return false;
        }
    }
}
