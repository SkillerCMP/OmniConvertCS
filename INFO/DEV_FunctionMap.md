# OmniConvertCS v1.03 – Function Map

This is a **high-level map of where things live** in the v1.02 baseline,  
updated with features **as of v1.03** (drag & drop, ARMAX button glyphs/notes, PNACH filename tweaks).

---

## Root

### Program.cs
- `Program.Main()` – WinForms entry point.
  - Enables visual styles and runs `Gui.MainForm`.

### OmniconvertCS.csproj
- Project definition: target framework, WinForms settings, resources (`OmniconvertCS.ico`), etc.

---

## Core – Data Structures & Common Helpers

### Core/cheat_t.cs
Managed equivalents of the original `cheat_t` / `game_t` structs.

- `class cheat_t`
  - Fields for `id`, `name`, `comment`, code list, flags, and ARMAX folder IDs.
  - Uses managed collections instead of raw pointers.
- `class game_t`
  - Minimal fields: `id`, `name`, `cnt` (cheat count).

### Core/Cheat.cs
Helper API around `cheat_t` – port of `cheat.c` behaviour.

- Constants such as:
  - `NAME_MAX_SIZE`, `CODE_INVALID`, `CODE_NO_TARGET`, `FLAG_MCODE`, etc.
- Key methods:
  - `cheatInit(cheat_t cheat, uint id, string name)`  
    Initialize a cheat with name/id and default state.
  - `cheatAppendCodeFromText(cheat_t cheat, string text)`  
    Parse a single code line (address/value) and append.
  - `cheatAppendOctet(...)`, `cheatPrependOctet(...)`, `cheatRemoveOctets(...)`  
    Low-level code-word manipulations (insert/remove words).
  - `cheatFinalizeData(cheat_t cheat)`  
    Final size/bookkeeping once code list is assembled.
  - `cheatDestroy(cheat_t cheat)`  
    Clear fields / code lists.

### Core/Common.cs
Shared utility helpers used across the project.

- Validation helpers:
  - `IsHexStr(string)`, `IsNumStr(string)`, `IsEmptyStr(string)`.
- Bit/byte helpers:
  - `swapbytes(uint)` – endian swapping.
- Text helpers used by the legacy code:
  - `MsgBox(IntPtr hwnd, int flags, string message)` – wrapper for user messages.
  - `AppendText(...)`, `PrependText(...)`, `AppendNewLine(...)` – legacy-style text building used in some device writers.

---

## Core – Conversion Pipeline

### Core/ConvertCore.cs
Central static class that holds global conversion state and the main “cheat pipeline”.

- Global enums & state:
  - `enum Device` – AR1, AR2, ARMAX, CB, GS3, GS5, RAW, PNACH, etc.
  - `enum Crypt` – `CRYPT_AR1`, `CRYPT_AR2`, `CRYPT_ARMAX`, `CRYPT_CB`, `CRYPT_GS3`, `CRYPT_GS5`, `CRYPT_MAXRAW`, `CRYPT_RAW`, etc.
  - `g_indevice`, `g_outdevice` – current input/output device.
  - `g_incrypt`, `g_outcrypt` – current input/output crypt mode.
  - `g_gameid` – numeric game ID used for some formats.
  - `ar2seeds`, ARMAX verifier settings, GS3 key version, etc.

- Main pipeline methods:
  - `DecryptCode(cheat_t cheat)`  
    Decrypts a single cheat from the selected input device/crypt
    into an internal “standard” representation (RAW words).
  - `TranslateCode(cheat_t cheat)`  
    Calls into `Translate.TransBatchTranslate` to convert opcodes between
    device formats (ARMAX ↔ CB ↔ GS ↔ RAW, etc.).
  - `EncryptCode(cheat_t cheat)`  
    Re-encrypts a translated cheat according to `g_outdevice` / `g_outcrypt`
    using AR2/ARMAX/CB/GS3 helpers.
  - `ConvertCode(cheat_t cheat)`  
    **Top-level per-cheat pipeline**:
    1. Decrypt from input mode.
    2. Translate between devices.
    3. Encrypt to output mode.  
    Returns an error code (0 = success) for per-cheat error reporting.

