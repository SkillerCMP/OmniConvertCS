using System.Drawing;
using System.Windows.Forms;

namespace OmniconvertCS.Gui
{
    internal partial class ArmaxOptionsForm
    {
        private System.ComponentModel.IContainer components = null;
        private Label lblGameId;
        private TextBox txtGameId;

        private GroupBox grpVerifiers;
        private RadioButton radioVerifiersAuto;
        private RadioButton radioVerifiersManual;

        private GroupBox grpRegion;
        private RadioButton radioRegionUsa;
        private RadioButton radioRegionPal;
        private RadioButton radioRegionJapan;

        private GroupBox grpDiscHash;
        private ComboBox comboDiscHash;

        private Button btnOk;
        private Button btnCancel;

        protected override void Dispose(bool disposing)
        {
            if (disposing && components != null)
                components.Dispose();
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();

            this.lblGameId = new Label();
            this.txtGameId = new TextBox();

            this.grpVerifiers = new GroupBox();
            this.radioVerifiersAuto = new RadioButton();
            this.radioVerifiersManual = new RadioButton();

            this.grpRegion = new GroupBox();
            this.radioRegionUsa = new RadioButton();
            this.radioRegionPal = new RadioButton();
            this.radioRegionJapan = new RadioButton();

            this.grpDiscHash = new GroupBox();
            this.comboDiscHash = new ComboBox();

            this.btnOk = new Button();
            this.btnCancel = new Button();

            // 
            // lblGameId
            // 
            this.lblGameId.AutoSize = true;
            this.lblGameId.Location = new Point(12, 15);
            this.lblGameId.Name = "lblGameId";
            this.lblGameId.Size = new Size(58, 13);
            this.lblGameId.TabIndex = 0;
            this.lblGameId.Text = "Game ID:";
            // 
            // txtGameId
            // 
            this.txtGameId.Location = new Point(90, 12);
            this.txtGameId.Name = "txtGameId";
            this.txtGameId.Size = new Size(60, 20);
            this.txtGameId.TabIndex = 1;
            this.txtGameId.KeyPress += new KeyPressEventHandler(this.txtGameId_KeyPress);

            // 
            // grpVerifiers
            // 
            this.grpVerifiers.Controls.Add(this.radioVerifiersAuto);
            this.grpVerifiers.Controls.Add(this.radioVerifiersManual);
            this.grpVerifiers.Location = new Point(15, 45);
            this.grpVerifiers.Name = "grpVerifiers";
            this.grpVerifiers.Size = new Size(200, 70);
            this.grpVerifiers.TabIndex = 2;
            this.grpVerifiers.TabStop = false;
            this.grpVerifiers.Text = "AR MAX Verifiers";

            // 
            // radioVerifiersAuto
            // 
            this.radioVerifiersAuto.AutoSize = true;
            this.radioVerifiersAuto.Location = new Point(15, 20);
            this.radioVerifiersAuto.Name = "radioVerifiersAuto";
            this.radioVerifiersAuto.Size = new Size(72, 17);
            this.radioVerifiersAuto.TabIndex = 0;
            this.radioVerifiersAuto.TabStop = true;
            this.radioVerifiersAuto.Text = "Automatic";
            this.radioVerifiersAuto.UseVisualStyleBackColor = true;

            // 
            // radioVerifiersManual
            // 
            this.radioVerifiersManual.AutoSize = true;
            this.radioVerifiersManual.Location = new Point(15, 40);
            this.radioVerifiersManual.Name = "radioVerifiersManual";
            this.radioVerifiersManual.Size = new Size(60, 17);
            this.radioVerifiersManual.TabIndex = 1;
            this.radioVerifiersManual.TabStop = true;
            this.radioVerifiersManual.Text = "Manual";
            this.radioVerifiersManual.UseVisualStyleBackColor = true;

            // 
            // grpRegion
            // 
            this.grpRegion.Controls.Add(this.radioRegionUsa);
            this.grpRegion.Controls.Add(this.radioRegionPal);
            this.grpRegion.Controls.Add(this.radioRegionJapan);
            this.grpRegion.Location = new Point(230, 45);
            this.grpRegion.Name = "grpRegion";
            this.grpRegion.Size = new Size(180, 90);
            this.grpRegion.TabIndex = 3;
            this.grpRegion.TabStop = false;
            this.grpRegion.Text = "AR MAX Region";

            // 
            // radioRegionUsa
            // 
            this.radioRegionUsa.AutoSize = true;
            this.radioRegionUsa.Location = new Point(15, 20);
            this.radioRegionUsa.Name = "radioRegionUsa";
            this.radioRegionUsa.Size = new Size(47, 17);
            this.radioRegionUsa.TabIndex = 0;
            this.radioRegionUsa.TabStop = true;
            this.radioRegionUsa.Text = "USA";
            this.radioRegionUsa.UseVisualStyleBackColor = true;

            // 
            // radioRegionPal
            // 
            this.radioRegionPal.AutoSize = true;
            this.radioRegionPal.Location = new Point(15, 40);
            this.radioRegionPal.Name = "radioRegionPal";
            this.radioRegionPal.Size = new Size(45, 17);
            this.radioRegionPal.TabIndex = 1;
            this.radioRegionPal.TabStop = true;
            this.radioRegionPal.Text = "PAL";
            this.radioRegionPal.UseVisualStyleBackColor = true;

            // 
            // radioRegionJapan
            // 
            this.radioRegionJapan.AutoSize = true;
            this.radioRegionJapan.Location = new Point(15, 60);
            this.radioRegionJapan.Name = "radioRegionJapan";
            this.radioRegionJapan.Size = new Size(54, 17);
            this.radioRegionJapan.TabIndex = 2;
            this.radioRegionJapan.TabStop = true;
            this.radioRegionJapan.Text = "Japan";
            this.radioRegionJapan.UseVisualStyleBackColor = true;

            // 
            // grpDiscHash
            // 
            this.grpDiscHash.Controls.Add(this.comboDiscHash);
            this.grpDiscHash.Location = new Point(15, 130);
            this.grpDiscHash.Name = "grpDiscHash";
            this.grpDiscHash.Size = new Size(395, 60);
            this.grpDiscHash.TabIndex = 4;
            this.grpDiscHash.TabStop = false;
            this.grpDiscHash.Text = "AR MAX Disc Hash";

            // 
            // comboDiscHash
            // 
            this.comboDiscHash.DropDownStyle = ComboBoxStyle.DropDownList;
            this.comboDiscHash.FormattingEnabled = true;
            this.comboDiscHash.Location = new Point(15, 25);
            this.comboDiscHash.Name = "comboDiscHash";
            this.comboDiscHash.Size = new Size(360, 21);
            this.comboDiscHash.TabIndex = 0;

            // 
            // btnOk
            // 
            this.btnOk.Location = new Point(254, 205);
            this.btnOk.Name = "btnOk";
            this.btnOk.Size = new Size(75, 23);
            this.btnOk.TabIndex = 5;
            this.btnOk.Text = "OK";
            this.btnOk.UseVisualStyleBackColor = true;
            this.btnOk.Click += new System.EventHandler(this.btnOk_Click);

            // 
            // btnCancel
            // 
            this.btnCancel.Location = new Point(335, 205);
            this.btnCancel.Name = "btnCancel";
            this.btnCancel.Size = new Size(75, 23);
            this.btnCancel.TabIndex = 6;
            this.btnCancel.Text = "Cancel";
            this.btnCancel.UseVisualStyleBackColor = true;
            this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);

            // 
            // ArmaxOptionsForm
            // 
            this.AutoScaleMode = AutoScaleMode.Font;
            this.ClientSize = new Size(430, 240);
            this.Controls.Add(this.btnCancel);
            this.Controls.Add(this.btnOk);
            this.Controls.Add(this.grpDiscHash);
            this.Controls.Add(this.grpRegion);
            this.Controls.Add(this.grpVerifiers);
            this.Controls.Add(this.txtGameId);
            this.Controls.Add(this.lblGameId);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "ArmaxOptionsForm";
            this.StartPosition = FormStartPosition.CenterParent;
            this.Text = "ARMAX Options";

            this.grpVerifiers.ResumeLayout(false);
            this.grpVerifiers.PerformLayout();
            this.grpRegion.ResumeLayout(false);
            this.grpRegion.PerformLayout();
            this.grpDiscHash.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();
        }
    }
}
