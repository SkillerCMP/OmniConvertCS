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
        private sealed class CryptOption
{
    public ConvertCore.Crypt Crypt { get; }
    public ConvertCore.Device Device { get; }
    public string Text { get; }
    public bool UsePnachFormat { get; }
    public bool IsInlineMode { get; }   // NEW

    public CryptOption(
        ConvertCore.Crypt crypt,
        ConvertCore.Device device,
        string text,
        bool usePnachFormat = false,
        bool isInlineMode = false)
    {
        Crypt          = crypt;
        Device         = device;
        Text           = text;
        UsePnachFormat = usePnachFormat;
        IsInlineMode   = isInlineMode;
    }
}

                // Unencrypted >
                private static readonly CryptOption[] Options_Unencrypted = new[]
                {
                    new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_STD,   "Standard(RAW)"),
        			// NEW: INLINE mode – per-block CRYPT_XXXX from header lines
    new CryptOption(ConvertCore.Crypt.CRYPT_RAW,
                    ConvertCore.Device.DEV_STD,
                    "INLINE (from ', CRYPT_XXXX' headers)",
                    usePnachFormat: false,
                    isInlineMode:   true),
					
					new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_STD,   "Pnach(RAW)", usePnachFormat: true),  // NEW
                    new CryptOption(ConvertCore.Crypt.CRYPT_MAXRAW, ConvertCore.Device.DEV_ARMAX, "ARMAX(RAW)"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_ARAW,    ConvertCore.Device.DEV_AR2,   "AR/GS V1-2(RAW)"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_CRAW,    ConvertCore.Device.DEV_CB,    "Codebreaker(RAW)"),
                    // new CryptOption(ConvertCore.Crypt.CRYPT_RAW,    ConvertCore.Device.DEV_AR2,   "GS V1-2(RAW)"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_GRAW,    ConvertCore.Device.DEV_GS3,   "XP/GS V3(RAW)"),
                };

                // Action Replay >
                private static readonly CryptOption[] Options_ActionReplay = new[]
                {
                    new CryptOption(ConvertCore.Crypt.CRYPT_AR1,   ConvertCore.Device.DEV_AR1,   "AR-V1"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_AR2,   ConvertCore.Device.DEV_AR2,   "AR-V2"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_ARMAX, ConvertCore.Device.DEV_ARMAX, "ARMAX"),
                };

                // CodeBreaker >
                private static readonly CryptOption[] Options_CodeBreaker = new[]
                {
                    new CryptOption(ConvertCore.Crypt.CRYPT_CB,         ConvertCore.Device.DEV_CB, "CB - V1+ (All v7 Keys)"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_CB7_COMMON, ConvertCore.Device.DEV_CB, "CB - V7+ (Common Key)"),
                };

                // GameShark > Interact >
                private static readonly CryptOption[] Options_GameShark_Interact = new[]
                {
                    new CryptOption(ConvertCore.Crypt.CRYPT_AR1, ConvertCore.Device.DEV_AR1, "GS - V1"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_AR2, ConvertCore.Device.DEV_AR2, "GS - V2"),
                };

                // GameShark > MadCatz >
                private static readonly CryptOption[] Options_GameShark_MadCatz = new[]
                {
                    new CryptOption(ConvertCore.Crypt.CRYPT_GS3, ConvertCore.Device.DEV_GS3, "GS - V3-4"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_GS5, ConvertCore.Device.DEV_GS3, "GS - V5+"),
                };

                // Swap Magic >
                private static readonly CryptOption[] Options_SwapMagic = new[]
                {
                    new CryptOption(ConvertCore.Crypt.CRYPT_AR1, ConvertCore.Device.DEV_AR1, "SM - Coder V3.x"),
                };

                // Xploder >
                private static readonly CryptOption[] Options_Xploder = new[]
                {
                    new CryptOption(ConvertCore.Crypt.CRYPT_CB,  ConvertCore.Device.DEV_CB,  "XP - V1-3"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_GS3, ConvertCore.Device.DEV_GS3, "XP - V4"),
                    new CryptOption(ConvertCore.Crypt.CRYPT_GS5, ConvertCore.Device.DEV_GS3, "XP - V5+"),
                };

                public MainForm()
                {
                    InitializeComponent();
                }


 
 

        private void CryptMenuItem_Click(object? sender, EventArgs e)
                {
                    if (sender is not ToolStripMenuItem mi)
                        return;
                    if (mi.Tag is not Tuple<bool, CryptOption> data)
                        return;

                    bool isInput = data.Item1;
                    CryptOption opt = data.Item2;

                    SetCryptSelection(isInput, opt);
                }

        private void BuildCryptMenu(ToolStripMenuItem root, bool isInput)
                {
                    root.DropDownItems.Clear();

                    // Unencrypted >
                    var unenc = new ToolStripMenuItem("Unencrypted");
            foreach (var opt in Options_Unencrypted)
                AddCryptMenuItem(unenc, opt, isInput);
            root.DropDownItems.Add(unenc);

                    // Action Replay >
                    var ar = new ToolStripMenuItem("Action Replay");
                    foreach (var opt in Options_ActionReplay)
                        AddCryptMenuItem(ar, opt, isInput);
                    root.DropDownItems.Add(ar);

                    // CodeBreaker >
                    var cb = new ToolStripMenuItem("CodeBreaker");
                    foreach (var opt in Options_CodeBreaker)
                        AddCryptMenuItem(cb, opt, isInput);
                    root.DropDownItems.Add(cb);

                    // GameShark > Interact / MadCatz
                    var gs = new ToolStripMenuItem("GameShark");

                    var gsInteract = new ToolStripMenuItem("Interact");
                    foreach (var opt in Options_GameShark_Interact)
                        AddCryptMenuItem(gsInteract, opt, isInput);
                    gs.DropDownItems.Add(gsInteract);

                    var gsMad = new ToolStripMenuItem("MadCatz");
                    foreach (var opt in Options_GameShark_MadCatz)
                        AddCryptMenuItem(gsMad, opt, isInput);
                    gs.DropDownItems.Add(gsMad);

                    root.DropDownItems.Add(gs);

                    // Swap Magic >
                    var sm = new ToolStripMenuItem("Swap Magic");
                    foreach (var opt in Options_SwapMagic)
                        AddCryptMenuItem(sm, opt, isInput);
                    root.DropDownItems.Add(sm);

                    // Xploder >
                    var xp = new ToolStripMenuItem("Xploder");
                    foreach (var opt in Options_Xploder)
                        AddCryptMenuItem(xp, opt, isInput);
                    root.DropDownItems.Add(xp);
                }

        private void AddCryptMenuItem(ToolStripMenuItem parent, CryptOption opt, bool isInput)
{
    // INLINE is an input-only mode: do not show it on the Output menu
    if (!isInput && opt.IsInlineMode)
    {
        return;
    }

    var mi = new ToolStripMenuItem(opt.Text);
    mi.Tag = new Tuple<bool, CryptOption>(isInput, opt);
    mi.Click += CryptMenuItem_Click;
    parent.DropDownItems.Add(mi);
}

        private CryptOption FindCryptOption(ConvertCore.Device device,
                                            ConvertCore.Crypt crypt,
                                            CryptOption fallback)
        {
            CryptOption[][] allGroups =
            {
                Options_Unencrypted,
                Options_ActionReplay,
                Options_CodeBreaker,
                Options_GameShark_Interact,
                Options_GameShark_MadCatz,
                Options_SwapMagic,
                Options_Xploder
            };

            foreach (var group in allGroups)
            {
                foreach (var opt in group)
                {
                    if (opt.Device == device && opt.Crypt == crypt)
                        return opt;
                }
            }

            return fallback;
        }

        void SetCryptSelection(bool isInput, CryptOption opt)
{
    if (isInput)
    {
        // NEW: toggle INLINE mode
        _inlineInputMode = opt.IsInlineMode;

        // For INLINE, keep base "STD/RAW" globals; per-cheat we will override g_incrypt/g_indevice
        if (_inlineInputMode)
        {
            ConvertCore.g_indevice = ConvertCore.Device.DEV_STD;
            ConvertCore.g_incrypt  = ConvertCore.Crypt.CRYPT_RAW;
        }
        else
        {
            ConvertCore.g_indevice = opt.Device;
            ConvertCore.g_incrypt  = opt.Crypt;
        }

        lblInputFormat.Text = "Input: " + opt.Text;
        // INLINE never means PNACH input – that is a separate toggle
        _inputAsPnachRaw = false;
		// 🔑 IMPORTANT: ALWAYS refresh AR2 key box after any input-mode change
        RefreshAr2KeyDisplayFromSeed();
    }
    else
    {
        ConvertCore.g_outdevice = opt.Device;
        ConvertCore.g_outcrypt  = opt.Crypt;
        lblOutputFormat.Text    = "Output: " + opt.Text;
        _outputAsPnachRaw       = opt.UsePnachFormat;

        chkPnachCrcActive.Visible = _outputAsPnachRaw;
        chkPnachCrcActive.Checked = _outputAsPnachRaw && _pnachCrcActive && _pnachCrc.HasValue;
    }

    // Keep ARMAX Game ID boxes in sync with current devices
    RefreshArmaxGameIdDisplay();
}


    }
}
