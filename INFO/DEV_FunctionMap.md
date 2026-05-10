# OmniConvertCS v1.05 – Function Map

This is a **high-level developer map of where things live** in the current v1.05 codebase.

v1.05 moves the project to **.NET 10 / C# 14**, keeps the WinForms GUI, and adds/updates the command-line conversion/analyze tooling. It also includes the tighter RAW/ARMAX text parsing rules so normal title lines such as `Never Have More Than 500000 Cash` are not mistaken for code lines.

This map is intentionally not a full API reference. It is meant to help you quickly jump to the right file when working on a feature, parser issue, conversion issue, or file format export.

---

## Root

### `Program.cs`
Application entry point.

- `Program.Main(string[] args)`
  - Starts **CLI mode** when arguments are supplied.
  - Starts **GUI mode** when no arguments are supplied.
  - Wraps CLI execution with a top-level exception guard so command-line runs do not fail silently.
- `ConsoleBootstrap.EnsureConsole()`
  - Since the app is a WinForms `WinExe`, this attaches to the parent console when launched from `cmd`/batch.
  - Allocates a console when launched with CLI args but no parent console exists.
  - Rebinds `Console.Out`, `Console.Error`, and `Console.In` so CLI/manual mode works correctly.

### `OmniconvertCS.csproj`
Project definition.

- Targets `net10.0-windows`.
- Uses explicit `LangVersion` `14.0`.
- Uses WinForms via `<UseWindowsForms>true</UseWindowsForms>`.
- Keeps nullable enabled for hand-written code.
- Allows unsafe blocks.
- Enables Windows targeting.
- Version is `1.05.00`.
- Uses `OmniconvertCS.ico` as the application icon.

### `0-PUBLISH.bat`
Publish helper.

- Builds/publishes the Win64 project output.
- Intended for local release packaging.
- Should be kept aligned with the current target framework/runtime.

---

## Core – Data Structures & Common Helpers

### `Core/cheat_t.cs`
Managed equivalents of the original C-style `cheat_t` / `game_t` structs.

- `class cheat_t`
  - Holds cheat ID, name, comments, state/flags, code words, and ARMAX folder metadata.
- `class game_t`
  - Holds game ID, game name, and cheat count metadata used by some file writers.

### `Core/Cheat.cs`
Helper API around `cheat_t`.

- Constants:
  - `NAME_MAX_SIZE`, `CODE_INVALID`, `CODE_NO_TARGET`, `FLAG_MCODE`, and related legacy flags.
- Key methods:
  - `cheatInit()` / `cheatInit(cheat_t cheat, uint id, string name)`
    - Create or initialize a cheat object.
  - `cheatAppendCodeFromText(...)`
    - Append an address/value text line into the cheat word list.
  - `cheatAppendOctet(...)`
    - Append one raw 32-bit word.
  - `cheatPrependOctet(...)`
    - Insert a raw word at the beginning.
  - `cheatRemoveOctets(...)`
    - Remove words from the cheat code list.
  - `cheatFinalizeData(...)`
    - Final bookkeeping after code words are assembled.
  - `cheatDestroy(...)`
    - Clear object fields/lists.
  - `cheatClearFolderId(...)`
    - Clear ARMAX folder metadata.

### `Core/Common.cs`
Shared legacy-style helpers.

- Validation:
  - `IsHexStr(string)`
  - `IsNumStr(string)`
  - `IsEmptyStr(string)`
- Data helpers:
  - `swapbytes(uint)`
- UI/text helpers:
  - `MsgBox(IntPtr hwnd, int flags, string message)`
  - `AppendText(...)`
  - `PrependText(...)`
  - `AppendNewLine(...)`

---

## Core – Conversion Pipeline

### `Core/ConvertCore.cs`
Central conversion state and per-cheat conversion pipeline.

- Global enums/state:
  - `enum Device`
    - AR1, AR2, ARMAX, CB, GS3, GS5, RAW, PNACH-style paths, etc.
  - `enum Crypt`
    - `CRYPT_AR1`, `CRYPT_AR2`, `CRYPT_ARMAX`, `CRYPT_CB`, `CRYPT_CB7_COMMON`, `CRYPT_GS3`, `CRYPT_GS5`, `CRYPT_MAXRAW`, `CRYPT_RAW`, `CRYPT_ARAW`, `CRYPT_CRAW`, `CRYPT_GRAW`, etc.
  - `g_indevice`, `g_outdevice`
  - `g_incrypt`, `g_outcrypt`
  - `g_gameid`
  - AR2 seeds, ARMAX hash/verifier settings, GS3 key, CBC save version, organizer/folder options.

