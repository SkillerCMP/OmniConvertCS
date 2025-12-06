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
    public partial class MainForm
    {
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
         {   // 1) Require a successful conversion first
            if (_lastConvertedCheats == null || _lastConvertedCheats.Count == 0)
            {
                MessageBox.Show(
                    this,
                    "No converted cheats are available.\r\nPlease click Convert first.",
                    "Swap Magic Export",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
                return;
            }

            bool swapMagicCompatible =
            ConvertCore.g_outdevice == ConvertCore.Device.DEV_AR1 ||
            ConvertCore.g_outdevice == ConvertCore.Device.DEV_STD;

        if (!swapMagicCompatible)
        {
            MessageBox.Show(
                this,
                "Swap Magic export requires the *output* device to be AR1\\SW or STD(RAW).\r\n" +
                "Please select AR1\\SW or STD(RAW) as the output type, click Convert, then try again.",
                "Swap Magic Export",
                MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
            return;
        }

            // 3) Let the user pick a .bin file name (historically Swap Magic uses .bin)
            using (var dlg = new SaveFileDialog())
            {
                dlg.Title      = "Save As Swap Magic File";
                dlg.Filter     = "Swap Magic files (*.bin)|*.bin|All files (*.*)|*.*";
                dlg.DefaultExt = "bin";

                if (dlg.ShowDialog(this) != DialogResult.OK)
                    return;

                // 4) Build game_t from the last conversion
                string gameName = txtGameName.Text?.Trim() ?? string.Empty;

                var game = new game_t
                {
                    id   = _lastGameId,   // set in btnConvert_Click
                    name = gameName,
                    cnt  = 0              // not used by SCF writer but kept for parity
                };

                // 5) Call the Swap Magic writer
                int ret = SwapMagicWriter.CreateFile(_lastConvertedCheats, game, dlg.FileName);

                if (ret != 0)
                {
                    // SwapMagicWriter already showed specific MsgBox-es for
                    // "no master code", "too many master codes", etc.
                    // This is just a generic "something went wrong" wrapper.
                    MessageBox.Show(
                        this,
                        "An error occurred while creating the Swap Magic file.",
                        "Swap Magic Export",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                }
            }
        }

    }
}
