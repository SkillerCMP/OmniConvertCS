using System.Windows.Forms;

namespace OmniconvertCS.Gui
{
    partial class PnachCrcForm
    {
        private System.ComponentModel.IContainer components = null;
        private Label lblElf;
        private TextBox txtElfPath;
        private Button btnBrowseElf;
        private Label lblCrc;
        private TextBox txtCrc;
        private Label lblGameName;
        private TextBox txtGameName;
        private Label lblKnown;
        private ComboBox comboKnown;
        private Button btnOk;
        private Button btnCancel;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
                components.Dispose();
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            this.lblElf = new System.Windows.Forms.Label();
            this.txtElfPath = new System.Windows.Forms.TextBox();
            this.btnBrowseElf = new System.Windows.Forms.Button();
            this.lblCrc = new System.Windows.Forms.Label();
            this.txtCrc = new System.Windows.Forms.TextBox();
            this.lblGameName = new System.Windows.Forms.Label();
            this.txtGameName = new System.Windows.Forms.TextBox();
            this.lblKnown = new System.Windows.Forms.Label();
            this.comboKnown = new System.Windows.Forms.ComboBox();
            this.btnOk = new System.Windows.Forms.Button();
            this.btnCancel = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // lblElf
            // 
            this.lblElf.AutoSize = true;
            this.lblElf.Location = new System.Drawing.Point(12, 15);
            this.lblElf.Name = "lblElf";
            this.lblElf.Size = new System.Drawing.Size(150, 13);
            this.lblElf.TabIndex = 0;
            this.lblElf.Text = "Drop ELF here or click Browse:";
            // 
            // txtElfPath
            // 
            this.txtElfPath.Location = new System.Drawing.Point(15, 31);
            this.txtElfPath.Name = "txtElfPath";
            this.txtElfPath.ReadOnly = true;
            this.txtElfPath.Size = new System.Drawing.Size(320, 20);
            this.txtElfPath.TabIndex = 1;
            this.txtElfPath.TabStop = false;
            // 
            // btnBrowseElf
            // 
            this.btnBrowseElf.Location = new System.Drawing.Point(341, 29);
            this.btnBrowseElf.Name = "btnBrowseElf";
            this.btnBrowseElf.Size = new System.Drawing.Size(75, 23);
            this.btnBrowseElf.TabIndex = 2;
            this.btnBrowseElf.Text = "Browse...";
            this.btnBrowseElf.UseVisualStyleBackColor = true;
            this.btnBrowseElf.Click += new System.EventHandler(this.btnBrowseElf_Click);
            // 
            // lblCrc
            // 
            this.lblCrc.AutoSize = true;
            this.lblCrc.Location = new System.Drawing.Point(12, 64);
            this.lblCrc.Name = "lblCrc";
            this.lblCrc.Size = new System.Drawing.Size(31, 13);
            this.lblCrc.TabIndex = 3;
            this.lblCrc.Text = "CRC:";
            // 
            // txtCrc
            // 
            this.txtCrc.CharacterCasing = System.Windows.Forms.CharacterCasing.Upper;
            this.txtCrc.Location = new System.Drawing.Point(80, 61);
            this.txtCrc.MaxLength = 8;
            this.txtCrc.Name = "txtCrc";
            this.txtCrc.Size = new System.Drawing.Size(100, 20);
            this.txtCrc.TabIndex = 4;
            // 
            // lblGameName
            // 
            this.lblGameName.AutoSize = true;
            this.lblGameName.Location = new System.Drawing.Point(12, 93);
            this.lblGameName.Name = "lblGameName";
            this.lblGameName.Size = new System.Drawing.Size(69, 13);
            this.lblGameName.TabIndex = 5;
            this.lblGameName.Text = "Game name:";
            // 
            // txtGameName
            // 
            this.txtGameName.Location = new System.Drawing.Point(80, 90);
            this.txtGameName.Name = "txtGameName";
            this.txtGameName.Size = new System.Drawing.Size(336, 20);
            this.txtGameName.TabIndex = 6;
            // 
            // lblKnown
            // 
            this.lblKnown.AutoSize = true;
            this.lblKnown.Location = new System.Drawing.Point(12, 123);
            this.lblKnown.Name = "lblKnown";
            this.lblKnown.Size = new System.Drawing.Size(89, 13);
            this.lblKnown.TabIndex = 7;
            this.lblKnown.Text = "Known mappings:";
            // 
            // comboKnown
            // 
            this.comboKnown.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboKnown.FormattingEnabled = true;
            this.comboKnown.Location = new System.Drawing.Point(15, 139);
            this.comboKnown.Name = "comboKnown";
            this.comboKnown.Size = new System.Drawing.Size(401, 21);
            this.comboKnown.TabIndex = 8;
            this.comboKnown.SelectedIndexChanged += new System.EventHandler(this.comboKnown_SelectedIndexChanged);
            // 
            // btnOk
            // 
            this.btnOk.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.btnOk.Location = new System.Drawing.Point(260, 176);
            this.btnOk.Name = "btnOk";
            this.btnOk.Size = new System.Drawing.Size(75, 23);
            this.btnOk.TabIndex = 9;
            this.btnOk.Text = "OK";
            this.btnOk.UseVisualStyleBackColor = true;
            this.btnOk.Click += new System.EventHandler(this.btnOk_Click);
            // 
            // btnCancel
            // 
            this.btnCancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.btnCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.btnCancel.Location = new System.Drawing.Point(341, 176);
            this.btnCancel.Name = "btnCancel";
            this.btnCancel.Size = new System.Drawing.Size(75, 23);
            this.btnCancel.TabIndex = 10;
            this.btnCancel.Text = "Cancel";
            this.btnCancel.UseVisualStyleBackColor = true;
            this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);
            // 
            // PnachCrcForm
            // 
            this.AcceptButton = this.btnOk;
            this.AllowDrop = true;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.btnCancel;
            this.ClientSize = new System.Drawing.Size(428, 211);
            this.Controls.Add(this.btnCancel);
            this.Controls.Add(this.btnOk);
            this.Controls.Add(this.comboKnown);
            this.Controls.Add(this.lblKnown);
            this.Controls.Add(this.txtGameName);
            this.Controls.Add(this.lblGameName);
            this.Controls.Add(this.txtCrc);
            this.Controls.Add(this.lblCrc);
            this.Controls.Add(this.btnBrowseElf);
            this.Controls.Add(this.txtElfPath);
            this.Controls.Add(this.lblElf);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "PnachCrcForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Set PNACH CRC";
            this.DragEnter += new System.Windows.Forms.DragEventHandler(this.PnachCrcForm_DragEnter);
            this.DragDrop += new System.Windows.Forms.DragEventHandler(this.PnachCrcForm_DragDrop);
            this.ResumeLayout(false);
            this.PerformLayout();
        }
    }
}
