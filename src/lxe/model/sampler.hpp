#pragma once

#include "lxe/ids.hpp"
#include "lxe/model/process_model.hpp"
#include "lxe/proc/proc_fs.hpp"
#include "lxe/sys/users.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace lxe::model {

/// Turns successive snapshots into models. CPU usage is the difference between two snapshots,
/// so the sampler remembers the previous one.
class Sampler
{
public:
    explicit Sampler(std::uint64_t page_size, sys::UserNames users = sys::UserNames{});

    /// The model for `snapshot`. The first call has no CPU figures; later calls measure CPU
    /// usage since the previous call.
    [[nodiscard]] Model update(const proc::Snapshot& snapshot);

private:
    struct CpuSample
    {
        std::uint64_t start_time_ticks = 0; ///< Tells a reused pid apart from the old process.
        std::uint64_t cpu_ticks = 0;
    };

    std::uint64_t page_size_;
    sys::UserNames users_;
    std::optional<proc::CpuTimes> previous_cpu_;
    std::unordered_map<ProcessId, CpuSample> previous_processes_;
};

} // namespace lxe::model
