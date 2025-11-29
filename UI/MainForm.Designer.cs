using System.Drawing;
using System.Windows.Forms;

namespace OmniconvertCS.Gui
{
    partial class MainForm
    {
        private System.ComponentModel.IContainer components = null;
        private MenuStrip menuStrip;
        private ToolStripMenuItem menuFile;
        private ToolStripMenuItem menuEdit;
        private ToolStripMenuItem menuInput;
        private ToolStripMenuItem menuOutput;
        private ToolStripMenuItem menuOptions;
		private ToolStripMenuItem menuOptionsAr2Key;
		private ToolStripMenuItem menuOptionsPnachCrc;
		private ToolStripMenuItem menuOptionsMakeOrganizers;
		private ToolStripMenuItem menuOptionsArmaxDiscHash;
		private ToolStripMenuItem menuOptionsArmaxDiscHashNone;
		private ToolStripSeparator menuOptionsSeparatorAfterOrganizers;
		private ToolStripSeparator menuOptionsSeparatorAfterDiscHash;
		private ToolStripMenuItem menuOptionsArmaxVerifiers;
        private ToolStripMenuItem menuOptionsArmaxVerifiersAuto;
        private ToolStripMenuItem menuOptionsArmaxVerifiersManual;
        private ToolStripMenuItem menuOptionsArmaxRegion;
        private ToolStripMenuItem menuOptionsArmaxRegionUsa;
        private ToolStripMenuItem menuOptionsArmaxRegionPal;
        private ToolStripMenuItem menuOptionsArmaxRegionJapan;
        private ToolStripSeparator menuOptionsSeparatorArmax;
		private ToolStripMenuItem menuOptionsCbcSaveVersion;
		private ToolStripMenuItem menuOptionsCbcVersion7;
		private ToolStripMenuItem menuOptionsCbcVersion8;
		private ToolStripMenuItem menuOptionsGs3Key;
		private ToolStripMenuItem menuOptionsGs3Key0;
		private ToolStripMenuItem menuOptionsGs3Key1;
		private ToolStripMenuItem menuOptionsGs3Key2;
		private ToolStripMenuItem menuOptionsGs3Key3;
		private ToolStripMenuItem menuOptionsGs3Key4;
		private ToolStripMenuItem menuFileLoad;
		private ToolStripMenuItem menuFileLoadText;
		private ToolStripMenuItem menuFileLoadCbc;
		private ToolStripMenuItem menuFileLoadArmaxBin;
		private ToolStripMenuItem menuFileLoadP2m;
		private ToolStripMenuItem menuFileSaveAsText;
		private ToolStripMenuItem menuFileSaveAsPnach;     // NEW
		private ToolStripMenuItem menuFileSaveAsArmaxBin;
		private ToolStripMenuItem menuFileSaveAsCbc;
		private ToolStripMenuItem menuFileSaveAsP2m;
		private ToolStripMenuItem menuFileSaveAsSwapBin;
		private ToolStripSeparator menuFileSeparator;
		private ToolStripMenuItem menuFileSaveAs;
		private ToolStripMenuItem menuEditUndo;
		private ToolStripMenuItem menuEditCut;
		private ToolStripMenuItem menuEditCopy;
		private ToolStripMenuItem menuEditPaste;
		private ToolStripMenuItem menuEditSelectAll;
		private ToolStripMenuItem menuEditClearInput;
		private ToolStripMenuItem menuEditClearOutput;


        private Label   lblInputFormat;
        private Button  btnClearInput;
        private TextBox txtInput;

        private Label   lblOutputFormat;
        private Button  btnClearOutput;
		private CheckBox chkPnachCrcActive;   // NEW
        private TextBox txtOutput;