- Main pipeline methods:
  - `DecryptCode(cheat_t cheat)`
    - Decrypts from selected input device/crypt into internal RAW words.
  - `TranslateCode(cheat_t cheat)`
    - Routes through `Translate.TransBatchTranslate(...)` for opcode translation.
  - `EncryptCode(cheat_t cheat)`
    - Encrypts translated words into the selected output crypt/device.
  - `ConvertCode(cheat_t cheat)`
    - Top-level per-cheat flow:
      1. Decrypt input.
      2. Translate opcode family.
      3. Encrypt/output.
    - Returns an integer error code for per-cheat reporting.

### `Core/InlineCryptParser.cs`
INLINE crypt tag parser.

- `TryGetCryptFromLabel(string label, out ConvertCore.Crypt crypt)`
  - Reads labels like `Some Code, CRYPT_ARMAX`.
  - Maps known `CRYPT_XXXX` text into a `ConvertCore.Crypt` value.
- `StripCryptTag(string label)`
  - Removes a `, CRYPT_XXXX` suffix from a label.
- `UpdateCryptTagForOutput(string label, ConvertCore.Crypt outCrypt)`
  - Rewrites an existing `CRYPT_XXXX` label suffix to match the output crypt.
- `GetDeviceForCrypt(ConvertCore.Crypt crypt)`
  - Maps a crypt enum to the matching device enum for conversion setup.
- Private helper:
  - `GetCryptTokenForOutput(ConvertCore.Crypt crypt)`

---

## Core – Translate Engine

The old monolithic translate logic is split into partial files under `Core/Translate/`.

### `Core/Translate/Translate.Batch.cs`
- `TransBatchTranslate(cheat_t src)`
  - Main batch translation entry point.
  - Calls the appropriate translation path based on current input/output crypt/device.

### `Core/Translate/Translate.Constants.Std.cs`
Standard/RAW opcode constants.

- Defines standard command nibbles such as:
  - write byte/half/word
  - increment write
  - multi-write
  - copy bytes
  - pointer write
  - bitwise
  - master/test/hook/timer command families
- Holds size conversion tables and masks used during translation.

### `Core/Translate/Translate.Constants.Armax.cs`
ARMAX opcode constants.

- Defines ARMAX compare/write types.
- Defines ARMAX direct/pointer/increment/hook subtypes.
- Defines ARMAX skip behavior constants.
- Defines masks and special command values used by ARMAX translation.

### `Core/Translate/Translate.Helpers.Bit.cs`
Bit-packing helpers.

- `ADDR(...)`, `LOHALF(...)`, `HIHALF(...)`, `LOBYTE32(...)`, `HIBYTE32(...)`
- `GET_STD_CMD(...)`
- `GET_ARM_CMD(...)`, `GET_ARM_TYPE(...)`, `GET_ARM_SUBTYPE(...)`, `GET_ARM_SIZE(...)`
- `MAKE_ARM_CMD(...)`, `MAKE_STD_CMD(...)`, `MAKEWORD32(...)`, `MAKEHALF(...)`

### `Core/Translate/Translate.Helpers.SmashExplode.cs`
Compression/expansion helpers.

- `TransSmash(...)`
  - Collapses repeated lines where supported.
- `TransExplode(...)`
  - Expands multi-line/multi-write style commands.

### `Core/Translate/Translate.TransMaxToStd.cs`
ARMAX-to-standard translation.

- `TransMaxToStd(...)`
  - Converts ARMAX command words into standard/internal command words.
  - Handles ARMAX-specific special/multi-write/test behavior.

### `Core/Translate/Translate.TransStdToMax.cs`
Standard-to-ARMAX translation.

- `TransStdToMax(...)`
  - Converts standard/internal command words to ARMAX-style commands.

### `Core/Translate/Translate.TransOther.cs`
Non-ARMAX translation path.

- `TransOther(...)`
  - Handles translation between compatible non-ARMAX opcode families.

### `Core/Translate/Translate.Errors.cs`
Translation error handling.

- Error constants such as:
  - `ERR_NONE`
  - `ERR_VALUE_INCREMENT`
  - `ERR_COPYBYTES`
  - `ERR_EXCESS_OFFSETS`
  - `ERR_BITWISE`
  - `ERR_TIMER`
  - `ERR_TEST_TYPE`
  - `ERR_PTR_SIZE`
  - `ERR_INVALID_CODE`
- `TransSetErrorSuppress(byte val)`
- `TransToggleErrorSuppress()`
- `TransGetErrorText(int idx)`

### `Core/Translate/Translate.Debug.cs`
Debug trace helpers.

