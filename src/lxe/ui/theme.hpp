#pragma once

#include <imgui.h>

namespace lxe::ui {

enum class Theme
{
    light,
    dark,
};

/// Colours that carry meaning in the process view, on top of the ImGui style.
struct Palette
{
    ImU32 own_process_row = 0;   ///< Rows of the current user's processes (Process Explorer's blue).
    ImU32 cpu_heat = 0;          ///< CPU cell background at full load; fainter for lower load.
    ImVec4 kernel_thread_text{}; ///< Kernel threads are shown dimmed.
    ImVec4 muted_text{};         ///< Secondary text such as the status bar.
    ImVec4 accent_text{};        ///< Highlighted status text, e.g. "Paused".
    ImU32 page_present = 0;      ///< Presence strip: pages resident in memory.
    ImU32 page_swapped = 0;      ///< Presence strip: pages in swap.
    ImU32 page_absent = 0;       ///< Presence strip: pages never touched or reclaimed.
};

/// Installs the ImGui style for `theme`, scaled for the display, and returns its palette.
/// `font_size` is the unscaled base font size in pixels.
Palette apply_theme(Theme theme, float scale, float font_size);

} // namespace lxe::ui