        private Button  btnConvert;
        private Label   lblGameId;
        internal TextBox txtGameId;
        private Label   lblGameName;
        internal TextBox txtGameName;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
                components.Dispose();
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            this.components    = new System.ComponentModel.Container();
            this.menuStrip     = new MenuStrip();
            this.menuFile      = new ToolStripMenuItem();
            this.menuEdit      = new ToolStripMenuItem();
            this.menuInput     = new ToolStripMenuItem();
            this.menuOutput    = new ToolStripMenuItem();
            this.menuOptions   = new ToolStripMenuItem();
			// File menu sub-items
			// Load Items
			this.menuFileLoad = new ToolStripMenuItem();
			this.menuFileLoadText      = new ToolStripMenuItem();
			this.menuFileLoadCbc        = new ToolStripMenuItem();
			this.menuFileLoadArmaxBin   = new ToolStripMenuItem();
			this.menuFileLoadP2m        = new ToolStripMenuItem();
			//SaveAs Items
			this.menuFileSaveAsText    = new ToolStripMenuItem();
			this.menuFileSaveAsPnach    = new ToolStripMenuItem();   // NEW
			this.menuFileSaveAsArmaxBin = new ToolStripMenuItem();
			this.menuFileSaveAsCbc     = new ToolStripMenuItem();
			this.menuFileSaveAsP2m     = new ToolStripMenuItem();
			this.menuFileSaveAsSwapBin = new ToolStripMenuItem();
			this.menuFileSaveAs         = new ToolStripMenuItem();
			this.menuFileSeparator     = new ToolStripSeparator();
			//Options Menu
			this.menuOptionsAr2Key               = new ToolStripMenuItem();
			this.menuOptionsGs3Key               = new ToolStripMenuItem();
			this.menuOptionsMakeOrganizers      = new ToolStripMenuItem();
			this.menuOptionsArmaxDiscHash       = new ToolStripMenuItem();
			this.menuOptionsArmaxDiscHashNone   = new ToolStripMenuItem();
			this.menuOptionsSeparatorAfterOrganizers = new ToolStripSeparator();
			this.menuOptionsSeparatorAfterDiscHash   = new ToolStripSeparator();
			this.menuOptionsArmaxVerifiers       = new ToolStripMenuItem();
            this.menuOptionsArmaxVerifiersAuto   = new ToolStripMenuItem();
            this.menuOptionsArmaxVerifiersManual = new ToolStripMenuItem();
            this.menuOptionsArmaxRegion          = new ToolStripMenuItem();
            this.menuOptionsArmaxRegionUsa       = new ToolStripMenuItem();
            this.menuOptionsArmaxRegionPal       = new ToolStripMenuItem();
            this.menuOptionsArmaxRegionJapan     = new ToolStripMenuItem();
            this.menuOptionsSeparatorArmax       = new ToolStripSeparator();
			this.menuOptionsGs3Key0              = new ToolStripMenuItem();   // <--
			this.menuOptionsGs3Key1              = new ToolStripMenuItem();
			this.menuOptionsGs3Key2              = new ToolStripMenuItem();
			this.menuOptionsGs3Key3              = new ToolStripMenuItem();
			this.menuOptionsGs3Key4              = new ToolStripMenuItem();
			this.menuOptionsCbcSaveVersion = new ToolStripMenuItem();
			this.menuOptionsCbcVersion7    = new ToolStripMenuItem();
			this.menuOptionsCbcVersion8    = new ToolStripMenuItem();
			//Edit Menu
			
            this.lblInputFormat  = new Label();
            this.btnClearInput   = new Button();
            this.txtInput        = new TextBox();

            this.lblOutputFormat = new Label();
            this.btnClearOutput  = new Button();
            this.txtOutput       = new TextBox();

            this.btnConvert  = new Button();
            this.lblGameId   = new Label();
            this.txtGameId   = new TextBox();
            this.lblGameName = new Label();
            this.txtGameName = new TextBox();

            // menuStrip
            this.menuStrip.Items.AddRange(new ToolStripItem[]
            {
                this.menuFile,
                this.menuEdit,
                this.menuInput,
                this.menuOutput,
                this.menuOptions
            });
            this.menuStrip.Location = new Point(0, 0);
            this.menuStrip.Name     = "menuStrip";
            this.menuStrip.Size     = new Size(661, 24);
            this.menuStrip.TabIndex = 0;
            this.menuStrip.Text     = "menuStrip";