- Also holds:
  - ARMAX disc hash helpers (using `Devices.Armax`).
  - CBC save version flags.
  - PNACH/RAW handling helpers used by the UI.

### Core/Translate.cs
Implements the internal **opcode translation engine** (port of `translate.c`).

- Bit helpers:
  - `ADDR`, `LOHALF`, `HIHALF`, `LOBYTE32`, `HIBYTE32`.
  - `GET_STD_CMD`, `GET_ARM_CMD`, `GET_ARM_TYPE`, `GET_ARM_SUBTYPE`, `GET_ARM_SIZE`.
  - `MAKE_STD_CMD`, `MAKE_ARM_CMD`, `MAKEWORD32`, `MAKEHALF`.
- Internal translation helpers:
  - `TransSmash(...)`, `TransExplode(...)` – handle compression/expansion of ARMAX vs. standard code streams.
- Public/internal surface:
  - `TransBatchTranslate(List<uint> code, ...)`  
    Called from `ConvertCore.TranslateCode` – translates whole code streams.
  - `TransSetErrorSuppress(bool)`, `TransToggleErrorSuppress()`  
    Control whether translation errors are thrown or accumulated.
  - `TransGetErrorText()`  
    Returns the last human-readable translation error for UI display.

### Core/InlineCryptParser.cs
Powers **INLINE crypt mode**.

- `TryGetCryptFromLabel(string label, out ConvertCore.Crypt crypt)`  
  - Scans a label for `", CRYPT_XXXX"` and maps it to a `ConvertCore.Crypt`.
  - Returns `true` for known tags (AR1/AR2/ARMAX/CB/CB7_COMMON/GS3/GS5/MAXRAW/RAW).
- `StripCryptTag(string label)`  
  - Returns the label with any `", CRYPT_XXXX"` suffix removed.
- `UpdateCryptTagForOutput(string label, ConvertCore.Crypt outCrypt)`  
  - For INLINE input → non-PNACH output:
    - Rewrites any `, CRYPT_XXXX` to match the **output** crypt.
  - For PNACH(RAW) output:
    - Can be used to drop the crypt tag entirely.
- Private helper:
  - `GetCryptTokenForOutput(ConvertCore.Crypt crypt)`  
    Converts a `Crypt` enum value back to `"CRYPT_XXXX"` text.

---

## Devices – Crypto Engines

### Devices/Ar2.cs
Action Replay 1/2 encryption/decryption.

- Core methods:
  - `Ar2Decrypt(...)`, `Ar2Encrypt(...)` – word-level AR2 crypto.
  - `Ar2SetSeed(...)`, `Ar2GetSeed()` – seed management.
  - `Ar2BatchDecrypt(...)`, `Ar2BatchEncrypt(...)` – batch operations on buffers.
  - `Ar1BatchDecrypt(...)`, `Ar1BatchEncrypt(...)` – AR1 compatibility helpers.
  - `Ar2AddKeyCode(...)` – merges AR2 key codes into a cheat.
- Uses a `NibbleFlip` helper internally.

### Devices/Armax.cs
Action Replay MAX crypto and disc hash / verifier logic.

- Utility/text helpers:
  - `MaxRemoveDashes(string)` – strip dashes from MAX code strings.
  - `IsArMaxStr(string)` – quick ARMAX code detection.
  - `TryParseEncryptedLine(...)` / `FormatEncryptedLine(...)` – parse/format encrypted ARMAX text lines.
- Crypto functions:
  - `ArmBatchDecryptFull(...)`, `ArmBatchEncryptFull(...)` – full ARMAX block crypto.
