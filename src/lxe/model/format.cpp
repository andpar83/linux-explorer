#include "lxe/model/format.hpp"

#include <array>
#include <format>
#include <ranges>

namespace lxe::model {

std::string format_bytes(std::uint64_t bytes)
{
    using namespace std::string_view_literals;
    constexpr std::array units{"KiB"sv, "MiB"sv, "GiB"sv, "TiB"sv, "PiB"sv, "EiB"sv};
    constexpr double step = 1024.0;
    if (static_cast<double>(bytes) < step) {
        return std::format("{} B", bytes);
    }
    double value = static_cast<double>(bytes) / step;
    std::size_t unit = 0;
    while (value >= step && unit + 1 < units.size()) {
        value /= step;
        ++unit;
    }
    return std::format("{:.1f} {}", value, units.at(unit));
}

std::string format_cpu(std::optional<double> percent)
{
    constexpr double smallest_shown = 0.01;
    if (!percent || *percent <= 0.0) {
        return {};
    }
    if (*percent < smallest_shown) {
        return "< 0.01";
    }
    return std::format("{:.2f}", *percent);
}

std::string_view state_name(proc::ProcessState state) noexcept
{
    using enum proc::ProcessState;
    switch (state) {
    case running:
        return "Running";
    case sleeping:
        return "Sleeping";
    case disk_sleep:
        return "Disk sleep";
    case zombie:
        return "Zombie";
    case stopped:
        return "Stopped";
    case tracing_stop:
        return "Traced";
    case dead:
        return "Dead";
    case idle:
        return "Idle";
    case parked:
        return "Parked";
    }
    return "Unknown";
}

std::string join_command_line(std::span<const std::string> args)
{
    return args | std::views::join_with(' ') | std::ranges::to<std::string>();
}

std::string format_permissions(const proc::MemoryMapping& mapping)
{
    return {
        mapping.readable ? 'r' : '-',
        mapping.writable ? 'w' : '-',
        mapping.executable ? 'x' : '-',
        mapping.shared ? 's' : 'p',
    };
}

} // namespace lxe::model
