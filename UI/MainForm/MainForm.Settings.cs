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
        private sealed class AppSettings
        {
            public ConvertCore.Device       InputDevice  { get; set; }
            public ConvertCore.Crypt        InputCrypt   { get; set; }
            public ConvertCore.Device       OutputDevice { get; set; }
            public ConvertCore.Crypt        OutputCrypt  { get; set; }
        	public bool                     OutputPnachRaw { get; set; }
            public ConvertCore.VerifierMode VerifierMode { get; set; }
            public uint                     Region       { get; set; }
            public bool                     MakeFolders  { get; set; }
            public char                     HashDrive    { get; set; }
            public uint                     Gs3Key       { get; set; }
        	public int                      CbcSaveVersion { get; set; }  // 7 or 8
        	public uint                     GameId         { get; set; }
			public bool InlineInputMode { get; set; }  // NEW
        }

        private static string GetSettingsPath()
        {
            try
            {
                return Path.Combine(AppContext.BaseDirectory, "OmniconvertSettings.json");
            }
            catch
            {
                // Fallback to current working directory if BaseDirectory is unavailable
                return "OmniconvertSettings.json";
            }
        }

        private void LoadAppSettings()
        {
            try
            {
                var path = GetSettingsPath();
                if (!File.Exists(path))
                    return;

                string json = File.ReadAllText(path);
                var settings = JsonSerializer.Deserialize<AppSettings>(json);
                if (settings == null)
                    return;

                ConvertCore.g_indevice     = settings.InputDevice;
                ConvertCore.g_incrypt      = settings.InputCrypt;
                ConvertCore.g_outdevice    = settings.OutputDevice;
                ConvertCore.g_outcrypt     = settings.OutputCrypt;
                ConvertCore.g_verifiermode = settings.VerifierMode;
        		_outputAsPnachRaw      = settings.OutputPnachRaw;
				_inlineInputMode = settings.InlineInputMode;
                ConvertCore.g_region       = settings.Region;
                ConvertCore.g_makefolders  = settings.MakeFolders;
                ConvertCore.g_hashdrive    = settings.HashDrive;
                ConvertCore.g_gs3key       = settings.Gs3Key;
        		ConvertCore.g_cbcSaveVersion = settings.CbcSaveVersion == 8 ? 8 : 7;
        		if (settings.GameId != 0)
        {
            ConvertCore.g_gameid = settings.GameId;
        }
            }
            catch
            {
                // Ignore corrupt/invalid settings and fall back to defaults.
            }
        }

        private void SaveAppSettings()
        {
            try
            {
                var settings = new AppSettings
                {
                    InputDevice  = ConvertCore.g_indevice,
                    InputCrypt   = ConvertCore.g_incrypt,
                    OutputDevice = ConvertCore.g_outdevice,
                    OutputCrypt  = ConvertCore.g_outcrypt,
        			OutputPnachRaw = _outputAsPnachRaw,
                    VerifierMode = ConvertCore.g_verifiermode,
                    Region       = ConvertCore.g_region,
                    MakeFolders  = ConvertCore.g_makefolders,
                    HashDrive    = ConvertCore.g_hashdrive,
                    Gs3Key       = ConvertCore.g_gs3key,
        			CbcSaveVersion = ConvertCore.g_cbcSaveVersion,
        			GameId         = ConvertCore.g_gameid,
					InlineInputMode = _inlineInputMode
                };

                var options = new JsonSerializerOptions { WriteIndented = true };
                string json = JsonSerializer.Serialize(settings, options);

                var path = GetSettingsPath();
                File.WriteAllText(path, json);
            }
            catch
            {
                // Best-effort only; ignore IO/serialization errors.
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            // Save settings before closing
            SaveAppSettings();
            base.OnFormClosing(e);
        }

    }
}
