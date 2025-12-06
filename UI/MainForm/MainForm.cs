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
private bool _updatingGameIdFromUi;
// --- Options > AR MAX Disc Hash -------------------------------------------
// --- Options > Make Organizers --------------------------------------------
// --- Options > AR MAX Verifiers / Region -----------------------------
// --- Options > GS3 Key -----------------------------------------------
    private void menuOptionsGs3Key0_Click(object? sender, EventArgs e) => SetGs3Key(0);
    private void menuOptionsGs3Key1_Click(object? sender, EventArgs e) => SetGs3Key(1);
    private void menuOptionsGs3Key2_Click(object? sender, EventArgs e) => SetGs3Key(2);
    private void menuOptionsGs3Key3_Click(object? sender, EventArgs e) => SetGs3Key(3);
    private void menuOptionsGs3Key4_Click(object? sender, EventArgs e) => SetGs3Key(4);
	// Remember the last successful conversion so that
        // "File -> Save As AR MAX" can export those cheats.
        private System.Collections.Generic.List<cheat_t>? _lastConvertedCheats;
        private uint _lastGameId;
		private bool _outputAsPnachRaw;   // NEW: track if output should be PNACH format
		private bool _inputAsPnachRaw;    // NEW: track if INPUT is PNACH(RAW)
		private uint?  _pnachCrc;
		private string? _pnachGameName;
		private string? _pnachElfName;     // <--- NEW: e.g. "SLUS_203.12"
		private bool _pnachCrcActive;   // whether to actually emit the CRC line in PNACH output
		private bool _inlineInputMode;    // NEW: INLINE mode (per-cheat CRYPT_XXXX from labels)
		// Helper representing one crypt/device option (like wincryptopt_t, minus ID)
        private static string CleanCbSiteFormat(string input)
 {
     if (string.IsNullOrEmpty(input))
         return input;
     // Matches a CodeBreaker/RAW pair: XXXXXXXX YYYYYYYY
     var codePairRegex = new Regex(
         @"[0-9A-Fa-f]{8}\s+[0-9A-Fa-f]{8}",
         RegexOptions.Compiled);
     var sb = new StringBuilder();
     using (var reader = new StringReader(input))
     {
         string line;
         while ((line = reader.ReadLine()) != null)
         {
             if (string.IsNullOrWhiteSpace(line))
             {
                 sb.AppendLine();
                 continue;
             }
             var matches = codePairRegex.Matches(line);
             if (matches.Count == 0)
             {
                 // No code pair on this line → leave as-is.
                 sb.AppendLine(line.TrimEnd());
             }
             else
             {
                 // 1) Anything before the first code pair is treated as the name.
                 int firstIndex = matches[0].Index;
                 string before = line.Substring(0, firstIndex).TrimEnd();
                 if (!string.IsNullOrEmpty(before))
                     sb.AppendLine(before);
                 // 2) Each code pair becomes its own line.
                 foreach (Match m in matches)
                 {
                     sb.AppendLine(m.Value.Trim());
                 }
                 // 3) If there is any trailing text after the last pair, keep it.
                 Match last = matches[matches.Count - 1];
                 int cursor = last.Index + last.Length;
                 if (cursor < line.Length)
                 {
                     string tail = line.Substring(cursor).Trim();
                     if (!string.IsNullOrEmpty(tail))
                         sb.AppendLine(tail);
                 }
             }
         }
     }
     // Trim trailing blank lines so we don't end with extra newlines
     return sb.ToString().TrimEnd('\r', '\n');
 }
        private void MainForm_Load(object sender, EventArgs e)
        {
            // Build hierarchical menus under Input / Output
            BuildCryptMenu(menuInput,  isInput: true);
            BuildCryptMenu(menuOutput, isInput: false);
            // Default both sides to Unencrypted > Standard (RAW)
            SetCryptSelection(true,  Options_Unencrypted[0]);
            SetCryptSelection(false, Options_Unencrypted[0]);
			// Load persisted settings (if any)
			LoadAppSettings();
			// Apply current globals (either defaults or loaded) to the menus
			var defaultOpt = Options_Unencrypted[0];
			var inOpt  = FindCryptOption(ConvertCore.g_indevice, ConvertCore.g_incrypt,  defaultOpt);
			var outOpt = FindCryptOption(ConvertCore.g_outdevice, ConvertCore.g_outcrypt, defaultOpt);
// If settings requested PNACH RAW output, prefer that specific option
if (_outputAsPnachRaw)
{
    foreach (var opt in Options_Unencrypted)
    {
        if (opt.Device == ConvertCore.g_outdevice &&
            opt.Crypt  == ConvertCore.g_outcrypt &&
            opt.UsePnachFormat)
        {
            outOpt = opt;
            break;
        }
    }
}
// If INLINE mode was saved, prefer the INLINE option for input
if (_inlineInputMode)
{
    foreach (var opt in Options_Unencrypted)
    {
        if (opt.IsInlineMode)
        {
            inOpt = opt;
            break;
        }
    }
}
			SetCryptSelection(true,  inOpt);
			SetCryptSelection(false, outOpt);
			// Sync ARMAX Options menu state with current globals
            UpdateVerifierMenuChecks();
            UpdateRegionMenuChecks();
			UpdateGs3MenuChecks();
			// If settings didn’t provide a Game ID, default to 0x0357
			if (ConvertCore.g_gameid == 0)
			{
			ConvertCore.g_gameid = 0x0357;
			}
// Push the current Game ID into all UI boxes
SyncGameIdFromGlobals();
			// Show version from OmniconvertCS.csproj in the title bar
    string version = Application.ProductVersion;
    int plusIndex = version.IndexOf('+');
    if (plusIndex >= 0)
        version = version.Substring(0, plusIndex);
    this.Text = $"Omniconvert C# v{version}";
			// --- NEW: initialise Options menu state ---
			UpdateMakeOrganizersCheck();
			EnumerateArmaxDiscHashDrives();
			SetArmaxHashDrive(ConvertCore.g_hashdrive);  // will select "Do Not Create" initially
        }
