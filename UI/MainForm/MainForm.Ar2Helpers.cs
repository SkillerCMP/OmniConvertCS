using System;
using System.Globalization;
using System.Text;
using System.Windows.Forms;
using OmniconvertCS;
using System.IO;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace OmniconvertCS.Gui
{
    public partial class MainForm : Form
    {
        private void RefreshAr2KeyDisplayFromSeed()
                {
                    if (txtAr2CurrentKey == null || lblAr2CurrentKey == null)
                        return;

                    bool show = (ConvertCore.g_incrypt == ConvertCore.Crypt.CRYPT_AR2);

                    lblAr2CurrentKey.Visible = show;
                    txtAr2CurrentKey.Visible = show;

                    if (!show)
                    {
                        txtAr2CurrentKey.Text = string.Empty;
                        return;
                    }

                    // Mirror Ar2KeyForm.InitializeFromCurrentSeed():
                    //   key = ar2encrypt(ar2seeds, tseed[1], tseed[0]);
                    uint ar1seed = Ar2.AR1_SEED;
                    byte[] tseed = BitConverter.GetBytes(ar1seed); // little-endian

                    uint key = Ar2.Ar2Encrypt(ConvertCore.ar2seeds, tseed[1], tseed[0]);
                    txtAr2CurrentKey.Text = key.ToString("X8");
                }

        private bool TryApplyAr2KeyFromCheatHeader(cheat_t cheat)
        {
            if (cheat == null)
                return false;

            // Only relevant when input crypt is AR2 (Input: AR-V2).
            if (ConvertCore.g_incrypt != ConvertCore.Crypt.CRYPT_AR2)
                return false;

            // Need at least one address/value pair.
            if (cheat.codecnt < 2)
                return false;

            const uint AR2_HEADER_ADDR = 0x0E3C7DF2u;

            // AR2 master codes put the key in the very first address/value pair.
            if (cheat.code[0] != AR2_HEADER_ADDR)
                return false;

            // Encrypted AR2 key code (eight hex digits).
            uint key = cheat.code[1];

            // Mirror Ar2KeyForm.btnOk_Click:
            //   ar2seeds = ar2decrypt(key, tseed[1], tseed[0]);
            uint   ar1seed = Ar2.AR1_SEED;
            byte[] tseed   = BitConverter.GetBytes(ar1seed); // little-endian
            ConvertCore.ar2seeds = Ar2.Ar2Decrypt(key, tseed[1], tseed[0]);

            // Remove this header pair from the cheat so ConvertCode never
            // sees it as a normal "code". cheatRemoveOctets works in 32-bit
            // words ("octets"), so 2 = [addr,val].
            Cheat.cheatRemoveOctets(cheat, 1, 2);

            // Keep the small "current AR2 key" box in sync.
            RefreshAr2KeyDisplayFromSeed();

            return true;
        }

    }
}
