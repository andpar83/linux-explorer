#pragma once

#include "lxe/ids.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace lxe::proc {

/// Why /proc content could not be parsed. Zero is reserved for "no error", as std::error_code requires.
enum class ParseError
{
    malformed = 1, ///< The overall layout is wrong, e.g. no "(comm)" in a stat line.
    truncated,     ///< Fewer fields than expected.
    bad_number,    ///< A numeric field doesn't parse or is out of range.
    missing_field, ///< A required "Key:" line is absent.
};

[[nodiscard]] const std::error_category& parse_error_category() noexcept;

[[nodiscard]] std::error_code make_error_code(ParseError error) noexcept;

} // namespace lxe::proc

template <>
struct std::is_error_code_enum<lxe::proc::ParseError> : std::true_type
{};

namespace lxe::proc {

/// Scheduler state letter from /proc/<pid>/stat. Letters not listed here are kept as-is.
enum class ProcessState : char
{
    running = 'R',
    sleeping = 'S',
    disk_sleep = 'D',
    zombie = 'Z',
    stopped = 'T',
    tracing_stop = 't',
    dead = 'X',
    idle = 'I',
    parked = 'P',
};

/// PF_KTHREAD from <linux/sched.h>: the task is a kernel thread.
inline constexpr std::uint32_t pf_kthread = 0x0020'0000;

/// The fields of /proc/<pid>/stat the application uses (see proc_pid_stat(5)).
struct StatInfo
{
    ProcessId pid{};
    std::string comm; ///< Command name, at most 15 bytes for user processes (TASK_COMM_LEN).
    ProcessState state{};
    ProcessId ppid{};
    std::uint32_t flags = 0;
    std::uint64_t utime_ticks = 0;
    std::uint64_t stime_ticks = 0;
    std::int64_t priority = 0;
    std::int64_t nice = 0;
    std::int64_t threads = 0;
    std::uint64_t start_time_ticks = 0; ///< Clock ticks after boot. With the pid it identifies a process.
    std::uint64_t virtual_bytes = 0;
    std::int64_t resident_pages = 0;

    [[nodiscard]] bool is_kernel_thread() const noexcept { return (flags & pf_kthread) != 0; }

    [[nodiscard]] std::uint64_t cpu_ticks() const noexcept { return utime_ticks + stime_ticks; }

    friend bool operator==(const StatInfo&, const StatInfo&) = default;
};

/// The fields of /proc/<pid>/status the application uses.
struct StatusInfo
{
    UserId real_uid{};
    UserId effective_uid{};

    friend bool operator==(const StatusInfo&, const StatusInfo&) = default;
};

/// Aggregate CPU time of all CPUs, from the first line of /proc/stat, in clock ticks.
struct CpuTimes
{
    std::uint64_t total_ticks = 0;
    std::uint64_t idle_ticks = 0; ///< idle + iowait.

    friend bool operator==(const CpuTimes&, const CpuTimes&) = default;
};

/// System memory from /proc/meminfo.
struct MemoryInfo
{
    std::uint64_t total_bytes = 0;
    std::uint64_t available_bytes = 0;

    friend bool operator==(const MemoryInfo&, const MemoryInfo&) = default;
};

/// A /proc directory name such as "1234" as a process id; nullopt for anything else ("self", "net", ...).
[[nodiscard]] std::optional<ProcessId> parse_pid(std::string_view text) noexcept;

/// Parses /proc/<pid>/stat. The command name may contain any byte, including spaces and ')'.
[[nodiscard]] std::expected<StatInfo, std::error_code> parse_stat(std::string_view text);

/// Parses the Uid line of /proc/<pid>/status.
[[nodiscard]] std::expected<StatusInfo, std::error_code> parse_status(std::string_view text);

/// Splits /proc/<pid>/cmdline into arguments. Trailing empty arguments (NUL padding left by processes
/// that rewrite their title) are dropped. Kernel threads and zombies have none.
[[nodiscard]] std::vector<std::string> parse_cmdline(std::string_view text);

/// Parses the aggregate "cpu" line of /proc/stat.
[[nodiscard]] std::expected<CpuTimes, std::error_code> parse_cpu_times(std::string_view text);

/// Parses /proc/meminfo. Falls back to MemFree on kernels without MemAvailable.
[[nodiscard]] std::expected<MemoryInfo, std::error_code> parse_meminfo(std::string_view text);

} // namespace lxe::proc