- `Trace(string message)`
- `Trace(string format, params object[] args)`

---

## Core – CLI / Batch Conversion

v1.05 includes a fuller command-line workflow for batch conversion, analysis, and analyze/fix mode.

### `Core/Cli/CliOptions.cs`
CLI argument parser and option model.

- `AnalyzeMode`
  - `None`
  - `Analyze`
  - `AnalyzeFix`
- `CliOptions`
  - Tracks mode, output crypt, manual mode flags, E001 rewrite option, input path, output root, output folder, and analysis folder.
- `TryParse(string[] args, out CliOptions options, out string error)`
  - Parses command-line flags:
    - `-a` analyze
    - `-af` analyze + fix
    - `-m` manual on failures
    - `-mc` manual check all
    - `-e` CRAW E001-to-D rewrite
    - `-o` / `--out` output root
    - `-h` / `--help` / `/?`
- `TryParseCrypt(string token, out ConvertCore.Crypt crypt)`
  - Supports tokens with or without `CRYPT_` prefix.

### `Core/Cli/CliRunner/`
Partial CLI runner split by responsibility.

#### `CliRunner.cs`
- `Run(string[] args)`
  - Parses options.
  - Prints usage or errors.
  - Calls execution.

#### `CliRunner.Execute.cs`
- `Execute(CliOptions opt)`
  - Creates output/analysis folders.
  - Gathers input files.
  - Processes each file without allowing one bad file to stop the whole folder run.
- `ComputeExitCode(...)`
  - Exit code logic:
    - normal success = `0`
    - argument/no-file style errors = `1`
    - analyze failures = `2`
    - analyze/fix still failing = `3`

#### `CliRunner.Input.cs`
- `GatherInputFiles(CliOptions opt, out string inputRoot)`
  - Accepts one `.txt` file or a folder.
  - Recurses folder input.
  - Skips existing output/analysis folders so previous CLI results are not reprocessed.

#### `CliRunner.Process.cs`
- `ProcessOneFile(...)`
  - Parses input via `TextCheatParser`.
  - Converts each block.
  - Runs analyze/analyze-fix checks when requested.
  - Writes converted output.
  - Writes analysis reports.
  - Applies optional `E001ToDFixer` post-processing for CRAW output.
- `WriteAnalysisReport(...)`
  - Writes per-file `.analysis.txt` reports under `<OutputCrypt>/Analysis/`.
- `HandleFileException(...)`
  - Logs exceptions and writes report details without stopping the batch.

#### `CliRunner.Helpers.cs`
- `MakeAnalysisRelPath(...)`
- `ShortLabel(...)`
- `GetDeclaredCrypt(CheatBlock b)`
- `CloneCheat(cheat_t src)`

#### `CliRunner.Usage.cs`
- `PrintUsage()`
  - Prints CLI syntax, options, output paths, and examples.

### `Core/Cli/TextCheatParser.cs`
Text input parser for CLI mode.

- `CheatBlock`
  - `Label`
  - `CheatInput`
  - `Wildcards`
  - `OriginalLines`
  - `HasArmaxEncryptedTextLine`
- `WildcardMask`
  - Tracks wildcard/mask information for value words so masks can be reapplied after conversion.
- `ParseFile(string path)`
  - Reads `.txt` using non-throwing UTF-8.
- `ParseText(string inputText)`
  - Splits input into logical cheat blocks.
  - Accepts ARMAX encrypted lines such as `XXXXX-XXXXX-XXXXX`.
  - Accepts RAW-style lines only when the **first two tokens** are valid `ADDR VALUE` pairs.
  - Treats other lines as labels/group/name lines.
- `TryParseHexAddressToken(...)`
  - Allows 6–8 hex digits for addresses.
  - Does not allow wildcard characters in addresses.
- `TrySanitizeRawValueToken(...)`
  - Allows 1–8 value nibbles.
  - Allows only hex plus explicit wildcard markers `?`, `X`, or `x`.
  - Rejects normal words such as `Cash`, `Money`, or `Unlocked` as code values.
- `SanitizeHexWithMask(...)`
  - Converts wildcard nibbles to `0` for conversion while preserving a mask for final output.

### `Core/Cli/TextCheatWriter.cs`
Text output writer for CLI mode.

- `WriteToText(List<(CheatBlock Block, cheat_t CheatConverted)> converted, ConvertCore.Crypt outCrypt)`
  - Writes labels and converted code lines.
  - Formats ARMAX encrypted output using `Armax.FormatEncryptedLine(...)`.
  - Updates existing inline `CRYPT_XXXX` tags when present.
  - Reapplies wildcard masks to value words.
