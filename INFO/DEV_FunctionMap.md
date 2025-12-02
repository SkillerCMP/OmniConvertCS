# OmniConvertCS v1.00 – Function Map

This is a **high-level** map of where things live. It focuses on entry points and device-specific logic, not every tiny helper method.

---

## Root

### Program.cs
- `Program.Main()` – WinForms startup; enables visual styles and runs `MainForm`.

### OmniconvertCS.csproj
- Project configuration: target framework, WinForms settings, resources (`OmniconvertCS.ico`), etc.

---

## Core

### Core/cheat_t.cs
- `class cheat_t`
  - Managed equivalent of the original `cheat_t` struct.
  - Fields: `id`, `name`, `comment`, `flag[3]` (default-on / master-code / comments).
  - `List<uint> code` – dynamic list of 32‑bit code words.
  - `int codecnt` – read‑only mirror of `code.Count`.
  - `byte state`, `cheat_t? nx` – legacy state/linked‑list fields kept for parity.

### Core/Cheat.cs
- `cheatInit(cheat_t)` – initialize a new cheat (defaults, cleared lists).
- `cheatAppendOctet(cheat_t, uint val)` – append a 32‑bit word to a cheat.
- `cheatAppendCodeFromText(cheat_t, string line, bool allowComments, ...)`
  - Parse `XXXXXXXX YYYYYYYY` text lines (plus optional comments) into code words.
  - Handles label/comment text and updates `code` + `codecnt`.
- `cheatPrependOctet(...)` – insert a 32‑bit word at the front.
- `cheatRemoveOctets(...)` – remove a range of code words.
- `cheatFinalizeData(cheat_t)` – any final tidy-up once codes are loaded.
- `cheatClearFolderId(List<cheat_t>)` – clears folder IDs for a list.
- `cheatDestroy(cheat_t)` – clear lists and reset fields (managed clean‑up).

### Core/Common.cs
- `IsHexStr(string)` / `IsNumStr(string)` / `IsEmptyStr(string)` – small validation helpers.
- `swapbytes(uint)` – endianness helper used by various device crypt routines.
- `MsgBox(string text, string? caption = null)` – wrapper around `MessageBox.Show`.
- `AppendText(TextBox, string)` / `PrependText(TextBox, string)` / `AppendNewLine(TextBox)` – UI helpers for Input/Output text manipulation.

### Core/ConvertCore.cs
Static class that owns **conversion globals** and the main cheat pipeline.

- Global state (fields, enums):
  - `g_incrypt`, `g_outcrypt` – current input/output crypt mode (`Crypt` enum).
  - `g_indevice`, `g_outdevice` – current input/output device (ARMAX, AR2, CB, GS, PNACH, etc.).
  - AR2 seeds, CB version flags, ARMAX disc hash, etc.

- `DecryptCode(cheat_t cheat)`
  - Looks at `g_incrypt` / `g_indevice` and dispatches to:
    - `Armax.ArmBatchDecryptFull`, `Ar2.Ar2BatchDecrypt`, `Cb2Crypto.CBBatchDecrypt`,
      `Gs3.BatchDecrypt`, etc.
  - Produces a standard internal representation in `cheat.code`.

- `TranslateCode(cheat_t cheat)`
  - Calls `Translate.TransBatchTranslate(...)` to convert between device families
    (AR2/ARMAX/CB/GS/RAW/PNACH).
  - Uses translation mode and options chosen in the UI.

- `EncryptCode(cheat_t cheat)`
  - Re‑encrypts the translated cheat according to `g_outcrypt` / `g_outdevice`.
  - Calls into device crypto (ARMAX/CB/GS/AR2) as needed.

- `ConvertCode(cheat_t cheat)`
  - **Top-level pipeline** used by `MainForm`:
    1. Decrypt from input mode.
    2. Translate between devices.
    3. Encrypt to output mode.
  - Returns an error code (0 = success) and sets translation error/success state.

### Core/Translate.cs
Implements the internal **code translation** engine.

