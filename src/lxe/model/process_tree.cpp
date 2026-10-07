#include "lxe/model/process_model.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace lxe::model {
namespace {

constexpr std::size_t no_parent = std::numeric_limits<std::size_t>::max();

/// Marks `start` and everything below it as reachable. Iterative: trees can be arbitrarily deep.
void mark_reachable(
    std::size_t start,
    const std::vector<std::vector<std::size_t>>& children,
    std::vector<std::uint8_t>& reachable
)
{
    std::vector<std::size_t> pending{start};
    while (!pending.empty()) {
        const std::size_t node = pending.back();
        pending.pop_back();
        if (reachable.at(node) != 0) {
            continue;
        }
        reachable.at(node) = 1;
        pending.insert(pending.end(), children.at(node).begin(), children.at(node).end());
    }
}

} // namespace

ProcessTree::ProcessTree(std::span<const ProcessEntry> processes)
: children_(processes.size())
{
    std::unordered_map<ProcessId, std::size_t> index_of;
    index_of.reserve(processes.size());
    for (std::size_t i = 0; i < processes.size(); ++i) {
        index_of.emplace(processes[i].pid, i);
    }

    std::vector<std::size_t> parent(processes.size(), no_parent);
    for (std::size_t i = 0; i < processes.size(); ++i) {
        const auto found = index_of.find(processes[i].ppid);
        if (found != index_of.end() && found->second != i) {
            parent[i] = found->second;
            children_[found->second].push_back(i);
        }
        else {
            roots_.push_back(i);
        }
    }

    // Whatever isn't reachable from a root is on a parent cycle or hangs below one. Promoting
    // such a node to a root breaks the cycle; repeat until every node is reachable exactly once.
    std::vector<std::uint8_t> reachable(processes.size(), 0);
    for (const std::size_t root : roots_) {
        mark_reachable(root, children_, reachable);
    }
    for (std::size_t i = 0; i < processes.size(); ++i) {
        if (reachable[i] != 0) {
            continue;
        }
        std::erase(children_[parent[i]], i);
        parent[i] = no_parent;
        roots_.push_back(i);
        mark_reachable(i, children_, reachable);
    }

    const auto by_pid = [&processes](std::size_t a, std::size_t b) {
        return processes[a].pid != processes[b].pid ? processes[a].pid < processes[b].pid : a < b;
    };
    std::ranges::sort(roots_, by_pid);
    for (auto& siblings : children_) {
        std::ranges::sort(siblings, by_pid);
    }
}

} // namespace lxe::model
