using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal sealed class AutoFixResult
    {
        public bool Fixed { get; init; }
        public bool Deleted { get; init; }
        public ConvertCore.Crypt UsedInputCrypt { get; init; }
        public cheat_t CheatConverted { get; init; } = null!;
        public string Note { get; init; } = string.Empty;
        public RawPlausibilityResult? Plausibility { get; init; }
    }
}
