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

/// What fills the window between the toolbar and the status bar.
enum class Page
{
    processes,   ///< The process tree.
    memory_maps, ///< The selected process's memory maps and the selected mapping's pages.
};

/// UI state that lives across frames.
struct ViewState
{
    bool paused = false;
    std::optional<ProcessId> selected;
    ExpandRequest expand = ExpandRequest::none; ///< Applied to every tree node on the next frame.
    Page page = Page::processes;
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

/// Draws the whole main window for one frame, filling the viewport: toolbar, the current page
/// (process tree, or memory maps of the selected process), and the status bar.
/// Keyboard: Space pauses/resumes, F5 refreshes, Ctrl+M switches between the two pages.
[[nodiscard]] FrameRequests draw_main_window(
    const model::Model& model,
    const model::ProcessDetails& details,
    ViewState& state,
    const ViewConfig& config
);

} // namespace lxe::ui