- Verifiers & disc hash:
  - `ArmReadVerifier(...)`, `ArmMakeVerifier(...)` – ARMAX verifier management.
  - `ArmEnableExpansion(...)` – enable expansion device flags.
  - `ComputeDiscHash(string path)` – compute ARMAX “disc hash” from an ISO/drive.
  - `ArmMakeFolder(...)` – helper for ARMAX folder entries.

### Devices/Cb2Crypto.cs
CodeBreaker v1/v7/v8 RSA + ARC4 crypto helper.

- RSA & multiplicative inverse:
  - `RSACrypt(...)`, `RSADecode(...)`, `RSAEncode(...)`.
  - Internal: `MulInverse`, `MulEncrypt`, `MulDecrypt`.
- ARC4 and state helpers:
  - `CBReset()`, `CBSetCommonV7()` – configure CBC v7 common keys.
  - `CBCryptFileData(...)` – encrypt/decrypt payloads for CBC files.
- Code encryption:
  - `CB1EncryptCode(...)`, `CB1DecryptCode(...)` – legacy v1.
  - `CBEncryptCode(...)`, `CBDecryptCode(...)`.
  - `CBBatchDecrypt(...)`, `CBBatchEncrypt(...)`.

### Devices/Gs3.cs
GameShark v3 / v5 stream cipher and verifier support.

- CRC & seed helpers:
  - `SetVersion5()`, `IsVersion5()` – GS3 vs GS5 behavior.
  - `CcittCrc(...)`, `GenCrc(...)`, `Crc32(...)`, `GenCrc32(...)`.
  - `InitMtStateTbl(...)`, `GetMtNum(...)`, `BuildByteSeedTbl(...)`, `ReverseSeeds(...)`, `BuildSeeds(...)`.
- Crypto:
  - `Init(...)`, `Update(...)` – initialize/update cipher state.
  - `Encrypt(...)`, `Decrypt(...)`.
  - `BatchEncrypt(...)`, `BatchDecrypt(...)`.
- Verifier helpers:
  - `CreateVerifier(...)`, `AddVerifier(...)`.
- File crypto:
  - `CryptFileData(...)` – used by P2M/CBC flows.

---

## Devices – Loaders (File → cheat_t)

### Devices/Load/ArmaxBinReader.cs
Reader for **ARMAX `.bin` codelist** files (`PS2_CODELIST` layout).

- Internal:
  - Validates header (`LIST_IDENT`, header size).
  - Handles two layout variants for game headers (writer-style and official).
- Public surface:
  - `Result` class – holds `List<cheat_t> Cheats` + game name/ID metadata.
  - `Load(string path)` (via public static entry) – read file, decode ARMAX payload using `Devices.Armax`, populate cheats.
- **As of v1.03:**
  - Wide-string reader (`ReadWideStringZeroTerminated`) now maps embedded ARMAX **button glyph code units**
    (e.g. `0x0010`–`0x001D`) into readable tokens like `{X}`, `{Square}`, `{Triangle}`, `{Circle}`,
    `{L1}`, `{L2}`, `{R1}`, `{R2}`, `{Up}`, `{Right}`, `{Down}`, `{Left}` in both names and notes.
  - When non-empty game or cheat notes are present, they are appended in braces to the label text, e.g.  
    `Infinite Time {Press {L1}+{L2}+{X} to activate.}`.

### Devices/Load/CbcReader.cs
Reader for **CodeBreaker `.cbc`** Day1/CFU files.

- Constants for CBC v7/v8 headers; `CBC_FILE_ID`.
- `Result` class:
  - `string GameName`, `List<cheat_t> Cheats`.
- `Load(string path)`:
  - Detects v7 vs v8 CFU layout.
  - Decrypts payload with `Cb2Crypto`.
  - Parses the internal cheat list into `cheat_t` objects.

### Devices/Load/P2mReader.cs
Reader for **GameShark v5 / XP `.p2m`** archives.