- `ApplyMaskToHex(...)`
  - Overlays saved wildcard mask characters onto an 8-digit value string.

### `Core/Cli/RawPlausibility.cs`
RAW-family analysis heuristics.

- `ValidateFirstCheckableAddress(cheat_t cheat, ConvertCore.Crypt outputCrypt, AnalyzeMode mode)`
  - Validates the first checkable output line for RAW-family analysis.
  - Skips CB7 master/enabler prologue lines.
  - Checks basic structure such as even code count and plausible address range.
  - Adds stricter CRAW analysis rules for some command families.
- `IsCb7MasterPrologueLine(...)`
  - Detects known CB7 master/enabler prologue lines.

### `Core/Cli/RawPlausibilityResult.cs`
Result model for RAW-family analysis.

- `IsValid`
- `CheckedWord`
- `CheckedAddr28`
- `Rule`
- `FailReason`
- `Note`

### `Core/Cli/AutoFix/`
Analyze/fix crypt selection engine.

#### `AutoFixCryptSelector.cs`
- `ConvertWithAutoFix(...)`
  - Main analyze/fix entry point.
  - Reads declared inline crypt when present.
  - Converts directly when possible.
  - Tries alternate crypt candidates when analysis fails.
  - Supports manual and manual-all flows.

#### `AutoFixCryptSelector.Candidates.cs`
- `BuildCandidateList(...)`
  - Builds crypt retry order.
  - Moves likely neighbor crypts earlier, such as CB/CB7 and AR1/AR2 swaps.

#### `AutoFixCryptSelector.Attempts.cs`
- `TryConvertCandidate(...)`
  - Attempts conversion for a candidate crypt.
  - Handles AR2 key variants when testing AR2.
- `TryConvertOnce(...)`
  - Runs one conversion and optional plausibility check.

#### `AutoFixCryptSelector.Helpers.cs`
- `SeedFromAr2Key(...)`
- `CloneCheat(...)`
- `IsRawFamilyCrypt(...)`

#### `AutoFixCryptSelector.Heuristics.cs`
- `InputAddr28InStdRange(...)`
  - Checks whether the input itself already looks plausible.
  - Used to detect cases where an auto-fix pass may be a false positive and should be confirmed.
- `IsCb7MasterPrologueLineLocal(...)`

#### `AutoFixCryptSelector.Manual.cs`
- `PromptManualOnFail(...)`
  - Interactive prompt after an attempted crypt fails.
- `PromptManualConfirmOnPass(...)`
  - Interactive confirmation when a candidate passes but the input also looks plausible.

#### `AutoFixCryptSelector.ManualAll.cs`
- `InitManualAllPick(...)`
  - Forces manual selection/checking for every cheat when `-mc` is used.

#### `AutoFixCryptSelector.ManualExtras.cs`
- `PromptManualPickInputCrypt(...)`
- `ParseManualFlags(...)`
- `TryParseHexUInt(...)`
  - Supports manual command flags such as `-y` and `-k<AR2Key>`.

#### `AutoFixResult.cs`
Result model for analyze/fix.

- `Deleted`
- `Fixed`
- `UsedInputCrypt`
- `CheatConverted`
- `Plausibility`
- `RetCode`
- `Note`

### `Core/Cli/CliUi.cs`
Centralized console UI.

- Wraps console streams:
  - `Out`, `Err`, `In`
- Prints usage, run summaries, errors, and manual-mode prompts.
- Displays original input block and converted output during manual crypt selection.
- Supports manual commands:
  - `Enter` continue/accept
  - `S` skip cheat
  - `D` delete cheat
  - `Q` quit
  - `-y` force accept selected crypt
  - `-k<hex>` apply manual AR2 key for an AR2 attempt

### `Core/Cli/CliScreenContext.cs`
Small shared context for CLI display.

- Tracks the current file name/path for manual-mode banners.

### `Core/Cli/E001ToDFixer.cs`
Optional CRAW post-processor.

- `RewriteE001ToD(string text)`
  - Rewrites lines like `$E001vvvv 0aaaaaaa` into `$Daaaaaaa 0000vvvv`.
  - Only intended for the `-e` option with CRAW output.

---

## Devices – Crypto Engines

### `Devices/Ar2.cs`
Action Replay 1/2 crypto.

