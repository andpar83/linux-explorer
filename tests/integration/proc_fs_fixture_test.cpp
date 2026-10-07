#include "lxe/proc/proc_fs.hpp"
#include "test_paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

using lxe::ProcessId;
using lxe::UserId;
using lxe::proc::ProcFs;

namespace {

ProcFs fixture_proc()
{
    return ProcFs{std::filesystem::path{lxe::test::fixtures_dir} / "proc"};
}

} // namespace

TEST_CASE("proc.proc_fs.lists_only_process_directories")
{
    const auto pids = fixture_proc().list_processes();
    REQUIRE(pids.has_value());
    const std::vector<ProcessId> expected{
        ProcessId{1}, ProcessId{2}, ProcessId{3}, ProcessId{50}, ProcessId{60}, ProcessId{61}, ProcessId{100},
        ProcessId{101}, ProcessId{102}};
    CHECK(*pids == expected);
}

TEST_CASE("proc.proc_fs.reads_one_process")
{
    const auto process = fixture_proc().read_process(ProcessId{101});
    REQUIRE(process.has_value());
    CHECK(process->stat.comm == "my (weird) prog");
    CHECK(process->stat.ppid == ProcessId{100});
    CHECK(process->stat.threads == 4);
    CHECK(process->uid == UserId{1000});
    CHECK(process->command_line == std::vector<std::string>{"./my prog", "--flag"});
}

TEST_CASE("proc.proc_fs.reports_missing_processes")
{
    const auto process = fixture_proc().read_process(ProcessId{999});
    REQUIRE_FALSE(process.has_value());
    CHECK(process.error() == std::errc::no_such_file_or_directory);
}

TEST_CASE("proc.proc_fs.snapshot_skips_processes_that_cannot_be_read")
{
    const auto snapshot = fixture_proc().read_snapshot();
    REQUIRE(snapshot.has_value());
    std::vector<ProcessId> pids;
    for (const auto& process : snapshot->processes) {
        pids.push_back(process.stat.pid);
    }
    // 102 has no status file: it "exited" while being read.
    const std::vector<ProcessId> expected{
        ProcessId{1}, ProcessId{2}, ProcessId{3}, ProcessId{50}, ProcessId{60}, ProcessId{61}, ProcessId{100},
        ProcessId{101}};
    CHECK(pids == expected);
    CHECK(snapshot->system.cpu.total_ticks == 60'377'929U);
    CHECK(snapshot->system.cpu.idle_ticks == 46'845'166U);
    CHECK(snapshot->system.memory.total_bytes == 16'318'048ULL * 1024);
    CHECK(snapshot->system.memory.available_bytes == 8'159'024ULL * 1024);
}

TEST_CASE("proc.proc_fs.missing_root_is_an_error")
{
    const ProcFs fs{"/nonexistent/proc"};
    CHECK_FALSE(fs.read_snapshot().has_value());
    CHECK_FALSE(fs.list_processes().has_value());
}
