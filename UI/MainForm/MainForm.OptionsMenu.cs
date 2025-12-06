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
        private void menuOptionsArmaxOptions_Click(object? sender, EventArgs e)
        {
            // Prefer the UI textbox if it parses, otherwise fall back to ConvertCore.g_gameid
            uint gameId = ConvertCore.g_gameid;
            if (uint.TryParse(txtGameId.Text.Trim(),
                              NumberStyles.HexNumber,
                              CultureInfo.InvariantCulture,
                              out uint parsed) &&
                parsed <= 0xFFFFu)
            {
                gameId = parsed;
            }

            uint region = ConvertCore.g_region;
            var verifierMode = ConvertCore.g_verifiermode;
            char hashDrive = ConvertCore.g_hashdrive;

            using (var dlg = new Gui.ArmaxOptionsForm(gameId, region, verifierMode, hashDrive))
            {
                if (dlg.ShowDialog(this) == DialogResult.OK)
                {
                    ConvertCore.g_gameid       = dlg.SelectedGameId;
                    ConvertCore.g_region       = dlg.SelectedRegion;
                    ConvertCore.g_verifiermode = dlg.SelectedVerifierMode;
                    ConvertCore.g_hashdrive    = dlg.SelectedHashDrive;

                    // Update main Game ID box
                    SyncGameIdFromGlobals();

                    // Make sure the old menu items stay in sync
                    UpdateVerifierMenuChecks();
                    UpdateRegionMenuChecks();
                    SetArmaxHashDrive(ConvertCore.g_hashdrive);

                    SaveAppSettings();
                }
            }
        }

        private void EnumerateArmaxDiscHashDrives()
        {
            if (menuOptionsArmaxDiscHash == null)
                return;

            // Keep the first item ("Do Not Create"), remove any previously-added drives
            while (menuOptionsArmaxDiscHash.DropDownItems.Count > 1)
            {
                menuOptionsArmaxDiscHash.DropDownItems.RemoveAt(1);
            }

            try
            {
                foreach (var drive in DriveInfo.GetDrives())
                {
                    if (drive.DriveType != DriveType.CDRom)
                        continue;

                    string root = drive.Name;     // e.g. "D:\\"
                    if (string.IsNullOrEmpty(root))
                        continue;

                    char letter = root[0];

                    var mi = new ToolStripMenuItem
                    {
                        Text    = $"Use Drive {root}",
                        Tag     = letter,
                        Checked = false
                    };
                    mi.Click += menuOptionsArmaxDiscHashDrive_Click;

                    menuOptionsArmaxDiscHash.DropDownItems.Add(mi);
                }
            }
            catch
            {
                // If drive enumeration fails, just leave only "Do Not Create" in the menu.
            }
        }

        private void SetArmaxHashDrive(char driveLetter)
        {
            ConvertCore.g_hashdrive = driveLetter;

            if (menuOptionsArmaxDiscHash == null)
                return;

            foreach (ToolStripItem item in menuOptionsArmaxDiscHash.DropDownItems)
            {
                if (item is ToolStripMenuItem mi)
                {
                    char itemDrive = '\0';
                    if (mi.Tag is char c)
                        itemDrive = c;

                    mi.Checked = (itemDrive == driveLetter);
                }
            }
        }

        private void menuOptionsArmaxDiscHashNone_Click(object sender, EventArgs e)
        {
            // "Do Not Create" selected
            SetArmaxHashDrive('\0');
        }

        private void menuOptionsArmaxDiscHashDrive_Click(object sender, EventArgs e)
        {
            if (sender is ToolStripMenuItem mi && mi.Tag is char letter)
            {
                SetArmaxHashDrive(letter);
            }
        }

        private void menuOptionsAr2Key_Click(object? sender, EventArgs e)
                {
                    using (var dlg = new Ar2KeyForm())
                    {
                        if (dlg.ShowDialog(this) == DialogResult.OK)
                        {
                            // Underlying ConvertCore.ar2seeds was updated by the dialog.
                            RefreshAr2KeyDisplayFromSeed();
                        }
                    }
                }

        private void menuOptionsPnachCrc_Click(object? sender, EventArgs e)
        {
            // Use last-known values as defaults, otherwise use current GameName box
            string initialCrc   = _pnachCrc.HasValue ? _pnachCrc.Value.ToString("X8") : string.Empty;
            string initialName  = !string.IsNullOrWhiteSpace(_pnachGameName)
                ? _pnachGameName
                : txtGameName.Text.Trim();

            using (var dlg = new PnachCrcForm(initialCrc, initialName))
            {
                if (dlg.ShowDialog(this) == DialogResult.OK)
                {
                    // Store CRC in a private field only – do NOT touch GameID box
                    if (uint.TryParse(dlg.SelectedCrcHex,
                                      System.Globalization.NumberStyles.HexNumber,
                                      System.Globalization.CultureInfo.InvariantCulture,
                                      out uint crc))
                    {
                        _pnachCrc = crc;
                    }
                    else
                    {
                        _pnachCrc = null;
                    }

                    // Store name and optionally sync Game Name textbox
                    _pnachGameName = dlg.SelectedGameName;
        			_pnachElfName  = dlg.SelectedElfName;   // <--- NEW

                    if (!string.IsNullOrWhiteSpace(_pnachGameName))
                        txtGameName.Text = _pnachGameName;
                }
            }
        }

        private void UpdateMakeOrganizersCheck()
        {
            // Keep the menu check state in sync with the global flag
            if (menuOptionsMakeOrganizers != null)
                menuOptionsMakeOrganizers.Checked = ConvertCore.g_makefolders;
        }

        private void menuOptionsMakeOrganizers_Click(object sender, EventArgs e)
        {
            // Toggle the flag and update the checkmark
            ConvertCore.g_makefolders = !ConvertCore.g_makefolders;
            UpdateMakeOrganizersCheck();
        }

        private void UpdateVerifierMenuChecks()
                {
                    menuOptionsArmaxVerifiersAuto.Checked   = ConvertCore.g_verifiermode == ConvertCore.VerifierMode.AUTO;
                    menuOptionsArmaxVerifiersManual.Checked = ConvertCore.g_verifiermode == ConvertCore.VerifierMode.MANUAL;
                }

        private void UpdateRegionMenuChecks()
                {
                    // C-side enum: REGION_USA = 0, REGION_PAL = 1, REGION_JAPAN = 2
                    menuOptionsArmaxRegionUsa.Checked   = ConvertCore.g_region == 0;
                    menuOptionsArmaxRegionPal.Checked   = ConvertCore.g_region == 1;
                    menuOptionsArmaxRegionJapan.Checked = ConvertCore.g_region == 2;
                }

        private void menuOptionsArmaxVerifiersAuto_Click(object? sender, EventArgs e)
                {
                    ConvertCore.g_verifiermode = ConvertCore.VerifierMode.AUTO;
                    UpdateVerifierMenuChecks();
                }

        private void menuOptionsArmaxVerifiersManual_Click(object? sender, EventArgs e)
                {
                    ConvertCore.g_verifiermode = ConvertCore.VerifierMode.MANUAL;
                    UpdateVerifierMenuChecks();
                }

        private void menuOptionsArmaxRegionUsa_Click(object? sender, EventArgs e)
                {
                    ConvertCore.g_region = 0; // REGION_USA
                    UpdateRegionMenuChecks();
                }

        private void menuOptionsArmaxRegionPal_Click(object? sender, EventArgs e)
                {
                    ConvertCore.g_region = 1; // REGION_PAL
                    UpdateRegionMenuChecks();
                }

        private void menuOptionsArmaxRegionJapan_Click(object? sender, EventArgs e)
                {
                    ConvertCore.g_region = 2; // REGION_JAPAN
                    UpdateRegionMenuChecks();
                }

        private void UpdateGs3MenuChecks()
            {
                menuOptionsGs3Key0.Checked = ConvertCore.g_gs3key == 0;
                menuOptionsGs3Key1.Checked = ConvertCore.g_gs3key == 1;
                menuOptionsGs3Key2.Checked = ConvertCore.g_gs3key == 2;
                menuOptionsGs3Key3.Checked = ConvertCore.g_gs3key == 3;
                menuOptionsGs3Key4.Checked = ConvertCore.g_gs3key == 4;
            }

        private void SetGs3Key(int key)
            {
                ConvertCore.g_gs3key = (uint)key;
                UpdateGs3MenuChecks();
            }

        private void UpdateCbcSaveVersionMenuChecks()
        {
            menuOptionsCbcVersion7.Checked = ConvertCore.g_cbcSaveVersion == 7;
            menuOptionsCbcVersion8.Checked = ConvertCore.g_cbcSaveVersion == 8;
        }

        private void menuOptionsCbcVersion7_Click(object? sender, EventArgs e)
        {
            ConvertCore.g_cbcSaveVersion = 7;
            UpdateCbcSaveVersionMenuChecks();
            SaveAppSettings(); // optional, or wait until exit – your call
        }

        private void menuOptionsCbcVersion8_Click(object? sender, EventArgs e)
        {
            ConvertCore.g_cbcSaveVersion = 8;
            UpdateCbcSaveVersionMenuChecks();
            SaveAppSettings();
        }

    }
}
