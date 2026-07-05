#include "gui/win32/layout.hpp"

#include "gui/win32/resources/resource.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace omni::gui::win32 {
namespace {

struct ControlPlacement {
    HWND control{};
    int x{};
    int y{};
    int width{};
    int height{};
};

template <std::size_t Capacity>
void add_placement(std::array<ControlPlacement, Capacity>& placements,
                   std::size_t& count,
                   HWND parent,
                   int id,
                   int x,
                   int y,
                   int width,
                   int height) noexcept {
    if (count >= placements.size()) return;
    if (HWND control = GetDlgItem(parent, id); control != nullptr) {
        placements[count++] = ControlPlacement{
            control,
            x,
            y,
            std::max(width, 1),
            std::max(height, 1),
        };
    }
}

template <std::size_t Capacity>
void apply_placements(HWND parent,
                      const std::array<ControlPlacement, Capacity>& placements,
                      std::size_t count) noexcept {
    constexpr UINT position_flags =
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;

    bool applied = false;
    if (HDWP batch = BeginDeferWindowPos(static_cast<int>(count)); batch != nullptr) {
        bool batch_valid = true;
        for (std::size_t index = 0; index < count; ++index) {
            const ControlPlacement& item = placements[index];
            batch = DeferWindowPos(batch,
                                   item.control,
                                   nullptr,
                                   item.x,
                                   item.y,
                                   item.width,
                                   item.height,
                                   position_flags);
            if (batch == nullptr) {
                batch_valid = false;
                break;
            }
        }
        if (batch_valid) {
            applied = EndDeferWindowPos(batch) != FALSE;
        }
    }

    // A failed deferred batch must not leave only some controls repositioned.
    // Reapply the complete layout directly before issuing the single repaint.
    if (!applied) {
        for (std::size_t index = 0; index < count; ++index) {
            const ControlPlacement& item = placements[index];
            SetWindowPos(item.control,
                         nullptr,
                         item.x,
                         item.y,
                         item.width,
                         item.height,
                         position_flags);
        }
    }

    // Static labels use the dialog background. Erase the vacated rectangles and
    // redraw every child once after the batch so resizing cannot leave ghost text,
    // duplicate button faces, or old edit-control borders behind.
    RedrawWindow(parent,
                 nullptr,
                 nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

} // namespace

void layout_controls(HWND window, int client_width, int client_height) noexcept {
    constexpr int margin = 8;
    constexpr int gap = 8;
    constexpr int label_height = 20;
    constexpr int button_width = 62;
    constexpr int crc_width = 72;
    constexpr int aux_width = 118;
    constexpr int bottom_row_height = 25;
    constexpr int status_height = 18;

    std::array<ControlPlacement, 18> placements{};
    std::size_t placement_count = 0;
    const auto place = [&](int id, int x, int y, int width, int height) noexcept {
        add_placement(placements, placement_count, window, id, x, y, width, height);
    };

    const int content_width = std::max(client_width - margin * 2, 200);
    const int pane_width = std::max((content_width - gap) / 2, 90);
    const int right_x = margin + pane_width + gap;
    const int editor_top = margin + label_height;
    const int editor_bottom = client_height - margin - bottom_row_height - status_height - 6;
    const int editor_height = std::max(editor_bottom - editor_top, 80);

    const int input_aux_x = margin + pane_width - button_width - aux_width - 8;
    place(ID_STATIC_INPUT, margin, margin + 2,
          std::max(input_aux_x - margin - 4, 40), 16);
    place(ID_STATIC_AR2_CURRENT, input_aux_x, margin + 2, 42, 16);
    place(IDC_EDIT_AR2_CURRENT, input_aux_x + 44, margin, 68, 20);
    place(ID_STATIC_GAMEID_INPUT, input_aux_x, margin + 2, 42, 16);
    place(IDC_EDIT_GAMEID_INPUT, input_aux_x + 44, margin, 48, 20);
    place(ID_BTN_CLEAR_IN, margin + pane_width - button_width, margin,
          button_width, 20);

    const int output_aux_x = right_x + pane_width - button_width - crc_width - aux_width - 8;
    place(ID_STATIC_OUTPUT, right_x, margin + 2,
          std::max(output_aux_x - right_x - 4, 40), 16);
    place(ID_STATIC_GAMEID_OUTPUT, output_aux_x, margin + 2, 42, 16);
    place(IDC_EDIT_GAMEID_OUTPUT, output_aux_x + 44, margin, 48, 20);
    place(IDC_CHECK_PNACH_CRC,
          right_x + pane_width - button_width - crc_width - 4,
          margin, crc_width, 20);
    place(ID_BTN_CLEAR_OUT, right_x + pane_width - button_width, margin,
          button_width, 20);

    place(IDC_EDIT_IN, margin, editor_top, pane_width, editor_height);
    place(IDC_EDIT_OUT, right_x, editor_top, pane_width, editor_height);

    const int bottom_y = editor_bottom + 6;
    place(ID_BTN_CONVERT, margin, bottom_y, 72, 23);
    place(ID_BTN_SWAP, margin + 78, bottom_y, 58, 23);

    const int game_name_label_x = margin + 146;
    place(ID_STATIC_GAMENAME, game_name_label_x, bottom_y + 4, 72, 17);
    place(IDC_EDIT_GAMENAME, game_name_label_x + 74, bottom_y,
          std::max(client_width - margin - (game_name_label_x + 74), 80), 23);

    place(ID_STATIC_STATUS, margin, client_height - margin - status_height,
          content_width, status_height);

    apply_placements(window, placements, placement_count);
}

} // namespace omni::gui::win32
