using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace OmniconvertCS
{
    /// <summary>
    /// Helper for computing and storing PNACH CRC mappings.
    /// Supports multiple entries that share the same CRC but have different ELF names.
    /// PnachCRC.json lines:
    ///   CRC>GameName
    ///   ELFName>CRC>GameName
    /// </summary>
    public static class PnachCrcHelper
    {
        private const string MappingFileName = "PnachCRC.json";

        public sealed class CrcEntry
        {
            public uint   Crc      { get; set; }
            public string GameName { get; set; } = string.Empty;
            public string ElfName  { get; set; } = string.Empty;  // e.g. SLES_526.41

            public string CrcHex => Crc.ToString("X8");

            public override string ToString()
            {
                if (!string.IsNullOrEmpty(ElfName))
                    return $"{CrcHex} - {GameName} ({ElfName})";
                return $"{CrcHex} - {GameName}";
            }
        }

        /// <summary>
        /// Computes the PCSX2/PNACH "Game CRC" from a PS2 ELF file.
        /// </summary>
        public static uint ComputeElfCrc(string elfPath)
        {
            if (elfPath == null) throw new ArgumentNullException(nameof(elfPath));
            byte[] data = File.ReadAllBytes(elfPath);

            uint crc = 0;
            int words = data.Length / 4;

            for (int i = 0; i < words; i++)
            {
                uint word = BitConverter.ToUInt32(data, i * 4);
                crc ^= word;
            }

            return crc;
        }

        private static string GetMappingPath()
        {
            string baseDir = AppContext.BaseDirectory;
            return Path.Combine(baseDir, MappingFileName);
        }

        /// <summary>
        /// Load all CRC entries from PnachCRC.json.
        /// Allows multiple entries with the same CRC (different ELF names).
        /// </summary>
        private static List<CrcEntry> LoadEntries()
        {
            var list = new List<CrcEntry>();
            string path = GetMappingPath();

            if (!File.Exists(path))
                return list;

            foreach (string raw in File.ReadAllLines(path))
            {
                string line = raw.Trim();
                if (line.Length == 0) continue;
                if (line.StartsWith("#") || line.StartsWith("//"))
                    continue;

                string[] parts = line.Split('>');
                if (parts.Length == 3)
                {
                    // ELFName>CRC>GameName
                    string elfName = parts[0].Trim();
                    string crcHex  = parts[1].Trim();
                    string name    = parts[2].Trim();

                    if (uint.TryParse(crcHex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint crc))
                    {
                        list.Add(new CrcEntry
                        {
                            Crc      = crc,
                            GameName = name,
                            ElfName  = elfName
                        });
                    }
                }
                else if (parts.Length == 2)
                {
                    // Old format: CRC>GameName (no ELF stored)
                    string crcHex = parts[0].Trim();
                    string name   = parts[1].Trim();

                    if (uint.TryParse(crcHex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint crc))
                    {
                        list.Add(new CrcEntry
                        {
                            Crc      = crc,
                            GameName = name,
                            ElfName  = string.Empty
                        });
                    }
                }
                // anything else is ignored
            }

            return list;
        }

        /// <summary>
        /// Save all entries back to PnachCRC.json, sorted by GameName, then CRC, then ELF name.
        /// Entries with ElfName are written as ELFName>CRC>GameName,
        /// entries without ElfName are written as CRC>GameName (backward compat).
        /// </summary>
        private static void SaveEntries(List<CrcEntry> entries)
        {
            string path = GetMappingPath();

            var ordered = entries
                .OrderBy(e => e.GameName, StringComparer.OrdinalIgnoreCase)
                .ThenBy(e => e.Crc)
                .ThenBy(e => e.ElfName, StringComparer.OrdinalIgnoreCase);

            var lines = ordered.Select(e =>
                string.IsNullOrEmpty(e.ElfName)
                    ? $"{e.CrcHex}>{e.GameName}"
                    : $"{e.ElfName}>{e.CrcHex}>{e.GameName}");

            File.WriteAllLines(path, lines);
        }

        /// <summary>
        /// Returns a sorted list of CrcEntry for populating the dropdown.
        /// </summary>
        public static List<CrcEntry> GetEntries()
        {
            var entries = LoadEntries();

            return entries
                .OrderBy(e => e.GameName, StringComparer.OrdinalIgnoreCase)
                .ThenBy(e => e.Crc)
                .ThenBy(e => e.ElfName, StringComparer.OrdinalIgnoreCase)
                .ToList();
        }

        /// <summary>
        /// Returns GameName for a given CRC (if known), or null.
        /// If multiple entries share the same CRC, returns the first one.
        /// </summary>
        public static string? TryGetGameName(uint crc)
        {
            var entries = LoadEntries();
            var match = entries.FirstOrDefault(e => e.Crc == crc);
            return match?.GameName;
        }

        /// <summary>
        /// Returns ELF name for a given CRC (if known), or null.
        /// If multiple entries share the same CRC, returns the first one that has an ELF name.
        /// </summary>
        public static string? TryGetElfName(uint crc)
        {
            var entries = LoadEntries();
            var matchWithElf = entries.FirstOrDefault(e => e.Crc == crc && !string.IsNullOrEmpty(e.ElfName));
            if (matchWithElf != null)
                return matchWithElf.ElfName;

            var anyMatch = entries.FirstOrDefault(e => e.Crc == crc);
            return anyMatch?.ElfName;
        }

        /// <summary>
        /// Adds or updates an ELF/CRC/GameName entry.
        /// If an entry with the same CRC and ELF name already exists (case-insensitive ELF match),
        /// its GameName is updated; otherwise a new entry is appended.
        /// </summary>
        public static void AddOrUpdate(uint crc, string gameName, string elfName)
        {
            if (string.IsNullOrWhiteSpace(gameName))
                throw new ArgumentException("Game name cannot be empty.", nameof(gameName));

            var entries = LoadEntries();

            string cleanName = gameName.Trim();
            string cleanElf  = (elfName ?? string.Empty).Trim();

            if (!string.IsNullOrEmpty(cleanElf))
            {
                try
                {
                    cleanElf = Path.GetFileName(cleanElf);
                }
                catch
                {
                    // ignore, keep whatever came in
                }
            }

            // Look for an existing entry with same CRC + same ELF name (case-insensitive)
            var existing = entries.FirstOrDefault(e =>
                e.Crc == crc &&
                string.Equals(e.ElfName ?? string.Empty, cleanElf, StringComparison.OrdinalIgnoreCase));

            if (existing != null)
            {
                existing.GameName = cleanName;
            }
            else
            {
                entries.Add(new CrcEntry
                {
                    Crc      = crc,
                    GameName = cleanName,
                    ElfName  = cleanElf
                });
            }

            SaveEntries(entries);
        }
    }
}