- `Ar2Decrypt(...)`, `Ar2Encrypt(...)`
- `Ar2SetSeed(...)`, `Ar2GetSeed()`
- `Ar2BatchDecryptArr(...)`, `Ar2BatchEncryptArr(...)`
- `Ar2BatchDecrypt(...)`, `Ar2BatchEncrypt(...)`
- `Ar1BatchDecrypt(...)`, `Ar1BatchEncrypt(...)`
- `Ar2AddKeyCode(...)`
- Internal helper:
  - `NibbleFlip(...)`

### `Devices/Armax.cs`
Action Replay MAX crypto, text formatting, verifier, and disc hash support.

- Text helpers:
  - `MaxRemoveDashes(string)`
  - `IsArMaxStr(string)`
  - `TryParseEncryptedLine(...)`
  - `FormatEncryptedLine(...)`
- Crypto helpers:
  - `ArmBatchDecryptFull(...)`
  - `ArmBatchEncryptFull(...)`
  - Internal seed/build/encrypt/decrypt helpers.
- Verifier/hash helpers:
  - `ArmReadVerifier(...)`
  - `ArmMakeVerifier(...)`
  - `ArmEnableExpansion(...)`
  - `ComputeDiscHash(string path)`
  - `ArmMakeFolder(...)`

### `Devices/Cb2Crypto.cs`
CodeBreaker crypto.

- RSA helpers:
  - `RSACrypt(...)`
  - `RSADecode(...)`
  - `RSAEncode(...)`
- Multiplicative helpers:
  - `MulInverse(...)`
  - `MulEncrypt(...)`
  - `MulDecrypt(...)`
- ARC4 helpers:
  - `CBReset()`
  - `CBSetCommonV7()`
  - `CBCryptFileData(...)`
- Code crypto:
  - `CB1EncryptCode(...)`, `CB1DecryptCode(...)`
  - `CBEncryptCode(...)`, `CBDecryptCode(...)`
  - `CBBatchDecrypt(...)`, `CBBatchEncrypt(...)`
  - CB7 helper path such as `CB7EncryptCode(...)` / `CB7DecryptCode(...)`

### `Devices/Gs3.cs`
GameShark v3/v5 crypto and verifier support.

- Version/key behavior:
  - `SetVersion5()`
  - `IsVersion5()`
- CRC/seed helpers:
  - `CcittCrc(...)`
  - `GenCrc(...)`
  - `Crc32(...)`
  - `GenCrc32(...)`
  - `InitMtStateTbl(...)`
  - `GetMtNum(...)`
  - `BuildByteSeedTbl(...)`
  - `ReverseSeeds(...)`
  - `BuildSeeds(...)`
- Crypto:
  - `Init(...)`
  - `Update(...)`
  - `Encrypt(...)`
  - `Decrypt(...)`
  - `BatchEncrypt(...)`
  - `BatchDecrypt(...)`
- Verifier/file helpers:
  - `CreateVerifier(...)`
  - `AddVerifier(...)`
  - `CryptFileData(...)`

---

## Devices – Loaders

### `Devices/Load/ArmaxBinReader.cs`
Reader for ARMAX `.bin` codelist files.

- `LoadAsArmaxEncryptedTextWithGames(...)`
  - Reads an ARMAX list and returns encrypted ARMAX text grouped by game.
- `LoadAsArmaxEncryptedLines(...)`
  - Reads an ARMAX list as encrypted line output.
- `Load(...)`
  - Reads and decodes ARMAX `.bin` payloads into cheats/metadata.
- `MapArmaxButtonGlyph(...)`
  - Converts embedded ARMAX button glyph code units into readable tokens such as `{X}`, `{Square}`, `{Triangle}`, `{Circle}`, `{L1}`, `{R1}`, etc.
- Header/string helpers:
  - `IsReasonableCheatCount(...)`
  - `TryReadGameCheatHeader(...)`
  - `ReadWideStringZeroTerminated(...)`
  - `ReadUInt32LE(...)`, `ReadUInt16LE(...)`

### `Devices/Load/CbcReader.cs`
Reader for CodeBreaker `.cbc` Day1/CFU files.

- `Load(string path)`
  - Detects CBC v7/v8 and dispatches accordingly.
- `LoadV7(...)`
- `LoadV8(...)`
- `IsCbcV8(...)`
- `ParseCheatsFromPayload(...)`
- ASCII/little-endian helpers.

### `Devices/Load/P2mReader.cs`
Reader for GameShark v5 / XP `.p2m` archives.

- `Load(string path)`
  - Validates the P2M archive.
  - Locates and decrypts embedded `user.dat` using GS3 logic.
  - Builds a result containing extracted cheats and metadata.
- Helpers:
  - `ReadUInt32LE(...)`
  - `ReadUInt16LE(...)`
  - `ReadFixedAscii(...)`
  - `ReadWideStringWithTag(...)`