private void btnConvert_Click(object sender, EventArgs e)
{
    txtOutput.Clear();
    string inputText = txtInput.Text;
    if (string.IsNullOrWhiteSpace(inputText))
    {
        txtOutput.Text = "// No input provided.";
        return;
    }
    // Are we reading ARMAX encrypted text (XXXXX-XXXXX-XXXXX) as input?
    bool isArmaxEncryptedInput =
        ConvertCore.g_indevice == ConvertCore.Device.DEV_ARMAX &&
        ConvertCore.g_incrypt  == ConvertCore.Crypt.CRYPT_ARMAX;
	//Pnach Input
	bool isPnachInput = _inputAsPnachRaw;   // NEW
    // Parse into logical "cheats": optional name line + one or more code lines
    var cheats = new System.Collections.Generic.List<cheat_t>();
    var labels = new System.Collections.Generic.List<string?>();
    cheat_t? currentCheat = null;
    cheat_t EnsureCurrentCheat()
    {
        if (currentCheat == null)
        {
            currentCheat = Cheat.cheatInit();
            cheats.Add(currentCheat);
            labels.Add(null);
        }
        return currentCheat;
    }
    void StartNewCheatWithLabel(string label)
    {
        currentCheat = Cheat.cheatInit();
        cheats.Add(currentCheat);
        labels.Add(label);
    }
    string currentPnachGroup = null;   // NEW: tracks [Group\Name] group while parsing
string[] lines = inputText.Replace("\r\n", "\n").Split('\n');
foreach (string rawLine in lines)
{
    string line = rawLine.Trim();
    if (string.IsNullOrEmpty(line))
        continue;
    if (line.StartsWith("//") || line.StartsWith("#") || line.StartsWith(";"))
        continue;
    // ----------------------------
    // PNACH(RAW) input: only [] + patch= lines
    // ----------------------------
    if (isPnachInput)
    {
        // Skip PNACH header lines
        if (line.StartsWith("gametitle=", StringComparison.OrdinalIgnoreCase) ||
            line.StartsWith("comment=",   StringComparison.OrdinalIgnoreCase) ||
            line.StartsWith("crc=",       StringComparison.OrdinalIgnoreCase) ||
            line.StartsWith("author=",    StringComparison.OrdinalIgnoreCase) ||
			line.StartsWith("description=", StringComparison.OrdinalIgnoreCase))   // NEW
        {
            continue;
        }
        // Section header: [Group\Name] or [Name]
        if (line.Length >= 3 && line[0] == '[' && line[line.Length - 1] == ']')
        {
            string inner = line.Substring(1, line.Length - 2).Trim();
            if (inner.Length == 0)
                continue;
            string groupName = null;
            string codeName;
            int slashIndex = inner.IndexOf('\\');
            if (slashIndex >= 0)
            {
                groupName = inner.Substring(0, slashIndex).Trim();
                codeName  = inner.Substring(slashIndex + 1).Trim();
            }
            else
            {
                codeName = inner;
            }
            // If the group changed, emit a "group-only" cheat
            if (!string.IsNullOrEmpty(groupName) &&
                !string.Equals(groupName, currentPnachGroup, StringComparison.Ordinal))
            {
                StartNewCheatWithLabel(groupName);  // Group 1, Group 2, etc.
                currentPnachGroup = groupName;
            }
            // Now start the actual cheat for this section
            if (!string.IsNullOrEmpty(codeName))
            {
                StartNewCheatWithLabel(codeName);   // Hello world, NEW TO ME, etc.
            }
            // Next lines (patch=...) will attach to this cheat
            continue;
        }
        // patch=1,EE,ADDR,extended,VALUE
        if (line.StartsWith("patch=", StringComparison.OrdinalIgnoreCase))
        {
            string[] partsComma = line.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            if (partsComma.Length >= 5)
            {
                string addr = partsComma[2].Trim();
                string val  = partsComma[4].Trim();
                // Strip trailing comments or junk (just in case)
                int commentIdx = addr.IndexOfAny(new[] { ' ', '\t', '#', ';', '/' });
                if (commentIdx >= 0) addr = addr.Substring(0, commentIdx);
                commentIdx = val.IndexOfAny(new[] { ' ', '\t', '#', ';', '/' });
                if (commentIdx >= 0) val = val.Substring(0, commentIdx);
                if (uint.TryParse(addr, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _) &&
                    uint.TryParse(val,  NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _))
                {
                    var cheatForLine = EnsureCurrentCheat();
                    Cheat.cheatAppendCodeFromText(cheatForLine, addr, val);
                }
            }
            continue;
        }
		// In PNACH mode, anything that's not [] or patch= or header/meta is ignored.
        continue;
    }
    // ----------------------------
    // Generic input parsing (RAW / ARMAX)
    // ----------------------------
    string[] parts = line.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
    if (parts.Length == 0)
        continue;
    bool handledCode = false;
    // ARMAX encrypted input: treat the last token as the text code
if (isArmaxEncryptedInput || _inlineInputMode)   // NEW: allow ARMAX text in INLINE mode
{
    string lastToken = parts[parts.Length - 1];
    uint w0, w1;
    if (Armax.IsArMaxStr(lastToken) &&
        Armax.TryParseEncryptedLine(lastToken, out w0, out w1))
    {
        var cheatForLine = EnsureCurrentCheat();
        Cheat.cheatAppendOctet(cheatForLine, w0);
        Cheat.cheatAppendOctet(cheatForLine, w1);
        handledCode = true;
    }
}

    // Default: ADDR VALUE hex pairs on a line
if (!handledCode && parts.Length >= 2)
{
    string addr = parts[parts.Length - 2];
    string val  = parts[parts.Length - 1];
    if (uint.TryParse(addr, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _) &&
        uint.TryParse(val,  NumberStyles.HexNumber, CultureInfo.InvariantCulture, out _))
    {
        var cheatForLine = EnsureCurrentCheat();
        Cheat.cheatAppendCodeFromText(cheatForLine, addr, val);
        handledCode = true;
    }
}
    // If it wasn't recognized as a code line, treat it as a *name* line
    if (!handledCode)
    {
        StartNewCheatWithLabel(line);
    }
}
		// Drop any cheats that ended up with no codes,
		// EXCEPT when we need heading-style entries:
		//  - AR MAX + Make Organizers (folders)
		//  - GS3 / XP-style headings
		//  - CodeBreaker (CBC) group labels (sw=4)
		bool keepEmptyCheatsAsFolders =
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_ARMAX && ConvertCore.g_makefolders) ||
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_GS3) ||
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_CB) ||
			(ConvertCore.g_outdevice == ConvertCore.Device.DEV_STD);
		for (int i = cheats.Count - 1; i >= 0; i--)
{
		if (cheats[i].codecnt == 0 && !keepEmptyCheatsAsFolders)
    {
			cheats.RemoveAt(i);
			labels.RemoveAt(i);
    }
}
    if (cheats.Count == 0)
    {
        txtOutput.Text = "// No valid code lines parsed from input.";
        return;
    }
    // Game ID (hex)
    if (uint.TryParse(txtGameId.Text.Trim(), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint gameId))
        ConvertCore.g_gameid = gameId;
    else
        ConvertCore.g_gameid = 0;
    // NOTE:
    // We *do not* touch g_region, g_verifiermode, g_gs3key here.
    // Those are controlled via the Options menu (ARMAX Verifier Mode, Region, GS3 Key, etc.)
