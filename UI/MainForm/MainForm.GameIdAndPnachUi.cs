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
        private void SyncGameIdFromGlobals()
        {
            if (_updatingGameIdFromUi)
                return;

            string hex = ConvertCore.g_gameid.ToString("X4");

            _updatingGameIdFromUi = true;
            try
            {
                if (txtGameId != null)
                    txtGameId.Text = hex;
                if (txtGameIdInput != null)
                    txtGameIdInput.Text = hex;
                if (txtGameIdOutput != null)
                    txtGameIdOutput.Text = hex;
            }
            finally
            {
                _updatingGameIdFromUi = false;
            }
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

        private void txtGameId_TextChanged(object? sender, EventArgs e)
        {
            if (_updatingGameIdFromUi)
                return;

            var tb = sender as TextBox;
            if (tb == null)
                return;

            string text = tb.Text.Trim().ToUpperInvariant();
            if (text.Length == 0)
            {
                // User is clearing / hasn’t typed anything yet.
                // Don’t spam zeros into everything.
                return;
            }

            // Allow partial valid hex – if it parses, use it, otherwise ignore.
            if (!uint.TryParse(text, System.Globalization.NumberStyles.HexNumber,
                               System.Globalization.CultureInfo.InvariantCulture,
                               out uint value) ||
                value > 0xFFFFu)
            {
                // Invalid/partial – ignore this change.
                return;
            }

            _updatingGameIdFromUi = true;
            try
            {
                // Update global
                ConvertCore.g_gameid = value;

                // Propagate the *same raw text* (NOT padded) to the other boxes
                if (!ReferenceEquals(tb, txtGameId) && txtGameId != null && txtGameId.Text != text)
                    txtGameId.Text = text;

                if (!ReferenceEquals(tb, txtGameIdInput) && txtGameIdInput != null && txtGameIdInput.Text != text)
                    txtGameIdInput.Text = text;

                if (!ReferenceEquals(tb, txtGameIdOutput) && txtGameIdOutput != null && txtGameIdOutput.Text != text)
                    txtGameIdOutput.Text = text;
            }
            finally
            {
                _updatingGameIdFromUi = false;
            }
        }

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

        private void RefreshArmaxGameIdDisplay()
        {
            if (lblGameIdInput == null || txtGameIdInput == null ||
                lblGameIdOutput == null || txtGameIdOutput == null)
                return;

            bool inputIsArmax  = (ConvertCore.g_indevice == ConvertCore.Device.DEV_ARMAX);
            bool outputIsArmax = (ConvertCore.g_outdevice == ConvertCore.Device.DEV_ARMAX);

            // Input side
            lblGameIdInput.Visible = inputIsArmax;
            txtGameIdInput.Visible = inputIsArmax;

            // Output side
            lblGameIdOutput.Visible = outputIsArmax;
            txtGameIdOutput.Visible = outputIsArmax;

            // PNACH CRC checkbox only matters for PNACH RAW output,
            // so hide it when ARMAX is active on the output side.
            if (ConvertCore.g_outdevice == ConvertCore.Device.DEV_ARMAX)
            {
                chkPnachCrcActive.Visible = false;
            }
            else
            {
                // Leave PNACH logic alone; it will be toggled by SetCryptSelection
                // based on _outputAsPnachRaw / _pnachCrcActive.
            }
        }

    }
}
