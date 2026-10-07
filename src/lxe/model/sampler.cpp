#include "lxe/model/sampler.hpp"

#include "lxe/model/format.hpp"

#include <algorithm>
#include <utility>

namespace lxe::model {
namespace {

[[nodiscard]] double percent(std::uint64_t part, std::uint64_t whole) noexcept
{
    constexpr double hundred = 100.0;
    return std::clamp(hundred * static_cast<double>(part) / static_cast<double>(whole), 0.0, hundred);
}

} // namespace

Sampler::Sampler(std::uint64_t page_size, sys::UserNames users)
: page_size_{page_size}
, users_{std::move(users)}
{}

Model Sampler::update(const proc::Snapshot& snapshot)
{
    const proc::CpuTimes& cpu = snapshot.system.cpu;
    // Ticks of all CPUs together since the previous sample: the denominator for every CPU %.
    std::optional<std::uint64_t> elapsed;
    if (previous_cpu_ && cpu.total_ticks > previous_cpu_->total_ticks) {
        elapsed = cpu.total_ticks - previous_cpu_->total_ticks;
    }

    Model model;
    model.processes.reserve(snapshot.processes.size());
    std::unordered_map<ProcessId, CpuSample> current;
    current.reserve(snapshot.processes.size());

    for (const proc::ProcessInfo& process : snapshot.processes) {
        const proc::StatInfo& stat = process.stat;
        ProcessEntry entry{
            .pid = stat.pid,
            .ppid = stat.ppid,
            .name = stat.comm,
            .uid = process.uid,
            .user = users_.name(process.uid),
            .command_line = join_command_line(process.command_line),
            .state = stat.state,
            .threads = stat.threads,
            .memory_bytes = stat.resident_pages > 0 ? static_cast<std::uint64_t>(stat.resident_pages) * page_size_ : 0,
            .cpu_percent = std::nullopt,
            .kernel_thread = stat.is_kernel_thread(),
        };
        if (elapsed) {
            const auto previous = previous_processes_.find(stat.pid);
            if (previous != previous_processes_.end() &&
                previous->second.start_time_ticks == stat.start_time_ticks) {
                const std::uint64_t before = previous->second.cpu_ticks;
                const std::uint64_t now = stat.cpu_ticks();
                entry.cpu_percent = percent(now >= before ? now - before : 0, *elapsed);
            }
        }
        current.emplace(stat.pid, CpuSample{.start_time_ticks = stat.start_time_ticks, .cpu_ticks = stat.cpu_ticks()});
        model.summary.threads += stat.threads;
        model.processes.push_back(std::move(entry));
    }

    if (elapsed) {
        const std::uint64_t idle =
            cpu.idle_ticks >= previous_cpu_->idle_ticks ? cpu.idle_ticks - previous_cpu_->idle_ticks : 0;
        model.summary.cpu_percent = percent(*elapsed - std::min(idle, *elapsed), *elapsed);
    }
    const proc::MemoryInfo& memory = snapshot.system.memory;
    model.summary.memory_total_bytes = memory.total_bytes;
    model.summary.memory_used_bytes = memory.total_bytes - std::min(memory.available_bytes, memory.total_bytes);
    model.summary.processes = model.processes.size();
    model.tree = ProcessTree{model.processes};

    previous_cpu_ = cpu;
    previous_processes_ = std::move(current);
    return model;
}

} // namespace lxe::model
