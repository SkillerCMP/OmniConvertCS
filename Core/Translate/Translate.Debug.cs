using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
    internal static partial class Translate
    {
        
        [Conditional("DEBUG")]
        private static void Trace(string message)
            => Debug.WriteLine(message);
		[Conditional("DEBUG")]
		private static void Trace(string format, params object[] args)
		=> Debug.WriteLine(string.Format(format, args));
    }
}

