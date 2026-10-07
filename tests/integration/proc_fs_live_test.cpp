// Reads the real /proc, using only facts about the test process itself, which are deterministic.
#include "lxe/model/sampler.hpp"
#include "lxe/proc/proc_fs.hpp"
#include "lxe/sys/system.hpp"
#include "lxe/sys/users.hpp"

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

using lxe::ProcessId;
using lxe::UserId;
using lxe::proc::ProcFs;

TEST_CASE("proc.live.reads_this_process")
{
    const ProcFs fs;
    const ProcessId self{::getpid()};
    const auto process = fs.read_process(self);
    REQUIRE(process.has_value());
    CHECK(process->stat.pid == self);
    CHECK(process->stat.ppid == ProcessId{::getppid()});
    CHECK(process->uid == UserId{::geteuid()});
    CHECK(process->stat.comm == "integration_tes"); // the kernel truncates names to 15 bytes
    REQUIRE_FALSE(process->command_line.empty());
    CHECK(process->command_line.front().ends_with("integration_tests"));
    CHECK(process->stat.threads >= 1);
    CHECK_FALSE(process->stat.is_kernel_thread());
}

TEST_CASE("proc.live.snapshot_contains_this_process_and_system_totals")
{
    const auto snapshot = ProcFs{}.read_snapshot();
    REQUIRE(snapshot.has_value());
    const ProcessId self{::getpid()};
    CHECK(std::ranges::any_of(snapshot->processes, [self](const auto& p) { return p.stat.pid == self; }));
    CHECK(std::ranges::is_sorted(snapshot->processes, {}, [](const auto& p) { return p.stat.pid; }));
    CHECK(snapshot->system.cpu.total_ticks > 0);
    CHECK(snapshot->system.memory.total_bytes > 0);
}

TEST_CASE("proc.live.sampler_measures_this_process")
{
    const ProcFs fs;
    lxe::model::Sampler sampler{lxe::sys::page_size()};
    std::ignore = sampler.update(fs.read_snapshot().value());

    // Burn CPU for a moment so the machine-wide tick counter certainly advances.
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds{40};
    volatile std::uint64_t sink = 0;
    while (std::chrono::steady_clock::now() < until) {
        sink = sink + 1;
    }

    const auto model = sampler.update(fs.read_snapshot().value());
    const ProcessId self{::getpid()};
    const auto entry = std::ranges::find_if(model.processes, [self](const auto& p) { return p.pid == self; });
    REQUIRE(entry != model.processes.end());
    CHECK(entry->cpu_percent.has_value());
    CHECK(model.summary.cpu_percent.has_value());
    CHECK(entry->user == lxe::sys::UserNames{}.name(UserId{::geteuid()}));
}

TEST_CASE("sys.users.root_is_known")
{
    CHECK(lxe::sys::lookup_system_user(UserId{0}) == "root");
}
