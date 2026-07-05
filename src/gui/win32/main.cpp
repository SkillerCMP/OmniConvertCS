#include "cli/cli_app.hpp"
#include "omni/identity.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/dialog_proc.hpp"
#include "gui/win32/resources/resource.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool valid_standard_handle(DWORD kind) noexcept {
    const HANDLE handle = GetStdHandle(kind);
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

void reopen_stream(FILE* stream, const char* device, const char* mode) {
#ifdef _MSC_VER
    FILE* reopened = nullptr;
    (void)freopen_s(&reopened, device, mode, stream);
#else
    (void)freopen(device, mode, stream);
#endif
}

void prepare_cli_console() {
    const bool had_stdin = valid_standard_handle(STD_INPUT_HANDLE);
    const bool had_stdout = valid_standard_handle(STD_OUTPUT_HANDLE);
    const bool had_stderr = valid_standard_handle(STD_ERROR_HANDLE);

    if (!had_stdin || !had_stdout || !had_stderr) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            (void)AllocConsole();
        }
    }

    // Preserve inherited redirected handles. Reopen only streams that were absent
    // when the GUI-subsystem process started.
    if (!had_stdin) reopen_stream(stdin, "CONIN$", "r");
    if (!had_stdout) reopen_stream(stdout, "CONOUT$", "w");
    if (!had_stderr) reopen_stream(stderr, "CONOUT$", "w");

    std::ios::sync_with_stdio(true);
}

std::string wide_to_active_code_page(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') return {};
    const int size = WideCharToMultiByte(CP_ACP, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    (void)WideCharToMultiByte(CP_ACP, 0, value, -1, result.data(), size,
                              nullptr, nullptr);
    if (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}

struct NarrowArguments {
    std::vector<std::string> storage;
    std::vector<char*> pointers;
};

NarrowArguments command_line_arguments() {
    int count = 0;
    wchar_t** wide_arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    NarrowArguments result;
    if (wide_arguments == nullptr || count <= 0) return result;

    result.storage.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        result.storage.push_back(wide_to_active_code_page(wide_arguments[index]));
    }
    LocalFree(wide_arguments);

    result.pointers.reserve(result.storage.size());
    for (std::string& argument : result.storage) result.pointers.push_back(argument.data());
    return result;
}

int run_gui(HINSTANCE instance, int show_command) {
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);

    omni::gui::win32::AppState state{};
    state.instance = instance;

    HWND window = CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_DIALOG1), nullptr,
                                     omni::gui::win32::main_dialog_proc,
                                     reinterpret_cast<LPARAM>(&state));
    if (window == nullptr) {
        MessageBoxW(nullptr, L"Unable to create the application window.", omni::identity::product_name_w.data(),
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(window, show_command == 0 ? SW_SHOW : show_command);
    UpdateWindow(window);

    HACCEL accelerators = LoadAcceleratorsW(instance, MAKEINTRESOURCEW(IDA_ACCEL_TABLE));
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (accelerators != nullptr && TranslateAcceleratorW(window, accelerators, &message)) continue;
        if (IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    NarrowArguments arguments = command_line_arguments();
    const bool force_gui = arguments.storage.size() >= 2U && arguments.storage[1] == "--gui";
    if (arguments.storage.size() >= 2U && !force_gui) {
        prepare_cli_console();
        return omni::cli::run_cli(static_cast<int>(arguments.pointers.size()),
                                  arguments.pointers.data());
    }
    return run_gui(instance, show_command);
}