            // menuFile
this.menuFile.Text = "&File";
this.menuFile.DropDownItems.AddRange(new ToolStripItem[]
{
    this.menuFileLoad,
    this.menuFileSeparator,
    this.menuFileSaveAs
});


// menuFileLoad
this.menuFileLoad.Name = "menuFileLoad";
this.menuFileLoad.Size = new System.Drawing.Size(180, 22);
this.menuFileLoad.Text = "Load";
this.menuFileLoad.DropDownItems.AddRange(new ToolStripItem[] {
    this.menuFileLoadText,
    this.menuFileLoadCbc,
    this.menuFileLoadArmaxBin,
    this.menuFileLoadP2m
});

// menuFileSaveAs (parent submenu)
this.menuFileSaveAs.Name = "menuFileSaveAs";
this.menuFileSaveAs.Size = new System.Drawing.Size(210, 22);
this.menuFileSaveAs.Text = "Save &As";

this.menuFileSaveAs.DropDownItems.AddRange(new ToolStripItem[]
{
    this.menuFileSaveAsText,
	this.menuFileSaveAsPnach,      // NEW
    this.menuFileSaveAsArmaxBin,
    this.menuFileSaveAsCbc,
    this.menuFileSaveAsP2m,
    this.menuFileSaveAsSwapBin
});

// menuFileLoadText
this.menuFileLoadText.Name = "menuFileLoadText";
this.menuFileLoadText.Size = new System.Drawing.Size(210, 22);
this.menuFileLoadText.Text = "Text File...";
this.menuFileLoadText.Click += new System.EventHandler(this.menuFileLoadText_Click);

// menuFileLoadCbc
this.menuFileLoadCbc.Name = "menuFileLoadCbc";
this.menuFileLoadCbc.Size = new System.Drawing.Size(210, 22);
this.menuFileLoadCbc.Text = "CodeBreaker CBC...";
this.menuFileLoadCbc.Click += new System.EventHandler(this.menuFileLoadCbc_Click);

// menuFileLoadArmaxBin
this.menuFileLoadArmaxBin.Name = "menuFileLoadArmaxBin";
this.menuFileLoadArmaxBin.Size = new System.Drawing.Size(210, 22);
this.menuFileLoadArmaxBin.Text = "AR MAX .bin...";
this.menuFileLoadArmaxBin.Click += new System.EventHandler(this.menuFileLoadArmaxBin_Click);

// menuFileLoadP2m
this.menuFileLoadP2m.Name = "menuFileLoadP2m";
this.menuFileLoadP2m.Size = new System.Drawing.Size(210, 22);
this.menuFileLoadP2m.Text = "XP/GS P2M...";
this.menuFileLoadP2m.Click += new System.EventHandler(this.menuFileLoadP2m_Click);

// menuFileSeparator
this.menuFileSeparator.Name = "menuFileSeparator";
this.menuFileSeparator.Size = new System.Drawing.Size(207, 6);

// menuFileSaveAsText
this.menuFileSaveAsText.Name = "menuFileSaveAsText";
this.menuFileSaveAsText.Size = new System.Drawing.Size(210, 22);
this.menuFileSaveAsText.Text = "Save As Text...";
this.menuFileSaveAsText.Click += new System.EventHandler(this.menuFileSaveAsText_Click);

// menuFileSaveAsPnach   <-- NEW
this.menuFileSaveAsPnach.Name = "menuFileSaveAsPnach";
this.menuFileSaveAsPnach.Size = new System.Drawing.Size(210, 22);
this.menuFileSaveAsPnach.Text = "Save As PNACH File (.pnach)";
this.menuFileSaveAsPnach.Click += new System.EventHandler(this.menuFileSaveAsPnach_Click);

// menuFileSaveAsArmaxBin
this.menuFileSaveAsArmaxBin.Name = "menuFileSaveAsArmaxBin";
this.menuFileSaveAsArmaxBin.Size = new System.Drawing.Size(210, 22);
this.menuFileSaveAsArmaxBin.Text = "Save As AR MAX File (.bin)";
this.menuFileSaveAsArmaxBin.Click += new System.EventHandler(this.menuFileSaveAsArmaxBin_Click);

// menuFileSaveAsCbc
this.menuFileSaveAsCbc.Name = "menuFileSaveAsCbc";
this.menuFileSaveAsCbc.Size = new System.Drawing.Size(210, 22);
this.menuFileSaveAsCbc.Text = "Save As CBC File (.cbc)";
this.menuFileSaveAsCbc.Click += new System.EventHandler(this.menuFileSaveAsCbc_Click);

// menuFileSaveAsP2m
this.menuFileSaveAsP2m.Name = "menuFileSaveAsP2m";
this.menuFileSaveAsP2m.Size = new System.Drawing.Size(210, 22);
this.menuFileSaveAsP2m.Text = "Save As XP/GS File (.P2M)";
this.menuFileSaveAsP2m.Click += new System.EventHandler(this.menuFileSaveAsP2m_Click);

// menuFileSaveAsSwapBin
this.menuFileSaveAsSwapBin.Name = "menuFileSaveAsSwapBin";
this.menuFileSaveAsSwapBin.Size = new System.Drawing.Size(210, 22);
this.menuFileSaveAsSwapBin.Text = "Save As Swap Magic File (.bin)";
this.menuFileSaveAsSwapBin.Click += new System.EventHandler(this.menuFileSaveAsSwapBin_Click);