- Private helpers:
  - `ADDR`, `LOHALF`, `HIHALF`, `LOBYTE32`, `HIBYTE32` – bit-twiddling helpers.
  - `GET_STD_CMD`, `GET_ARM_CMD`, `GET_ARM_TYPE`, `GET_ARM_SUBTYPE`, `GET_ARM_SIZE` – extract opcode fields.
  - `MAKE_STD_CMD`, `MAKE_ARM_CMD`, `MAKEWORD32`, `MAKEHALF` – compose opcodes/values.

- Public/internal surface:
  - `TransBatchTranslate(List<uint> code, ...)`
    - Main translator for an entire code stream, used by `ConvertCore.TranslateCode`.
    - Handles `TransSmash`/`TransExplode` operations when converting between
      ARMAX/CB/GS/RAW formats.
  - `TransOther(...)`, `TransStdToMax(...)`, `TransMaxToStd(...)`
    - Direction-specific translation helpers.
  - `TransSetErrorSuppress(bool)` / `TransToggleErrorSuppress()`
    - Control whether translation errors throw or are accumulated silently.
  - `TransGetErrorText()`
    - Retrieve last human-readable translation error text for UI display.

---

## Devices – Crypto & Format Logic

### Devices/Armax.cs
AR MAX (Action Replay MAX) text & binary helpers.

- `MaxRemoveDashes(string)` – strip `-` from `XXXXX-XXXXX-XXXXX` strings.
- `IsArMaxStr(string)` – quick detection of ARMAX code strings.
- `TryParseEncryptedLine(string, out uint addr, out uint value, ...)`
  - Parse one encrypted ARMAX line into code words.
- `FormatEncryptedLine(uint addr, uint value, ...)`
  - Build a `XXXXX-XXXXX-XXXXX` style string from code words.
- `ArmBatchDecryptFull(...)` / `ArmBatchEncryptFull(...)`
  - Main ARMAX batch decrypt/encrypt entry points used by `ConvertCore`.
- `ArmReadVerifier(...)` / `ArmMakeVerifier(...)`
  - Handle ARMAX verifier codes.
- `ArmEnableExpansion(...)`
  - Enable/disable ARMAX expansion.
- `ComputeDiscHash(string driveRoot)`
  - Compute the ARMAX disc hash from a PS2 disc; used when “Use Armax Disc Hash” is on.
- `ArmMakeFolder(...)`
  - Create ARMAX folder structures for grouped cheats.

### Devices/Ar2.cs
GameShark / Action Replay 2 crypt.

- `Ar2Decrypt(uint addr, uint value, out uint outAddr, out uint outValue)`
- `Ar2Encrypt(...)`
- `Ar2SetSeed(uint)` / `Ar2GetSeed()`
  - Set/get global AR2 seed value.
- `Ar2BatchDecryptArr(uint[] codes, ...)` / `Ar2BatchDecrypt(List<uint> codes, ...)`
- `Ar2BatchEncryptArr(...)` / `Ar2BatchEncrypt(...)`
  - Batch crypt entry points used by `ConvertCore.DecryptCode` / `EncryptCode`.
- `Ar1BatchDecrypt(...)` / `Ar1BatchEncrypt(...)`
  - Support for AR1 input.
- `Ar2AddKeyCode(cheat_t cheat)`
  - Append an AR2 “key code” line to a cheat, based on current seed.

### Devices/Cb2Crypto.cs
CodeBreaker v1/v7/v8 crypto and RSA helpers.

- `RSACrypt(...)`, `RSADecode(...)`, `RSAEncode(...)`
  - Low‑level RSA routines (used by v7/v8).
- `CBReset()`
  - Reset internal CB crypto state (keys, seeds).
- `CBSetCommonV7()`
  - Set “common v7” keys (BEEF codes, etc.).
- `CBEncryptCode(...)` / `CBDecryptCode(...)`
  - Per‑code encryption/decryption.
- `CBBatchDecrypt(...)` / `CBBatchEncrypt(...)`
  - Batch entry points used by ConvertCore and CBC file load/save.
