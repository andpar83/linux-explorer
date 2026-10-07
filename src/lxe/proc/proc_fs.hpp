#pragma once

#include "lxe/ids.hpp"
#include "lxe/proc/parse.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace lxe::proc {

/// One process as read from /proc.
struct ProcessInfo
{
    StatInfo stat;
    UserId uid{}; ///< Effective user id.
    std::vector<std::string> command_line;
};

/// Machine-wide counters.
struct SystemInfo
{
    CpuTimes cpu;
    MemoryInfo memory;
};

/// Everything read from /proc in one pass, processes sorted by pid.
struct Snapshot
{
    std::vector<ProcessInfo> processes;
    SystemInfo system;
};

/// Reads processes from a procfs mount. The root is a parameter so tests can point it at fixtures.
class ProcFs
{
public:
    explicit ProcFs(std::filesystem::path root = "/proc");

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

    /// Ids of all visible processes, ascending.
    [[nodiscard]] std::expected<std::vector<ProcessId>, std::error_code> list_processes() const;

    [[nodiscard]] std::expected<ProcessInfo, std::error_code> read_process(ProcessId pid) const;

    [[nodiscard]] std::expected<SystemInfo, std::error_code> read_system() const;

    /// Memory mappings of a process (/proc/<pid>/maps). Another user's process needs
    /// CAP_SYS_PTRACE; the kernel answers with EACCES, reported as such.
    [[nodiscard]] std::expected<std::vector<MemoryMapping>, std::error_code> read_maps(ProcessId pid) const;

    /// Reads every process it can. Processes that exit, deny access or can't be parsed while
    /// being read are skipped: the process table changes constantly, so that is not an error.
    [[nodiscard]] std::expected<Snapshot, std::error_code> read_snapshot() const;

private:
    std::filesystem::path root_;
};

} // namespace lxe::proc