            this.menuEdit.Text    = "&Edit";
            // menuEdit
// 
// menuEditUndo
// 
this.menuEditUndo = new ToolStripMenuItem();
this.menuEditUndo.Name = "menuEditUndo";
this.menuEditUndo.Size = new System.Drawing.Size(180, 22);
this.menuEditUndo.Text = "Undo";
this.menuEditUndo.ShortcutKeys = ((Keys)((Keys.Control | Keys.Z)));
this.menuEditUndo.Click += new System.EventHandler(this.menuEditUndo_Click);
//
// menuEditCut
//
this.menuEditCut = new ToolStripMenuItem();
this.menuEditCut.Name = "menuEditCut";
this.menuEditCut.Size = new System.Drawing.Size(180, 22);
this.menuEditCut.Text = "Cut";
this.menuEditCut.ShortcutKeys = ((Keys)((Keys.Control | Keys.X)));
this.menuEditCut.Click += new System.EventHandler(this.menuEditCut_Click);
//
// menuEditCopy
//
this.menuEditCopy = new ToolStripMenuItem();
this.menuEditCopy.Name = "menuEditCopy";
this.menuEditCopy.Size = new System.Drawing.Size(180, 22);
this.menuEditCopy.Text = "Copy";
this.menuEditCopy.ShortcutKeys = ((Keys)((Keys.Control | Keys.C)));
this.menuEditCopy.Click += new System.EventHandler(this.menuEditCopy_Click);
//
// menuEditPaste
//
this.menuEditPaste = new ToolStripMenuItem();
this.menuEditPaste.Name = "menuEditPaste";
this.menuEditPaste.Size = new System.Drawing.Size(180, 22);
this.menuEditPaste.Text = "Paste";
this.menuEditPaste.ShortcutKeys = ((Keys)((Keys.Control | Keys.V)));
this.menuEditPaste.Click += new System.EventHandler(this.menuEditPaste_Click);
//
// menuEditSelectAll
//
this.menuEditSelectAll = new ToolStripMenuItem();
this.menuEditSelectAll.Name = "menuEditSelectAll";
this.menuEditSelectAll.Size = new System.Drawing.Size(180, 22);
this.menuEditSelectAll.Text = "Select All";
this.menuEditSelectAll.ShortcutKeys = ((Keys)((Keys.Control | Keys.A)));
this.menuEditSelectAll.Click += new System.EventHandler(this.menuEditSelectAll_Click);
//
// menuEditClearInput
//
this.menuEditClearInput = new ToolStripMenuItem();
this.menuEditClearInput.Name = "menuEditClearInput";
this.menuEditClearInput.Size = new System.Drawing.Size(180, 22);
this.menuEditClearInput.Text = "Clear Input";
this.menuEditClearInput.Click += new System.EventHandler(this.menuEditClearInput_Click);
//
// menuEditClearOutput
//
this.menuEditClearOutput = new ToolStripMenuItem();
this.menuEditClearOutput.Name = "menuEditClearOutput";
this.menuEditClearOutput.Size = new System.Drawing.Size(180, 22);
this.menuEditClearOutput.Text = "Clear Output";
this.menuEditClearOutput.Click += new System.EventHandler(this.menuEditClearOutput_Click);
//
// Add items to the Edit menu
//
this.menuEdit.DropDownItems.AddRange(new ToolStripItem[] {
    this.menuEditUndo,
    new ToolStripSeparator(),
    this.menuEditCut,
    this.menuEditCopy,
    this.menuEditPaste,
    new ToolStripSeparator(),
    this.menuEditSelectAll,
    new ToolStripSeparator(),
    this.menuEditClearInput,
    this.menuEditClearOutput});

			
            this.menuInput.Text   = "&Input";
            this.menuOutput.Text  = "&Output";
            this.menuOptions.Text = "&Options";
			
