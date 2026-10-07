#pragma once

#include "lxe/proc/parse.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace lxe::model {

/// "512 B", "1.5 KiB", "12.3 MiB", ...
[[nodiscard]] std::string format_bytes(std::uint64_t bytes);

/// CPU column text: blank when idle or unknown (as Process Explorer does), "< 0.01" for tiny values.
[[nodiscard]] std::string format_cpu(std::optional<double> percent);

/// Human-readable scheduler state, e.g. "Sleeping".
[[nodiscard]] std::string_view state_name(proc::ProcessState state) noexcept;

/// Arguments joined with spaces, the way a shell would show them (without quoting).
[[nodiscard]] std::string join_command_line(std::span<const std::string> args);

} // namespace lxe::model