---

## Devices – Writers / Save As

### `Devices/SaveAs/ArmaxBinWriter.cs`
Writer for ARMAX `.bin` codelist files.

- `CreateList(List<cheat_t> cheats, game_t game, string fileName)`
  - Builds ARMAX list header, game metadata, cheat blocks, and encrypted payload.
- Helpers:
  - `WriteUInt32LE(...)`
  - `WriteUInt16LE(...)`

### `Devices/SaveAs/CbcWriter.cs`
Writer for CodeBreaker `.cbc` files.

- `CreateFile(List<cheat_t> cheats, game_t game, string fileName, bool doHeadings)`
  - Writes CBC v7 Day1 or v8 CFU output depending on current settings.
- `CreateV8Signature(...)`
- `WriteUInt32LE(...)`

### `Devices/SaveAs/P2m.cs`
Writer for GameShark v5+ `.p2m` archives.

- Contains managed versions of P2M header/file descriptor/date structures.
- `CreateFile(List<cheat_t> cheats, game_t game, string path, bool doHeadings)`
  - Builds P2M archive structures.
  - Creates embedded code-list/user data.
  - Uses GS3 crypto for the embedded payload.
- Helpers:
  - `InitHeader(...)`
  - `InitFd(...)`
  - `WriteDate(...)`
  - `WriteHeader(...)`
  - `WriteArcStat(...)`
  - `WriteFileDescriptor(...)`
  - `WriteStringW(...)`
  - `WriteFixedAscii(...)`
  - little-endian writers.

### `Devices/SaveAs/SwapMagicBinWriter.cs`
Writer for Swap Magic Coder `.bin` files.

- `CreateFile(List<cheat_t> cheats, game_t game, string path)`
  - Builds Swap Magic game header and cheat list.

### `Devices/SaveAs/PnachWriter.cs`
PNACH text save helper.

- `SaveFromText(string path, string? pnachText)`
  - Writes PNACH text as UTF-8.

### `Devices/SaveAs/Crc32.cs`
CRC-32 helper.

- `Compute(ReadOnlySpan<byte> buffer, uint crc)`
  - CRC-32 implementation used by writers.

---

## Devices – Options & PNACH CRC

### `Devices/Options/PnachCrcHelper.cs`
Manages `PnachCRC.json` mappings.

- `CrcEntry`
  - `Crc`
  - `GameName`
  - `ElfName`
- `NormalizeElfName(...)`
- `ComputeElfCrc(string elfPath)`
  - Computes PCSX2-style game CRC from a PS2 ELF.
- `GetEntries()`
- `TryGetGameName(uint crc, out string gameName)`
- `TryGetElfName(uint crc, out string elfName)`
- `AddOrUpdate(uint crc, string elfName, string gameName)`
- Private helpers:
  - `GetMappingPath()`
  - `LoadEntries()`
  - `SaveEntries(List<CrcEntry>)`

---

## UI – Main Form

### `UI/MainForm/MainForm.Designer.cs`
Designer-generated layout for the main window.

- Input/output text boxes.
- Convert/Clear buttons.
- MenuStrip:
  - File
  - Edit
  - Input
  - Output
  - Options
- PNACH CRC controls.
- AR2 current-key controls.
- Game ID / Game Name controls.
- Input/output format labels.
- Drag/drop wiring for input loading.

### `UI/MainForm/MainForm.cs`
Main form behavior and GUI conversion pipeline.

- State:
  - `_lastConvertedCheats`
  - `_lastGameId`
  - `_outputAsPnachRaw`
  - `_inputAsPnachRaw`
  - `_pnachCrc`, `_pnachGameName`, `_pnachElfName`, `_pnachCrcActive`
  - `_inlineInputMode`
- RAW/wildcard parser helpers:
  - `TryParseHexAddressToken(...)`
    - Requires 6–8 hex digits for addresses.
    - Does not allow address wildcards.
  - `IsRawValueNibble(...)`
    - Allows hex plus `?`, `X`, `x` only.
  - `TrySanitizeRawValueToken(...)`
    - Rejects normal words as values.
    - Preserves wildcard masks for later output.
  - `SanitizeHexWithMask(...)`
  - `ApplyMaskToHex(...)`
  - `OutputSupportsWildcardValues()`
    - Allows wildcard values only for safe outputs:
      - RAW family outputs.
      - CodeBreaker v1–6.
      - GS3 key 0, 1, or 3.
    - Blocks wildcard values for ARMAX RAW, ARMAX encrypted, CB7, GS5, AR1/AR2, etc.