            // menuOptionsPnachCrc
            this.menuOptionsPnachCrc = new System.Windows.Forms.ToolStripMenuItem();
            this.menuOptionsPnachCrc.Name = "menuOptionsPnachCrc";
            this.menuOptionsPnachCrc.Size = new System.Drawing.Size(220, 22);
            this.menuOptionsPnachCrc.Text = "Set PNACH CRC...";
            this.menuOptionsPnachCrc.Click += new System.EventHandler(this.menuOptionsPnachCrc_Click);

            // menuOptions
			this.menuOptions.DropDownItems.AddRange(new ToolStripItem[]
{
				// Make Organizers
				this.menuOptionsMakeOrganizers,
				this.menuOptionsSeparatorAfterOrganizers,
				// AR MAX Verifiers / Region (if you already have these, include them here)
                this.menuOptionsArmaxVerifiers,
                this.menuOptionsArmaxRegion,
				// AR MAX Disc Hash submenu
				this.menuOptionsArmaxDiscHash,
				this.menuOptionsSeparatorAfterDiscHash,
				// Keys
				this.menuOptionsPnachCrc,      // <-- NEW
				this.menuOptionsAr2Key,
				this.menuOptionsGs3Key,
				this.menuOptionsCbcSaveVersion
            });
// menuOptionsCbcSaveVersion
this.menuOptionsCbcSaveVersion.Name = "menuOptionsCbcSaveVersion";
this.menuOptionsCbcSaveVersion.Size = new System.Drawing.Size(220, 22);
this.menuOptionsCbcSaveVersion.Text = "CBC Save Version";
this.menuOptionsCbcSaveVersion.DropDownItems.AddRange(new ToolStripItem[]
{
    this.menuOptionsCbcVersion7,
    this.menuOptionsCbcVersion8
});

// menuOptionsCbcVersion7
this.menuOptionsCbcVersion7.Name = "menuOptionsCbcVersion7";
this.menuOptionsCbcVersion7.Size = new System.Drawing.Size(140, 22);
this.menuOptionsCbcVersion7.Text = "Version 7 (Day1)";
this.menuOptionsCbcVersion7.Click += new System.EventHandler(this.menuOptionsCbcVersion7_Click);

// menuOptionsCbcVersion8
this.menuOptionsCbcVersion8.Name = "menuOptionsCbcVersion8";
this.menuOptionsCbcVersion8.Size = new System.Drawing.Size(140, 22);
this.menuOptionsCbcVersion8.Text = "Version 8 (CFU)";
this.menuOptionsCbcVersion8.Click += new System.EventHandler(this.menuOptionsCbcVersion8_Click);

            // menuOptionsAr2Key
            this.menuOptionsAr2Key.Name = "menuOptionsAr2Key";
            this.menuOptionsAr2Key.Size = new System.Drawing.Size(220, 22);
            this.menuOptionsAr2Key.Text = "Set AR2 Key Code...";
			this.menuOptionsAr2Key.ShortcutKeys = ((Keys)
			((Keys.Control | Keys.K)));
            this.menuOptionsAr2Key.Click += new System.EventHandler(this.menuOptionsAr2Key_Click);
			// menuOptionsGs3Key
this.menuOptionsGs3Key.Name = "menuOptionsGs3Key";
this.menuOptionsGs3Key.Size = new System.Drawing.Size(220, 22);
this.menuOptionsGs3Key.Text = "GS3 Key";
this.menuOptionsGs3Key.DropDownItems.AddRange(new ToolStripItem[]
{
    this.menuOptionsGs3Key0,
    this.menuOptionsGs3Key1,
    this.menuOptionsGs3Key2,
    this.menuOptionsGs3Key3,
    this.menuOptionsGs3Key4
});

// menuOptionsGs3Key0
this.menuOptionsGs3Key0.Name = "menuOptionsGs3Key0";
this.menuOptionsGs3Key0.Size = new System.Drawing.Size(90, 22);
this.menuOptionsGs3Key0.Text = "0";
this.menuOptionsGs3Key0.Click += new System.EventHandler(this.menuOptionsGs3Key0_Click);

// menuOptionsGs3Key1
this.menuOptionsGs3Key1.Name = "menuOptionsGs3Key1";
this.menuOptionsGs3Key1.Size = new System.Drawing.Size(90, 22);
this.menuOptionsGs3Key1.Text = "1";
this.menuOptionsGs3Key1.Click += new System.EventHandler(this.menuOptionsGs3Key1_Click);

// menuOptionsGs3Key2
this.menuOptionsGs3Key2.Name = "menuOptionsGs3Key2";
this.menuOptionsGs3Key2.Size = new System.Drawing.Size(90, 22);
this.menuOptionsGs3Key2.Text = "2";
this.menuOptionsGs3Key2.Click += new System.EventHandler(this.menuOptionsGs3Key2_Click);

// menuOptionsGs3Key3
this.menuOptionsGs3Key3.Name = "menuOptionsGs3Key3";
this.menuOptionsGs3Key3.Size = new System.Drawing.Size(90, 22);
this.menuOptionsGs3Key3.Text = "3";
this.menuOptionsGs3Key3.Click += new System.EventHandler(this.menuOptionsGs3Key3_Click);

// menuOptionsGs3Key4
this.menuOptionsGs3Key4.Name = "menuOptionsGs3Key4";
this.menuOptionsGs3Key4.Size = new System.Drawing.Size(90, 22);
this.menuOptionsGs3Key4.Text = "4";
this.menuOptionsGs3Key4.Click += new System.EventHandler(this.menuOptionsGs3Key4_Click);

