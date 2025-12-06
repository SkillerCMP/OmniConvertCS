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

        private void SwapInputOutput()
        {
            // Determine the current crypt options for input and output
            var defaultOpt      = Options_Unencrypted[0];
            var currentInputOpt = FindCryptOption(ConvertCore.g_indevice,  ConvertCore.g_incrypt,  defaultOpt);
            var currentOutOpt   = FindCryptOption(ConvertCore.g_outdevice, ConvertCore.g_outcrypt, defaultOpt);

            // Apply swapped selections
            SetCryptSelection(true,  currentOutOpt);
            SetCryptSelection(false, currentInputOpt);

            // If there are codes on the Output side, move them to Input
            string outputText = txtOutput.Text;
            if (!string.IsNullOrWhiteSpace(outputText))
            {
                txtInput.Text = outputText;
                txtOutput.Clear();

                // Place caret at the end and keep the view scrolled correctly
                txtInput.SelectionStart  = txtInput.TextLength;
                txtInput.SelectionLength = 0;
                txtInput.ScrollToCaret();
            }

            // Focus the Input box after swapping
            txtInput.Focus();

            // Persist the new selection for next launch
            SaveAppSettings();
        }

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

        private void menuEditSwapInputOutput_Click(object sender, EventArgs e)
        {
            SwapInputOutput();
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

    }
}
