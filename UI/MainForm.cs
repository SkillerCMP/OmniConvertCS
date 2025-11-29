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
		private void chkPnachCrcActive_CheckedChanged(object? sender, EventArgs e)
{
    // Safety: should never be visible when not PNACH, but guard anyway
    if (!_outputAsPnachRaw)
    {
        _pnachCrcActive = false;
        chkPnachCrcActive.Checked = false;
        return;
    }

    if (chkPnachCrcActive.Checked)
    {
        // User wants CRC active; prompt them to set/confirm it
        _pnachCrcActive = true;

        // Reuse the existing menu handler to open the Set Pnach CRC window
        menuOptionsPnachCrc_Click(sender, e);

        // If user canceled or no CRC set, turn it back off
        if (!_pnachCrc.HasValue)
        {
            _pnachCrcActive = false;
            chkPnachCrcActive.Checked = false;
        }
    }
    else
    {
        // User turned CRC emission off (but we keep the last CRC stored)
        _pnachCrcActive = false;
    }
}
		
		private void menuFileLoadCbc_Click(object sender, EventArgs e)
{
    using (var dlg = new OpenFileDialog())
    {
        dlg.Title  = "Load CodeBreaker CBC File";
        dlg.Filter = "CodeBreaker files (*.cbc)|*.cbc|All files (*.*)|*.*";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        try
        {
            var result = CbcReader.Load(dlg.FileName);

            // Set the game name textbox from the file.
            txtGameName.Text = result.GameName;

            // For now, we just dump everything as plain ADDR VALUE lines +
            // headings into the *input* box, so you can re-convert.
            var sb = new StringBuilder();

            foreach (var cheat in result.Cheats)
            {
                string name = cheat.name ?? string.Empty;

                if (!string.IsNullOrWhiteSpace(name))
                    sb.AppendLine(name);

                if (cheat.codecnt > 0)
                {
                    for (int i = 0; i + 1 < cheat.codecnt; i += 2)
                    {
                        uint addr = cheat.code[i];
                        uint val  = cheat.code[i + 1];
                        sb.AppendLine($"{addr:X8} {val:X8}");
                    }
                }

                // Blank line between cheats
                sb.AppendLine();
            }

            txtInput.Text = sb.ToString().TrimEnd();
            txtOutput.Clear();

            // Because we just loaded raw codes, treat this as "fresh input".
            _lastConvertedCheats = null;
        }
        catch (Exception ex)
        {
            MessageBox.Show(this,
                "Error loading CBC file:\r\n" + ex.Message,
                "Load CodeBreaker CBC",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}
private void menuFileLoadArmaxBin_Click(object sender, EventArgs e)
{
    using (var dlg = new OpenFileDialog())
    {
        dlg.Title  = "Load AR MAX .bin File";
        dlg.Filter = "AR MAX files (*.bin)|*.bin|All files (*.*)|*.*";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        try
        {
            var textResult = ArmaxBinReader.LoadAsArmaxEncryptedTextWithGames(dlg.FileName);

            // If the list only has a single game, put that into the Game Name box
            // and do NOT print "Game Name: ..." inside the text area.
            if (textResult.GameCount == 1)
            {
                txtGameName.Text = textResult.GameName ?? string.Empty;
            }
            else
            {
                // Multi-game codelist: leave the Game Name box empty so the user
                // sees the per-game headings instead.
                txtGameName.Clear();
            }

            // Show the ARMAX encrypted codes (with optional "Game Name: ..." headings)
            txtInput.Text = string.Join(Environment.NewLine, textResult.Lines);
            txtOutput.Clear();

            // Fresh input – user will hit Convert to target a device.
            _lastConvertedCheats = null;
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                this,
                "Error loading AR MAX .bin file:\r\n" + ex.Message,
                "Load AR MAX .bin",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}
private void menuFileLoadP2m_Click(object sender, EventArgs e)
{
    using (var dlg = new OpenFileDialog())
    {
        dlg.Title  = "Load XP/GS .p2m File";
        dlg.Filter = "GameShark XP/GS files (*.p2m)|*.p2m|All files (*.*)|*.*";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        try
        {
            var result = P2mReader.Load(dlg.FileName);

            // Populate the Game Name textbox from user.dat.
            txtGameName.Text = result.GameName;

            var sb = new StringBuilder();
            foreach (var cheat in result.Cheats)
            {
                string name = cheat.name ?? string.Empty;
                if (!string.IsNullOrWhiteSpace(name))
                    sb.AppendLine(name);

                if (cheat.codecnt > 0)
                {
                    for (int i = 0; i + 1 < cheat.codecnt; i += 2)
                    {
                        uint addr = cheat.code[i];
                        uint val  = cheat.code[i + 1];
                        sb.AppendLine($"{addr:X8} {val:X8}");
                    }
                }

                sb.AppendLine();
            }

            txtInput.Text = sb.ToString().TrimEnd();
            txtOutput.Clear();
            _lastConvertedCheats = null;
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                this,
                "Error loading P2M file:\r\n" + ex.Message,
                "Load XP/GS P2M",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}

		private void menuFileLoadText_Click(object sender, EventArgs e)
{
    using (var dlg = new OpenFileDialog())
    {
        dlg.Title  = "Load Text File";
        dlg.Filter = "Text files (*.txt)|*.txt|All files (*.*)|*.*";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        try
        {
            // Read as UTF-8; adjust if you prefer ANSI/other encodings.
            string text = File.ReadAllText(dlg.FileName, Encoding.UTF8);
             txtInput.Text = CleanCbSiteFormat(text);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this,
                "Error loading file:\r\n" + ex.Message,
                "Load Text File",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}		
private void menuFileSaveAsText_Click(object sender, EventArgs e)
{
    string text = txtOutput.Text ?? string.Empty;

    if (string.IsNullOrWhiteSpace(text))
    {
        var result = MessageBox.Show(this,
            "The Output window is empty. Save anyway?",
            "Save As Text",
            MessageBoxButtons.YesNo,
            MessageBoxIcon.Question);

        if (result != DialogResult.Yes)
            return;
    }

    using (var dlg = new SaveFileDialog())
    {
        dlg.Title      = "Save As Text";
        dlg.Filter     = "Text files (*.txt)|*.txt|All files (*.*)|*.*";
        dlg.DefaultExt = "txt";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        try
        {
            File.WriteAllText(dlg.FileName, text, Encoding.UTF8);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this,
                "Error saving file:\r\n" + ex.Message,
                "Save As Text",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}
private void menuFileSaveAsPnach_Click(object sender, EventArgs e)
{
    string text = txtOutput.Text ?? string.Empty;

    // Warn if output isn’t currently PNACH(RAW)
    if (!_outputAsPnachRaw)
    {
        var result = MessageBox.Show(this,
            "The current output format is not set to Pnach(RAW).\r\n" +
            "The file will be saved as-is. Continue?",
            "Save As .pnach",
            MessageBoxButtons.YesNo,
            MessageBoxIcon.Warning);

        if (result != DialogResult.Yes)
            return;
    }

    if (string.IsNullOrWhiteSpace(text))
    {
        var result = MessageBox.Show(this,
            "The Output window is empty. Save anyway?",
            "Save As .pnach",
            MessageBoxButtons.YesNo,
            MessageBoxIcon.Question);

        if (result != DialogResult.Yes)
            return;
    }

    using (var dlg = new SaveFileDialog())
    {
        dlg.Title      = "Save As .pnach";
        dlg.Filter     = "PCSX2 PNACH files (*.pnach)|*.pnach|All files (*.*)|*.*";
        dlg.DefaultExt = "pnach";

        // Default filename: if Add CRC is active AND we have ELF + CRC, use ELF_CRC.pnach
// otherwise fall back to PUTNAME.pnach so the user can fill it in.
string suggestedName = "PUTNAME.pnach";

if (_pnachCrcActive &&                      // Add CRC checkbox is checked
    _pnachCrc.HasValue &&                   // we actually have a CRC
    !string.IsNullOrWhiteSpace(_pnachElfName))   // and an ELF name
{
    suggestedName = $"{_pnachElfName}_{_pnachCrc.Value:X8}.pnach";
}

dlg.FileName = suggestedName;

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        try
        {
            PnachWriter.SaveFromText(dlg.FileName, text);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this,
                "Error saving .pnach file:\r\n" + ex.Message,
                "Save As .pnach",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}

private void ShowBinaryExportNotImplemented(string title, string description)
{
    MessageBox.Show(this,
        description + "\r\n\r\nFor now, you can use \"Save As Text\" and the original Omniconvert to build this file type.",
        title,
        MessageBoxButtons.OK,
        MessageBoxIcon.Information);
}
private void menuFileSaveAsArmaxBin_Click(object sender, EventArgs e)
{
    // Require a successful conversion first.
    if (_lastConvertedCheats == null || _lastConvertedCheats.Count == 0)
    {
        MessageBox.Show(
            this,
            "No converted cheats are available.\r\nPlease click Convert first.",
            "AR MAX Export",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
        return;
    }

    // Make sure the current output settings actually target AR MAX.
    bool armaxOutput =
        ConvertCore.g_outdevice == ConvertCore.Device.DEV_ARMAX &&
        ConvertCore.g_outcrypt  == ConvertCore.Crypt.CRYPT_ARMAX;

    if (!armaxOutput)
    {
        MessageBox.Show(
            this,
            "The output device/crypt settings are not set to AR MAX.\r\n" +
            "Please select AR MAX as the output device, click Convert, then try again.",
            "AR MAX Export",
            MessageBoxButtons.OK,
            MessageBoxIcon.Warning);
        return;
    }

    using (var dlg = new SaveFileDialog())
    {
        dlg.Title      = "Save As AR MAX File";
        dlg.Filter     = "AR MAX files (*.bin)|*.bin|All files (*.*)|*.*";
        dlg.DefaultExt = "bin";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;
// Use the Game Name textbox value here
        string gameName = txtGameName.Text?.Trim() ?? string.Empty;
        // Build game_t for the writer. Name can be blank; the writer will
        // substitute "Unnamed Game" if needed (matching the original C code).
        var game = new game_t
        {
            id   = _lastGameId,   // from last Convert
            name = gameName,      // <-- THIS is the important change
            cnt  = 0              // filled in by ArmaxBinWriter.CreateList
        };

        int ret = ArmaxBinWriter.CreateList(_lastConvertedCheats, game, dlg.FileName);
        if (ret != 0)
        {
            MessageBox.Show(
                this,
                "An error occurred while creating the AR MAX .bin file.",
                "AR MAX Export",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}


private void menuFileSaveAsCbc_Click(object sender, EventArgs e)
{
    // Require a successful conversion first (same pattern as AR MAX).
    if (_lastConvertedCheats == null || _lastConvertedCheats.Count == 0)
    {
        MessageBox.Show(
            this,
            "No converted cheats are available.\r\nPlease click Convert first.",
            "CodeBreaker CBC Export",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
        return;
    }

    // Make sure the current output settings actually target CodeBreaker.
    bool cbOutput =
        ConvertCore.g_outdevice == ConvertCore.Device.DEV_CB &&
        (ConvertCore.g_outcrypt == ConvertCore.Crypt.CRYPT_CB ||
         ConvertCore.g_outcrypt == ConvertCore.Crypt.CRYPT_CB7_COMMON);

    if (!cbOutput)
    {
        MessageBox.Show(
            this,
            "The output device/crypt settings are not set to CodeBreaker.\r\n" +
            "Please select CodeBreaker as the output device, click Convert, then try again.",
            "CodeBreaker CBC Export",
            MessageBoxButtons.OK,
            MessageBoxIcon.Warning);
        return;
    }

    using (var dlg = new SaveFileDialog())
    {
        dlg.Title      = "Save As CodeBreaker CBC File";
        dlg.Filter     = "CodeBreaker files (*.cbc)|*.cbc|All files (*.*)|*.*";
        dlg.DefaultExt = "cbc";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        // Use the Game Name textbox for the file's internal game name.
        string gameName = txtGameName.Text?.Trim() ?? string.Empty;

        var game = new game_t
        {
            id   = _lastGameId,  // from last Convert
            name = gameName,
            cnt  = 0             // filled in by CbcWriter.CreateFile
        };

        // For now, we always include headings (doHeadings = true),
        // matching the original Day 1 cbcCreateFile usage.
        int ret = CbcWriter.CreateFile(
            _lastConvertedCheats,
            game,
            dlg.FileName,
            doHeadings: true);

        if (ret != 0)
        {
            // CbcWriter will already have shown a specific MsgBox
            // for "no master code", "too many master codes", etc.
            // This is just a generic "something went wrong" wrapper.
            MessageBox.Show(
                this,
                "An error occurred while creating the CodeBreaker .cbc file.",
                "CodeBreaker CBC Export",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}

private void menuFileSaveAsP2m_Click(object sender, EventArgs e)
{
    // 1) Require a successful conversion first
    if (_lastConvertedCheats == null || _lastConvertedCheats.Count == 0)
    {
        MessageBox.Show(
            this,
            "No converted cheats are available.\r\nPlease click Convert first.",
            "XP/GS P2M Export",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
        return;
    }

    // 2) Make sure the current output settings match XP/GS (GS3/GS5)
    bool xpOutput =
        ConvertCore.g_outdevice == ConvertCore.Device.DEV_GS3 &&
        (ConvertCore.g_outcrypt == ConvertCore.Crypt.CRYPT_GS3 ||
         ConvertCore.g_outcrypt == ConvertCore.Crypt.CRYPT_GS5);

    if (!xpOutput)
    {
        MessageBox.Show(
            this,
            "The output device/crypt settings are not set to XP/GS (GS3/GS5).\r\n" +
            "Please select an XP/GS format as the *output* type, click Convert, then try again.",
            "XP/GS P2M Export",
            MessageBoxButtons.OK,
            MessageBoxIcon.Warning);
        return;
    }

    using (var dlg = new SaveFileDialog())
    {
        dlg.Title      = "Save As XP/GS File";
        dlg.Filter     = "XP/GS files (*.p2m)|*.p2m|All files (*.*)|*.*";
        dlg.DefaultExt = "p2m";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        // 3) Build game_t from the last conversion
        string gameName = txtGameName.Text?.Trim() ?? string.Empty;

        var game = new game_t
        {
            id   = _lastGameId,   // set in btnConvert_Click
            name = gameName,
            cnt  = 0              // filled/used internally as needed
        };

        // 4) Call the C# P2M writer (1:1 port of p2mCreateFile)
        int ret = P2m.CreateFile(
            _lastConvertedCheats,
            game,
            dlg.FileName,
            doHeadings: true);

        if (ret != 0)
        {
            // P2m will already have shown specific MsgBoxes for
            // "no master code", "too many master codes", etc.
            // This is just a generic wrapper.
            MessageBox.Show(
                this,
                "An error occurred while creating the XP/GS .p2m file.",
                "XP/GS P2M Export",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}

private void menuFileSaveAsSwapBin_Click(object sender, EventArgs e)
{
    using (var dlg = new SaveFileDialog())
    {
        dlg.Title      = "Save As Swap Magic File";
        dlg.Filter     = "Swap Magic files (*.bin)|*.bin|All files (*.*)|*.*";
        dlg.DefaultExt = "bin";

        if (dlg.ShowDialog(this) != DialogResult.OK)
            return;

        ShowBinaryExportNotImplemented(
            "Swap Magic Export Not Implemented",
            "Saving Swap Magic files has not been implemented in the C# port yet.");
    }
}
		// --- Options > AR MAX Disc Hash -------------------------------------------
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
                dlg.ShowDialog(this);
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


		
// --- Options > Make Organizers --------------------------------------------

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

        // --- Options > AR MAX Verifiers / Region -----------------------------

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
// --- Options > GS3 Key -----------------------------------------------

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

    private void menuOptionsGs3Key0_Click(object? sender, EventArgs e) => SetGs3Key(0);
    private void menuOptionsGs3Key1_Click(object? sender, EventArgs e) => SetGs3Key(1);
    private void menuOptionsGs3Key2_Click(object? sender, EventArgs e) => SetGs3Key(2);
    private void menuOptionsGs3Key3_Click(object? sender, EventArgs e) => SetGs3Key(3);
    private void menuOptionsGs3Key4_Click(object? sender, EventArgs e) => SetGs3Key(4);
	
	// Remember the last successful conversion so that
        // "File -> Save As AR MAX" can export those cheats.
        private System.Collections.Generic.List<cheat_t>? _lastConvertedCheats;
        private uint _lastGameId;
		private bool _outputAsPnachRaw;   // NEW: track if output should be PNACH format
		private bool _inputAsPnachRaw;    // NEW: track if INPUT is PNACH(RAW)
		private uint?  _pnachCrc;
		private string? _pnachGameName;
		private string? _pnachElfName;     // <--- NEW: e.g. "SLUS_203.12"
		private bool _pnachCrcActive;   // whether to actually emit the CRC line in PNACH output
		// Helper representing one crypt/device option (like wincryptopt_t, minus ID)
        private sealed class CryptOption
{
    public ConvertCore.Crypt Crypt { get; }
    public ConvertCore.Device Device { get; }
    public string Text { get; }
    public bool UsePnachFormat { get; }   // NEW

    public CryptOption(
        ConvertCore.Crypt crypt,
        ConvertCore.Device device,
        string text,
        bool usePnachFormat = false)      // NEW optional flag
    {
        Crypt = crypt;
        Device = device;
        Text = text;
        UsePnachFormat = usePnachFormat;
    }
}

        // Unencrypted >
        private static readonly CryptOption[] Options_Unencrypted = new[]
        {
            new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_STD,   "Standard(RAW)"),
			new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_STD,   "Pnach(RAW)", usePnachFormat: true),  // NEW
            new CryptOption(ConvertCore.Crypt.CRYPT_MAXRAW, ConvertCore.Device.DEV_ARMAX, "ARMAX(RAW)"),
            new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_AR2,   "AR/GS V1-2(RAW)"),
            new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_CB,    "Codebreaker(RAW)"),
            // new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_AR2,   "GS V1-2(RAW)"),
            new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_GS3,   "XP/GS V3(RAW)"),
        };

        // Action Replay >
        private static readonly CryptOption[] Options_ActionReplay = new[]
        {
            new CryptOption(ConvertCore.Crypt.CRYPT_AR1,   ConvertCore.Device.DEV_AR1,   "AR-V1"),
            new CryptOption(ConvertCore.Crypt.CRYPT_AR2,   ConvertCore.Device.DEV_AR2,   "AR-V2"),
            new CryptOption(ConvertCore.Crypt.CRYPT_ARMAX, ConvertCore.Device.DEV_ARMAX, "ARMAX"),
        };

        // CodeBreaker >
        private static readonly CryptOption[] Options_CodeBreaker = new[]
        {
            new CryptOption(ConvertCore.Crypt.CRYPT_CB,         ConvertCore.Device.DEV_CB, "CB - V1+ (All v7 Keys)"),
            new CryptOption(ConvertCore.Crypt.CRYPT_CB7_COMMON, ConvertCore.Device.DEV_CB, "CB - V7+ (Common Key)"),
        };

        // GameShark > Interact >
        private static readonly CryptOption[] Options_GameShark_Interact = new[]
        {
            new CryptOption(ConvertCore.Crypt.CRYPT_AR1, ConvertCore.Device.DEV_AR1, "GS - V1"),
            new CryptOption(ConvertCore.Crypt.CRYPT_AR2, ConvertCore.Device.DEV_AR2, "GS - V2"),
        };

        // GameShark > MadCatz >
        private static readonly CryptOption[] Options_GameShark_MadCatz = new[]
        {
            new CryptOption(ConvertCore.Crypt.CRYPT_GS3, ConvertCore.Device.DEV_GS3, "GS - V3-4"),
            new CryptOption(ConvertCore.Crypt.CRYPT_GS5, ConvertCore.Device.DEV_GS3, "GS - V5+"),
        };

        // Swap Magic >
        private static readonly CryptOption[] Options_SwapMagic = new[]
        {
            new CryptOption(ConvertCore.Crypt.CRYPT_AR1, ConvertCore.Device.DEV_AR1, "SM - Coder V3.x"),
        };

        // Xploder >
        private static readonly CryptOption[] Options_Xploder = new[]
        {
            new CryptOption(ConvertCore.Crypt.CRYPT_CB,  ConvertCore.Device.DEV_CB,  "XP - V1-3"),
            new CryptOption(ConvertCore.Crypt.CRYPT_GS3, ConvertCore.Device.DEV_GS3, "XP - V4"),
            new CryptOption(ConvertCore.Crypt.CRYPT_GS5, ConvertCore.Device.DEV_GS3, "XP - V5+"),
        };

        public MainForm()
        {
            InitializeComponent();
        }
private System.Windows.Forms.TextBox GetActiveTextBox()
{
    // If the active control *is* a TextBox, use it.
    if (this.ActiveControl is System.Windows.Forms.TextBox tb)
        return tb;

    // Fallback: just check which one is focused.
    if (this.txtInput.Focused)
        return this.txtInput;

    if (this.txtOutput.Focused)
        return this.txtOutput;

    return null;
}
 
 private static string CleanCbSiteFormat(string input)
 {
     if (string.IsNullOrEmpty(input))
         return input;
 
     // Matches a CodeBreaker/RAW pair: XXXXXXXX YYYYYYYY
     var codePairRegex = new Regex(
         @"[0-9A-Fa-f]{8}\s+[0-9A-Fa-f]{8}",
         RegexOptions.Compiled);
 
     var sb = new StringBuilder();
 
     using (var reader = new StringReader(input))
     {
         string line;
         while ((line = reader.ReadLine()) != null)
         {
             if (string.IsNullOrWhiteSpace(line))
             {
                 sb.AppendLine();
                 continue;
             }
 
             var matches = codePairRegex.Matches(line);
 
             if (matches.Count == 0)
             {
                 // No code pair on this line → leave as-is.
                 sb.AppendLine(line.TrimEnd());
             }
             else
             {
                 // 1) Anything before the first code pair is treated as the name.
                 int firstIndex = matches[0].Index;
                 string before = line.Substring(0, firstIndex).TrimEnd();
                 if (!string.IsNullOrEmpty(before))
                     sb.AppendLine(before);
 
                 // 2) Each code pair becomes its own line.
                 foreach (Match m in matches)
                 {
                     sb.AppendLine(m.Value.Trim());
                 }
 
                 // 3) If there is any trailing text after the last pair, keep it.
                 Match last = matches[matches.Count - 1];
                 int cursor = last.Index + last.Length;
                 if (cursor < line.Length)
                 {
                     string tail = line.Substring(cursor).Trim();
                     if (!string.IsNullOrEmpty(tail))
                         sb.AppendLine(tail);
                 }
             }
         }
     }
 
     // Trim trailing blank lines so we don't end with extra newlines
     return sb.ToString().TrimEnd('\r', '\n');
 }
 
        private void MainForm_Load(object sender, EventArgs e)
        {
            // Build hierarchical menus under Input / Output
            BuildCryptMenu(menuInput,  isInput: true);
            BuildCryptMenu(menuOutput, isInput: false);

            // Default both sides to Unencrypted > Standard (RAW)
            SetCryptSelection(true,  Options_Unencrypted[0]);
            SetCryptSelection(false, Options_Unencrypted[0]);
			
			// Load persisted settings (if any)
			LoadAppSettings();
			
			// Apply current globals (either defaults or loaded) to the menus
			var defaultOpt = Options_Unencrypted[0];
			var inOpt  = FindCryptOption(ConvertCore.g_indevice, ConvertCore.g_incrypt,  defaultOpt);
			var outOpt = FindCryptOption(ConvertCore.g_outdevice, ConvertCore.g_outcrypt, defaultOpt);
// If settings requested PNACH RAW output, prefer that specific option
if (_outputAsPnachRaw)
{
    foreach (var opt in Options_Unencrypted)
    {
        if (opt.Device == ConvertCore.g_outdevice &&
            opt.Crypt  == ConvertCore.g_outcrypt &&
            opt.UsePnachFormat)
        {
            outOpt = opt;
            break;
        }
    }
}
			SetCryptSelection(true,  inOpt);
			SetCryptSelection(false, outOpt);
			
			// Sync ARMAX Options menu state with current globals
            UpdateVerifierMenuChecks();
            UpdateRegionMenuChecks();
			UpdateGs3MenuChecks();

            txtGameId.Text = "0357";
			
			// Show version from OmniconvertCS.csproj in the title bar
    string version = Application.ProductVersion;
    int plusIndex = version.IndexOf('+');
    if (plusIndex >= 0)
        version = version.Substring(0, plusIndex);

    this.Text = $"Omniconvert C# v{version}";
			
			// --- NEW: initialise Options menu state ---
			UpdateMakeOrganizersCheck();
			EnumerateArmaxDiscHashDrives();
			SetArmaxHashDrive(ConvertCore.g_hashdrive);  // will select "Do Not Create" initially
        }
private void UpdateCbcSaveVersionMenuChecks()
{
    menuOptionsCbcVersion7.Checked = ConvertCore.g_cbcSaveVersion == 7;
    menuOptionsCbcVersion8.Checked = ConvertCore.g_cbcSaveVersion == 8;
}
        // ---------------------------------------------------------------------
        // UI handlers
        // ---------------------------------------------------------------------
private void menuEditUndo_Click(object sender, EventArgs e)
{
    var tb = GetActiveTextBox();
    if (tb != null && tb.CanUndo)
    {
        tb.Undo();
        // after Undo, Windows Forms disables further Undo by default
        tb.ClearUndo();
    }
}

private void menuEditCut_Click(object sender, EventArgs e)
{
    var tb = GetActiveTextBox();
    if (tb != null && tb.SelectedText.Length > 0)
    {
        tb.Cut();
    }
}

private void menuEditCopy_Click(object sender, EventArgs e)
{
    var tb = GetActiveTextBox();
    if (tb != null && tb.SelectedText.Length > 0)
    {
        tb.Copy();
    }
}

private void menuEditPaste_Click(object sender, EventArgs e)
 {
     var tb = GetActiveTextBox();
     if (tb != null)
     {
         // If we're pasting into the Input window, run the CB-site cleaner.
         if (tb == this.txtInput && Clipboard.ContainsText())
         {
             string raw = Clipboard.GetText();
             string cleaned = CleanCbSiteFormat(raw);
             tb.SelectedText = cleaned;
         }
         else
         {
             tb.Paste();
         }
     }
 }

private void menuEditSelectAll_Click(object sender, EventArgs e)
{
    var tb = GetActiveTextBox();
    if (tb != null)
    {
        tb.SelectAll();
        tb.Focus();
    }
}

private void menuEditClearInput_Click(object sender, EventArgs e)
{
    this.txtInput.Clear();
    this.txtInput.Focus();
}

private void menuEditClearOutput_Click(object sender, EventArgs e)
{
    this.txtOutput.Clear();
    this.txtOutput.Focus();
}
        private void btnClearInput_Click(object sender, EventArgs e)
        {
            txtInput.Clear();
        }

        private void btnClearOutput_Click(object sender, EventArgs e)
        {
            txtOutput.Clear();
        }

        private void CryptMenuItem_Click(object? sender, EventArgs e)
        {
            if (sender is not ToolStripMenuItem mi)
                return;
            if (mi.Tag is not Tuple<bool, CryptOption> data)
                return;

            bool isInput = data.Item1;
            CryptOption opt = data.Item2;

            SetCryptSelection(isInput, opt);
        }

        private void btnConvert_Click(object sender, EventArgs e)
{
    txtOutput.Clear();

    string inputText = txtInput.Text;
    if (string.IsNullOrWhiteSpace(inputText))
    {
        txtOutput.Text = "// No input provided.";
        return;
    }

    // Are we reading ARMAX encrypted text (XXXXX-XXXXX-XXXXX) as input?
    bool isArmaxEncryptedInput =
        ConvertCore.g_indevice == ConvertCore.Device.DEV_ARMAX &&
        ConvertCore.g_incrypt  == ConvertCore.Crypt.CRYPT_ARMAX;
	//Pnach Input
	bool isPnachInput = _inputAsPnachRaw;   // NEW

    // Parse into logical "cheats": optional name line + one or more code lines
    var cheats = new System.Collections.Generic.List<cheat_t>();
    var labels = new System.Collections.Generic.List<string?>();

    cheat_t? currentCheat = null;

    cheat_t EnsureCurrentCheat()
    {
        if (currentCheat == null)
        {
            currentCheat = Cheat.cheatInit();
            cheats.Add(currentCheat);
            labels.Add(null);
        }
        return currentCheat;
    }

    void StartNewCheatWithLabel(string label)
    {
        currentCheat = Cheat.cheatInit();
        cheats.Add(currentCheat);
        labels.Add(label);
    }

    string currentPnachGroup = null;   // NEW: tracks [Group\Name] group while parsing

string[] lines = inputText.Replace("\r\n", "\n").Split('\n');
foreach (string rawLine in lines)
{
    string line = rawLine.Trim();
    if (string.IsNullOrEmpty(line))
        continue;
    if (line.StartsWith("//") || line.StartsWith("#") || line.StartsWith(";"))
        continue;

    // ----------------------------
    // PNACH(RAW) input: only [] + patch= lines
    // ----------------------------
    if (isPnachInput)
    {
        // Skip PNACH header lines
        if (line.StartsWith("gametitle=", StringComparison.OrdinalIgnoreCase) ||
            line.StartsWith("comment=",   StringComparison.OrdinalIgnoreCase) ||
            line.StartsWith("crc=",       StringComparison.OrdinalIgnoreCase) ||
            line.StartsWith("author=",    StringComparison.OrdinalIgnoreCase) ||
			line.StartsWith("description=", StringComparison.OrdinalIgnoreCase))   // NEW
        {
            continue;
        }

        // Section header: [Group\Name] or [Name]
        if (line.Length >= 3 && line[0] == '[' && line[line.Length - 1] == ']')
        {
            string inner = line.Substring(1, line.Length - 2).Trim();
            if (inner.Length == 0)
                continue;

            string groupName = null;
            string codeName;

            int slashIndex = inner.IndexOf('\\');
            if (slashIndex >= 0)
            {
                groupName = inner.Substring(0, slashIndex).Trim();
                codeName  = inner.Substring(slashIndex + 1).Trim();
            }
            else
            {
                codeName = inner;
            }

            // If the group changed, emit a "group-only" cheat
            if (!string.IsNullOrEmpty(groupName) &&
                !string.Equals(groupName, currentPnachGroup, StringComparison.Ordinal))
            {
                StartNewCheatWithLabel(groupName);  // Group 1, Group 2, etc.
                currentPnachGroup = groupName;
            }

            // Now start the actual cheat for this section
            if (!string.IsNullOrEmpty(codeName))
            {
                StartNewCheatWithLabel(codeName);   // Hello world, NEW TO ME, etc.
            }

            // Next lines (patch=...) will attach to this cheat
            continue;
        }

        // patch=1,EE,ADDR,extended,VALUE
        if (line.StartsWith("patch=", StringComparison.OrdinalIgnoreCase))
        {
            string[] partsComma = line.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            if (partsComma.Length >= 5)
            {
                string addr = partsComma[2].Trim();
                string val  = partsComma[4].Trim();

                // Strip trailing comments or junk (just in case)
                int commentIdx = addr.IndexOfAny(new[] { ' ', '\t', '#', ';', '/' });
                if (commentIdx >= 0) addr = addr.Substring(0, commentIdx);

                commentIdx = val.IndexOfAny(new[] { ' ', '\t', '#', ';', '/' });
                if (commentIdx >= 0) val = val.Substring(0, commentIdx);

                if (uint.TryParse(addr, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _) &&
                    uint.TryParse(val,  NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _))
                {
                    var cheatForLine = EnsureCurrentCheat();
                    Cheat.cheatAppendCodeFromText(cheatForLine, addr, val);
                }
            }

            continue;
        }
		// In PNACH mode, anything that's not [] or patch= or header/meta is ignored.
        continue;
    }

    // ----------------------------
    // Generic input parsing (RAW / ARMAX)
    // ----------------------------
    string[] parts = line.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
    if (parts.Length == 0)
        continue;

    bool handledCode = false;

    // ARMAX encrypted input: treat the last token as the text code
    if (isArmaxEncryptedInput)
    {
        string lastToken = parts[parts.Length - 1];

        uint w0, w1;
        if (Armax.IsArMaxStr(lastToken) &&
            Armax.TryParseEncryptedLine(lastToken, out w0, out w1))
        {
            var cheatForLine = EnsureCurrentCheat();
            Cheat.cheatAppendOctet(cheatForLine, w0);
            Cheat.cheatAppendOctet(cheatForLine, w1);
            handledCode = true;
        }
    }

    // Default: ADDR VALUE hex pairs on a line
    if (!handledCode && parts.Length >= 2)
    {
        string addr = parts[parts.Length - 2];
        string val  = parts[parts.Length - 1];

        if (uint.TryParse(addr, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _) &&
            uint.TryParse(val,  NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _))
        {
            var cheatForLine = EnsureCurrentCheat();
            Cheat.cheatAppendCodeFromText(cheatForLine, addr, val);
            handledCode = true;
        }
    }

    // If it wasn't recognized as a code line, treat it as a *name* line
    if (!handledCode)
    {
        StartNewCheatWithLabel(line);
    }
}


		// Drop any cheats that ended up with no codes,
		// EXCEPT when we need heading-style entries:
		//  - AR MAX + Make Organizers (folders)
		//  - GS3 / XP-style headings
		//  - CodeBreaker (CBC) group labels (sw=4)
		bool keepEmptyCheatsAsFolders =
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_ARMAX && ConvertCore.g_makefolders) ||
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_GS3) ||
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_CB) ||
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_STD);

		for (int i = cheats.Count - 1; i >= 0; i--)
{
		if (cheats[i].codecnt == 0 && !keepEmptyCheatsAsFolders)
    {
			cheats.RemoveAt(i);
			labels.RemoveAt(i);
    }
}


    if (cheats.Count == 0)
    {
        txtOutput.Text = "// No valid code lines parsed from input.";
        return;
    }

    // Game ID (hex)
    if (uint.TryParse(txtGameId.Text.Trim(), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint gameId))
        ConvertCore.g_gameid = gameId;
    else
        ConvertCore.g_gameid = 0;

    // NOTE:
    // We *do not* touch g_region, g_verifiermode, g_gs3key here.
    // Those are controlled via the Options menu (ARMAX Verifier Mode, Region, GS3 Key, etc.)
Cheat.cheatClearFolderId();
    int resultCode = 0;
    try
    {
        foreach (var cheat in cheats)
        {
            resultCode = ConvertCore.ConvertCode(cheat);
            if (resultCode != 0)
                break;
        }
    }
    catch (Exception ex)
    {
        txtOutput.Text = "// Error during conversion:\r\n// " + ex.Message;
        return;
    }

    if (resultCode != 0)
{
    txtOutput.Text = "// ConvertCode returned error code: " + resultCode;
    return;
}

// Are we outputting ARMAX encrypted text?
bool outputAsArmaxEncrypted =
    ConvertCore.g_outdevice == ConvertCore.Device.DEV_ARMAX &&
    ConvertCore.g_outcrypt  == ConvertCore.Crypt.CRYPT_ARMAX;

// Are we outputting PNACH (RAW) format?
bool outputAsPnachRaw = _outputAsPnachRaw;

var sb = new StringBuilder();

// Optional PNACH header at the top of the file
if (outputAsPnachRaw)
{
    // Prefer explicit PNACH game name if set, otherwise use the UI textbox.
    string gameName = !string.IsNullOrWhiteSpace(_pnachGameName)
        ? _pnachGameName!
        : txtGameName.Text?.Trim();

    if (string.IsNullOrEmpty(gameName))
        gameName = "Unknown Game";

    // Keep UI in sync with what we actually use
    txtGameName.Text = gameName;
	sb.AppendLine($"// Converted by Omniconvert C#");

    if (_pnachCrcActive && _pnachCrc.HasValue)
    {
        // CRC goes to the OUTPUT WINDOW as text
		sb.AppendLine($"// GameName: {gameName}");
        sb.AppendLine($"// CRC: {_pnachCrc.Value:X8}");
    }
    else
    {
        sb.AppendLine($"// NO CRC/NAME SET");
    }

    sb.AppendLine();
}

string currentGroupForPnach = null;   // NEW: track current PNACH group

for (int idx = 0; idx < cheats.Count; idx++)
{
    var cheat = cheats[idx];
    string? label = labels[idx];

    // 1) try user label line
    // 2) otherwise auto-detected cheat.name (Enable Code / (M) / Unnamed)
    string? header = !string.IsNullOrWhiteSpace(label)
        ? label
        : (!string.IsNullOrWhiteSpace(cheat.name) ? cheat.name : null);

    // Keep internal name in sync for other exporters (ARMAX bin, etc.)
    if (!string.IsNullOrEmpty(header))
        cheat.name = header;

    // -------------------------
    // PNACH RAW small-output
    // -------------------------
    if (outputAsPnachRaw)
    {
        // A cheat with no codes is treated as a "group" header (Group 1, Group 2, ...)
        if (cheat.codecnt == 0)
        {
            if (!string.IsNullOrWhiteSpace(header))
                currentGroupForPnach = header.Trim();
            continue;
        }

        // Name for this section
        string baseName = header;
        if (string.IsNullOrWhiteSpace(baseName))
            baseName = $"Code {idx + 1}";

        // Combine with current group if any: Group1\Hello world
        string finalName = currentGroupForPnach != null
            ? $"{currentGroupForPnach}\\{baseName}"
            : baseName;

        // [Group1\Hello world]
        sb.AppendLine($"[{finalName}]");

        // patch=1,EE,ADDR,extended,VALUE
        for (int i = 0; i < cheat.codecnt; i += 2)
        {
            uint addr = cheat.code[i];
            uint val  = (i + 1 < cheat.codecnt) ? cheat.code[i + 1] : 0u;
            sb.AppendLine($"patch=1,EE,{addr:X8},extended,{val:X8}");
        }

        sb.AppendLine();  // blank line between sections
        continue;         // skip normal RAW / ARMAX formatting
    }

    // -------------------------
    // Non-PNACH output paths
    // -------------------------

    if (!string.IsNullOrEmpty(header))
    {
        sb.AppendLine(header);
    }

    if (outputAsArmaxEncrypted)
    {
        // Encrypted ARMAX text output: XXXXX-XXXXX-XXXXX
        for (int i = 0; i + 1 < cheat.codecnt; i += 2)
        {
            string armCode = Armax.FormatEncryptedLine(cheat.code[i], cheat.code[i + 1]);
            sb.AppendLine(armCode);
        }
    }
    else
    {
        // Default: two octets per line (ADDR VALUE)
        for (int i = 0; i < cheat.codecnt; i += 2)
        {
            uint addr = cheat.code[i];
            uint val  = (i + 1 < cheat.codecnt) ? cheat.code[i + 1] : 0u;
            sb.AppendLine($"{addr:X8} {val:X8}");
        }
    }

    // Blank line between cheats (but not after the last one)
    if (idx + 1 < cheats.Count)
        sb.AppendLine();
}


// Remember this conversion for "Save As AR MAX (.bin)"
_lastConvertedCheats = cheats;
_lastGameId = ConvertCore.g_gameid;

txtOutput.Text = sb.ToString();
}
// ---------------------------------------------------------------------
// Persistent settings (Input/Output + Options)
// ---------------------------------------------------------------------

private sealed class AppSettings
{
    public ConvertCore.Device       InputDevice  { get; set; }
    public ConvertCore.Crypt        InputCrypt   { get; set; }
    public ConvertCore.Device       OutputDevice { get; set; }
    public ConvertCore.Crypt        OutputCrypt  { get; set; }
	public bool                     OutputPnachRaw { get; set; }   // NEW
    public ConvertCore.VerifierMode VerifierMode { get; set; }
    public uint                     Region       { get; set; }
    public bool                     MakeFolders  { get; set; }
    public char                     HashDrive    { get; set; }
    public uint                     Gs3Key       { get; set; }
	public int                      CbcSaveVersion { get; set; }  // 7 or 8
}

private static string GetSettingsPath()
{
    try
    {
        return Path.Combine(AppContext.BaseDirectory, "OmniconvertSettings.json");
    }
    catch
    {
        // Fallback to current working directory if BaseDirectory is unavailable
        return "OmniconvertSettings.json";
    }
}

private void LoadAppSettings()
{
    try
    {
        var path = GetSettingsPath();
        if (!File.Exists(path))
            return;

        string json = File.ReadAllText(path);
        var settings = JsonSerializer.Deserialize<AppSettings>(json);
        if (settings == null)
            return;

        ConvertCore.g_indevice     = settings.InputDevice;
        ConvertCore.g_incrypt      = settings.InputCrypt;
        ConvertCore.g_outdevice    = settings.OutputDevice;
        ConvertCore.g_outcrypt     = settings.OutputCrypt;
        ConvertCore.g_verifiermode = settings.VerifierMode;
		_outputAsPnachRaw      = settings.OutputPnachRaw;   // NEW
        ConvertCore.g_region       = settings.Region;
        ConvertCore.g_makefolders  = settings.MakeFolders;
        ConvertCore.g_hashdrive    = settings.HashDrive;
        ConvertCore.g_gs3key       = settings.Gs3Key;
		ConvertCore.g_cbcSaveVersion = settings.CbcSaveVersion == 8 ? 8 : 7;
    }
    catch
    {
        // Ignore corrupt/invalid settings and fall back to defaults.
    }
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

private void SaveAppSettings()
{
    try
    {
        var settings = new AppSettings
        {
            InputDevice  = ConvertCore.g_indevice,
            InputCrypt   = ConvertCore.g_incrypt,
            OutputDevice = ConvertCore.g_outdevice,
            OutputCrypt  = ConvertCore.g_outcrypt,
			OutputPnachRaw = _outputAsPnachRaw,     // NEW
            VerifierMode = ConvertCore.g_verifiermode,
            Region       = ConvertCore.g_region,
            MakeFolders  = ConvertCore.g_makefolders,
            HashDrive    = ConvertCore.g_hashdrive,
            Gs3Key       = ConvertCore.g_gs3key,
			CbcSaveVersion = ConvertCore.g_cbcSaveVersion
        };

        var options = new JsonSerializerOptions { WriteIndented = true };
        string json = JsonSerializer.Serialize(settings, options);

        var path = GetSettingsPath();
        File.WriteAllText(path, json);
    }
    catch
    {
        // Best-effort only; ignore IO/serialization errors.
    }
}

// Map (Device, Crypt) -> CryptOption, or fall back if not found.
private CryptOption FindCryptOption(ConvertCore.Device device,
                                    ConvertCore.Crypt crypt,
                                    CryptOption fallback)
{
    CryptOption[][] allGroups =
    {
        Options_Unencrypted,
        Options_ActionReplay,
        Options_CodeBreaker,
        Options_GameShark_Interact,
        Options_GameShark_MadCatz,
        Options_SwapMagic,
        Options_Xploder
    };

    foreach (var group in allGroups)
    {
        foreach (var opt in group)
        {
            if (opt.Device == device && opt.Crypt == crypt)
                return opt;
        }
    }

    return fallback;
}

        // ---------------------------------------------------------------------
        // Menu construction / selection
        // ---------------------------------------------------------------------

        private void BuildCryptMenu(ToolStripMenuItem root, bool isInput)
        {
            root.DropDownItems.Clear();

            // Unencrypted >
            var unenc = new ToolStripMenuItem("Unencrypted");
    foreach (var opt in Options_Unencrypted)
        AddCryptMenuItem(unenc, opt, isInput);
    root.DropDownItems.Add(unenc);

            // Action Replay >
            var ar = new ToolStripMenuItem("Action Replay");
            foreach (var opt in Options_ActionReplay)
                AddCryptMenuItem(ar, opt, isInput);
            root.DropDownItems.Add(ar);

            // CodeBreaker >
            var cb = new ToolStripMenuItem("CodeBreaker");
            foreach (var opt in Options_CodeBreaker)
                AddCryptMenuItem(cb, opt, isInput);
            root.DropDownItems.Add(cb);

            // GameShark > Interact / MadCatz
            var gs = new ToolStripMenuItem("GameShark");

            var gsInteract = new ToolStripMenuItem("Interact");
            foreach (var opt in Options_GameShark_Interact)
                AddCryptMenuItem(gsInteract, opt, isInput);
            gs.DropDownItems.Add(gsInteract);

            var gsMad = new ToolStripMenuItem("MadCatz");
            foreach (var opt in Options_GameShark_MadCatz)
                AddCryptMenuItem(gsMad, opt, isInput);
            gs.DropDownItems.Add(gsMad);

            root.DropDownItems.Add(gs);

            // Swap Magic >
            var sm = new ToolStripMenuItem("Swap Magic");
            foreach (var opt in Options_SwapMagic)
                AddCryptMenuItem(sm, opt, isInput);
            root.DropDownItems.Add(sm);

            // Xploder >
            var xp = new ToolStripMenuItem("Xploder");
            foreach (var opt in Options_Xploder)
                AddCryptMenuItem(xp, opt, isInput);
            root.DropDownItems.Add(xp);
        }

        private void AddCryptMenuItem(ToolStripMenuItem parent, CryptOption opt, bool isInput)
        {
            var mi = new ToolStripMenuItem(opt.Text);
            mi.Tag = new Tuple<bool, CryptOption>(isInput, opt);
            mi.Click += CryptMenuItem_Click;
            parent.DropDownItems.Add(mi);
        }

        private void SetCryptSelection(bool isInput, CryptOption opt)
        {
            if (isInput)
            {
                ConvertCore.g_indevice = opt.Device;
                ConvertCore.g_incrypt  = opt.Crypt;
                lblInputFormat.Text    = "Input: " + opt.Text;
				_inputAsPnachRaw      = opt.UsePnachFormat;    // NEW
            }
            else
            {
                ConvertCore.g_outdevice = opt.Device;
                ConvertCore.g_outcrypt  = opt.Crypt;
                lblOutputFormat.Text    = "Output: " + opt.Text;
				_outputAsPnachRaw      = opt.UsePnachFormat;   // NEW
				
				chkPnachCrcActive.Visible = _outputAsPnachRaw;
				chkPnachCrcActive.Checked = _outputAsPnachRaw && _pnachCrcActive && _pnachCrc.HasValue;
            }
        }
		protected override void OnFormClosing(FormClosingEventArgs e)
{
    // Save settings before closing
    SaveAppSettings();
    base.OnFormClosing(e);
}
    }
}
