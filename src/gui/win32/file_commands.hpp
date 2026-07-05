#pragma once

#include "formats/binary/application_formats.hpp"

#include <windows.h>
#include <shellapi.h>

namespace omni::gui::win32 {

void open_text_file(HWND window);
void open_application_file(HWND window, formats::binary::ApplicationFormat format);
void save_output_text(HWND window);
void save_application_file(HWND window, formats::binary::ApplicationFormat format);
void initialize_cbc_version_menu(HWND window);
bool handle_cbc_version_command(HWND window, int command_id);
void load_dropped_file(HWND window, HDROP drop);

} // namespace omni::gui::win32