- `CBCryptFileData(...)`
  - Encrypt/decrypt CBC payload blocks when reading/writing `.cbc`.

### Devices/Gs3.cs
Gameshark 3 / related crypt + verifiers.

- `SetVersion5(bool)` / `IsVersion5()`
  - Manage GS version mode.
- `Init(...)`, `Update(uint addr, uint value)`, `Decrypt(...)`, `Encrypt(...)`
  - Core crypt state machine.
- `BatchDecrypt(...)` / `BatchEncrypt(...)`
  - Batch wrappers for code lists.
- `CreateVerifier(...)` / `AddVerifier(...)`
  - Build and add GS verifier codes.
- `CryptFileData(...)`
  - Encrypt/decrypt GS file payloads.

---

## Devices – Loaders

### Devices/Load/ArmaxBinReader.cs
Binary ARMAX `.bin` loader.

- `Load(string fileName)`
  - Main entry point to read an ARMAX `.bin` codelist and return cheats.
- `LoadAsArmaxEncryptedLines(...)`
  - Read `.bin`, but return encrypted ARMAX text lines instead of decoded cheats.
- `LoadAsArmaxEncryptedTextWithGames(...)`
  - Read `.bin` and group encrypted lines by game (for multi‑game lists).
- Internal helpers:
  - `IsReasonableCheatCount(int)` – sanity‑checking count.
  - `TryReadGameCheatHeader(...)` – parse per‑game headers.
  - `ReadUInt32LE`, `ReadUInt16LE`, `ReadWideStringZeroTerminated` – binary readers.

### Devices/Load/CbcReader.cs
CodeBreaker `.cbc` loader (v7 and v8).

- `Load(string path, bool forceV7 = false)`
  - Main entry: opens `.cbc`, auto‑detects v7 vs v8 where needed.
- `LoadV7(...)` / `LoadV8(...)`
  - Version‑specific readers (Day1 vs CFU).
- `IsCbcV8(...)`
  - Heuristic to detect v8 headers.
- `ParseCheatsFromPayload(...)`
  - Parse the decrypted CBC payload into `cheat_t` objects.
- `ReadAsciiZeroTerminated*`, `ReadUInt16LE`, `ReadUInt32LE`
  - Helpers to decode the structured CBC header/payload.

### Devices/Load/P2mReader.cs
XP/GS P2M loader.

- `Load(string path)`
  - Parse a `.p2m` save and turn it into cheats for conversion.
- Helpers: `ReadUInt32LE`, `ReadUInt16LE`, `ReadFixedAscii`, `ReadWideStringWithTag`.

---

## Devices – Options & PNACH CRC

### Devices/Options/PnachCrcHelper.cs
Manages ELF → CRC → game name mapping via `PnachCRC.json`.

- `ComputeElfCrc(string elfPath)`
  - Compute PCSX2/PNACH “Game CRC” from a PS2 ELF file.
- `GetEntries()`
  - Load all known entries from `PnachCRC.json`.
- `TryGetGameName(uint crc, out string gameName)`
- `TryGetElfName(uint crc, out string elfName)`
  - Look up names by CRC.
- `AddOrUpdate(uint crc, string elfName, string gameName)`
  - Add or update an entry and persist it.
- Private helpers: `GetMappingPath()`, `LoadEntries()`, `SaveEntries()`.

---

## Devices – Writers / Save As

### Devices/SaveAs/ArmaxBinWriter.cs
ARMAX `.bin` writer.

- `CreateList(List<cheat_t> cheats, game_t game, string fileName)`
  - C# port of `alCreateList` – writes a full ARMAX `.bin` list.
  - Returns `0` on success; non‑zero on error.

### Devices/SaveAs/CbcWriter.cs
CodeBreaker `.cbc` writer.

- `CreateFile(List<cheat_t> cheats, string fileName, bool useV7Header, string title, ...)`
  - C# port of `cbcCreateFile`.
  - Writes either:
    - v7 CBC (64‑byte header + payload), or
    - v8 CBC (344‑byte header + payload),
    depending on flags.