Cheat.cheatClearFolderId();
// Track an error code per cheat instead of bailing on the first one.
int[] perCheatErrorCodes = new int[cheats.Count];
try
{
    for (int i = 0; i < cheats.Count; i++)
    {
        var cheat = cheats[i];

        // NEW: INLINE mode – pick the input crypt/device per-cheat from its label.
        if (_inlineInputMode)
        {
            string? labelForCheat = (i < labels.Count) ? labels[i] : null;

            if (InlineCryptParser.TryGetCryptFromLabel(labelForCheat, out var inlineCrypt))
            {
                // Override the globals for this one conversion.
                ConvertCore.g_incrypt  = inlineCrypt;
                ConvertCore.g_indevice = InlineCryptParser.GetDeviceForCrypt(inlineCrypt);
            }
            // If no CRYPT_XXXX tag found, we just keep whatever
            // g_indevice/g_incrypt were set to from the menu.
        }

        // Auto-pull AR2 key from 0E3C7DF2 XXXXXXXX, and strip that header
        // out of the cheat before we run ConvertCode.
        TryApplyAr2KeyFromCheatHeader(cheat);

        int ret = ConvertCore.ConvertCode(cheat);
        perCheatErrorCodes[i] = ret;
        // NOTE: we intentionally do NOT break here.
        // Even if this cheat fails, we continue with the others.
    }
}
catch (Exception ex)
{
    txtOutput.Text = "// Error during conversion:\r\n// " + ex.Message;
    return;
}
// Sync UI from current globals after conversion.
// If ARMAX input was used, DecryptCode may have discovered game ID/region
// from the verifier header; reflect that here so the user can reuse them.
if (ConvertCore.g_gameid != 0)
{
    SyncGameIdFromGlobals();
}
// Keep the ARMAX Region menu in sync with g_region (0 = USA, 1 = PAL, 2 = Japan)
UpdateRegionMenuChecks();
// Are we outputting ARMAX encrypted text?
bool outputAsArmaxEncrypted =
    ConvertCore.g_outdevice == ConvertCore.Device.DEV_ARMAX &&
    ConvertCore.g_outcrypt  == ConvertCore.Crypt.CRYPT_ARMAX;
