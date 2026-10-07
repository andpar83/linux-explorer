#pragma once

#include "lxe/ids.hpp"
#include "lxe/proc/parse.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lxe::model {

/// Everything the views show about one process.
struct ProcessEntry
{
    ProcessId pid{};
    ProcessId ppid{};
    std::string name;
    UserId uid{};
    std::string user;
    std::string command_line;
    proc::ProcessState state{};
    std::int64_t threads = 0;
    std::uint64_t memory_bytes = 0;    ///< Resident set size.
    std::optional<double> cpu_percent; ///< Share of total machine CPU time since the previous sample.
    bool kernel_thread = false;
};

/// Machine-wide totals for the status bar.
struct SystemSummary
{
    std::optional<double> cpu_percent;
    std::uint64_t memory_total_bytes = 0;
    std::uint64_t memory_used_bytes = 0;
    std::size_t processes = 0;
    std::int64_t threads = 0;
};

/// Parent/child structure over a list of processes. Nodes are indexes into that list;
/// roots and children are ordered by pid.
class ProcessTree
{
public:
    ProcessTree() = default;

    /// Links processes by parent pid. A process becomes a root when its parent is absent or itself,
    /// or when it sits on a parent cycle (impossible in a real kernel, possible in corrupt input).
    explicit ProcessTree(std::span<const ProcessEntry> processes);

    [[nodiscard]] std::span<const std::size_t> roots() const noexcept { return roots_; }

    [[nodiscard]] std::span<const std::size_t> children(std::size_t index) const { return children_.at(index); }

    [[nodiscard]] std::size_t size() const noexcept { return children_.size(); }

private:
    std::vector<std::size_t> roots_;
    std::vector<std::vector<std::size_t>> children_;
};

/// What the UI renders: processes plus their tree and the system totals.
struct Model
{
    std::vector<ProcessEntry> processes;
    ProcessTree tree;
    SystemSummary summary;
};

} // namespace lxe::model