- `MainForm_Load(...)`
  - Loads settings.
  - Initializes menus and UI state.
  - Shows version in title bar.
- `btnConvert_Click(...)`
  - GUI conversion entry point.
  - Parses PNACH, INLINE, ARMAX encrypted text, and RAW text.
  - Prevents ARMAX encrypted input from falling back to RAW parsing.
  - Reads RAW lines from the first two tokens only.
  - Treats unrecognized lines as labels/titles.
  - Converts each cheat through `ConvertCore.ConvertCode(...)`.
  - Emits PNACH headers/groups when needed.
  - Reapplies wildcard masks in supported output formats.
  - Writes per-cheat error comment blocks on failure.
- `CleanCbSiteFormat(string input)`
  - Cleans pasted CodeBreaker/GameHacking style text.
- `AppendErrorCommentBlock(...)`
  - Adds readable conversion error comments to output.

### `UI/MainForm/MainForm.CryptMenus.cs`
Input/output crypt menu handling.

- `CryptMenuItem_Click(...)`
  - Updates selected input/output crypt/device.
  - Handles PNACH RAW and INLINE mode state.
  - Updates UI labels.
- `BuildCryptMenu(...)`
- `AddCryptMenuItem(...)`
- `FindCryptOption(...)`
- Holds the menu option table for supported crypts/devices.

### `UI/MainForm/MainForm.FileMenu.cs`
File load/save behavior.

- Shared load helpers:
  - `LoadTextFromPath(string path)`
  - `LoadCbcFromPath(string path)`
  - `LoadArmaxBinFromPath(string path)`
  - `LoadP2mFromPath(string path)`
- Load menu handlers:
  - `menuFileLoadText_Click(...)`
  - `menuFileLoadArmaxBin_Click(...)`
  - `menuFileLoadCbc_Click(...)`
  - `menuFileLoadP2m_Click(...)`
- Save menu handlers:
  - `menuFileSaveAsText_Click(...)`
  - `menuFileSaveAsPnach_Click(...)`
  - `menuFileSaveAsArmaxBin_Click(...)`
  - `menuFileSaveAsCbc_Click(...)`
  - `menuFileSaveAsP2m_Click(...)`
  - `menuFileSaveAsSwapBin_Click(...)`
- `ShowBinaryExportNotImplemented(...)`
  - Fallback for unavailable binary export paths.

### `UI/MainForm/MainForm.DragDrop.cs`
Drag-and-drop input loading.

- `IsSupportedLoadExtension(string? ext)`
  - Supports `.txt`, `.cbc`, `.bin`, `.p2m`.
- `txtInput_DragEnter(...)`
  - Allows copy drop effect only for supported files.
- `txtInput_DragDrop(...)`
  - Loads the first supported dropped file.
- `LoadFileIntoInput(string path)`
  - Routes file extension to the same shared helpers used by File menu loading.

### `UI/MainForm/MainForm.EditMenu.cs`
Edit menu and shortcuts.

- `SwapInputOutput()`
- `menuEditUndo_Click(...)`
- `menuEditCut_Click(...)`
- `menuEditCopy_Click(...)`
- `menuEditPaste_Click(...)`
- `menuEditSelectAll_Click(...)`
- `menuEditSwapInputOutput_Click(...)`
- `menuEditClearInput_Click(...)`
- `menuEditClearOutput_Click(...)`
- `btnClearInput_Click(...)`
- `btnClearOutput_Click(...)`

### `UI/MainForm/MainForm.Ar2Helpers.cs`
AR2 key UI helpers.

- `RefreshAr2KeyDisplayFromSeed()`
  - Shows current AR2 key state in the UI.
- `TryApplyAr2KeyFromCheatHeader(cheat_t cheat)`
  - Detects/applies AR2 key codes from cheat headers when possible.

### `UI/MainForm/MainForm.GameIdAndPnachUi.cs`
Game ID and PNACH UI helpers.

- `SyncGameIdFromGlobals()`
- `txtGameId_KeyPress(...)`
- `txtGameId_TextChanged(...)`
- `chkPnachCrcActive_CheckedChanged(...)`
- `RefreshArmaxGameIdDisplay()`

### `UI/MainForm/MainForm.OptionsMenu.cs`
Options menu handlers.

- ARMAX options:
  - `menuOptionsArmaxOptions_Click(...)`
  - `EnumerateArmaxDiscHashDrives()`
  - `SetArmaxHashDrive(...)`
  - `menuOptionsArmaxDiscHashNone_Click(...)`
  - `menuOptionsArmaxDiscHashDrive_Click(...)`
- AR2 key:
  - `menuOptionsAr2Key_Click(...)`
