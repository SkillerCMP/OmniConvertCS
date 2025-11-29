using System.Drawing;
using System.Windows.Forms;

namespace OmniconvertCS.Gui
{
    partial class Ar2KeyForm
    {
        private System.ComponentModel.IContainer components = null;
        private TextBox txtKey;
        private Button btnOk;
        private Button btnReset;
		private ComboBox cmbCommonKeys;
		private Label lblCommonKeys;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        private void InitializeComponent()
{
    this.components    = new System.ComponentModel.Container();
    this.cmbCommonKeys = new ComboBox();
	this.lblCommonKeys = new Label();
    this.txtKey        = new TextBox();
    this.btnOk         = new Button();
    this.btnReset      = new Button();

    // cmbCommonKeys
    this.cmbCommonKeys.DropDownStyle     = ComboBoxStyle.DropDownList;
    this.cmbCommonKeys.FormattingEnabled = true;
    this.cmbCommonKeys.Items.AddRange(new object[] {
        "1853E59E",
        "1645EBB3",
        "1746EAAD"
    });
    this.cmbCommonKeys.Location          = new Point(12, 12);
    this.cmbCommonKeys.Name              = "cmbCommonKeys";
    this.cmbCommonKeys.Size              = new Size(120, 21);
    this.cmbCommonKeys.TabIndex          = 0;
    this.cmbCommonKeys.SelectedIndexChanged +=
        new System.EventHandler(this.cmbCommonKeys_SelectedIndexChanged);


// lblCommonKeys
this.lblCommonKeys.AutoSize = true;
this.lblCommonKeys.Location = new System.Drawing.Point(140, 15);
this.lblCommonKeys.Name = "lblCommonKeys";
this.lblCommonKeys.Size = new System.Drawing.Size(100, 13);
this.lblCommonKeys.TabIndex = 1;
this.lblCommonKeys.Text = "Common AR2 Keys:";


    // txtKey
    this.txtKey.Location        = new Point(12, 43);
    this.txtKey.MaxLength       = 8;
    this.txtKey.Name            = "txtKey";
    this.txtKey.Size            = new Size(80, 20);
    this.txtKey.TabIndex        = 1;
    this.txtKey.CharacterCasing = CharacterCasing.Upper;
    this.txtKey.KeyPress       += new KeyPressEventHandler(this.txtKey_KeyPress);

    // btnOk
    this.btnOk.Location  = new Point(100, 42);
    this.btnOk.Name      = "btnOk";
    this.btnOk.Size      = new Size(60, 23);
    this.btnOk.TabIndex  = 2;
    this.btnOk.Text      = "OK";
    this.btnOk.UseVisualStyleBackColor = true;
    this.btnOk.Click    += new System.EventHandler(this.btnOk_Click);

    // btnReset
    this.btnReset.Location  = new Point(166, 42);
    this.btnReset.Name      = "btnReset";
    this.btnReset.Size      = new Size(100, 23);
    this.btnReset.TabIndex  = 3;
    this.btnReset.Text      = "RESET (AR1 Key)";
    this.btnReset.UseVisualStyleBackColor = true;
    this.btnReset.Click    += new System.EventHandler(this.btnReset_Click);

    // Ar2KeyForm
    this.AutoScaleMode   = AutoScaleMode.Font;
    this.ClientSize      = new Size(280, 80);
    this.Controls.Add(this.cmbCommonKeys);
	this.Controls.Add(this.lblCommonKeys);
    this.Controls.Add(this.txtKey);
    this.Controls.Add(this.btnOk);
    this.Controls.Add(this.btnReset);
    this.FormBorderStyle = FormBorderStyle.FixedDialog;
    this.MaximizeBox     = false;
    this.MinimizeBox     = false;
    this.Name            = "Ar2KeyForm";
    this.ShowInTaskbar   = false;
    this.StartPosition   = FormStartPosition.CenterParent;
    this.Text            = "Enter Encrypted AR2 Key Code";
    this.AcceptButton    = this.btnOk;
}

    }
}
