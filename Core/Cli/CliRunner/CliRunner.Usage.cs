using System;

namespace OmniconvertCS.Cli
{
    internal static partial class CliRunner
    {
        private static void PrintUsage()
        {
            Console.WriteLine("OmniConvertCS CLI");
            Console.WriteLine();
            Console.WriteLine("Usage:");
            Console.WriteLine("  OmniConvertCS.exe [-a|-af] [-m|-mc] [-o <outdir>] <Output-Crypt> <input.txt | input_folder>");
            Console.WriteLine();
            Console.WriteLine("Options:");
            Console.WriteLine("  -a   Analyze mode. If output crypt is CRYPT_RAW/CRYPT_GRAW/CRYPT_CRAW,");
            Console.WriteLine("       validates the first checkable RAW-family address is <= 0x01FFFFFF.");
            Console.WriteLine("  -af  Analyze+Fix. If analysis fails, tries alternate input crypts per-cheat.");
            Console.WriteLine("  -m   Manual. When used with -af, prompts you to pick the next crypt to try on failures.");
            Console.WriteLine("  -mc  Manual check ALL: prompt for every cheat (implies -m).");
            Console.WriteLine("  -e   (CRAW only) Rewrite E001vvvv 0aaaaaaa lines to Daaaaaaa 0000vvvv in the output text.");
            Console.WriteLine("  -o <dir>  Output root directory (default: current folder).");
            Console.WriteLine();
            Console.WriteLine("Output:");
            Console.WriteLine("  Outputs are written to: .\\<Output-Crypt>\\");
            Console.WriteLine("  Analysis reports are written to: .\\<Output-Crypt>\\Analysis\\");
            Console.WriteLine();
            Console.WriteLine("Examples:");
            Console.WriteLine("  OmniConvertCS.exe -a  CRYPT_RAW  input.txt");
            Console.WriteLine("  OmniConvertCS.exe -af CRYPT_GRAW input_folder");
            Console.WriteLine("  OmniConvertCS.exe -af -o D:\\Out CRYPT_CRAW input_folder");
            Console.WriteLine();
        }
    }
}