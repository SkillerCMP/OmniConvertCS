using System;
using System.Collections.Generic;
using System.IO;

namespace OmniconvertCS.Cli
{
    internal static partial class CliRunner
    {
        private struct BatchStats
        {
            public int FailFiles;
            public int FailTotal;
            public int FixedTotal;
        }

        private readonly struct FileProcessResult
        {
            public FileProcessResult(string relPath, int failCount, int fixedCount)
            {
                RelPath = relPath;
                FailCount = failCount;
                FixedCount = fixedCount;
            }

            public string RelPath { get; }
            public int FailCount { get; }
            public int FixedCount { get; }
        }

        private static int Execute(CliOptions opt)
        {
            bool analyzeRawFamily = (opt.Mode != AnalyzeMode.None) &&
                                    (opt.OutputCrypt == ConvertCore.Crypt.CRYPT_RAW ||
                                     opt.OutputCrypt == ConvertCore.Crypt.CRYPT_CRAW ||
                                     opt.OutputCrypt == ConvertCore.Crypt.CRYPT_GRAW);

            // Ensure output directories
            Directory.CreateDirectory(opt.OutputDir);
            if (opt.Mode != AnalyzeMode.None)
                Directory.CreateDirectory(opt.AnalysisDir);

            // Gather input files
            var inputFiles = GatherInputFiles(opt, out string inputRoot);

            if (inputFiles.Count == 0)
            {
                Console.Error.WriteLine("No .txt files found to process.");
                return 1;
            }

            var stats = new BatchStats();

            foreach (var inPath in inputFiles)
            {
                // Wrap each input file so one bad file can't stop a whole folder run.
                try
                {
                    FileProcessResult r = ProcessOneFile(opt, analyzeRawFamily, inputRoot, inPath);

                    if (analyzeRawFamily && r.FailCount > 0)
                    {
                        stats.FailFiles++;
                        stats.FailTotal += r.FailCount;
                    }

                    stats.FixedTotal += r.FixedCount;
                }
                catch (OperationCanceledException)
                {
                    Console.Error.WriteLine("Manual mode: quit requested.");
                    return 0;
                }
                catch (Exception ex)
                {
                    HandleFileException(opt, inputRoot, inPath, ex, ref stats);
                }
            }

            return ComputeExitCode(opt.Mode, analyzeRawFamily, stats.FailFiles);
        }

        private static int ComputeExitCode(AnalyzeMode mode, bool analyzeRawFamily, int failFiles)
        {
            // Exit codes (unchanged)
            if (mode == AnalyzeMode.AnalyzeFix)
            {
                // If analyze was active and we still have fails, return 3, else 0.
                return (analyzeRawFamily && failFiles > 0) ? 3 : 0;
            }

            if (mode == AnalyzeMode.Analyze)
            {
                return (analyzeRawFamily && failFiles > 0) ? 2 : 0;
            }

            return 0;
        }
    }
}