// Are we outputting PNACH (RAW) format?
bool outputAsPnachRaw = _outputAsPnachRaw;
var sb = new StringBuilder();
// Optional PNACH header at the top of the file
if (outputAsPnachRaw)
{
    // Prefer explicit PNACH game name if set, otherwise use the UI textbox.
    string gameName = !string.IsNullOrWhiteSpace(_pnachGameName)
        ? _pnachGameName!
        : txtGameName.Text?.Trim();
    if (string.IsNullOrEmpty(gameName))
        gameName = "Unknown Game";
    // Keep UI in sync with what we actually use
    txtGameName.Text = gameName;
	sb.AppendLine($"// Converted by Omniconvert C#");
    if (_pnachCrcActive && _pnachCrc.HasValue)
    {
        // CRC goes to the OUTPUT WINDOW as text
		sb.AppendLine($"// GameName: {gameName}");
        sb.AppendLine($"// CRC: {_pnachCrc.Value:X8}");
    }
    else
    {
        sb.AppendLine($"// NO CRC/NAME SET");
    }
    sb.AppendLine();
}
string currentGroupForPnach = null;   // NEW: track current PNACH group
for (int idx = 0; idx < cheats.Count; idx++)
{
    var cheat = cheats[idx];
    string? label = labels[idx];
    // 1) try user label line
    // 2) otherwise auto-detected cheat.name (Enable Code / (M) / Unnamed)
    string? header = !string.IsNullOrWhiteSpace(label)
        ? label
        : (!string.IsNullOrWhiteSpace(cheat.name) ? cheat.name : null);
// Start from the original header
    string? effectiveHeader = header;

    // INLINE-specific label tweak:
    if (_inlineInputMode && !string.IsNullOrWhiteSpace(effectiveHeader))
    {
        if (outputAsPnachRaw)
        {
            // PNACH output: remove ", CRYPT_XXXX" entirely
            effectiveHeader = InlineCryptParser.StripCryptTag(effectiveHeader);
        }
        else
        {
            // Non-PNACH output: update tag to match OUTPUT crypt
            effectiveHeader = InlineCryptParser.UpdateCryptTagForOutput(
                effectiveHeader,
                ConvertCore.g_outcrypt);
        }
    }

    // Keep internal name in sync for other exporters (ARMAX bin, etc.)
    if (!string.IsNullOrEmpty(effectiveHeader))
    {
        cheat.name = effectiveHeader;
    }
    // -------------------------
    // PNACH RAW small-output
    // -------------------------
    if (outputAsPnachRaw)
{
    // A cheat with no codes is treated as a "group" header (Group 1, Group 2, ...)
    if (cheat.codecnt == 0)
{
    if (!string.IsNullOrWhiteSpace(effectiveHeader))
        currentGroupForPnach = effectiveHeader.Trim();
    continue;
}

// Name for this section
string baseName = effectiveHeader;
if (string.IsNullOrWhiteSpace(baseName))
    baseName = $"Code {idx + 1}";
    // Combine with current group if any: Group1\Hello world
    string finalName = currentGroupForPnach != null
        ? $"{currentGroupForPnach}\\{baseName}"
        : baseName;
    // Look up this cheat's error code
    int errorCodeForCheat = (perCheatErrorCodes != null && idx < perCheatErrorCodes.Length)
        ? perCheatErrorCodes[idx]
        : 0;
    if (errorCodeForCheat != 0)
    {
        // Just emit an error block instead of bogus patch lines
        AppendErrorCommentBlock(sb, finalName, errorCodeForCheat);
        continue;
    }
    // [Group1\Hello world]
    sb.AppendLine($"[{finalName}]");
    // patch=1,EE,ADDR,extended,VALUE
    for (int i = 0; i < cheat.codecnt; i += 2)
    {
        uint addr = cheat.code[i];
        uint val  = (i + 1 < cheat.codecnt) ? cheat.code[i + 1] : 0u;
        sb.AppendLine($"patch=1,EE,{addr:X8},extended,{val:X8}");
    }
    sb.AppendLine();  // blank line between sections
    continue;         // skip normal RAW / ARMAX formatting
}
    // -------------------------
// Non-PNACH output paths
// -------------------------
int errorCodeForCheatNonPnach = (perCheatErrorCodes != null && idx < perCheatErrorCodes.Length)
    ? perCheatErrorCodes[idx]
    : 0;
if (errorCodeForCheatNonPnach != 0)
{
    // When there is an error, just print a neat error block
    string nameForError = effectiveHeader;
if (string.IsNullOrWhiteSpace(nameForError))
    nameForError = $"Code {idx + 1}";
    AppendErrorCommentBlock(sb, nameForError, errorCodeForCheatNonPnach);
}
else
{
    if (!string.IsNullOrEmpty(effectiveHeader))
{
    sb.AppendLine(effectiveHeader);
}
    if (outputAsArmaxEncrypted)
    {
        // Encrypted ARMAX text output: XXXXX-XXXXX-XXXXX
        for (int i = 0; i + 1 < cheat.codecnt; i += 2)
        {
            string armCode = Armax.FormatEncryptedLine(cheat.code[i], cheat.code[i + 1]);
            sb.AppendLine(armCode);
        }
    }
    else
    {
        // Default: two octets per line (ADDR VALUE)
        for (int i = 0; i < cheat.codecnt; i += 2)
        {
            uint addr = cheat.code[i];
            uint val  = (i + 1 < cheat.codecnt) ? cheat.code[i + 1] : 0u;
            sb.AppendLine($"{addr:X8} {val:X8}");
        }
    }
    // Blank line between *successful* cheats (but not after the last one)
    if (idx + 1 < cheats.Count)
        sb.AppendLine();
}
// Remember this conversion for "Save As AR MAX (.bin)"
_lastConvertedCheats = cheats;
_lastGameId = ConvertCore.g_gameid;
}
txtOutput.Text = sb.ToString();
}
/// <summary>
/// Append a small comment block for a cheat that failed to convert.
/// </summary>
private static void AppendErrorCommentBlock(StringBuilder sb, string logicalName, int errorCode)
{
    if (sb == null) throw new ArgumentNullException(nameof(sb));
    if (string.IsNullOrWhiteSpace(logicalName))
        logicalName = "Code";
    sb.AppendLine($"// [ERROR] {logicalName}");
    sb.AppendLine($"// Error code: {errorCode}");
    // Translate the numeric error into a friendly message (Translate.cs).
    string message = Translate.TransGetErrorText(errorCode);
    if (!string.IsNullOrWhiteSpace(message))
    {
        foreach (string rawLine in message.Replace("\r", string.Empty).Split('\n'))
        {
            string line = rawLine.TrimEnd();
            if (line.Length == 0)
                continue;
            sb.AppendLine("// " + line);
        }
    }
    // Extra blank line after the error block for readability
    sb.AppendLine();
}
    }
}