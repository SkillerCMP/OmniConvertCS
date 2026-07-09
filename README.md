# 🔄 OmniConvertCS

**OmniConvertCS** is a native **C++17** PlayStation 2 cheat-code converter based on  
**OmniConvert 1.1.1R**, the classic converter created by **Pyriel**.

The project retains the **OmniConvertCS** name for continuity with previous releases.  
The former C# implementation is preserved in the [`~Legacy~/`](./~Legacy~/) folder for reference, but the active application is now built from the native C++17 source.

<p>
  <a href="https://github.com/SkillerCMP/OmniConvertCS/releases">
    <img alt="GitHub Downloads — All Releases" src="https://img.shields.io/github/downloads/SkillerCMP/OmniConvertCS/total?style=social">
  </a>
  <a href="https://github.com/SkillerCMP/OmniConvertCS/releases/latest">
    <img alt="GitHub Downloads — Latest Release" src="https://img.shields.io/github/downloads/SkillerCMP/OmniConvertCS/latest/total?style=social">
  </a>
</p>

> **Current release:** OmniConvertCS v1.06.02  
> **Architecture:** Native C++17, Windows x64  
> **Compatibility:** Windows 7 SP1 and newer

---

## ✨ Highlights

- 🔐 Convert between RAW, CodeBreaker, Action Replay, ARMAX, GameShark, Xploder, and other PS2 formats
- 🧩 Mix different crypt types in one conversion using **INLINE** input
- 📝 Read and generate PCSX2 PNACH text
- 🔎 Compute and save PCSX2 ELF CRC mappings
- 🧠 Assemble and disassemble PS2 R5900/MIPS instructions
- 🗂️ Format supported text output for the CMP Code Database
- 🧬 Optional MGS C-Type Pointer Mode for Metal Gear Solid pointer-style CodeBreaker codes
- 📦 Read and write ARMAX `.bin` codelists
- 🎨 Native Win32 interface with persistent settings
- 🪟 Windows 7 SP1 x64 compatibility
- 🚫 No .NET runtime required

---

<details open>
<summary><strong>📖 Project Overview</strong></summary>

<br>

OmniConvertCS continues the work of the original **OmniConvert 1.1.1R** while adding new formats, utilities, and workflow improvements.

The application can be used to:

- Decrypt encrypted PS2 cheat codes to RAW
- Re-encrypt RAW codes for another cheat device
- Convert mixed-format code collections
- Generate PNACH files for PCSX2
- Convert PS2 R5900 instructions between opcodes and assembly text
- Prepare code lists for the CMP Code Database
- Read and write supported binary codelist formats

The original OmniConvert project is available here:

- [Pyriel's original OmniConvert repository](https://github.com/pyriell/omniconvert)

</details>

<details>
<summary><strong>🧬 Current C++17 Version and Legacy C# Source</strong></summary>

<br>

The active OmniConvertCS application is now written in native **C++17**.

The previous C# implementation has been retained in:

```text
~Legacy~/
```

The legacy folder is included for:

- Historical reference
- Comparing behavior with older releases
- Reviewing the earlier C# port
- Preserving previous development work

The files under `~Legacy~/` are not used by the current C++17 build unless explicitly referenced by a separate legacy project or workflow.

</details>

<details>
<summary><strong>🔐 Supported Conversion Features</strong></summary>

<br>

### Common formats

OmniConvertCS supports conversion between formats such as:

- RAW
- Action Replay 1 and 2
- Action Replay MAX
- CodeBreaker
- CodeBreaker v7+ Common
- GameShark
- Xploder
- MAX RAW
- PNACH
- PS2 MIPS
- INLINE mixed-format input

The exact available choices are shown in the application's **Input** and **Output** format lists.

### ARMAX and codelist support

- Import and export ARMAX `.bin` codelists
- Preserve game names, group names, and code descriptions where supported
- Load supported XP/GS P2M data
- Convert ARMAX codes to and from RAW and other formats

</details>

<details>
<summary><strong>🧩 INLINE Mixed-Crypt Input</strong></summary>

<br>

INLINE input allows separate code blocks to declare different crypt formats in the same conversion.

```text
Infinite Health, CRYPT_AR2
CEB1E0D2 BCA99C84

Master Code, CRYPT_ARMAX
VNVH-7KMA-DX61K
A4UT-3BKQ-TYYV5

Max Money, CRYPT_CRAW
20123456 05F5E0FF
```

Supported INLINE identifiers include:

```text
CRYPT_AR1
CRYPT_AR2
CRYPT_ARMAX
CRYPT_CB
CRYPT_CB7_COMMON
CRYPT_GS3
CRYPT_GS5
CRYPT_MAXRAW
CRYPT_RAW
CRYPT_ARAW
CRYPT_CRAW
CRYPT_GRAW
```

INLINE behavior:

- Headers are case-insensitive
- Each named block uses its own crypt type
- Blocks without a recognized header default to RAW
- INLINE is an input-only format

</details>

<details>
<summary><strong>📝 PNACH Support</strong></summary>

<br>

### PNACH input

OmniConvertCS can read PNACH text containing group headers and EE patch rows:

```ini
[Infinite Health]
patch=1,EE,20123456,extended,00000063
```

Common PNACH metadata such as the following is ignored by the code parser when appropriate:

```ini
gametitle=
comment=
crc=
description=
```

### PNACH output

OmniConvertCS can generate PCSX2-compatible PNACH output:

```ini
patch=1,EE,20123456,extended,00000063
```

The generated output can be copied as text or saved directly as a `.pnach` file.

</details>

<details>
<summary><strong>🔎 PNACH CRC Helper</strong></summary>

<br>

The PNACH CRC helper calculates the PCSX2 game CRC from a PS2 ELF and stores an ELF, CRC, and game-name mapping.

Mappings are stored in:

```text
PnachCRC.json
```

Example mapping:

```text
SLES_526.41>A1FD63D6>Game Name
```

### Using the helper

1. Select **PNACH (RAW)** as the output format
2. Enable **Add CRC**, or open **Set PNACH CRC**
3. Drag a PS2 ELF onto the dialog or select it with **Browse**
4. Confirm or enter the game name
5. Save the mapping

When a mapping is available, OmniConvertCS can suggest a PNACH filename such as:

```text
SLUS_203.12_A1FD63D6.pnach
```
</details>

<details>
<summary><strong>🗂️ CMP Output Mode</strong></summary>

<br>

CMP Output Mode formats supported text output for the CMP Code Database.

It is available under:

```text
Options
└── Omni Options
    └── CMP Output Mode
```

Example input:

```text
Master Codes
Enable Code (Must Be On), by Lajos Szalay
904903A8 0C109375
2012EF74 00441021
```

CMP-formatted output:

```text
!Master Codes:
+Enable Code (Must Be On)
%Credits: Lajos Szalay
$904903A8 0C109375
$2012EF74 00441021
!!
```

CMP Output Mode supports:

- `!Group Name:` group headers
- `!!` group-closing markers
- `+Code Name` rows
- `$XXXXXXXX YYYYYYYY` code rows
- Automatic `%Credits:` extraction
- Automatic group closing at the next group or end of input

CMP formatting is not applied to PNACH or PS2 MIPS output.

### MGS C-Type Pointer Mode

MGS C-Type Pointer Mode is available under:

```text
Options
└── Omni Options
    └── MGS C-Type Pointer Mode
```

When enabled, CodeBreaker-family input that contains Metal Gear Solid pointer-style `C` codes is normalized after decryption:

```text
CAAAAAAA OOOOVVVV
```

becomes:

```text
6AAAAAAA 0000VVVV
00010001 0000OOOO
```

This allows the special MGS format to convert into normal CodeBreaker type `6` pointers, GameShark pointers, or other supported output formats.

</details>

<details>
<summary><strong>🔀 Transpose Type</strong></summary>

<br>

Transpose behavior can be selected from:

```text
Options
└── Omni Options
    └── Transpose Type
        ├── Strict
        └── Original
```

**Original** is the default for new settings.

**Strict** selection remains respected This is based on Engine Research, it only converts items that are 1 to 1 so it wont convert the "Make it Work" conversions We did for the original.

</details>

<details>
<summary><strong>🧠 PS2 R5900 / MIPS Support</strong></summary>

<br>

OmniConvertCS includes a bidirectional PS2 R5900 assembler and disassembler.

Supported instruction areas include:

- Standard R5900 integer operations
- Branches and jumps
- COP0 system-control instructions
- COP1 floating-point operations
- R5900 MMI0–MMI3 packed instructions
- Trap instructions
- COP2 register transfers and branches
- Core VU0 macro-mode arithmetic
- Destination masks and broadcast forms

Example:

```asm
vadd.xyzw $vf1, $vf2, $vf3
vaddz.xw  $vf4, $vf5, $vf6
vmul.y    $vf7, $vf8, $vf9
```

Reserved or unsupported instruction encodings remain exact `.word` values instead of being silently changed.

</details>

<details>
<summary><strong>🎨 Interface and Settings</strong></summary>

<br>

The current application uses a native Windows desktop interface.

Features include:

- Resizable input and output areas
- Correct repainting during window resizing
- Persistent application settings
- Copy-and-paste text cleanup
- Input and output format selection
- Game-name field
- Convert and Swap controls
- Product name and version in the title bar

The interface is implemented using the native **Win32 API** and the application is compiled for **64-bit x64 Windows**.

</details>

<details>
<summary><strong>📥 Download and Basic Usage</strong></summary>

<br>

### Download

1. Open the [Releases page](https://github.com/SkillerCMP/OmniConvertCS/releases)
2. Download the latest release ZIP
3. Extract the ZIP to a writable folder
4. Run:

```text
OmniConvertCS.exe
```

### Basic usage

1. Select the input format
2. Select the output format
3. Paste or load the source codes
4. Click **Convert**
5. Copy the result or use **File → Save As**

Supported exports may include:

- Plain text
- ARMAX `.bin`
- PNACH `.pnach`
- Other supported format-specific files

</details>

<details>
<summary><strong>🛠️ Building from Source</strong></summary>

<br>

The active project is written in C++17 and uses CMake.

For the Windows 7 x64 build workflow, run:

```bat
build-windows7-x64.cmd Release clean auto
```

Generated release files and logs are placed under:

```text
0-Finished\
├── dist\
└── logs\
```

The temporary build folder is removed when the build script finishes.

The current build uses the static Microsoft C++ runtime, so a separate Visual C++ Redistributable should not normally be required.

</details>

<details>
<summary><strong>🙏 Credits and Acknowledgements</strong></summary>

<br>

### Pyriel

- Original **OmniConvert 1.1.1R** PS2 converter
- GS3 / XP4+ crypt implementations
- Major reverse-engineering work for cheat-device formats and crypt routines
- [Original OmniConvert repository](https://github.com/pyriell/omniconvert)

### a-n-t-i-b-a-r-y-o-n

- OmniConvert fork containing additional references and changes
- [OmniConvert fork](https://github.com/a-n-t-i-b-a-r-y-o-n/omniconvert)

### misfire / mlafeldt

- CB2Crypt source
- CodeBreaker v2 crypt
- Action Replay 2 crypt
- CodeBreaker PS2 File Utility
- [cb2util repository](https://github.com/mlafeldt/cb2util)

### Parasyte

- Action Replay MAX encryption implementation

### Alexander Valyalkin

- BIG_INT arbitrary-precision integer library

### PS2 community

OmniConvert and the tools surrounding it are the result of many years of public research, reverse engineering, testing, and documentation from the PlayStation 2 community.

> Pyriel was always someone I looked up to and is one of the reasons I was able to do what I do now. Porting and expanding OmniConvert has been both a challenge and an honor. Thank you to everyone who contributed to the PS2 scene and continues to keep it alive.

</details>

<details>
<summary><strong>🔗 Useful Links</strong></summary>

<br>

- [OmniConvertCS releases](https://github.com/SkillerCMP/OmniConvertCS/releases)
- [Original OmniConvert 1.1.1R](https://github.com/pyriell/omniconvert)
- [a-n-t-i-b-a-r-y-o-n OmniConvert fork](https://github.com/a-n-t-i-b-a-r-y-o-n/omniconvert)
- [CodeBreaker PS2 File Utility](https://github.com/mlafeldt/cb2util)

</details>

---

## 📋 Current Project Information

```text
Product: OmniConvertCS
Current Version: 1.06.02
Language: C++17
Architecture: 64-bit x64
Interface: Native Windows desktop (Win32 API)
Minimum Target: Windows 7 SP1
Legacy C# Source: ~Legacy~/
```