### Devices/SaveAs/P2m.cs
XP/GS P2M writer.

- `CreateFile(List<cheat_t> cheats, game_t game, string fileName, bool doHeadings)`
  - C# port of `p2mCreateFile`: creates `.p2m` save from cheats.
- Helpers: `WriteUInt16LE`, `WriteUInt32LE`, `WriteStringW`, `InitHeader`, `WriteHeader`, etc.

### Devices/SaveAs/PnachWriter.cs
Simple PNACH text writer.

- `SaveFromText(string path, string? pnachText)`
  - Writes Output text to a `.pnach` file as UTF‑8.

### Devices/SaveAs/Crc32.cs
Generic CRC32 helper.

- `Compute(byte[] data)`
  - Compute a standard CRC32 for buffers.

---

## UI / Forms

### UI/MainForm.cs
Main application window and primary UI logic.

**File load/save handlers**
- `menuFileLoadText_Click(...)`
  - Open `.txt` / `.pnach` text and put it into the **Input** textbox (runs `CleanCbSiteFormat` on load).
- `menuFileLoadCbc_Click(...)`
  - Load `.cbc` via `CbcReader.Load` and populate Input/Output appropriately.
- `menuFileLoadArmaxBin_Click(...)`
  - Load ARMAX `.bin` via `ArmaxBinReader.Load`.
- `menuFileLoadP2m_Click(...)`
  - Load `.p2m` via `P2mReader.Load`.

- `menuFileSaveAsText_Click(...)`
  - Save **Output** textbox as plain text.
- `menuFileSaveAsPnach_Click(...)`
  - Save **Output** textbox as `.pnach` via `PnachWriter.SaveFromText`.
- `menuFileSaveAsArmaxBin_Click(...)`
  - Save `_lastConvertedCheats` as ARMAX `.bin` via `ArmaxBinWriter.CreateList`.
- `menuFileSaveAsCbc_Click(...)`
  - Save `_lastConvertedCheats` as CodeBreaker `.cbc` via `CbcWriter.CreateFile`.
- `menuFileSaveAsP2m_Click(...)`
  - Save `_lastConvertedCheats` as `.p2m` via `P2m.CreateFile`.
- `menuFileSaveAsSwapBin_Click(...)`
  - Shows `ShowBinaryExportNotImplemented` (placeholder for future Swap Magic export).

**Options / device settings**
- `EnumerateArmaxDiscHashDrives()` / `SetArmaxHashDrive(char)`
  - Build and update the “ARMAX Disc Hash” drive list for Options menu.
- `menuOptionsArmaxDiscHashNone_Click(...)`
  - Disable disc hash usage.
- `menuOptionsArmaxDiscHashDrive_Click(...)`
  - Select a specific drive letter for ARMAX disc hashing.

- `menuOptionsAr2Key_Click(object? sender, EventArgs e)`
  - Open `Ar2KeyForm` dialog to edit AR2 encrypted key code seed.
- `menuOptionsPnachCrc_Click(...)`
  - Open `PnachCrcForm` dialog to compute/select PNACH CRC and game/ELF name.
- `menuOptionsCbcVersion7_Click(...)` / `menuOptionsCbcVersion8_Click(...)`
  - Toggle CB v7/v8 behavior (affects CBC load/save and crypt).

**PNACH CRC checkbox**
- `chkPnachCrcActive_CheckedChanged(object? sender, EventArgs e)`
  - When checked:
    - Ensures output is PNACH.
    - Launches `PnachCrcForm` via `menuOptionsPnachCrc_Click`.
    - Stores selected CRC + game/ELF.
  - When unchecked:
    - Disables CRC emission but retains last selected CRC in memory.

**Convert button & crypt menus**
- `btnConvert_Click(...)`
  - **Main UI entry point** for conversions:
    1. Parse Input textbox into `cheat_t` list via `Cheat.cheatInit`, `cheatAppendCodeFromText`, etc.
    2. For each cheat, call `ConvertCore.ConvertCode`.
    3. Render the converted cheats into the Output textbox according to Output device/crypt.
    4. Update `_lastConvertedCheats` for Save As operations.
