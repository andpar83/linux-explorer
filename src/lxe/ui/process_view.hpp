#pragma once

#include "lxe/ids.hpp"
#include "lxe/model/details.hpp"
#include "lxe/model/process_model.hpp"
#include "lxe/ui/theme.hpp"

#include <imgui.h>

#include <cstdint>
#include <optional>

namespace lxe::ui {

enum class ExpandRequest
{
    none,
    expand_all,
    collapse_all,
};

/// UI state that lives across frames.
struct ViewState
{
    bool paused = false;
    std::optional<ProcessId> selected;
    ExpandRequest expand = ExpandRequest::none; ///< Applied to every tree node on the next frame.
    bool show_details = true;                   ///< Lower pane with the selected process's memory maps.
    float details_height = 0.0F;                ///< Lower pane height in pixels; 0 = a third of the window.
    std::optional<std::uint64_t> selected_mapping; ///< Start address of the mapping whose pages are shown.
};

/// Fixed inputs of the view.
struct ViewConfig
{
    UserId current_user{};
    Palette palette{};
    bool icons = false;            ///< The icon font is loaded, so buttons may show icon glyphs.
    ImFont* mono_font = nullptr;   ///< Monospace font for addresses; the default font when null.
};

/// What a frame asks of the application loop.
struct FrameRequests
{
    bool refresh_now = false;
};

/// Draws the whole main window for one frame, filling the viewport: toolbar, process tree
/// table, the details pane for the selected process, and the status bar.
/// Keyboard: Space pauses/resumes, F5 refreshes, Ctrl+M shows/hides the details pane.
[[nodiscard]] FrameRequests draw_main_window(
    const model::Model& model,
    const model::ProcessDetails& details,
    ViewState& state,
    const ViewConfig& config
);

} // namespace lxe::ui
