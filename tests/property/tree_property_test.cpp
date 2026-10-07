// Random parent links, including cycles, self-parents and missing parents: the tree must still
// contain every process exactly once, with consistent parent/child links.
#include "lxe/model/process_model.hpp"
#include "lxe/model/text_tree.hpp"
#include "support/builders.hpp"

#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <random>

using lxe::test::entry;
using lxe::test::make_model;
using lxe::test::walk;

TEST_CASE("model.process_tree.contains_every_process_once_for_any_parent_links")
{
    std::mt19937 rng{Catch::getSeed()};
    for (int iteration = 0; iteration < 300; ++iteration) {
        const int count = std::uniform_int_distribution{0, 200}(rng);
        std::uniform_int_distribution pid_of{1, count + 20}; // some parents don't exist
        std::vector<lxe::model::ProcessEntry> processes;
        for (int i = 0; i < count; ++i) {
            processes.push_back(entry(pid_of(rng), pid_of(rng)));
        }
        const auto model = make_model(processes);

        auto visited = walk(model.tree);
        std::ranges::sort(visited);
        std::vector<std::size_t> all(processes.size());
        std::iota(all.begin(), all.end(), std::size_t{0});
        REQUIRE(visited == all);

        for (std::size_t parent = 0; parent < processes.size(); ++parent) {
            for (const std::size_t child : model.tree.children(parent)) {
                REQUIRE(processes[child].ppid == processes[parent].pid);
            }
        }
        const auto text = lxe::model::render_text_tree(model);
        REQUIRE(std::ranges::count(text, '\n') == count);
    }
}
