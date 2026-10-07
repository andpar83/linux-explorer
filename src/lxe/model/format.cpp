#include "lxe/model/format.hpp"

#include <array>
#include <format>
#include <ranges>
#include <utility>

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

std::string_view vm_flag_description(std::string_view code) noexcept
{
    using namespace std::string_view_literals;
    // From Documentation/filesystems/proc.rst, "VmFlags".
    constexpr std::array table{
        std::pair{"rd"sv, "readable"sv},
        std::pair{"wr"sv, "writable"sv},
        std::pair{"ex"sv, "executable"sv},
        std::pair{"sh"sv, "shared"sv},
        std::pair{"mr"sv, "may read"sv},
        std::pair{"mw"sv, "may write"sv},
        std::pair{"me"sv, "may execute"sv},
        std::pair{"ms"sv, "may share"sv},
        std::pair{"gd"sv, "stack segment grows down"sv},
        std::pair{"pf"sv, "pure PFN range"sv},
        std::pair{"lo"sv, "pages are locked in memory"sv},
        std::pair{"io"sv, "memory mapped I/O area"sv},
        std::pair{"sr"sv, "sequential read advise provided"sv},
        std::pair{"rr"sv, "random read advise provided"sv},
        std::pair{"dc"sv, "do not copy area on fork"sv},
        std::pair{"de"sv, "do not expand area on remapping"sv},
        std::pair{"ac"sv, "area is accountable"sv},
        std::pair{"nr"sv, "swap space is not reserved for the area"sv},
        std::pair{"ht"sv, "area uses huge tlb pages"sv},
        std::pair{"sf"sv, "synchronous page fault"sv},
        std::pair{"ar"sv, "architecture specific flag"sv},
        std::pair{"wf"sv, "wipe on fork"sv},
        std::pair{"dd"sv, "do not include area into core dump"sv},
        std::pair{"sd"sv, "soft-dirty flag"sv},
        std::pair{"mm"sv, "mixed map area"sv},
        std::pair{"hg"sv, "huge page advise flag"sv},
        std::pair{"nh"sv, "no-huge page advise flag"sv},
        std::pair{"mg"sv, "mergeable advise flag"sv},
        std::pair{"bt"sv, "arm64 BTI guarded page"sv},
        std::pair{"mt"sv, "arm64 MTE allocation tags are enabled"sv},
        std::pair{"um"sv, "userfaultfd missing tracking"sv},
        std::pair{"uw"sv, "userfaultfd wr-protect tracking"sv},
        std::pair{"ss"sv, "shadow stack page"sv},
        std::pair{"sl"sv, "sealed"sv},
        std::pair{"lf"sv, "lock on fault pages"sv},
        std::pair{"dp"sv, "always lazily freeable mapping"sv},
    };
    for (const auto& [flag, description] : table) {
        if (flag == code) {
            return description;
        }
    }
    return code;
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
