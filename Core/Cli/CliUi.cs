using System;
using System.Collections.Generic;
using System.IO;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    /// <summary>
    /// Centralized console UI for the CLI so all screen output can be adjusted in one place.
    /// Keep output text/format identical to current behavior unless explicitly changed.
    /// </summary>
    internal static class CliUi
    {
        public static TextWriter Out { get; set; } = Console.Out;
        public static TextWriter Err { get; set; } = Console.Error;
        public static TextReader In { get; set; } = Console.In;

        public static void WriteLine() => Out.WriteLine();
        public static void WriteLine(string text) => Out.WriteLine(text);
        public static void Write(string text) => Out.Write(text);
        public static void ErrorLine(string text) => Err.WriteLine(text);
        public static string? ReadLine() => In.ReadLine();

        public static void PrintUsage()
        {
            WriteLine("OmniConvertCS CLI");
            WriteLine();
            WriteLine("Usage:");
            WriteLine("  OmniConvertCS.exe [-a|-af] [-m|-mc] [-o <outdir>] <Output-Crypt> <input.txt | input_folder>");
            WriteLine();
            WriteLine("Options:");
            WriteLine("  -a   Analyze mode. If output crypt is CRYPT_RAW/CRYPT_GRAW/CRYPT_CRAW,");
            WriteLine("       validates the first checkable RAW-family address is <= 0x01FFFFFF.");
            WriteLine("  -af  Analyze+Fix. If analysis fails, tries alternate input crypts per-cheat.");
            WriteLine("  -m   Manual. When used with -af, prompts you to pick the next crypt to try on failures.");
            WriteLine("  -mc  100% manual mode. When used with -af, prompts on EVERY cheat (not just failures).");
            WriteLine("  -o <dir>  Output root directory (default: current folder).");
            WriteLine();
            WriteLine("Interactive manual prompt commands:");
            WriteLine("  Enter = continue auto order");
            WriteLine("  S     = skip this cheat (use declared crypt output)");
            WriteLine("  D     = delete this cheat (omit from output)");
            WriteLine("  Q     = quit");
            WriteLine("  -y  = force accept crypt # (Example: 9 -y)(bypass plausibility checks)");
            WriteLine("  -k<HEX> = for AR2 selection, set AR2 key for that attempt (Example: 3 -k1645EBB3)(1746EAAD , 1645EBB3 , 1853E59E)");
            WriteLine();
            WriteLine("Output:");
            WriteLine("  Outputs are written to: .\\<Output-Crypt>\\");
            WriteLine("  Analysis reports are written to: .\\<Output-Crypt>\\Analysis\\");
            WriteLine();
            WriteLine("Examples:");
            WriteLine("  OmniConvertCS.exe -a  CRYPT_RAW  input.txt");
            WriteLine("  OmniConvertCS.exe -af CRYPT_GRAW input_folder");
            WriteLine("  OmniConvertCS.exe -af -o D:\\Out CRYPT_CRAW input_folder");
            WriteLine();
        }

        public static void PrintNoTxtFilesFound()
        {
            ErrorLine("No .txt files found to process.");
        }

        public static void PrintManualQuitRequested()
        {
            ErrorLine("Manual mode: quit requested.");
        }

        public static void PrintRunSummaryLine(string relPath, string outputFolderName, int failCount, int fixedCount)
        {
            WriteLine($"{relPath} -> {outputFolderName}  (fail={failCount}, fixed={fixedCount})");
        }

        public static void PrintFileError(string relPath, Exception ex)
        {
            ErrorLine($"ERROR: {relPath}  {ex.GetType().Name}: {ex.Message}");
        }

        public static void PrintArgParseError(string err)
        {
            ErrorLine(err);
        }

        
public static void PrintCurrentFileBannerIfKnown()
{
    string? currentFile = CliScreenContext.CurrentFile;
    if (!string.IsNullOrWhiteSpace(currentFile))
    {
        WriteLine("----------------------------------------------");
        WriteLine($"Current File :  {currentFile}");
        WriteLine();
    }
}

public static void PrintOriginalBlockIfAny(CheatBlock block)
        {
            if (block.OriginalLines.Count <= 0)
                return;

            WriteLine();
            WriteLine("=== ORIGINAL (input) ===");
            foreach (var l in block.OriginalLines)
                WriteLine(l);
        }

        public static void PrintConvertedTextBlock(string titleLine, string text)
        {
            WriteLine();
            WriteLine(titleLine);
            WriteLine(text);
        }

        public static void PrintConvertedOutputAttempt(string text)
        {
            PrintConvertedTextBlock("--- Converted output (this attempt): ---", text);
        }

        public static void PrintConvertedOutputCurrentSelection(string text)
        {
            PrintConvertedTextBlock("--- Converted output (current selection): ---", text);
        }


        public static void PrintManualAttemptFailedHeader(CheatBlock block,
                                                          ConvertCore.Crypt outCrypt,
                                                          ConvertCore.Crypt attemptedCrypt,
                                                          int ret,
                                                          RawPlausibilityResult? plaus)
        {
            WriteLine();
			PrintCurrentFileBannerIfKnown();
            WriteLine("[MANUAL] Attempt failed:");
            WriteLine($"  {block.Label ?? block.CheatInput.name ?? "(no label)"}");
            WriteLine($"  attempted={attemptedCrypt}  output={outCrypt}");
            if (ret != 0)
                WriteLine($"  convert-error={ret}");
            if (plaus != null && !plaus.IsValid)
                WriteLine($"  plausibility FAIL addr28=0x{plaus.CheckedAddr28:X7} rule={plaus.Rule}");
            if (plaus != null && !string.IsNullOrWhiteSpace(plaus.Note))
                WriteLine($"  note: {plaus.Note}");
        }

        public static void PrintManualConfirmHeader(CheatBlock block,
                                                    ConvertCore.Crypt outCrypt,
                                                    ConvertCore.Crypt selectedCrypt,
                                                    uint inputAddr28,
                                                    string inputRule,
                                                    RawPlausibilityResult? plaus)
        {
            WriteLine();
			PrintCurrentFileBannerIfKnown();
            WriteLine("[MANUAL] Candidate PASSED checks, but input looks plausible (possible false-positive). Please confirm:");
            WriteLine($"  {block.Label ?? block.CheatInput.name ?? "(no label)"}");
            WriteLine($"  selected={selectedCrypt}  output={outCrypt}");
            WriteLine($"  input-hint PASS addr28=0x{inputAddr28:X7} hint={inputRule}");
            if (plaus != null && plaus.IsValid)
                WriteLine($"  output-check PASS addr28=0x{plaus.CheckedAddr28:X7} rule={plaus.Rule}");
        }

        public static void PrintManualSelectInputHeader(CheatBlock block,
                                                        ConvertCore.Crypt outCrypt,
                                                        ConvertCore.Crypt declaredCrypt)
        {
            WriteLine();
			PrintCurrentFileBannerIfKnown();
            WriteLine("[MANUAL] Select input crypt:");
            WriteLine($"  {block.Label ?? block.CheatInput.name ?? "(no label)"}");
            WriteLine($"  declared={declaredCrypt}  output={outCrypt}");
        }

        public static void PrintMenu_PickNextCrypt_Fail(IReadOnlyList<string> menu)
        {
			WriteLine("Pick next crypt to try:");
            for (int i = 0; i < menu.Count; i++)
                WriteLine($"  {i + 1,2}) {menu[i]}");
            WriteLine("  Enter = continue auto order");
            WriteLine("  S     = skip this cheat (use declared crypt output)");
            WriteLine("  Q     = quit");
            WriteLine("  D     = delete this cheat (omit from output)");
            WriteLine("  -y      = force accept crypt # (Example: 9 -y)(bypass plausibility checks)");
            WriteLine("  -k<hex> = set AR2 key for crypt # (AR2 only)(Example: 3 -k1645EBB3)(1746EAAD , 1645EBB3 , 1853E59E)");
        }

        private static IReadOnlyList<string> CryptMenuToStrings(IReadOnlyList<ConvertCore.Crypt> crypts)
        {
            if (crypts == null || crypts.Count <= 0)
                return Array.Empty<string>();

            var arr = new string[crypts.Count];
            for (int i = 0; i < crypts.Count; i++)
                arr[i] = crypts[i].ToString();
            return arr;
        }

        // Overloads so callers can pass ConvertCore.Crypt[] directly (preferred for logic)
        // while keeping all display formatting here in CliUi.
        public static void PrintMenu_PickNextCrypt_Fail(IReadOnlyList<ConvertCore.Crypt> menuCrypts)
            => PrintMenu_PickNextCrypt_Fail(CryptMenuToStrings(menuCrypts));

        public static void PrintMenu_PickNextCrypt_Confirm(IReadOnlyList<string> menu)
        {
            WriteLine("Pick next crypt to try (or press Enter to ACCEPT current):");
            for (int i = 0; i < menu.Count; i++)
                WriteLine($"  {i + 1,2}) {menu[i]}");
            WriteLine("  Enter = accept current selection");
            WriteLine("  Q     = quit");
            WriteLine("  D     = delete this cheat (omit from output)");
            WriteLine("  -y      = force accept crypt # (Example: 9 -y)(bypass plausibility checks)");
            WriteLine("  -k<hex> = set AR2 key for crypt # (AR2 only)(Example: 3 -k1645EBB3)(1746EAAD , 1645EBB3 , 1853E59E)");
        }

        public static void PrintMenu_PickNextCrypt_Confirm(IReadOnlyList<ConvertCore.Crypt> menuCrypts)
            => PrintMenu_PickNextCrypt_Confirm(CryptMenuToStrings(menuCrypts));

        public static void PrintMenu_SelectInputCrypt(IReadOnlyList<string> menu)
        {
            WriteLine();
            WriteLine("Pick input crypt to use (or press Enter to use declared):");
            for (int i = 0; i < menu.Count; i++)
                WriteLine($"  {i + 1,2}) {menu[i]}");
            WriteLine("  Enter = use declared");
            WriteLine("  Q     = quit");
            WriteLine("  D     = delete this cheat (omit from output)");
            WriteLine("  -y      = force accept crypt # (Example: 9 -y)(bypass plausibility checks)");
            WriteLine("  -k<hex> = set AR2 key for crypt # (AR2 only)(Example: 3 -k1645EBB3)(1746EAAD , 1645EBB3 , 1853E59E)");
        }

        public static void PrintMenu_SelectInputCrypt(IReadOnlyList<ConvertCore.Crypt> menuCrypts)
            => PrintMenu_SelectInputCrypt(CryptMenuToStrings(menuCrypts));

        public static void PrintSelectionPrompt()
        {
            Write("Selection: ");
        }

        public static string? PromptSelection()
        {
            PrintSelectionPrompt();
            return ReadLine();
        }

        public static void PrintInvalidSelectionUsingDeclared()
        {
            WriteLine("Invalid selection. Using declared.");
        }

        public static void PrintInvalidSelectionContinuingAuto()
        {
            WriteLine("Invalid selection. Continuing auto.");
        }

        public static void PrintInvalidSelectionAcceptingCurrent()
        {
            WriteLine("Invalid selection. Accepting current.");
        }
    }
}
