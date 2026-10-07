#pragma once

#include "lxe/ids.hpp"
#include "lxe/proc/parse.hpp"

#include <cstddef>
#include <cstdint>
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

    /// Memory mappings with page accounting (/proc/<pid>/smaps). Same access rule as read_maps.
    /// Costs the kernel a page-table walk: tens of milliseconds for a process with thousands
    /// of mappings.
    [[nodiscard]] std::expected<std::vector<SmapsEntry>, std::error_code> read_smaps(ProcessId pid) const;

    /// `count` entries of /proc/<pid>/pagemap starting at virtual page `first_page` (one entry
    /// per page, see decode_pagemap). Fewer entries come back past the end of the address space.
    [[nodiscard]] std::expected<std::vector<std::uint64_t>, std::error_code> read_pagemap(
        ProcessId pid,
        std::uint64_t first_page,
        std::size_t count
    ) const;

    /// Reads every process it can. Processes that exit, deny access or can't be parsed while
    /// being read are skipped: the process table changes constantly, so that is not an error.
    [[nodiscard]] std::expected<Snapshot, std::error_code> read_snapshot() const;

private:
    std::filesystem::path root_;
};

} // namespace lxe::proc