            // menuOptionsSeparatorArmax
            this.menuOptionsSeparatorArmax.Name = "menuOptionsSeparatorArmax";
            this.menuOptionsSeparatorArmax.Size = new System.Drawing.Size(217, 6);

            // menuOptionsArmaxVerifiers
            this.menuOptionsArmaxVerifiers.Name = "menuOptionsArmaxVerifiers";
            this.menuOptionsArmaxVerifiers.Size = new System.Drawing.Size(220, 22);
            this.menuOptionsArmaxVerifiers.Text = "AR MAX Verifiers";
            this.menuOptionsArmaxVerifiers.DropDownItems.AddRange(new ToolStripItem[]
            {
                this.menuOptionsArmaxVerifiersAuto,
                this.menuOptionsArmaxVerifiersManual
            });

            // menuOptionsArmaxVerifiersAuto
            this.menuOptionsArmaxVerifiersAuto.Name = "menuOptionsArmaxVerifiersAuto";
            this.menuOptionsArmaxVerifiersAuto.Size = new System.Drawing.Size(140, 22);
            this.menuOptionsArmaxVerifiersAuto.Text = "Automatic";
            this.menuOptionsArmaxVerifiersAuto.Click += new System.EventHandler(this.menuOptionsArmaxVerifiersAuto_Click);

            // menuOptionsArmaxVerifiersManual
            this.menuOptionsArmaxVerifiersManual.Name = "menuOptionsArmaxVerifiersManual";
            this.menuOptionsArmaxVerifiersManual.Size = new System.Drawing.Size(140, 22);
            this.menuOptionsArmaxVerifiersManual.Text = "Manual";
            this.menuOptionsArmaxVerifiersManual.Click += new System.EventHandler(this.menuOptionsArmaxVerifiersManual_Click);

            // menuOptionsArmaxRegion
            this.menuOptionsArmaxRegion.Name = "menuOptionsArmaxRegion";
            this.menuOptionsArmaxRegion.Size = new System.Drawing.Size(220, 22);
            this.menuOptionsArmaxRegion.Text = "AR MAX Region";
            this.menuOptionsArmaxRegion.DropDownItems.AddRange(new ToolStripItem[]
            {
                this.menuOptionsArmaxRegionUsa,
                this.menuOptionsArmaxRegionPal,
                this.menuOptionsArmaxRegionJapan
            });

            // menuOptionsArmaxRegionUsa
            this.menuOptionsArmaxRegionUsa.Name = "menuOptionsArmaxRegionUsa";
            this.menuOptionsArmaxRegionUsa.Size = new System.Drawing.Size(120, 22);
            this.menuOptionsArmaxRegionUsa.Text = "USA";
            this.menuOptionsArmaxRegionUsa.Click += new System.EventHandler(this.menuOptionsArmaxRegionUsa_Click);

            // menuOptionsArmaxRegionPal
            this.menuOptionsArmaxRegionPal.Name = "menuOptionsArmaxRegionPal";
            this.menuOptionsArmaxRegionPal.Size = new System.Drawing.Size(120, 22);
            this.menuOptionsArmaxRegionPal.Text = "PAL";
            this.menuOptionsArmaxRegionPal.Click += new System.EventHandler(this.menuOptionsArmaxRegionPal_Click);

