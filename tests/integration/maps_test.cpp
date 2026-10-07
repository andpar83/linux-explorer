#include "lxe/model/details.hpp"
#include "lxe/proc/proc_fs.hpp"
#include "support/builders.hpp"
#include "test_paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <filesystem>

using lxe::ProcessId;
using lxe::proc::ProcFs;
using lxe::test::mapping;

namespace {

ProcFs fixture_proc()
{
    return ProcFs{std::filesystem::path{lxe::test::fixtures_dir} / "proc"};
}

} // namespace

TEST_CASE("proc.proc_fs.reads_the_maps_of_a_fixture_process")
{
    const auto maps = fixture_proc().read_maps(ProcessId{100});
    REQUIRE(maps.has_value());
    REQUIRE(maps->size() == 33);
    CHECK(maps->front() == mapping(0x5f1e4121c000, 0x5f1e4124d000, "/usr/bin/bash", "r--p"));
    CHECK(maps->at(1) == mapping(0x5f1e4124d000, 0x5f1e41350000, "/usr/bin/bash", "r-xp", 0x31000));
    CHECK(maps->at(5).path.empty());
    CHECK(maps->at(6).path == "[heap]");
    CHECK(maps->back().path == "[vsyscall]");
    CHECK(std::ranges::is_sorted(*maps, {}, &lxe::proc::MemoryMapping::start));
}

TEST_CASE("proc.proc_fs.maps_of_a_missing_process_is_an_error")
{
    const auto maps = fixture_proc().read_maps(ProcessId{102});
    REQUIRE_FALSE(maps.has_value());
    CHECK(maps.error() == std::errc::no_such_file_or_directory);
}

TEST_CASE("proc.live.maps_of_this_process_contain_its_executable_and_stack")
{
    const auto maps = ProcFs{}.read_maps(ProcessId{::getpid()});
    REQUIRE(maps.has_value());
    CHECK(maps->size() > 10);
    CHECK(std::ranges::any_of(*maps, [](const auto& m) {
        return m.path.ends_with("integration_tests") && m.executable && m.readable;
    }));
    CHECK(std::ranges::any_of(*maps, [](const auto& m) { return m.path == "[stack]" && m.writable; }));
    CHECK(std::ranges::all_of(*maps, [](const auto& m) { return m.end > m.start; }));
    CHECK(std::ranges::is_sorted(*maps, {}, &lxe::proc::MemoryMapping::start));
}

TEST_CASE("model.load_details.reads_maps_or_records_the_error")
{
    const auto fs = fixture_proc();

    const auto none = lxe::model::load_details(fs, std::nullopt);
    CHECK_FALSE(none.pid.has_value());
    CHECK(none.maps.empty());
    CHECK_FALSE(none.error);

    const auto bash = lxe::model::load_details(fs, ProcessId{100});
    CHECK(bash.pid == ProcessId{100});
    CHECK(bash.maps.size() == 33);
    CHECK_FALSE(bash.error);
    CHECK(bash.mapped_bytes() > 0);

    const auto gone = lxe::model::load_details(fs, ProcessId{102});
    CHECK(gone.pid == ProcessId{102});
    CHECK(gone.maps.empty());
    CHECK(gone.error == std::errc::no_such_file_or_directory);
}
