using System;
using System.Collections.Generic;
using System.IO;

namespace OmniconvertCS.Cli
{
    internal static partial class CliRunner
    {
        private static List<string> GatherInputFiles(CliOptions opt, out string inputRoot)
        {
            var inputFiles = new List<string>();

            inputRoot = opt.InputIsDirectory
                ? Path.GetFullPath(opt.InputPath)
                : Path.GetFullPath(Path.GetDirectoryName(opt.InputPath) ?? Environment.CurrentDirectory);

            if (opt.InputIsDirectory)
            {
                string outDirFull = Path.GetFullPath(opt.OutputDir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
                string analysisDirFull = Path.GetFullPath(opt.AnalysisDir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);

                foreach (var f in Directory.EnumerateFiles(opt.InputPath, "*", SearchOption.AllDirectories))
                {
                    if (!f.EndsWith(".txt", StringComparison.OrdinalIgnoreCase))
                        continue;

                    string ff = Path.GetFullPath(f);

                    // Avoid accidentally re-processing previous outputs if input folder contains them.
                    if (ff.StartsWith(outDirFull + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) ||
                        ff.StartsWith(analysisDirFull + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                        continue;

                    inputFiles.Add(ff);
                }
            }
            else
            {
                inputFiles.Add(Path.GetFullPath(opt.InputPath));
            }

            return inputFiles;
        }
    }
}
