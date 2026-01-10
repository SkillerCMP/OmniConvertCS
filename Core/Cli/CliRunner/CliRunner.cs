using System;

namespace OmniconvertCS.Cli
{
    internal static partial class CliRunner
    {
        public static int Run(string[] args)
        {
            if (!CliOptions.TryParse(args, out var opt, out var err))
            {
                if (err == "help")
                {
                    PrintUsage();
                    return 0;
                }

                Console.Error.WriteLine(err);
                PrintUsage();
                return 1;
            }

            return Execute(opt);
        }
    }
}