- `CryptMenuItem_Click(...)`
  - Handles clicks on Input/Output crypt menu entries to update `ConvertCore.g_incrypt/g_outcrypt`.
- `BuildCryptMenu()`, `FindCryptOption(...)`, `AddCryptMenuItem(...)`
  - Build dynamic crypt-selection menus from the supported device/crypt matrix.

**Input / Output helpers**
- `menuEditClearInput_Click(...)` / `btnClearInput_Click(...)`
- `menuEditClearOutput_Click(...)` / `btnClearOutput_Click(...)`
  - Clear Input/Output textboxes.
- `CleanCbSiteFormat(string raw)`
  - Normalize CodeBreaker‑style web copies into clean `XXXXXXXX YYYYYYYY` lines.
- Other edit helpers: trimming whitespace, normalizing newlines, etc.

**Settings persistence**
- `GetSettingsPath()`
  - Resolve path to `OmniconvertSettings.json`.
- `LoadAppSettings()`
  - Load JSON settings on startup (last window size, input/output device/crypt, disc hash drive, etc.).
- `SaveAppSettings()`
  - Save settings on exit or when options change.

**AR2 header convenience**
- `RefreshAr2KeyDisplayFromSeed()`
  - Update small AR2 key display in the main form from `ConvertCore` seed.
- `TryApplyAr2KeyFromCheatHeader(cheat_t cheat)`
  - If Input is AR2 and cheat starts with the key header (`0E3C7DF2 XXXXXXXX`),
    pull that `XXXXXXXX` as encrypted AR2 key, update `ConvertCore` seed, and
    sync the UI display.

### UI/Ar2KeyForm.cs
Small dialog for editing AR2 encrypted key code.

- `InitializeFromCurrentSeed()`
  - Populate the grid/controls with the current seed from `ConvertCore`.
- `cmbCommonKeys_SelectedIndexChanged(...)`
  - Allow choosing a common preset AR2 key from the dropdown.
- `btnOk_Click(...)`
  - Validate input and commit new seed back to `ConvertCore`.
- `btnReset_Click(...)`
  - Reset AR2 key to default value.
- `txtKey_KeyPress(...)`
  - Restrict key cell input to valid hex digits.

### UI/PnachCrcForm.cs
Dialog for computing/selecting PNACH CRC and mapping it to ELF/game name.

- `btnBrowseElf_Click(...)`
  - Open file dialog for ELF selection and call `SetElfPathAndCompute`.
- `SetElfPathAndCompute(string elfPath)`
  - Compute CRC via `PnachCrcHelper.ComputeElfCrc`.
  - Look up existing entries via `PnachCrcHelper.GetEntries` / `TryGetGameName`.
  - Populate fields and list of known mappings.
- `comboKnown_SelectedIndexChanged(...)`
  - When selecting a known mapping, update the ELF/game fields.
- `btnOk_Click(...)`
  - Commit selected CRC + game/ELF name back to `MainForm`.
- `btnCancel_Click(...)`
  - Close without changing CRC.
- `PnachCrcForm_DragEnter(...)` / `PnachCrcForm_DragDrop(...)`
  - Support drag‑and‑drop of ELF files onto the dialog.

---

## Data / Settings Files

- **OmniconvertSettings.json**
  - Per‑user settings: window geometry, last devices/crypts, disc hash drive, etc.
  - Read/written by `MainForm.LoadAppSettings()` / `MainForm.SaveAppSettings()`.

- **PnachCRC.json**
  - Game/ELF/CRC mappings maintained by `Devices.Options.PnachCrcHelper`.
  - Lines like:
    - `CRC>GameName`
    - `ELFName>CRC>GameName`

---

This map is intentionally **not exhaustive** – it’s meant as a quick navigation guide for future edits. If we add new subsystems later (e.g. Swap Magic export, presets, history), we can append new sections here.
