#include "lxe/model/process_model.hpp"
#include "support/builders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <vector>

using lxe::model::ProcessTree;
using lxe::test::entry;
using lxe::test::walk;
using Indexes = std::vector<std::size_t>;

namespace {

Indexes to_vector(std::span<const std::size_t> span)
{
    return {span.begin(), span.end()};
}

/// True when walking the tree visits every process exactly once.
bool visits_each_once(const ProcessTree& tree)
{
    auto visited = walk(tree);
    std::ranges::sort(visited);
    Indexes expected(tree.size());
    std::iota(expected.begin(), expected.end(), std::size_t{0});
    return visited == expected;
}

} // namespace

TEST_CASE("model.process_tree.links_children_to_parents")
{
    const std::vector processes{entry(1, 0), entry(2, 0), entry(100, 1), entry(101, 100), entry(3, 2)};
    const ProcessTree tree{processes};
    CHECK(to_vector(tree.roots()) == Indexes{0, 1});
    CHECK(to_vector(tree.children(0)) == Indexes{2});
    CHECK(to_vector(tree.children(2)) == Indexes{3});
    CHECK(to_vector(tree.children(1)) == Indexes{4});
    CHECK(tree.children(3).empty());
}

TEST_CASE("model.process_tree.orders_siblings_by_pid")
{
    const std::vector processes{entry(1, 0), entry(300, 1), entry(20, 1), entry(100, 1)};
    const ProcessTree tree{processes};
    CHECK(to_vector(tree.children(0)) == Indexes{2, 3, 1});
}

TEST_CASE("model.process_tree.missing_or_self_parents_become_roots")
{
    const std::vector processes{entry(10, 999), entry(11, 11), entry(12, 10)};
    const ProcessTree tree{processes};
    CHECK(to_vector(tree.roots()) == Indexes{0, 1});
    CHECK(to_vector(tree.children(0)) == Indexes{2});
}

TEST_CASE("model.process_tree.breaks_parent_cycles")
{
    // Impossible in a real kernel, but /proc content is untrusted.
    const std::vector processes{entry(1, 0), entry(5, 6), entry(6, 5), entry(7, 6)};
    const ProcessTree tree{processes};
    CHECK(visits_each_once(tree));
    CHECK(tree.roots().size() == 2);
}

TEST_CASE("model.process_tree.handles_duplicate_pids")
{
    const std::vector processes{entry(1, 0), entry(5, 1), entry(5, 1)};
    const ProcessTree tree{processes};
    CHECK(visits_each_once(tree));
}

TEST_CASE("model.process_tree.handles_very_deep_chains")
{
    constexpr int depth = 100'000;
    std::vector<lxe::model::ProcessEntry> processes;
    processes.reserve(depth);
    for (int pid = 1; pid <= depth; ++pid) {
        processes.push_back(entry(pid, pid - 1));
    }
    const ProcessTree tree{processes};
    CHECK(tree.roots().size() == 1);
    CHECK(visits_each_once(tree));
}

TEST_CASE("model.process_tree.empty_input_has_no_roots")
{
    const ProcessTree tree{std::span<const lxe::model::ProcessEntry>{}};
    CHECK(tree.roots().empty());
    CHECK(tree.size() == 0);
}
