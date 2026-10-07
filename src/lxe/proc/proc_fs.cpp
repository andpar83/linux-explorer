#include "lxe/proc/proc_fs.hpp"

#include "lxe/sys/file.hpp"
#include "lxe/sys/unique_fd.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <format>
#include <span>
#include <utility>

namespace lxe::proc {
namespace {

/// Cap for cmdline: argument lists can be megabytes, and only the start is ever displayed.
constexpr std::size_t cmdline_read_limit = 64 * 1024;
/// Cap for maps: a browser's tens of thousands of mappings are a few megabytes of text.
constexpr std::size_t maps_read_limit = 64 * 1024 * 1024;
/// smaps is about 25 lines per mapping.
constexpr std::size_t smaps_read_limit = 512 * 1024 * 1024;

} // namespace

ProcFs::ProcFs(std::filesystem::path root)
: root_{std::move(root)}
{}

std::expected<std::vector<ProcessId>, std::error_code> ProcFs::list_processes() const
{
    std::error_code error;
    std::filesystem::directory_iterator entry{root_, error};
    if (error) {
        return std::unexpected{error};
    }
    std::vector<ProcessId> pids;
    for (; entry != std::filesystem::directory_iterator{}; entry.increment(error)) {
        if (error) {
            return std::unexpected{error};
        }
        if (const auto pid = parse_pid(entry->path().filename().native())) {
            pids.push_back(*pid);
        }
    }
    if (error) {
        return std::unexpected{error};
    }
    std::ranges::sort(pids);
    return pids;
}

std::expected<ProcessInfo, std::error_code> ProcFs::read_process(ProcessId pid) const
{
    const auto dir = root_ / std::format("{}", pid);
    auto stat = sys::read_file(dir / "stat").and_then(parse_stat);
    if (!stat) {
        return std::unexpected{stat.error()};
    }
    const auto status = sys::read_file(dir / "status").and_then(parse_status);
    if (!status) {
        return std::unexpected{status.error()};
    }
    ProcessInfo info{.stat = std::move(*stat), .uid = status->effective_uid, .command_line = {}};
    // cmdline is optional: it is empty for kernel threads and can disappear with the process.
    if (const auto cmdline = sys::read_file(dir / "cmdline", cmdline_read_limit)) {
        info.command_line = parse_cmdline(*cmdline);
    }
    return info;
}

std::expected<std::vector<MemoryMapping>, std::error_code> ProcFs::read_maps(ProcessId pid) const
{
    return sys::read_file(root_ / std::format("{}", pid) / "maps", maps_read_limit).and_then(parse_maps);
}

std::expected<std::vector<SmapsEntry>, std::error_code> ProcFs::read_smaps(ProcessId pid) const
{
    return sys::read_file(root_ / std::format("{}", pid) / "smaps", smaps_read_limit).and_then(parse_smaps);
}

std::expected<std::vector<std::uint64_t>, std::error_code> ProcFs::read_pagemap(
    ProcessId pid,
    std::uint64_t first_page,
    std::size_t count
) const
{
    constexpr std::size_t entry_bytes = sizeof(std::uint64_t);
    const auto path = root_ / std::format("{}", pid) / "pagemap";
    const sys::UniqueFd fd{::open(path.c_str(), O_RDONLY | O_CLOEXEC)};
    if (!fd.valid()) {
        return std::unexpected{std::error_code{errno, std::system_category()}};
    }
    std::vector<std::uint64_t> entries(count);
    std::size_t have = 0;
    while (have < count) {
        const auto want = (count - have) * entry_bytes;
        const auto offset = static_cast<off_t>((first_page + have) * entry_bytes);
        const ssize_t got = ::pread(fd.get(), std::span{entries}.subspan(have).data(), want, offset);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected{std::error_code{errno, std::system_category()}};
        }
        if (got == 0) {
            break; // past the end of the address space
        }
        have += static_cast<std::size_t>(got) / entry_bytes;
    }
    entries.resize(have);
    return entries;
}

std::expected<SystemInfo, std::error_code> ProcFs::read_system() const
{
    const auto cpu = sys::read_file(root_ / "stat").and_then(parse_cpu_times);
    if (!cpu) {
        return std::unexpected{cpu.error()};
    }
    const auto memory = sys::read_file(root_ / "meminfo").and_then(parse_meminfo);
    if (!memory) {
        return std::unexpected{memory.error()};
    }
    return SystemInfo{.cpu = *cpu, .memory = *memory};
}

std::expected<Snapshot, std::error_code> ProcFs::read_snapshot() const
{
    auto system = read_system();
    if (!system) {
        return std::unexpected{system.error()};
    }
    const auto pids = list_processes();
    if (!pids) {
        return std::unexpected{pids.error()};
    }
    Snapshot snapshot{.processes = {}, .system = *system};
    snapshot.processes.reserve(pids->size());
    for (const ProcessId pid : *pids) {
        if (auto process = read_process(pid)) {
            snapshot.processes.push_back(std::move(*process));
        }
    }
    return snapshot;
}

} // namespace lxe::proc
