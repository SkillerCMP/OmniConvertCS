using System;

namespace OmniconvertCS.Cli
{
internal sealed class RawPlausibilityResult
    {
        public bool IsValid { get; init; }
        public uint CheckedWord { get; init; }
        public uint CheckedAddr28 { get; init; }
        public string Rule { get; init; } = string.Empty;
        public string? FailReason { get; init; }
        public string? Note { get; init; }
    }
}