            // menuOptionsArmaxRegionJapan
            this.menuOptionsArmaxRegionJapan.Name = "menuOptionsArmaxRegionJapan";
            this.menuOptionsArmaxRegionJapan.Size = new System.Drawing.Size(120, 22);
            this.menuOptionsArmaxRegionJapan.Text = "Japan";
            this.menuOptionsArmaxRegionJapan.Click += new System.EventHandler(this.menuOptionsArmaxRegionJapan_Click);
// menuOptionsMakeOrganizers
this.menuOptionsMakeOrganizers.Name   = "menuOptionsMakeOrganizers";
this.menuOptionsMakeOrganizers.Size   = new System.Drawing.Size(200, 22);
this.menuOptionsMakeOrganizers.Text   = "Make Organizers";
this.menuOptionsMakeOrganizers.CheckOnClick = false; // we manage Checked ourselves
this.menuOptionsMakeOrganizers.Click += new System.EventHandler(this.menuOptionsMakeOrganizers_Click);

// menuOptionsArmaxDiscHash
this.menuOptionsArmaxDiscHash.Name = "menuOptionsArmaxDiscHash";
this.menuOptionsArmaxDiscHash.Size = new System.Drawing.Size(200, 22);
this.menuOptionsArmaxDiscHash.Text = "AR MAX Disc Hash";

// Root item has a dropdown; start with only "Do Not Create"
this.menuOptionsArmaxDiscHash.DropDownItems.AddRange(new ToolStripItem[]
{
    this.menuOptionsArmaxDiscHashNone
});

// menuOptionsArmaxDiscHashNone
this.menuOptionsArmaxDiscHashNone.Name = "menuOptionsArmaxDiscHashNone";
this.menuOptionsArmaxDiscHashNone.Size = new System.Drawing.Size(180, 22);
this.menuOptionsArmaxDiscHashNone.Text = "Do Not Create";
this.menuOptionsArmaxDiscHashNone.Tag  = '\0'; // special marker = no hash
this.menuOptionsArmaxDiscHashNone.Click += new System.EventHandler(this.menuOptionsArmaxDiscHashNone_Click);

            // lblInputFormat
            this.lblInputFormat.AutoSize = true;
            this.lblInputFormat.Location = new Point(12, 32);
            this.lblInputFormat.Name     = "lblInputFormat";
            this.lblInputFormat.Size     = new Size(93, 13);
            this.lblInputFormat.TabIndex = 1;
            this.lblInputFormat.Text     = "Input: (not set yet)";

            // btnClearInput
            this.btnClearInput.Location = new Point(270, 27);
            this.btnClearInput.Name     = "btnClearInput";
            this.btnClearInput.Size     = new Size(50, 23);
            this.btnClearInput.TabIndex = 2;
            this.btnClearInput.Text     = "Clear";
            this.btnClearInput.UseVisualStyleBackColor = true;
            this.btnClearInput.Click += new System.EventHandler(this.btnClearInput_Click);

            // txtInput
            this.txtInput.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left;
            this.txtInput.Font = new Font("Consolas", 9F);
            this.txtInput.Location = new Point(12, 56);
            this.txtInput.Multiline = true;
            this.txtInput.ScrollBars = ScrollBars.Both;
            this.txtInput.Name = "txtInput";
            this.txtInput.Size = new Size(308, 320);
            this.txtInput.TabIndex = 3;
            this.txtInput.WordWrap = false;

            // lblOutputFormat
            this.lblOutputFormat.AutoSize = true;
            this.lblOutputFormat.Location = new Point(332, 32);
            this.lblOutputFormat.Name     = "lblOutputFormat";
            this.lblOutputFormat.Size     = new Size(100, 13);
            this.lblOutputFormat.TabIndex = 4;
            this.lblOutputFormat.Text     = "Output: (not set yet)";
			
			// chkPnachCrcActive  <-- NEW / UPDATED
			this.chkPnachCrcActive = new System.Windows.Forms.CheckBox();
			this.chkPnachCrcActive.AutoSize = true;
			this.chkPnachCrcActive.Location = new System.Drawing.Point(520, 30);   // left of Clear
			this.chkPnachCrcActive.Name = "chkPnachCrcActive";
			this.chkPnachCrcActive.Size = new System.Drawing.Size(70, 17);
			this.chkPnachCrcActive.TabIndex = 5;
			this.chkPnachCrcActive.Text = "Add CRC";
			this.chkPnachCrcActive.UseVisualStyleBackColor = true;
			this.chkPnachCrcActive.Visible = false;  // only shown in PNACH mode
			this.chkPnachCrcActive.CheckedChanged += new System.EventHandler(this.chkPnachCrcActive_CheckedChanged);

