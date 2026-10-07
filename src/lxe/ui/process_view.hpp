#pragma once

#include "lxe/ids.hpp"
#include "lxe/model/process_model.hpp"
#include "lxe/ui/theme.hpp"

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
};

/// Fixed inputs of the view.
struct ViewConfig
{
    UserId current_user{};
    Palette palette{};
    bool icons = false; ///< The icon font is loaded, so buttons may show icon glyphs.
};

/// What a frame asks of the application loop.
struct FrameRequests
{
    bool refresh_now = false;
};

/// Draws the whole main window for one frame, filling the viewport: toolbar, process tree
/// table and status bar. Keyboard: Space pauses/resumes, F5 refreshes.
[[nodiscard]] FrameRequests draw_main_window(const model::Model& model, ViewState& state, const ViewConfig& config);

} // namespace lxe::ui
