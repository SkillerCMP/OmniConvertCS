# AR2 Input / Output Key Dropdown Update

This pass keeps `~Legacy~/` untouched and updates only the active C++ source tree.

## Added

- Added `1456E7A5` to the common AR2 key presets.
- Added separate AR2 input and output key state.
- Added an input-side AR2 key dropdown/edit box that appears when the selected input format uses Action Replay V2 encryption.
- Added an output-side AR2 key dropdown/edit box that appears when the selected output format uses Action Replay V2 encryption.
- Saved the selected input/output AR2 keys in `OmniconvertSettings.json` as:
  - `Ar2InputKey`
  - `Ar2OutputKey`

## Behavior

The dropdowns include common presets, but they are editable so a custom 8-digit encrypted AR2 key can still be typed directly.


- The input AR2 key is used only for decrypting AR2v2 input.
- The output AR2 key is used only for encrypting AR2v2 output.
- Input and output can both be set to Action Replay V2 with different keys, allowing re-keying from one AR2v2 key to another.
- Selecting `1456E7A5` acts as the AR2v2 RAW-enable/no-encryption key:
  - AR2v2 input with this key skips decryption and removes a leading `0E3C7DF2 1456E7A5` marker if present.
  - AR2v2 output with this key converts code types normally, prepends `0E3C7DF2 1456E7A5`, and skips AR2 encryption.

## Files changed

- `src/devices/action_replay/ar_crypto.hpp`
- `src/devices/action_replay/ar_crypto.cpp`
- `src/devices/action_replay/ar_batch.hpp`
- `src/devices/action_replay/ar_batch.cpp`
- `src/convert/conversion_service.hpp`
- `src/convert/conversion_service.cpp`
- `src/gui/win32/app_state.hpp`
- `src/gui/win32/context_controls.cpp`
- `src/gui/win32/conversion_controller.cpp`
- `src/gui/win32/key_dialog.cpp`
- `src/gui/win32/resources/resource.h`
- `src/gui/win32/resources/resource.rc`
- `src/gui/win32/settings.cpp`