            // btnClearOutput
            this.btnClearOutput.Location = new Point(599, 27);
            this.btnClearOutput.Name     = "btnClearOutput";
            this.btnClearOutput.Size     = new Size(50, 23);
            this.btnClearOutput.TabIndex = 5;
            this.btnClearOutput.Text     = "Clear";
            this.btnClearOutput.UseVisualStyleBackColor = true;
            this.btnClearOutput.Click += new System.EventHandler(this.btnClearOutput_Click);
			

            // txtOutput
            this.txtOutput.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Right;
            this.txtOutput.Font = new Font("Consolas", 9F);
            this.txtOutput.Location = new Point(332, 56);
            this.txtOutput.Multiline = true;
            this.txtOutput.ScrollBars = ScrollBars.Both;
            this.txtOutput.Name = "txtOutput";
            this.txtOutput.Size = new Size(317, 320);
            this.txtOutput.TabIndex = 6;
            this.txtOutput.WordWrap = false;

            // btnConvert
            this.btnConvert.Anchor = AnchorStyles.Bottom | AnchorStyles.Left;
            this.btnConvert.Location = new Point(12, 386);
            this.btnConvert.Name     = "btnConvert";
            this.btnConvert.Size     = new Size(75, 23);
            this.btnConvert.TabIndex = 7;
            this.btnConvert.Text     = "Convert";
            this.btnConvert.UseVisualStyleBackColor = true;
            this.btnConvert.Click += new System.EventHandler(this.btnConvert_Click);

            // lblGameId
            this.lblGameId.Anchor = AnchorStyles.Bottom | AnchorStyles.Left;
            this.lblGameId.AutoSize = true;
            this.lblGameId.Location = new Point(120, 391);
            this.lblGameId.Name     = "lblGameId";
            this.lblGameId.Size     = new Size(51, 13);
            this.lblGameId.TabIndex = 8;
            this.lblGameId.Text     = "Game ID:";

            // txtGameId
            this.txtGameId.Anchor = AnchorStyles.Bottom | AnchorStyles.Left;
            this.txtGameId.Location = new Point(177, 388);
            this.txtGameId.Name     = "txtGameId";
            this.txtGameId.Size     = new Size(60, 20);
            this.txtGameId.TabIndex = 9;

            // lblGameName
            this.lblGameName.Anchor = AnchorStyles.Bottom | AnchorStyles.Left;
            this.lblGameName.AutoSize = true;
            this.lblGameName.Location = new Point(255, 391);
            this.lblGameName.Name     = "lblGameName";
            this.lblGameName.Size     = new Size(69, 13);
            this.lblGameName.TabIndex = 10;
            this.lblGameName.Text     = "Game Name:";

            // txtGameName
            this.txtGameName.Anchor = AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            this.txtGameName.Location = new Point(330, 388);
            this.txtGameName.Name     = "txtGameName";
            this.txtGameName.Size     = new Size(319, 20);
            this.txtGameName.TabIndex = 11;

            // MainForm
            this.AutoScaleMode = AutoScaleMode.Font;
            this.ClientSize = new Size(661, 421);
            this.Controls.Add(this.txtGameName);
            this.Controls.Add(this.lblGameName);
            this.Controls.Add(this.txtGameId);
            this.Controls.Add(this.lblGameId);
            this.Controls.Add(this.btnConvert);
            this.Controls.Add(this.txtOutput);
            this.Controls.Add(this.btnClearOutput);
			this.Controls.Add(this.chkPnachCrcActive);   // NEW
            this.Controls.Add(this.lblOutputFormat);
            this.Controls.Add(this.txtInput);
            this.Controls.Add(this.btnClearInput);
            this.Controls.Add(this.lblInputFormat);
            this.Controls.Add(this.menuStrip);
            this.FormBorderStyle = FormBorderStyle.Sizable;
			this.MainMenuStrip = this.menuStrip;
			this.MaximizeBox = false;
			this.MinimumSize = new Size(680, 460);
			this.MaximumSize = new Size(680, 1000);
            this.Name = "MainForm";
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "Omniconvert C#";
			this.Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            this.Load += new System.EventHandler(this.MainForm_Load);

            this.menuStrip.ResumeLayout(false);
            this.menuStrip.PerformLayout();
        }
    }
}
