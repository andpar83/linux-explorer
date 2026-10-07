#pragma once

#include "lxe/ids.hpp"
#include "lxe/model/details.hpp"
#include "lxe/model/process_model.hpp"
#include "lxe/proc/proc_fs.hpp"
#include "lxe/sys/users.hpp"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lxe::test {

/// Minimal model entry: only what the tree needs, plus a name for text output.
inline model::ProcessEntry entry(pid_t pid, pid_t ppid, std::string name = "proc")
{
    return model::ProcessEntry{
        .pid = ProcessId{pid},
        .ppid = ProcessId{ppid},
        .name = std::move(name),
        .uid = UserId{0},
        .user = "root",
        .command_line = {},
        .state = proc::ProcessState::sleeping,
        .threads = 1,
        .memory_bytes = 0,
        .cpu_percent = std::nullopt,
        .kernel_thread = false,
    };
}

/// A model (processes + tree) from entries.
inline model::Model make_model(std::vector<model::ProcessEntry> processes)
{
    model::Model result;
    result.processes = std::move(processes);
    result.tree = model::ProcessTree{result.processes};
    result.summary.processes = result.processes.size();
    return result;
}

/// A /proc process record for sampler tests.
struct ProcessSpec
{
    pid_t pid = 1;
    pid_t ppid = 0;
    std::string comm = "proc";
    std::uint64_t cpu_ticks = 0;
    std::uint64_t start_time_ticks = 0;
    std::int64_t resident_pages = 0;
    uid_t uid = 0;
    std::uint32_t flags = 0;
    std::int64_t threads = 1;
    std::vector<std::string> command_line;
};

inline proc::ProcessInfo process(const ProcessSpec& spec)
{
    proc::StatInfo stat;
    stat.pid = ProcessId{spec.pid};
    stat.comm = spec.comm;
    stat.state = proc::ProcessState::sleeping;
    stat.ppid = ProcessId{spec.ppid};
    stat.flags = spec.flags;
    stat.utime_ticks = spec.cpu_ticks;
    stat.threads = spec.threads;
    stat.start_time_ticks = spec.start_time_ticks;
    stat.resident_pages = spec.resident_pages;
    return proc::ProcessInfo{.stat = std::move(stat), .uid = UserId{spec.uid}, .command_line = spec.command_line};
}

inline proc::Snapshot snapshot(
    std::initializer_list<ProcessSpec> specs,
    proc::CpuTimes cpu,
    proc::MemoryInfo memory = {.total_bytes = 1000, .available_bytes = 400}
)
{
    proc::Snapshot result{.processes = {}, .system = {.cpu = cpu, .memory = memory}};
    for (const auto& spec : specs) {
        result.processes.push_back(process(spec));
    }
    return result;
}

/// A memory mapping with the given permissions string ("r-xp").
inline proc::MemoryMapping mapping(
    std::uint64_t start,
    std::uint64_t end,
    std::string path = {},
    std::string_view perms = "r--p",
    std::uint64_t offset = 0
)
{
    return proc::MemoryMapping{
        .start = start,
        .end = end,
        .readable = perms[0] == 'r',
        .writable = perms[1] == 'w',
        .executable = perms[2] == 'x',
        .shared = perms[3] == 's',
        .offset = offset,
        .device_major = path.starts_with('/') ? 0x103U : 0U,
        .device_minor = path.starts_with('/') ? 3U : 0U,
        .inode = path.starts_with('/') ? 49414585U : 0U,
        .path = std::move(path),
    };
}

/// A mapping without page accounting, as the details hold it.
inline model::MappingInfo info(proc::MemoryMapping mapping)
{
    return model::MappingInfo{.mapping = std::move(mapping), .stats = std::nullopt};
}

/// A pagemap entry with the given flags.
inline std::uint64_t pagemap_entry(bool present, bool swapped = false, bool exclusive = false, bool file_or_shared = false, bool soft_dirty = false)
{
    std::uint64_t entry = 0;
    entry |= present ? std::uint64_t{1} << 63U : 0;
    entry |= swapped ? std::uint64_t{1} << 62U : 0;
    entry |= file_or_shared ? std::uint64_t{1} << 61U : 0;
    entry |= exclusive ? std::uint64_t{1} << 56U : 0;
    entry |= soft_dirty ? std::uint64_t{1} << 55U : 0;
    return entry;
}

/// User database stand-in: root and alice exist, everyone else is unknown.
inline sys::UserNames fake_users()
{
    return sys::UserNames{[](UserId uid) -> std::optional<std::string> {
        switch (std::to_underlying(uid)) {
        case 0:
            return "root";
        case 1000:
            return "alice";
        default:
            return std::nullopt;
        }
    }};
}

/// Every node index reachable from the roots, in visiting order (iterative, any depth).
inline std::vector<std::size_t> walk(const model::ProcessTree& tree)
{
    std::vector<std::size_t> visited;
    std::vector<std::size_t> pending(tree.roots().rbegin(), tree.roots().rend());
    while (!pending.empty()) {
        const std::size_t node = pending.back();
        pending.pop_back();
        visited.push_back(node);
        const auto children = tree.children(node);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
    return visited;
}

} // namespace lxe::test
