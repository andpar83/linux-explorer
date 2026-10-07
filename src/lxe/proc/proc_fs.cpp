#include "lxe/proc/proc_fs.hpp"

#include "lxe/sys/file.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace lxe::proc {
namespace {

/// Cap for cmdline: argument lists can be megabytes, and only the start is ever displayed.
constexpr std::size_t cmdline_read_limit = 64 * 1024;
/// Cap for maps: a browser's tens of thousands of mappings are a few megabytes of text.
constexpr std::size_t maps_read_limit = 64 * 1024 * 1024;

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
