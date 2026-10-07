#include "lxe/model/text_tree.hpp"

#include <format>
#include <span>
#include <vector>

namespace lxe::model {

std::string render_text_tree(const Model& model)
{
    struct Level
    {
        std::span<const std::size_t> nodes;
        std::size_t next = 0;
    };

    // Iterative depth-first walk: process chains can be deeper than a safe recursion depth.
    std::string out;
    std::string prefix;                    // "│ " / "  " for each ancestor level below the roots
    std::vector<std::size_t> prefix_sizes; // prefix length to restore when a level is finished
    std::vector<Level> stack{Level{.nodes = model.tree.roots()}};
    while (!stack.empty()) {
        Level& level = stack.back();
        if (level.next == level.nodes.size()) {
            stack.pop_back();
            if (!prefix_sizes.empty()) {
                prefix.resize(prefix_sizes.back());
                prefix_sizes.pop_back();
            }
            continue;
        }
        const std::size_t index = level.nodes[level.next++];
        const bool last = level.next == level.nodes.size();
        const bool root = stack.size() == 1;
        const ProcessEntry& process = model.processes.at(index);

        out += root ? std::format("{}({})\n", process.name, process.pid)
                    : std::format("{}{}{}({})\n", prefix, last ? "└─" : "├─", process.name, process.pid);

        const auto children = model.tree.children(index);
        if (!children.empty()) {
            prefix_sizes.push_back(prefix.size());
            if (!root) {
                prefix += last ? "  " : "│ ";
            }
            stack.push_back(Level{.nodes = children});
        }
    }
    return out;
}

} // namespace lxe::model
