using System;
using System.IO;
using System.Text;

namespace OmniconvertCS
{
    /// <summary>
    /// Simple helper for saving PNACH text to disk.
    /// For now it just writes the provided text as UTF-8.
    /// </summary>
    public static class PnachWriter
    {
        public static void SaveFromText(string path, string? pnachText)
        {
            if (path == null) throw new ArgumentNullException(nameof(path));

            string text = pnachText ?? string.Empty;
            File.WriteAllText(path, text, Encoding.UTF8);
        }
    }
}
