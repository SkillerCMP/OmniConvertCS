using System;
using System.Text;

namespace OmniconvertCS
{
    /// <summary>
    /// Partial port of common.c. Only what is needed for the AR2 module has been included so far.
    /// </summary>
    public static class Common
    {
        public const string APPNAME = "Omniconvert";
        public const string WINDOWTITLE = APPNAME + " (C# Port)";
        public const string NEWLINE = "\r\n";

        public static bool IsHexStr(string s)
        {
            if (s is null) return false;
            foreach (char c in s)
            {
                if (!Uri.IsHexDigit(c))
                    return false;
            }

            return true;
        }

        public static bool IsNumStr(string s)
        {
            if (s is null) return false;
            foreach (char c in s)
            {
                if (!char.IsDigit(c))
                    return false;
            }

            return true;
        }

        public static bool IsEmptyStr(string s)
        {
            if (s is null) return true;
            foreach (char c in s)
            {
                if (!char.IsWhiteSpace(c))
                    return false;
            }

            return true;
        }

        /// <summary>
        /// Swap the byte order of a 32‑bit unsigned integer (ABCD &lt;=&gt; DCBA).
        /// </summary>
        public static uint swapbytes(uint val)
        {
            return (val << 24)
                 | ((val << 8) & 0xFF0000u)
                 | ((val >> 8) & 0x00FF00u)
                 | (val >> 24);
        }

        /// <summary>
        /// Very lightweight stand‑in for the original MsgBox(), which wrapped
        /// the Win32 MessageBox API. Here we just write to stderr.
        /// </summary>
        public static int MsgBox(IntPtr hwnd, uint uType, string format, params object[] args)
        {
            string msg = (args is { Length: > 0 })
                ? string.Format(format, args)
                : format;
            Console.Error.WriteLine("[{0}] {1}", APPNAME, msg);
            return 0;
        }

        /// <summary>
        /// Simplified AppendText – we do not track buffer sizes like the C code,
        /// since System.String grows as needed.
        /// </summary>
        public static int AppendText(ref string dest, string src, ref uint textmax)
        {
            if (dest is null) dest = string.Empty;
            if (src is null) src = string.Empty;

            dest += src;
            if (dest.Length + 1 >= textmax)
            {
                textmax = (uint)(dest.Length + 16);
            }

            return 0;
        }

        /// <summary>
        /// Simplified PrependText implementation.
        /// </summary>
        public static int PrependText(ref string dest, string src, ref uint textmax)
        {
            if (dest is null) dest = string.Empty;
            if (src is null) src = string.Empty;

            dest = src + dest;
            if (dest.Length + 1 >= textmax)
            {
                textmax = (uint)(dest.Length + 16);
            }

            return 0;
        }

        public static int AppendNewLine(ref string dest, int num, ref uint textmax)
        {
            int ret = 0;
            for (int i = 0; i < num; i++)
            {
                ret = AppendText(ref dest, NEWLINE, ref textmax);
                if (ret != 0)
                    break;
            }

            return ret;
        }
    }
}