- Handles `P2MS` header and file descriptor table.
- `Load(string path)`:
  - Validates header, reads file descriptors.
  - Locates and decrypts `user.dat` using **GS3** logic.
  - Builds a `Result` containing extracted cheats and metadata.

---

## Devices – Writers / “Save As”

### Devices/SaveAs/ArmaxBinWriter.cs
Writer for **ARMAX `.bin` codelist** files (C# port of `armlist.c`).

- `CreateList(List<cheat_t> cheats, game_t game, string fileName)`  
  - Mirrors `alCreateList`:
    - Computes space for names/comments/flags.
    - Builds ARMAX list header and cheat blocks.
    - Writes out the `PS2_CODELIST` file.
- Uses `game.name` from the **Game Name** textbox in `MainForm` for the list’s internal game title.

### Devices/SaveAs/CbcWriter.cs
Writer for **CodeBreaker `.cbc`** files.

- `CreateFile(List<cheat_t> cheats, game_t game, string fileName, bool doHeadings)`  
  - C# port of `cbcCreateFile`.
  - Can emit:
    - v7 Day1 header + encrypted payload.
    - v8 CFU header + encrypted payload.
  - Uses `Cb2Crypto` for encryption and CRC.
  - Uses `game.name` as the file’s game title.

### Devices/SaveAs/P2m.cs
Writer for **GameShark v5+ `.p2m`** archives.

- Contains C# versions of `p2mheader_t`, `p2mfd_t`, `p2mdate_t`, constants, etc.
- `CreateFile(List<cheat_t> cheats, game_t game, string path, bool doHeadings)`  
  - Builds a P2M archive with code list / user data.
  - Uses `Gs3` crypto for the embedded `user.dat`.
  - Uses `game.name` as the internal game name.

### Devices/SaveAs/SwapMagicBinWriter.cs
Writer for **Swap Magic Coder `.bin`** files.

- `CreateFile(List<cheat_t> cheats, game_t game, string path)`  
  - Port of `scfCreateFile`.
  - Builds Swap Magic game header + cheat list.
  - Uses `Common.MsgBox` to report cases where no cheats are valid.

### Devices/SaveAs/PnachWriter.cs
Simple helper to save PNACH text.

- `SaveFromText(string path, string? pnachText)`  
  - Writes `pnachText` as UTF-8 to disk (null → empty string).

### Devices/SaveAs/Crc32.cs
CRC-32 implementation used by file writers.

- `Compute(ReadOnlySpan<byte> buffer, uint crc)`  
  - Standard CRC-32 with final XOR, using a precomputed table.
  - Matches original `crc32.c` behaviour.

---

## Devices – Options & PNACH CRC

### Devices/Options/PnachCrcHelper.cs
Manages **PNACH CRC mappings** via `PnachCRC.json`.

- `CrcEntry` type:
  - `uint Crc`, `string GameName`, `string ElfName`.
- CRC calculation:
  - `ComputeElfCrc(string elfPath)`  
    Reads a PS2 ELF and computes the PCSX2 “Game CRC” value.
- Mapping management:
  - `GetEntries()` – load all entries from `PnachCRC.json`.
  - `TryGetGameName(uint crc, out string gameName)`  
  - `TryGetElfName(uint crc, out string elfName)`  
  - `AddOrUpdate(uint crc, string elfName, string gameName)`  
    - Adds/updates entries and saves them.
- Private helpers:
  - `GetMappingPath()` – returns path to `PnachCRC.json`.
  - `LoadEntries()`, `SaveEntries(List<CrcEntry>)` – JSON round-trip.

---

## UI – Main Form (Core Behaviour)

### UI/MainForm/MainForm.Designer.cs
Designer-generated layout for the main window.

- Declares:
  - Input/Output text boxes.
  - Convert / Clear buttons.
  - MenuStrip (`File`, `Edit`, `Input`, `Output`, `Options`).
  - PNACH CRC checkbox and related labels.
  - AR2 key textbox & label.
  - Status text labels for input/output format.
  - Game ID / Game Name fields.
- **As of v1.03:**
  - Enables drag-and-drop on the Input textbox (`AllowDrop = true`) and wires
    `DragEnter` / `DragDrop` to `MainForm.DragDrop` handlers for file loading.

### UI/MainForm/MainForm.cs
Main behavior for the form; **core event handlers and convert pipeline entry**.

- Startup:
  - `MainForm_Load(...)` – initialization:
    - Loads app settings.
    - Initializes crypt/device menus.
    - Syncs AR2 key / PNACH UI.
- Convert:
  - `btnConvert_Click(...)` – the main “Convert” button handler:
    - Reads input text and current Input/Output devices.
    - Builds `cheat_t` list from input (RAW/PNACH/INLINE, etc.).
    - Calls `ConvertCore.ConvertCode(...)` per cheat.
    - Appends PNACH headers / group headings as needed.
    - Writes final text to `txtOutput`.
  - Uses `AppendErrorCommentBlock(...)` to include **per-cheat error comments** when non-zero error codes are returned.
- GS3 key menu handlers:
  - `menuOptionsGs3Key0_Click(...)` … `menuOptionsGs3Key4_Click(...)`  
    - Update `ConvertCore.g_gs3key` and refresh the UI.
- Utility:
  - `CleanCbSiteFormat(string)` – helper to clean up CodeTwink/GameHacking.org style pasted text.

### UI/MainForm/MainForm.Ar2Helpers.cs
Helpers specific to AR2 key handling in the UI.

- `RefreshAr2KeyDisplayFromSeed()`  
  - Shows/hides the AR2 key line based on current input crypt.
  - Updates `lblAr2CurrentKey` / `txtAr2CurrentKey` contents.
- `TryApplyAr2KeyFromCheatHeader(cheat_t cheat)`  
  - Looks at cheat headers (e.g. AR2 key codes) and auto-applies them
    to `ConvertCore.ar2seeds` / UI when possible.

### UI/MainForm/MainForm.CryptMenus.cs
Centralized handling for Input/Output crypt menus and device options.

- `CryptMenuItem_Click(...)`  
  - Handles all “Input/Output → (Device/Crypt)” menu clicks.
  - Updates `ConvertCore.g_indevice`, `g_outdevice`, `g_incrypt`, `g_outcrypt`.
  - Updates `lblInputFormat`, `lblOutputFormat`.
- Helpers:
  - `BuildCryptMenuItem(...)`, `FindCryptOption(...)`  
    - Internal helpers to construct and locate menu options.
  - Holds the table of `CryptOption` entries, including **INLINE** input mode.

### UI/MainForm/MainForm.EditMenu.cs
Edit menu and associated shortcuts.

- `SwapInputOutput()`  
  - Swaps text between Input/Output boxes and updates format labels.
- Clipboard actions:
  - `menuEditCopy_Click`, `menuEditCut_Click`, `menuEditPaste_Click`, `menuEditSelectAll_Click`.
- Clear handlers:
  - `menuEditClearInput_Click`, `menuEditClearOutput_Click`.
  - `btnClearInput_Click`, `btnClearOutput_Click`.

### UI/MainForm/MainForm.FileMenu.cs
File load/save behaviour for all supported formats.

- **Shared load helpers (new in v1.03):**
  - `LoadTextFromPath(string path)` – central text-file load logic (used by menu + drag & drop).
  - `LoadCbcFromPath(string path)` – wraps `CbcReader.Load` and pushes result into Input/Game Name.
  - `LoadArmaxBinFromPath(string path)` – wraps `ArmaxBinReader.LoadAsArmaxEncryptedTextWithGames`, handles single vs multi-game lists.
  - `LoadP2mFromPath(string path)` – wraps `P2mReader.Load`, fills Game Name and Input.
- Load menu handlers:
  - `menuFileLoadText_Click(...)` – show `OpenFileDialog` then call `LoadTextFromPath`.
  - `menuFileLoadArmaxBin_Click(...)` – show dialog then call `LoadArmaxBinFromPath`.
  - `menuFileLoadCbc_Click(...)` – show dialog then call `LoadCbcFromPath`.
  - `menuFileLoadP2m_Click(...)` – show dialog then call `LoadP2mFromPath`.
- Save menu handlers:
  - `menuFileSaveAsText_Click(...)` – save Output as plain text.
  - `menuFileSaveAsPnach_Click(...)` – PNACH text via `PnachWriter`:
    - Warns if Output format is not Pnach(RAW).
    - **As of v1.03**, when PNACH CRC is active and ELF/CRC are known,
      pre-fills the filename as `ELFNAME_CRC.pnach`; otherwise falls back to `PUTNAME.pnach`.
  - `menuFileSaveAsArmaxBin_Click(...)` – ARMAX `.bin` via `ArmaxBinWriter`, using last converted cheats and `txtGameName`.
  - `menuFileSaveAsCbc_Click(...)` – CBC `.cbc` via `CbcWriter` (Day1/CFU depending on options).
  - `menuFileSaveAsP2m_Click(...)` – P2M archive via `P2m.CreateFile` (XP/GS).
  - `menuFileSaveAsSwapBin_Click(...)` – Swap Magic `.bin` via `SwapMagicBinWriter`.

### UI/MainForm/MainForm.DragDrop.cs
Implements drag-and-drop loading into the Input text box. **(new in v1.03)**

- `IsSupportedLoadExtension(string? ext)`  
  - Returns `true` for `.txt`, `.cbc`, `.bin`, `.p2m` (files that can be loaded into Input).
- `txtInput_DragEnter(object sender, DragEventArgs e)`  
  - Checks for file-drop data and sets `DragDropEffects.Copy` only if at least one supported file is present.
- `txtInput_DragDrop(object sender, DragEventArgs e)`  
  - Extracts the first supported file from the drop and calls `LoadFileIntoInput(path)`.
- `LoadFileIntoInput(string path)`  
  - Switches on file extension and delegates to the shared load helpers in `MainForm.FileMenu`:
    - `.cbc` → `LoadCbcFromPath`
    - `.bin` → `LoadArmaxBinFromPath`
    - `.p2m` → `LoadP2mFromPath`
    - `.txt` / default → `LoadTextFromPath`
  - Ensures drag & drop loading behaves identically to the File → Load menu.

### UI/MainForm/MainForm.GameIdAndPnachUi.cs
Helpers for Game ID fields and PNACH CRC UI.

- `SyncGameIdFromGlobals()` / `SyncGameIdToGlobals()`  
  - Synchronize the three game ID text boxes with `ConvertCore.g_gameid`.
- `txtGameId*_TextChanged(...)`  
  - Validate and update the global game ID on edits.
- `RefreshArmaxGameIdDisplay()`  
  - Reflect changes for ARMAX lists and relevant UI.

### UI/MainForm/MainForm.OptionsMenu.cs
Handlers for the **Options** menu.

- `menuOptionsArmaxOptions_Click(...)` – opens the ARMAX options dialog.
- `menuOptionsArmaxDiscHashNone_Click(...)` / related handlers  
  - Configure ARMAX disc hash behaviour.
- `menuOptionsArmaxVerifiersAuto_Click(...)` / `Manual_Click(...)`  
  - Toggle ARMAX verifier mode.
- `menuOptionsCbcSaveVersion7_Click(...)` / `Version8_Click(...)`  
  - Set CBC v7 vs v8 file output.
- `menuOptionsGs3KeyX_Click(...)`  
  - Additional wrappers for GS3 key selection.
- `menuOptionsPnachCrc_Click(...)`  
  - Opens `PnachCrcForm` to edit PNACH CRC / ELF / Game Name mappings.
- `menuOptionsMakeOrganizers_Click(...)` (if present)  
  - Stub for future “organizer” output.

### UI/MainForm/MainForm.Settings.cs
User settings persistence.

- Internal `AppSettings` class:
  - Stores last used Input/Output device + crypt, game ID, window size/position, CBC save version, disc hash settings, INLINE state, etc.
- `GetSettingsPath()`  
  - Returns path to `OmniconvertSettings.json`.
- `LoadAppSettings()`  
  - Reads JSON (if present) and populates:
    - `ConvertCore.g_indevice`, `g_outdevice`, `g_incrypt`, `g_outcrypt`.
    - `ConvertCore.g_gameid`.
    - Window geometry and UI state.
- `SaveAppSettings()`  
  - Serializes and writes `OmniconvertSettings.json`.
- `OnFormClosing(...)`  
  - Overrides base to call `SaveAppSettings()` automatically on exit.

---

## UI – Dialogs & Forms

### UI/Ar2KeyForm.cs (+ Designer)
Dialog for managing AR2 key seeds / codes.

- Initializes from `ConvertCore.ar2seeds`.
- `btnApply_Click(...)`, `btnReset_Click(...)`, `btnCancel_Click(...)`.
- `txtKey_KeyPress(...)` – validation for key input.
- `cmbCommonKeys_SelectedIndexChanged(...)` – quickly select from known keys.

### UI/ArmaxOptionsForm.cs (+ Designer)
Dialog for ARMAX-specific options and disc hash configuration.

- `PopulateDiscHashCombo(...)` – fill drop-down with known drives/paths.
- `btnComputeDiscHash_Click(...)` – compute a disc hash using `Armax.ComputeDiscHash`.
- `btnOk_Click(...)`, `btnCancel_Click(...)` – apply/cancel configuration.

### UI/PnachCrcForm.cs (+ Designer)
Dialog for PNACH CRC / Game Name / ELF mapping.

- `btnBrowseElf_Click(...)` – choose ELF file; uses `PnachCrcHelper.ComputeElfCrc`.
- `btnComputeFromElf_Click(...)` – compute CRC from a supplied ELF.
- `btnOk_Click(...)`, `btnCancel_Click(...)`.
- Drag-and-drop:
  - `PnachCrcForm_DragEnter(...)`, `PnachCrcForm_DragDrop(...)` – allow dropping ELF files.
- Exposes:
  - `SelectedCrcHex`, `SelectedGameName`, `SelectedElfName` for use in `MainForm`.

---

## Docs / Help / Data Files

### INFO/OmniconvertCS_Help.html
- HTML help for end-users; linked from README and/or future Help menu.
- Describes formats, usage, and examples.
- **Updated for v1.03** to cover:
  - Drag & drop loading into Input.
  - ARMAX button glyph decoding & notes appended to labels.
  - PNACH filename defaults based on ELF + CRC.

### README.md
- Full project overview, supported devices/crypts, and usage notes.
- Mentions INLINE mode, PNACH support, and PNACH CRC helper.
- Should be kept in sync with v1.03 features as they settle.

### CREDITS.md
- Attribution for original Omniconvert, research, and contributors.

### LICENSE
- Project licensing information.

### OmniconvertSettings.json (runtime)
- Saved in the app directory (or alongside the EXE).
- Managed by `MainForm.LoadAppSettings()` / `SaveAppSettings()`.

### PnachCRC.json (runtime)
- Mapping file for PNACH CRC ↔ Game Name ↔ ELF name.
- Managed by `Devices.Options.PnachCrcHelper`.

---

This map is intentionally **not exhaustive** – it’s meant as a quick guide
so you can jump to the right file when you’re working on a feature.

As new v1.03+ features land (e.g. INLINE enhancements, extra loaders/writers, organizer export),
we can keep adding bullets marked “new in v1.03+” under the relevant sections.
