#include "lxe/model/details.hpp"
#include "lxe/proc/proc_fs.hpp"
#include "lxe/sys/system.hpp"
#include "support/builders.hpp"
#include "test_paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <cstdint>
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
    CHECK(maps->front() == mapping(0x558367a0a000, 0x558367a3b000, "/usr/bin/bash", "r--p"));
    CHECK(maps->at(1) == mapping(0x558367a3b000, 0x558367b3e000, "/usr/bin/bash", "r-xp", 0x31000));
    CHECK(maps->at(5).path.empty());
    CHECK(maps->at(6).path == "[heap]");
    CHECK(maps->back().path == "[vsyscall]");
    CHECK(std::ranges::is_sorted(*maps, {}, &lxe::proc::MemoryMapping::start));
}

TEST_CASE("proc.proc_fs.smaps_of_a_fixture_process_matches_its_maps")
{
    const auto fs = fixture_proc();
    const auto smaps = fs.read_smaps(ProcessId{100});
    const auto maps = fs.read_maps(ProcessId{100});
    REQUIRE(smaps.has_value());
    REQUIRE(maps.has_value());
    REQUIRE(smaps->size() == maps->size());
    for (std::size_t i = 0; i < maps->size(); ++i) {
        CHECK((*smaps)[i].mapping == (*maps)[i]);
        CHECK((*smaps)[i].stats.size == (*maps)[i].size());
        CHECK((*smaps)[i].stats.rss <= (*smaps)[i].stats.size);
        CHECK_FALSE((*smaps)[i].stats.vm_flags.empty());
    }
    CHECK(smaps->front().stats.rss > 0);
    CHECK(smaps->front().stats.kernel_page_size == 4096U);
}

TEST_CASE("proc.live.smaps_and_pagemap_of_this_process")
{
    const ProcFs fs;
    const ProcessId self{::getpid()};
    const auto smaps = fs.read_smaps(self);
    REQUIRE(smaps.has_value());
    // The stack is the one anonymous mapping every process has (ASan builds have no [heap]:
    // the sanitizer's allocator never uses brk).
    const auto stack = std::ranges::find(*smaps, "[stack]", [](const auto& e) { return e.mapping.path; });
    REQUIRE(stack != smaps->end());
    CHECK(stack->stats.rss > 0);
    CHECK(stack->stats.anonymous > 0);
    CHECK(std::ranges::contains(stack->stats.vm_flags, "rd"));
    CHECK(std::ranges::contains(stack->stats.vm_flags, "wr"));

    // The stack's last pages are in use right now: resident and private to this process.
    const std::uint64_t page = lxe::sys::page_size();
    const auto entries = fs.read_pagemap(self, stack->mapping.end / page - 16, 16);
    REQUIRE(entries.has_value());
    REQUIRE(entries->size() == 16);
    const auto last = lxe::proc::decode_pagemap(entries->back());
    CHECK(last.present);
    CHECK(last.exclusive);
    CHECK_FALSE(last.file_or_shared);

    // Past the end of the address space the kernel returns nothing.
    const auto beyond = fs.read_pagemap(self, (std::uint64_t{1} << 47U) / page, 4);
    REQUIRE(beyond.has_value());
    CHECK(beyond->empty());
}

TEST_CASE("proc.live.pagemap_of_another_users_process_is_denied")
{
    if (::geteuid() == 0) {
        SKIP("running as root: nothing is denied");
    }
    const auto entries = ProcFs{}.read_pagemap(ProcessId{1}, 0, 1);
    REQUIRE_FALSE(entries.has_value());
    CHECK(entries.error() == std::errc::permission_denied);
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
    using lxe::model::DetailsRequest;

    const auto none = lxe::model::load_details(fs, DetailsRequest{}, 4096);
    CHECK_FALSE(none.pid.has_value());
    CHECK(none.maps.empty());
    CHECK_FALSE(none.error);

    const auto bash = lxe::model::load_details(fs, DetailsRequest{.pid = ProcessId{100}}, 4096);
    CHECK(bash.pid == ProcessId{100});
    CHECK(bash.maps.size() == 33);
    CHECK(bash.maps.front().stats.has_value()); // from smaps
    CHECK_FALSE(bash.error);
    CHECK(bash.mapped_bytes() > 0);
    CHECK_FALSE(bash.pages.has_value());

    // 101 has maps but no smaps, like a kernel without CONFIG_PROC_PAGE_MONITOR.
    const auto plain = lxe::model::load_details(fs, DetailsRequest{.pid = ProcessId{101}}, 4096);
    CHECK(plain.maps.size() == 8);
    CHECK_FALSE(plain.maps.front().stats.has_value());
    CHECK_FALSE(plain.error);

    const auto gone = lxe::model::load_details(fs, DetailsRequest{.pid = ProcessId{102}}, 4096);
    CHECK(gone.pid == ProcessId{102});
    CHECK(gone.maps.empty());
    CHECK(gone.error == std::errc::no_such_file_or_directory);

    // A selected mapping of a fixture process: no pagemap file, so pages can't be read.
    const auto selected = lxe::model::load_details(fs, DetailsRequest{.pid = ProcessId{100}, .mapping = 0x558367a0a000}, 4096);
    REQUIRE(selected.selected() != nullptr);
    CHECK_FALSE(selected.pages.has_value());
    CHECK(selected.pages_error == std::errc::no_such_file_or_directory);

    // A mapping that isn't there: nothing is read.
    const auto missing = lxe::model::load_details(fs, DetailsRequest{.pid = ProcessId{100}, .mapping = 0x42}, 4096);
    CHECK(missing.selected() == nullptr);
    CHECK_FALSE(missing.pages_error);
}

TEST_CASE("model.load_details.classifies_the_pages_of_this_process")
{
    const ProcFs fs;
    const ProcessId self{::getpid()};
    const auto maps = fs.read_maps(self);
    REQUIRE(maps.has_value());
    const auto stack = std::ranges::find(*maps, "[stack]", &lxe::proc::MemoryMapping::path);
    REQUIRE(stack != maps->end());
    const auto details = lxe::model::load_details(fs, {.pid = self, .mapping = stack->start}, lxe::sys::page_size());
    REQUIRE(details.pages.has_value());
    CHECK_FALSE(details.pages_error);
    CHECK(details.pages->pages == stack->size() / lxe::sys::page_size());
    CHECK(details.pages->examined == details.pages->pages);
    CHECK(details.pages->present > 0);
    CHECK(details.pages->exclusive > 0);
    CHECK_FALSE(details.pages->buckets.empty());
}
