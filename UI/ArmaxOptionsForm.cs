using System;
using System.Globalization;
using System.IO;
using System.Windows.Forms;

namespace OmniconvertCS.Gui
{
    internal partial class ArmaxOptionsForm : Form
    {
        public uint SelectedGameId { get; private set; }
        public uint SelectedRegion { get; private set; }
        public ConvertCore.VerifierMode SelectedVerifierMode { get; private set; }
        public char SelectedHashDrive { get; private set; }

        private sealed class DiscHashItem
        {
            public char DriveLetter { get; }
            public string Text { get; }

            public DiscHashItem(char driveLetter, string text)
            {
                DriveLetter = driveLetter;
                Text = text;
            }

            public override string ToString() => Text;
        }

        public ArmaxOptionsForm(
            uint currentGameId,
            uint currentRegion,
            ConvertCore.VerifierMode currentVerifierMode,
            char currentHashDrive)
        {
            InitializeComponent();

            // Game ID textbox: 4-digit uppercase hex
            txtGameId.CharacterCasing = CharacterCasing.Upper;
            txtGameId.MaxLength = 4;
            txtGameId.Text = currentGameId.ToString("X4");

            // Verifiers
            switch (currentVerifierMode)
{
    case ConvertCore.VerifierMode.AUTO:
        radioVerifiersAuto.Checked = true;
        break;
    case ConvertCore.VerifierMode.MANUAL:
        radioVerifiersManual.Checked = true;
        break;
    default:
        radioVerifiersAuto.Checked = true;
        break;
}

            // Region (0 = USA, 1 = PAL, 2 = Japan – matches existing code)
            switch (currentRegion)
            {
                case 0:
                    radioRegionUsa.Checked = true;
                    break;
                case 1:
                    radioRegionPal.Checked = true;
                    break;
                case 2:
                    radioRegionJapan.Checked = true;
                    break;
                default:
                    radioRegionUsa.Checked = true;
                    break;
            }

            PopulateDiscHashCombo(currentHashDrive);
        }

        private void PopulateDiscHashCombo(char currentHashDrive)
{
    comboDiscHash.Items.Clear();

    // First, the "Do Not Create" option (same semantics as menuOptionsArmaxDiscHashNone)
    comboDiscHash.Items.Add(new DiscHashItem('\0', "Do Not Create"));

    try
    {
        foreach (var drive in DriveInfo.GetDrives())
        {
            // MATCHES MainForm.EnumerateArmaxDiscHashDrives:
            // only show CD/DVD drives, not every HDD/USB
            if (drive.DriveType != DriveType.CDRom)
                continue;

            string root = drive.Name;   // e.g. "D:\\"
            if (string.IsNullOrEmpty(root))
                continue;

            char letter = char.ToUpperInvariant(root[0]);
            string label = $"Use Drive {root}";

            comboDiscHash.Items.Add(new DiscHashItem(letter, label));
        }
    }
    catch
    {
        // If DriveInfo fails, just leave only "Do Not Create".
    }

    // Select current
    int indexToSelect = 0; // default = "Do Not Create"
    for (int i = 0; i < comboDiscHash.Items.Count; i++)
    {
        if (comboDiscHash.Items[i] is DiscHashItem item && item.DriveLetter == currentHashDrive)
        {
            indexToSelect = i;
            break;
        }
    }

    comboDiscHash.SelectedIndex = indexToSelect;
}
        private void txtGameId_KeyPress(object? sender, KeyPressEventArgs e)
        {
            if (char.IsControl(e.KeyChar))
                return;

            char c = char.ToUpperInvariant(e.KeyChar);
            bool isHex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
            if (!isHex)
                e.Handled = true;
        }

        private void btnOk_Click(object? sender, EventArgs e)
        {
            // Parse Game ID
            string hex = txtGameId.Text.Trim();
            if (string.IsNullOrEmpty(hex) ||
                !uint.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint gameId) ||
                gameId > 0xFFFFu)
            {
                MessageBox.Show(this,
                    "Game ID must be a 4-digit hexadecimal value (0000–FFFF).",
                    "Invalid Game ID",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                txtGameId.Focus();
                txtGameId.SelectAll();
                return;
            }

            SelectedGameId = gameId;

            // Verifiers
            if (radioVerifiersManual.Checked)
    SelectedVerifierMode = ConvertCore.VerifierMode.MANUAL;
else
    SelectedVerifierMode = ConvertCore.VerifierMode.AUTO;

            // Region
            if (radioRegionPal.Checked)
                SelectedRegion = 1;
            else if (radioRegionJapan.Checked)
                SelectedRegion = 2;
            else
                SelectedRegion = 0;

            // Disc hash drive
            SelectedHashDrive = '\0';
            if (comboDiscHash.SelectedItem is DiscHashItem item)
                SelectedHashDrive = item.DriveLetter;

            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private void btnCancel_Click(object? sender, EventArgs e)
        {
            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }
    }
}
