using System.Collections.Generic;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
        private static List<ConvertCore.Crypt> BuildCandidateList(ConvertCore.Crypt declaredCrypt)
        {
            var candidates = new List<ConvertCore.Crypt>(s_manualMenu.Length + 1)
            {
                declaredCrypt
            };

            for (int i = 0; i < s_manualMenu.Length; i++)
            {
                var c = s_manualMenu[i];
                if (c == declaredCrypt) continue;
                candidates.Add(c);
            }

            // Helpful neighbor ordering (common mis-declarations)
            if (declaredCrypt == ConvertCore.Crypt.CRYPT_CB7_COMMON)
                MoveUp(candidates, ConvertCore.Crypt.CRYPT_CB, 1);
            else if (declaredCrypt == ConvertCore.Crypt.CRYPT_CB)
                MoveUp(candidates, ConvertCore.Crypt.CRYPT_CB7_COMMON, 1);

            if (declaredCrypt == ConvertCore.Crypt.CRYPT_AR1)
                MoveUp(candidates, ConvertCore.Crypt.CRYPT_AR2, 1);
            else if (declaredCrypt == ConvertCore.Crypt.CRYPT_AR2)
                MoveUp(candidates, ConvertCore.Crypt.CRYPT_AR1, 1);

            return candidates;
        }

        private static void MoveUp(List<ConvertCore.Crypt> list, ConvertCore.Crypt c, int targetIndex)
        {
            int idx = list.IndexOf(c);
            if (idx <= targetIndex) return;
            list.RemoveAt(idx);
            list.Insert(targetIndex, c);
        }
    }
}
