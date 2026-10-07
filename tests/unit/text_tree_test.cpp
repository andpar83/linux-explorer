#include "lxe/model/text_tree.hpp"
#include "support/builders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using lxe::model::render_text_tree;
using lxe::test::entry;
using lxe::test::make_model;

TEST_CASE("model.text_tree.draws_like_pstree")
{
    const auto model = make_model({
        entry(1, 0, "systemd"),
        entry(2, 0, "kthreadd"),
        entry(3, 2, "kworker/0:0H"),
        entry(50, 1, "agetty"),
        entry(60, 1, "sshd"),
        entry(61, 60, "sshd"),
        entry(100, 1, "bash"),
        entry(101, 100, "my (weird) prog"),
    });
    CHECK(render_text_tree(model) == "systemd(1)\n"
                                     "├─agetty(50)\n"
                                     "├─sshd(60)\n"
                                     "│ └─sshd(61)\n"
                                     "└─bash(100)\n"
                                     "  └─my (weird) prog(101)\n"
                                     "kthreadd(2)\n"
                                     "└─kworker/0:0H(3)\n");
}

TEST_CASE("model.text_tree.empty_model_is_empty_text")
{
    CHECK(render_text_tree(make_model({})).empty());
}

TEST_CASE("model.text_tree.handles_very_deep_chains")
{
    constexpr int depth = 3000; // output grows with the square of the depth
    std::vector<lxe::model::ProcessEntry> processes;
    for (int pid = 1; pid <= depth; ++pid) {
        processes.push_back(entry(pid, pid - 1));
    }
    const auto text = render_text_tree(make_model(std::move(processes)));
    CHECK(std::ranges::count(text, '\n') == depth);
}
