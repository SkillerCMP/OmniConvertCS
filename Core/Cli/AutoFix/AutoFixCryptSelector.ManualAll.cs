using System;
using OmniconvertCS;

namespace OmniconvertCS.Cli
{
    internal static partial class AutoFixCryptSelector
    {
        private static AutoFixResult? InitManualAllPick(CheatBlock block,
                                                       ConvertCore.Crypt outCrypt,
                                                       ConvertCore.Crypt declaredCrypt,
                                                       out ConvertCore.Crypt picked,
                                                       out bool forceAccept,
                                                       out uint? ar2Key)
        {
            var pick = PromptManualPickInputCrypt(block, outCrypt, declaredCrypt);

            if (pick == ManualAction.Quit)
                throw new OperationCanceledException("Manual quit");

            if (pick == ManualAction.DeleteCheat)
            {
                picked = declaredCrypt;
                forceAccept = false;
                ar2Key = null;
                return new AutoFixResult
                {
                    Deleted = true,
                    Fixed = false,
                    CheatConverted = CloneCheat(block.CheatInput),
                    Note = "Deleted by user"
                };
            }

            picked = pick.OverrideCrypt ?? declaredCrypt;
            forceAccept = pick.ForceAccept;
            ar2Key = pick.Ar2Key;
            return null;
        }
    }
}
