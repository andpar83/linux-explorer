#pragma once

#include "lxe/proc/proc_fs.hpp"
#include "lxe/ui/theme.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace lxe::app {

struct Options
{
    /// Colour theme; the desktop's light/dark setting when not given.
    std::optional<ui::Theme> theme;
    /// Render a few frames, save the window contents to this file (binary PPM) and exit.
    /// For checking the UI from scripts and tests.
    std::optional<std::filesystem::path> screenshot;
    /// Process selected when the window opens.
    std::optional<ProcessId> selected;
    /// Mapping of that process selected when the window opens: a start address in hex, or a
    /// path as the maps list shows it (e.g. "[heap]").
    std::optional<std::string> selected_mapping;
};

/// Opens the main window and runs until it is closed. Returns the process exit code.
[[nodiscard]] int run(const proc::ProcFs& fs, const Options& options);

} // namespace lxe::app
