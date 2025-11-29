using System;
using System.Windows.Forms;
using OmniconvertCS;
using System.IO;

namespace OmniconvertCS.Gui
{
    public partial class PnachCrcForm : Form
    {
        public string SelectedCrcHex { get; private set; } = string.Empty;
        public string SelectedGameName { get; private set; } = string.Empty;
		public string SelectedElfName   { get; private set; } = string.Empty;   // NEW

        private bool _hasComputedCrc;

        public PnachCrcForm(string initialCrcHex, string initialGameName)
        {
            InitializeComponent();

            txtCrc.Text = (initialCrcHex ?? string.Empty).Trim();
            txtGameName.Text = (initialGameName ?? string.Empty).Trim();

            // Populate dropdown with existing mappings from PnachCRC.json
            var entries = PnachCrcHelper.GetEntries();
            comboKnown.Items.Clear();
            foreach (var entry in entries)
            {
                comboKnown.Items.Add(entry); // ToString() gives "XXXXXXXX - Name"
            }

            // Try to pre-select the matching entry, if any
            if (!string.IsNullOrEmpty(initialCrcHex) &&
                uint.TryParse(initialCrcHex, System.Globalization.NumberStyles.HexNumber,
                              System.Globalization.CultureInfo.InvariantCulture, out uint crc))
            {
                for (int i = 0; i < comboKnown.Items.Count; i++)
                {
                    if (comboKnown.Items[i] is PnachCrcHelper.CrcEntry ce && ce.Crc == crc)
                    {
                        comboKnown.SelectedIndex = i;
                        break;
                    }
                }
            }
        }

        private void btnBrowseElf_Click(object sender, EventArgs e)
        {
            using (var dlg = new OpenFileDialog())
            {
                dlg.Title = "Select PS2 ELF";
                dlg.Filter =
                    "PS2 ELF and executables (*.ELF;SLUS_*;SLES_*;SCES_*;SCUS_*;SLPS_*;SLPM_*)|" +
                    "*.ELF;SLUS_*;SLES_*;SCES_*;SCUS_*;SLPS_*;SLPM_*|All files (*.*)|*.*";

            if (dlg.ShowDialog(this) != DialogResult.OK)
                return;

            SetElfPathAndCompute(dlg.FileName);
            }
        }

        private void SetElfPathAndCompute(string path)
        {
            if (string.IsNullOrWhiteSpace(path))
                return;

            txtElfPath.Text = path;

            try
            {
                uint crc = PnachCrcHelper.ComputeElfCrc(path);
                string crcHex = crc.ToString("X8");
                txtCrc.Text = crcHex;
                _hasComputedCrc = true;

                var knownName = PnachCrcHelper.TryGetGameName(crc);
                if (!string.IsNullOrEmpty(knownName))
                {
                    txtGameName.Text = knownName;

                    for (int i = 0; i < comboKnown.Items.Count; i++)
                    {
                        if (comboKnown.Items[i] is PnachCrcHelper.CrcEntry entry &&
                            entry.Crc == crc)
                        {
                            comboKnown.SelectedIndex = i;
                            break;
                        }
                    }
                }
                else if (string.IsNullOrWhiteSpace(txtGameName.Text))
                {
                    txtGameName.Focus();
                    txtGameName.SelectAll();
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(this,
                    "Error computing CRC for ELF:\r\n" + ex.Message,
                    "PNACH CRC",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        private void comboKnown_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboKnown.SelectedItem is PnachCrcHelper.CrcEntry entry)
            {
                txtCrc.Text = entry.CrcHex;
                txtGameName.Text = entry.GameName;
                _hasComputedCrc = true;
            }
        }

        private void btnOk_Click(object sender, EventArgs e)
{
    string crcText = txtCrc.Text.Trim();

    if (crcText.Length == 0)
    {
        MessageBox.Show(this,
            "Please compute or select a CRC first.",
            "PNACH CRC",
            MessageBoxButtons.OK,
            MessageBoxIcon.Warning);
        return;
    }

    if (!uint.TryParse(crcText,
                       System.Globalization.NumberStyles.HexNumber,
                       System.Globalization.CultureInfo.InvariantCulture,
                       out uint crc))
    {
        MessageBox.Show(this,
            "CRC must be 8-digit hexadecimal.",
            "PNACH CRC",
            MessageBoxButtons.OK,
            MessageBoxIcon.Warning);
        txtCrc.Focus();
        txtCrc.SelectAll();
        return;
    }

    string name = txtGameName.Text.Trim();
    if (name.Length == 0)
    {
        MessageBox.Show(this,
            "Please enter a game name.",
            "PNACH CRC",
            MessageBoxButtons.OK,
            MessageBoxIcon.Warning);
        txtGameName.Focus();
        return;
    }

    // Work out the ELF name we want to store:
    //  1) If the selected entry in the dropdown has an ELF name, use that.
    //  2) Otherwise, fall back to the file name from the drop/browse path.
    string elfName = string.Empty;

    if (comboKnown.SelectedItem is PnachCrcHelper.CrcEntry selectedEntry &&
        !string.IsNullOrWhiteSpace(selectedEntry.ElfName))
    {
        elfName = selectedEntry.ElfName;
    }
    else if (!string.IsNullOrWhiteSpace(txtElfPath.Text))
    {
        try
        {
            elfName = Path.GetFileName(txtElfPath.Text);
        }
        catch
        {
            elfName = txtElfPath.Text.Trim(); // fallback, shouldn't really happen
        }
    }

    try
    {
        PnachCrcHelper.AddOrUpdate(crc, name, elfName);
    }
    catch (Exception ex)
    {
        MessageBox.Show(this,
            "Error saving PnachCRC.json:\r\n" + ex.Message,
            "PNACH CRC",
            MessageBoxButtons.OK,
            MessageBoxIcon.Error);
        // Still continue and return the values if we got here.
    }

    SelectedCrcHex   = crcText;
    SelectedGameName = name;
    SelectedElfName  = elfName;

    DialogResult = DialogResult.OK;
    Close();
}

        private void btnCancel_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
            Close();
        }

        // Drag & drop for the ELF
        private void PnachCrcForm_DragEnter(object sender, DragEventArgs e)
        {
            if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
            {
                e.Effect = DragDropEffects.Copy;
            }
        }

        private void PnachCrcForm_DragDrop(object sender, DragEventArgs e)
        {
            if (e.Data?.GetData(DataFormats.FileDrop) is string[] paths &&
                paths.Length > 0)
            {
                SetElfPathAndCompute(paths[0]);
            }
        }
    }
}