- PNACH CRC:
  - `menuOptionsPnachCrc_Click(...)`
- Organizers/folders:
  - `UpdateMakeOrganizersCheck()`
  - `menuOptionsMakeOrganizers_Click(...)`
- ARMAX verifiers/region:
  - `UpdateVerifierMenuChecks()`
  - `UpdateRegionMenuChecks()`
  - `menuOptionsArmaxVerifiersAuto_Click(...)`
  - `menuOptionsArmaxVerifiersManual_Click(...)`
  - `menuOptionsArmaxRegionUsa_Click(...)`
  - `menuOptionsArmaxRegionPal_Click(...)`
  - `menuOptionsArmaxRegionJapan_Click(...)`
- GS3 key:
  - `UpdateGs3MenuChecks()`
  - `SetGs3Key(int key)`
- CBC save version:
  - `UpdateCbcSaveVersionMenuChecks()`
  - `menuOptionsCbcVersion7_Click(...)`
  - `menuOptionsCbcVersion8_Click(...)`

### `UI/MainForm/MainForm.Settings.cs`
User settings persistence.

- `AppSettings`
  - Stores last selected input/output device/crypt, game ID, window geometry, CBC save version, ARMAX hash/verifier settings, INLINE mode state, PNACH state, etc.
- `GetSettingsPath()`
- `LoadAppSettings()`
- `SaveAppSettings()`
- `OnFormClosing(...)`

---

## UI – Dialogs & Forms

### `UI/Ar2KeyForm.cs` + Designer
Dialog for AR2 key seed/key selection.

- `InitializeFromCurrentSeed()`
- `cmbCommonKeys_SelectedIndexChanged(...)`
- `btnOk_Click(...)`
- `btnReset_Click(...)`
- `txtKey_KeyPress(...)`

### `UI/ArmaxOptionsForm.cs` + Designer
Dialog for ARMAX options and disc hash/game ID settings.

- `PopulateDiscHashCombo(...)`
- `txtGameId_KeyPress(...)`
- `btnOk_Click(...)`
- `btnCancel_Click(...)`

### `UI/PnachCrcForm.cs` + Designer
PNACH CRC / ELF / Game Name mapping dialog.

- `btnBrowseElf_Click(...)`
- `SetElfPathAndCompute(...)`
- `comboKnown_SelectedIndexChanged(...)`
- `btnOk_Click(...)`
- `btnCancel_Click(...)`
- Drag-and-drop handlers:
  - `PnachCrcForm_DragEnter(...)`
  - `PnachCrcForm_DragDrop(...)`
- Exposes selected CRC/game/ELF info back to `MainForm`.

---

## Docs / Help / Runtime Data

### `INFO/DEV_FunctionMap.md`
This file.

- Developer navigation map.
- Should be updated whenever files move, major parser behavior changes, CLI behavior changes, or new load/save formats are added.

### `INFO/OmniconvertCS_Help.html`
End-user help document.

- Describes supported formats and common workflows.
- Should be kept aligned with GUI-visible behavior.

### `README.md`
Project overview and usage notes if present in future packages.

### `CREDITS.md`
Attribution for the original OmniConvert work and related research/contributors.

### `LICENSE`
Project license if present in future packages.

### `OmniconvertSettings.json` runtime file
Saved app settings.

- Managed by `MainForm.LoadAppSettings()` and `MainForm.SaveAppSettings()`.
- Stored next to the executable/project runtime path.

### `PnachCRC.json` runtime file
PNACH CRC mapping data.

- Managed by `Devices.Options.PnachCrcHelper`.
- Maps CRC ↔ game name ↔ ELF name.

---

## v1.05 Notes Worth Remembering

- The project is now **.NET 10 / C# 14**.
- CLI mode is active whenever `Program.Main` receives arguments.
- GUI mode remains the no-argument default.
- CLI batch conversion supports analyze and analyze/fix flows.
- RAW-family analysis validates plausible output addresses and has stricter CRAW checks.
- Manual CLI mode can force/skip/delete/quit and can test AR2 key variants.
- RAW text parsing is intentionally stricter:
  - RAW code lines must begin with the address/value pair.
  - Address token must be 6–8 hex digits.
  - Value token must be hex plus optional explicit wildcard markers `?`, `X`, or `x`.
  - Normal words such as `Cash` are no longer accepted as wildcard values.
- ARMAX encrypted input does not fall back to RAW parsing when an encrypted line parse fails.
- Wildcard value masks are preserved and reapplied only for supported output formats.
- `ARMAX RAW` does **not** support wildcard values in the current safety rules.

