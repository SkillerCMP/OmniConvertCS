#!/usr/bin/env python3
"""Static parity check for the Win32 Options menu and its command wiring."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
rc = (root / "src/gui/win32/resources/resource.rc").read_text(encoding="utf-8")
resource_h = (root / "src/gui/win32/resources/resource.h").read_text(encoding="utf-8")
sources = "\n".join(
    p.read_text(encoding="utf-8")
    for p in (root / "src/gui/win32").glob("*.cpp")
)

match = re.search(r'POPUP "Options"\s*BEGIN(?P<body>.*?)\n\s*END\nEND', rc, re.S)
if not match:
    raise SystemExit("Options menu was not found")
body = match.group("body")

ordered_tokens = [
    'POPUP "Transpose Type"',
    'MENUITEM "Strict"',
    'MENUITEM "Original"',
    'MENUITEM "CMP Output Mode"',
    'MENUITEM "MGS C-Type Pointer Mode"',
    'MENUITEM "Make Organizers"',
    'MENUITEM "AR MAX Options...\\tCtrl+M"',
    'MENUITEM "Set PNACH CRC..."',
    'MENUITEM "Set AR2 Key Code...\\tCtrl+K"',
    'POPUP "GS3 Key"',
    'MENUITEM "0"',
    'MENUITEM "1"',
    'MENUITEM "2"',
    'MENUITEM "3"',
    'MENUITEM "4"',
    'POPUP "CBC Save Version"',
    'MENUITEM "Version 7 (Day1)"',
    'MENUITEM "Version 8 (CFU)"',
]
position = -1
for token in ordered_tokens:
    next_position = body.find(token, position + 1)
    if next_position < 0:
        raise SystemExit(f"Missing or out-of-order Options item: {token}")
    position = next_position

required_ids = [
    "ID_TRANSPOSE_STRICT", "ID_TRANSPOSE_ORIGINAL",
    "ID_OPTION_CMP_OUTPUT", "ID_OPTION_MGS_CTYPE_POINTER",
    "ID_OPTION_MAKEFOLDERS", "ID_OPTIONS_ARMAX",
    "ID_OPTIONS_PNACH_CRC", "ID_AR2_KEY",
    "ID_GS3_KEY_0", "ID_GS3_KEY_1", "ID_GS3_KEY_2",
    "ID_GS3_KEY_3", "ID_GS3_KEY_4",
    "ID_OPTIONS_CBC_V7", "ID_OPTIONS_CBC_V8",
]
for command_id in required_ids:
    if command_id not in resource_h:
        raise SystemExit(f"Missing resource ID: {command_id}")
    if command_id not in sources:
        raise SystemExit(f"Options command is not wired in C++: {command_id}")

accelerators = [
    '"A",        ID_EDIT_SELECTALL, VIRTKEY, CONTROL',
    '"C",        ID_EDIT_COPY, VIRTKEY, CONTROL',
    '"K",        ID_AR2_KEY, VIRTKEY, CONTROL',
    '"M",        ID_OPTIONS_ARMAX, VIRTKEY, CONTROL',
    '"S",        ID_FILE_SAVEASTEXT, VIRTKEY, CONTROL',
    '"S",        ID_EDIT_SWAP, VIRTKEY, CONTROL, SHIFT',
    '"V",        ID_EDIT_PASTE, VIRTKEY, CONTROL',
    '"X",        ID_EDIT_CUT, VIRTKEY, CONTROL',
    '"Z",        ID_EDIT_UNDO, VIRTKEY, CONTROL',
    'VK_ESCAPE,  ID_FILE_EXIT, VIRTKEY',
]
for accelerator in accelerators:
    if accelerator not in rc:
        raise SystemExit(f"Missing accelerator: {accelerator}")
if '"^' in rc:
    raise SystemExit('Quoted control-character accelerator syntax remains; ^M aliases Enter')

required_dialog_controls = [
    "IDC_ARMAX_GAMEID", "IDC_ARMAX_VERIFIER_AUTO",
    "IDC_ARMAX_VERIFIER_MANUAL", "IDC_ARMAX_REGION_USA",
    "IDC_ARMAX_REGION_PAL", "IDC_ARMAX_REGION_JAPAN",
    "IDC_ARMAX_DISC_HASH", "IDC_AR2_COMMON", "IDC_EDIT_AR2",
]
for control_id in required_dialog_controls:
    if control_id not in rc or control_id not in resource_h:
        raise SystemExit(f"Missing Options dialog control: {control_id}")

for obsolete_label in [
    'POPUP "AR MAX Verifiers"',
    'POPUP "AR MAX Region"',
    'POPUP "AR MAX Disc Hash"',
]:
    if obsolete_label in body:
        raise SystemExit(f"Obsolete Options submenu remains: {obsolete_label}")

print("Options menu parity check passed")
