using System;

namespace OmniconvertCS.Cli
{
    /// <summary>
    /// Small shared state for CLI UI. Set by CliRunner per file so manual prompt screens
    /// can display which file is currently being processed.
    /// </summary>
    internal static class CliScreenContext
    {
        public static string? CurrentFile { get; set; }
    }
}
