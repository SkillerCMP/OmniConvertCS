using System;
using System.Text.RegularExpressions;

namespace OmniconvertCS
{
    internal static class InlineCryptParser
    {
        // Matches ", CRYPT_ARMAX", ", CRYPT_AR2", etc. anywhere in the label.
        private static readonly Regex CryptTagRegex = new Regex(
            @",\s*CRYPT_(?<crypt>[A-Za-z0-9_]+)",
            RegexOptions.IgnoreCase | RegexOptions.Compiled);

        /// <summary>
        /// Try to parse a CRYPT_XXXX tag out of a cheat label.
        /// Returns true if a known crypt was found.
        /// </summary>
        public static bool TryGetCryptFromLabel(string? label, out ConvertCore.Crypt crypt)
        {
            crypt = ConvertCore.Crypt.CRYPT_RAW;

            if (string.IsNullOrWhiteSpace(label))
                return false;

            var m = CryptTagRegex.Match(label);
            if (!m.Success)
                return false;

            string token = m.Groups["crypt"].Value.Trim().ToUpperInvariant();

            switch (token)
            {
                case "AR1":
                    crypt = ConvertCore.Crypt.CRYPT_AR1;
                    return true;
                case "AR2":
                    crypt = ConvertCore.Crypt.CRYPT_AR2;
                    return true;
                case "ARMAX":
                    crypt = ConvertCore.Crypt.CRYPT_ARMAX;
                    return true;
                case "CB":
                    crypt = ConvertCore.Crypt.CRYPT_CB;
                    return true;
                case "CB7_COMMON":
                    crypt = ConvertCore.Crypt.CRYPT_CB7_COMMON;
                    return true;
                case "GS3":
                    crypt = ConvertCore.Crypt.CRYPT_GS3;
                    return true;
                case "GS5":
                    crypt = ConvertCore.Crypt.CRYPT_GS5;
                    return true;
                case "MAXRAW":
                    crypt = ConvertCore.Crypt.CRYPT_MAXRAW;
                    return true;
                case "RAW":
                    crypt = ConvertCore.Crypt.CRYPT_RAW;
                    return true;
				case "ARAW":
					crypt = ConvertCore.Crypt.CRYPT_ARAW;
					return true;
				case "CRAW":
					crypt = ConvertCore.Crypt.CRYPT_CRAW;
					return true;
				case "GRAW":
					crypt = ConvertCore.Crypt.CRYPT_GRAW;
					return true;
                default:
                    // Unknown tag: treat as RAW but report false so callers can fallback.
                    crypt = ConvertCore.Crypt.CRYPT_RAW;
                    return false;
            }
        }

        /// <summary>
        /// Map an input crypt to the logical "device" used by ConvertCore.
        /// </summary>
        public static ConvertCore.Device GetDeviceForCrypt(ConvertCore.Crypt crypt)
        {
            switch (crypt)
            {
                case ConvertCore.Crypt.CRYPT_AR1:
                    return ConvertCore.Device.DEV_AR1;

                case ConvertCore.Crypt.CRYPT_AR2:
				case ConvertCore.Crypt.CRYPT_ARAW:
                    return ConvertCore.Device.DEV_AR2;

                case ConvertCore.Crypt.CRYPT_ARMAX:
                case ConvertCore.Crypt.CRYPT_MAXRAW:
                    return ConvertCore.Device.DEV_ARMAX;

                case ConvertCore.Crypt.CRYPT_CB:
                case ConvertCore.Crypt.CRYPT_CB7_COMMON:
				case ConvertCore.Crypt.CRYPT_CRAW:
                    return ConvertCore.Device.DEV_CB;

                case ConvertCore.Crypt.CRYPT_GS3:
                case ConvertCore.Crypt.CRYPT_GS5:
				case ConvertCore.Crypt.CRYPT_GRAW:
                    return ConvertCore.Device.DEV_GS3;

                case ConvertCore.Crypt.CRYPT_RAW:
                default:
                    return ConvertCore.Device.DEV_STD;
            }
        }
    /// <summary>
        /// Remove ", CRYPT_XXXX" from the label, if present.
        /// Used for PNACH output when INLINE input is active.
        /// </summary>
        public static string StripCryptTag(string? label)
        {
            if (string.IsNullOrWhiteSpace(label))
                return label ?? string.Empty;

            var m = CryptTagRegex.Match(label);
            if (!m.Success)
                return label;

            string result = label.Remove(m.Index, m.Length);
            return result.TrimEnd();
        }

        /// <summary>
        /// Update or add a CRYPT_XXXX tag to match the given output crypt.
        /// Used for non-PNACH output when INLINE input is active.
        /// </summary>
        public static string UpdateCryptTagForOutput(string? label, ConvertCore.Crypt outCrypt)
{
    if (string.IsNullOrWhiteSpace(label))
        return label ?? string.Empty;

    // Work on a trimmed copy so we don't keep adding trailing spaces
    string trimmed = label.TrimEnd();

    string token = GetCryptTokenForOutput(outCrypt);
    // If we don't have a meaningful token, just strip any existing tag.
    if (string.IsNullOrEmpty(token))
        return StripCryptTag(trimmed);

    var m = CryptTagRegex.Match(trimmed);
    if (!m.Success)
    {
        // No existing CRYPT_ tag in the original label; DO NOT add a new one.
        // Only lines that already had ", CRYPT_XXXX" should be touched.
        return trimmed;
    }

    string replacement = ", " + token;

    // Replace the first existing CRYPT_ tag.
    string result = CryptTagRegex.Replace(trimmed, replacement, 1);
    return result.TrimEnd();
}

        /// <summary>
        /// Map an output crypt to its CRYPT_XXXX token string.
        /// </summary>
        private static string GetCryptTokenForOutput(ConvertCore.Crypt crypt)
        {
            switch (crypt)
            {
                case ConvertCore.Crypt.CRYPT_AR1:         return "CRYPT_AR1";
                case ConvertCore.Crypt.CRYPT_AR2:         return "CRYPT_AR2";
                case ConvertCore.Crypt.CRYPT_ARMAX:       return "CRYPT_ARMAX";
                case ConvertCore.Crypt.CRYPT_CB:          return "CRYPT_CB";
                case ConvertCore.Crypt.CRYPT_CB7_COMMON:  return "CRYPT_CB7_COMMON";
                case ConvertCore.Crypt.CRYPT_GS3:         return "CRYPT_GS3";
                case ConvertCore.Crypt.CRYPT_GS5:         return "CRYPT_GS5";
                case ConvertCore.Crypt.CRYPT_MAXRAW:      return "CRYPT_MAXRAW";
                case ConvertCore.Crypt.CRYPT_RAW:         return "CRYPT_RAW";
				case ConvertCore.Crypt.CRYPT_ARAW:        return "CRYPT_ARAW";
				case ConvertCore.Crypt.CRYPT_CRAW:        return "CRYPT_CRAW";
				case ConvertCore.Crypt.CRYPT_GRAW:        return "CRYPT_GRAW";
				
                default:
                    return string.Empty;
            }
        }
    }
}
