using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace OmniconvertCS.Cli
{
    internal static partial class CliRunner
    {
        private static FileProcessResult ProcessOneFile(CliOptions opt, bool analyzeRawFamily, string inputRoot, string inPath)
        {
            // Parse input
            List<CheatBlock> blocks = TextCheatParser.ParseFile(inPath);

            var converted = new List<(CheatBlock Block, cheat_t CheatConverted)>();
            var report = new StringBuilder();

            int failCount = 0;
            int fixedCount = 0;

            foreach (var b in blocks)
            {
                // Group-only blocks: keep as-is
                if (b.CheatInput.codecnt == 0)
                {
                    converted.Add((b, b.CheatInput));
                    continue;
                }

                if (opt.Mode == AnalyzeMode.AnalyzeFix)
                {
                    var res = AutoFixCryptSelector.ConvertWithAutoFix(b, opt.OutputCrypt, opt.Mode, analyzeRawFamily, opt.Manual, opt.ManualAll);

                    if (res.Deleted)
                    {
                        report.AppendLine($"DELETED: {ShortLabel(b.Label)}");
                        continue;
                    }

                    converted.Add((b, res.CheatConverted));

                    if (analyzeRawFamily && res.Plausibility != null && !res.Plausibility.IsValid)
                    {
                        failCount++;
                        var note = (!string.IsNullOrWhiteSpace(res.Plausibility.Note) ? $"  note={res.Plausibility.Note}" : "");
                        report.AppendLine($"FAIL: {ShortLabel(b.Label)}  used={res.UsedInputCrypt}  addr28=0x{res.Plausibility.CheckedAddr28:X7}  rule={res.Plausibility.Rule}{note}");
                    }
                    else if (res.Fixed)
                    {
                        fixedCount++;
                        report.AppendLine($"FIXED: {ShortLabel(b.Label)}  declared→used: {GetDeclaredCrypt(b)} → {res.UsedInputCrypt}");
                    }
                }
                else
                {
                    // Analyze or normal convert using declared crypt
                    ConvertCore.Crypt inCrypt = GetDeclaredCrypt(b);
                    cheat_t outCheat = CloneCheat(b.CheatInput);

                    ConvertCore.g_incrypt = inCrypt;
                    ConvertCore.g_indevice = InlineCryptParser.GetDeviceForCrypt(inCrypt);
                    ConvertCore.g_outcrypt = opt.OutputCrypt;
                    ConvertCore.g_outdevice = InlineCryptParser.GetDeviceForCrypt(opt.OutputCrypt);

                    int ret = ConvertCore.ConvertCode(outCheat);

                    converted.Add((b, outCheat));

                    if (ret != 0)
                    {
                        failCount++;
                        report.AppendLine($"FAIL: {ShortLabel(b.Label)}  convert-error={ret}  in={inCrypt}");
                        continue;
                    }

                    if (analyzeRawFamily && opt.Mode == AnalyzeMode.Analyze)
                    {
                        var plaus = RawPlausibility.ValidateFirstCheckableAddress(outCheat, opt.OutputCrypt, opt.Mode);
                        if (!plaus.IsValid)
                        {
                            failCount++;
                            report.AppendLine($"FAIL: {ShortLabel(b.Label)}  in={inCrypt}  addr28=0x{plaus.CheckedAddr28:X7}  rule={plaus.Rule}");
                        }
                    }
                }
            }

            // Compute output paths (preserve relative layout when input is a folder)
            string relPath = opt.InputIsDirectory
                ? Path.GetRelativePath(inputRoot, Path.GetFullPath(inPath))
                : Path.GetFileName(inPath);

            string outPath = Path.Combine(opt.OutputDir, relPath);
            Directory.CreateDirectory(Path.GetDirectoryName(outPath) ?? opt.OutputDir);

            // Write output file
            string outText = TextCheatWriter.WriteToText(converted, opt.OutputCrypt);
            if (opt.FixE001ToD && opt.OutputCrypt == ConvertCore.Crypt.CRYPT_CRAW)
            {
                outText = E001ToDFixer.RewriteE001ToD(outText);
            }
            File.WriteAllText(outPath, outText, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

            // Write report (into OutputCrypt\Analysis\...)
            if (opt.Mode != AnalyzeMode.None)
            {
                WriteAnalysisReport(opt, analyzeRawFamily, inPath, outPath, relPath, failCount, fixedCount, report.ToString());
            }

            Console.WriteLine($"{relPath} -> {opt.OutputFolderName}  (fail={failCount}, fixed={fixedCount})");
            return new FileProcessResult(relPath, failCount, fixedCount);
        }

        private static void WriteAnalysisReport(CliOptions opt,
                                                bool analyzeRawFamily,
                                                string inPath,
                                                string outPath,
                                                string relPath,
                                                int failCount,
                                                int fixedCount,
                                                string reportBody)
        {
            string analysisRel = MakeAnalysisRelPath(relPath);
            string rptPath = Path.Combine(opt.AnalysisDir, analysisRel);
            Directory.CreateDirectory(Path.GetDirectoryName(rptPath) ?? opt.AnalysisDir);

            var header = new StringBuilder();
            header.AppendLine($"Input: {inPath}");
            header.AppendLine($"Output: {outPath}");
            header.AppendLine($"OutputCrypt: {opt.OutputCrypt}");
            header.AppendLine($"Mode: {opt.Mode}");
            header.AppendLine($"AnalyzeRawFamily: {analyzeRawFamily}");
            header.AppendLine($"Failures: {failCount}");
            header.AppendLine($"Fixed: {fixedCount}");
            header.AppendLine();
            header.Append(reportBody);

            File.WriteAllText(rptPath, header.ToString(), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        }

        private static void HandleFileException(CliOptions opt, string inputRoot, string inPath, Exception ex, ref BatchStats stats)
        {
            // Continue processing other files.
            string relPath = opt.InputIsDirectory
                ? Path.GetRelativePath(inputRoot, Path.GetFullPath(inPath))
                : Path.GetFileName(inPath);

            stats.FailFiles++;
            Console.Error.WriteLine($"ERROR: {relPath}  {ex.GetType().Name}: {ex.Message}");

            if (opt.Mode != AnalyzeMode.None)
            {
                string analysisRel = MakeAnalysisRelPath(relPath);
                string rptPath = Path.Combine(opt.AnalysisDir, analysisRel);
                Directory.CreateDirectory(Path.GetDirectoryName(rptPath) ?? opt.AnalysisDir);

                var header = new StringBuilder();
                header.AppendLine($"Input: {inPath}");
                header.AppendLine($"OutputCrypt: {opt.OutputCrypt}");
                header.AppendLine($"Mode: {opt.Mode}");
                header.AppendLine();
                header.AppendLine("EXCEPTION:");
                header.AppendLine(ex.ToString());

                File.WriteAllText(rptPath, header.ToString(), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
            }
        }
    }
}
