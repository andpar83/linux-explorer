#include "lxe/model/sampler.hpp"
#include "support/builders.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using lxe::model::Sampler;
using lxe::proc::CpuTimes;
using lxe::test::fake_users;
using lxe::test::snapshot;

namespace {

constexpr std::uint64_t page_size = 4096;

} // namespace

TEST_CASE("model.sampler.first_sample_has_no_cpu_figures")
{
    Sampler sampler{page_size, fake_users()};
    const auto model = sampler.update(snapshot(
        {{.pid = 1, .comm = "init", .cpu_ticks = 50, .resident_pages = 10, .uid = 0, .threads = 1},
         {.pid = 7, .ppid = 1, .comm = "app", .resident_pages = 3, .uid = 1000, .threads = 4,
          .command_line = {"/bin/app", "--fast"}}},
        CpuTimes{.total_ticks = 1000, .idle_ticks = 900}
    ));
    REQUIRE(model.processes.size() == 2);
    CHECK_FALSE(model.summary.cpu_percent.has_value());
    CHECK_FALSE(model.processes[0].cpu_percent.has_value());

    const auto& app = model.processes[1];
    CHECK(app.name == "app");
    CHECK(app.user == "alice");
    CHECK(app.command_line == "/bin/app --fast");
    CHECK(app.memory_bytes == 3 * page_size);
    CHECK(app.threads == 4);
    CHECK(model.summary.processes == 2);
    CHECK(model.summary.threads == 5);
    CHECK(model.summary.memory_total_bytes == 1000);
    CHECK(model.summary.memory_used_bytes == 600);
    CHECK(model.tree.children(0).size() == 1);
}

TEST_CASE("model.sampler.cpu_usage_is_relative_to_all_cpus")
{
    Sampler sampler{page_size, fake_users()};
    std::ignore = sampler.update(
        snapshot({{.pid = 1, .cpu_ticks = 100, .start_time_ticks = 5}}, CpuTimes{.total_ticks = 10'000, .idle_ticks = 8'000})
    );
    const auto model = sampler.update(
        snapshot({{.pid = 1, .cpu_ticks = 200, .start_time_ticks = 5}}, CpuTimes{.total_ticks = 11'000, .idle_ticks = 8'750})
    );
    REQUIRE(model.processes[0].cpu_percent.has_value());
    CHECK(*model.processes[0].cpu_percent == Approx(10.0));
    REQUIRE(model.summary.cpu_percent.has_value());
    CHECK(*model.summary.cpu_percent == Approx(25.0));
}

TEST_CASE("model.sampler.reused_pid_is_a_new_process")
{
    Sampler sampler{page_size, fake_users()};
    std::ignore = sampler.update(snapshot({{.pid = 9, .cpu_ticks = 500, .start_time_ticks = 1}}, CpuTimes{.total_ticks = 100}));
    const auto model =
        sampler.update(snapshot({{.pid = 9, .cpu_ticks = 10, .start_time_ticks = 2}}, CpuTimes{.total_ticks = 200}));
    CHECK_FALSE(model.processes[0].cpu_percent.has_value());
}

TEST_CASE("model.sampler.tolerates_counters_that_go_backwards")
{
    Sampler sampler{page_size, fake_users()};
    std::ignore = sampler.update(snapshot({{.pid = 1, .cpu_ticks = 500}}, CpuTimes{.total_ticks = 100, .idle_ticks = 50}));

    SECTION("process ticks decrease: 0%")
    {
        const auto model = sampler.update(snapshot({{.pid = 1, .cpu_ticks = 400}}, CpuTimes{.total_ticks = 200, .idle_ticks = 40}));
        CHECK(model.processes[0].cpu_percent == 0.0);
        CHECK(model.summary.cpu_percent == 100.0);
    }
    SECTION("machine ticks don't advance: no figure")
    {
        const auto model = sampler.update(snapshot({{.pid = 1, .cpu_ticks = 600}}, CpuTimes{.total_ticks = 100}));
        CHECK_FALSE(model.processes[0].cpu_percent.has_value());
        CHECK_FALSE(model.summary.cpu_percent.has_value());
    }
    SECTION("more process time than machine time: clamped to 100%")
    {
        const auto model = sampler.update(snapshot({{.pid = 1, .cpu_ticks = 5'000}}, CpuTimes{.total_ticks = 200}));
        CHECK(model.processes[0].cpu_percent == 100.0);
    }
}

TEST_CASE("model.sampler.flags_kernel_threads_and_unknown_users")
{
    Sampler sampler{page_size, fake_users()};
    const auto model = sampler.update(snapshot(
        {{.pid = 2, .comm = "kthreadd", .flags = lxe::proc::pf_kthread}, {.pid = 50, .ppid = 2, .uid = 4242}},
        CpuTimes{.total_ticks = 1}
    ));
    CHECK(model.processes[0].kernel_thread);
    CHECK(model.processes[1].user == "4242");
    CHECK(model.processes[0].memory_bytes == 0);
}
