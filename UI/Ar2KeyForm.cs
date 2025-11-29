using System;
using System.Globalization;
using System.Windows.Forms;

namespace OmniconvertCS.Gui
{
    public partial class Ar2KeyForm : Form
    {
        public Ar2KeyForm()
        {
            InitializeComponent();
            InitializeFromCurrentSeed();
        }

        // On open: show the encrypted AR2 key based on current ar2seeds,
        // exactly like the C code:
        //   key = ar2encrypt(ar2seeds, tseed[1], tseed[0]);
        private void InitializeFromCurrentSeed()
        {
            uint ar1seed = Ar2.AR1_SEED;
            byte[] tseed = BitConverter.GetBytes(ar1seed); // little-endian

            uint key = Ar2.Ar2Encrypt(ConvertCore.ar2seeds, tseed[1], tseed[0]);
            txtKey.Text = key.ToString("X8");
            txtKey.SelectionStart = txtKey.TextLength;
			
			// If the current key is one of the presets, select it in the dropdown
			if (cmbCommonKeys != null)
			{
				int idx = cmbCommonKeys.FindStringExact(txtKey.Text);
				cmbCommonKeys.SelectedIndex = idx;
			}
        }
private void cmbCommonKeys_SelectedIndexChanged(object? sender, EventArgs e)
{
    if (cmbCommonKeys.SelectedItem is string key && !string.IsNullOrWhiteSpace(key))
    {
        txtKey.Text = key;
        txtKey.SelectionStart = txtKey.TextLength;
        txtKey.Focus();
    }
}

        private void btnOk_Click(object? sender, EventArgs e)
        {
            string text = txtKey.Text.Trim();

            if (text.Length != 8 ||
                !uint.TryParse(text, NumberStyles.HexNumber,
                               CultureInfo.InvariantCulture, out uint key))
            {
                MessageBox.Show(this,
                    "Please enter exactly 8 hexadecimal digits (0–9, A–F).",
                    "Invalid AR2 key code",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);

                txtKey.Focus();
                txtKey.SelectAll();
                return;
            }

            // Mirror C:
            //   ar2seeds = ar2decrypt(key, tseed[1], tseed[0]);
            uint ar1seed = Ar2.AR1_SEED;
            byte[] tseed = BitConverter.GetBytes(ar1seed);
            ConvertCore.ar2seeds = Ar2.Ar2Decrypt(key, tseed[1], tseed[0]);

            // Actual AR2 seed will be applied at Convert time via DecryptCode().
            DialogResult = DialogResult.OK;
            Close();
        }

        private void btnReset_Click(object? sender, EventArgs e)
        {
            // Same as C: ar2seeds = AR1_SEED;
            ConvertCore.ar2seeds = Ar2.AR1_SEED;
            DialogResult = DialogResult.OK;
            Close();
        }

        // Only allow hex chars in the box
        private void txtKey_KeyPress(object? sender, KeyPressEventArgs e)
        {
            if (char.IsControl(e.KeyChar))
                return;

            char c = char.ToUpperInvariant(e.KeyChar);
            bool isHex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');

            if (!isHex)
                e.Handled = true;
        }
    }
}
