#include "lxe/proc/parse.hpp"
#include "support/builders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

using lxe::proc::decode_pagemap;
using lxe::proc::parse_smaps;
using lxe::proc::ParseError;
using lxe::test::mapping;
using lxe::test::pagemap_entry;

namespace {

constexpr std::string_view block =
    "62c9c0fdc000-62c9c10d0000 r--p 00000000 103:03 49425784                  /usr/lib/cargo/bin/coreutils/cat\n"
    "Size:                976 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Rss:                 976 kB\n"
    "Pss:                 488 kB\n"
    "Pss_Dirty:             0 kB\n"
    "Shared_Clean:        976 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:         0 kB\n"
    "Referenced:          976 kB\n"
    "Anonymous:             0 kB\n"
    "KSM:                   0 kB\n"
    "LazyFree:              0 kB\n"
    "AnonHugePages:      2048 kB\n"
    "ShmemPmdMapped:        0 kB\n"
    "FilePmdMapped:         0 kB\n"
    "Shared_Hugetlb:        0 kB\n"
    "Private_Hugetlb:       0 kB\n"
    "Swap:                  8 kB\n"
    "SwapPss:               4 kB\n"
    "Locked:                0 kB\n"
    "THPeligible:           1\n"
    "VmFlags: rd mr mw me sd \n";

} // namespace

TEST_CASE("proc.parse_smaps.reads_a_block")
{
    const auto entries = parse_smaps(block);
    REQUIRE(entries.has_value());
    REQUIRE(entries->size() == 1);
    const auto& [m, stats] = entries->front();
    CHECK(m.start == 0x62c9c0fdc000U);
    CHECK(m.path == "/usr/lib/cargo/bin/coreutils/cat");
    CHECK(stats.size == 976U * 1024);
    CHECK(stats.kernel_page_size == 4096U);
    CHECK(stats.rss == 976U * 1024);
    CHECK(stats.pss == 488U * 1024);
    CHECK(stats.shared_clean == 976U * 1024);
    CHECK(stats.private_dirty == 0);
    CHECK(stats.anon_huge_pages == 2048U * 1024);
    CHECK(stats.swap == 8U * 1024);
    CHECK(stats.swap_pss == 4U * 1024);
    CHECK(stats.thp_eligible);
    CHECK(stats.vm_flags == std::vector<std::string>{"rd", "mr", "mw", "me", "sd"});
}

TEST_CASE("proc.parse_smaps.handles_several_blocks_and_unknown_keys")
{
    const auto entries = parse_smaps(
        "00400000-00401000 r-xp 00000000 00:00 0                                  [a]\n"
        "Rss:                   4 kB\n"
        "FutureKey:            12 kB\n"
        "AnotherFuture:    weird value here\n"
        "00401000-00402000 rw-p 00000000 00:00 0 \n"
        "Rss:                   8 kB\n"
        "VmFlags: rd wr\n"
    );
    REQUIRE(entries.has_value());
    REQUIRE(entries->size() == 2);
    CHECK((*entries)[0].mapping == mapping(0x400000, 0x401000, "[a]", "r-xp"));
    CHECK((*entries)[0].stats.rss == 4096U);
    CHECK((*entries)[0].stats.vm_flags.empty());
    CHECK((*entries)[1].mapping.path.empty());
    CHECK((*entries)[1].stats.rss == 8192U);
    CHECK((*entries)[1].stats.vm_flags == std::vector<std::string>{"rd", "wr"});
}

TEST_CASE("proc.parse_smaps.empty_and_maps_only_input")
{
    CHECK(parse_smaps("").value().empty());
    const auto entries = parse_smaps("00400000-00401000 r-xp 00000000 00:00 0\n");
    REQUIRE(entries.has_value());
    CHECK(entries->front().stats == lxe::proc::MappingStats{});
}

TEST_CASE("proc.parse_smaps.rejects_bad_input")
{
    CHECK(parse_smaps("Rss: 4 kB\n").error() == ParseError::malformed);
    CHECK(parse_smaps("00400000-00401000 r-xp 00000000 00:00 0\nRss: 4\n").error() == ParseError::truncated);
    CHECK(parse_smaps("00400000-00401000 r-xp 00000000 00:00 0\nRss: 4 MB\n").error() == ParseError::malformed);
    CHECK(parse_smaps("00400000-00401000 r-xp 00000000 00:00 0\nRss: four kB\n").error() == ParseError::bad_number);
    CHECK(parse_smaps("00400000-00401000 r-xp 00000000 00:00 0\nRss: 99999999999999999999 kB\n").error() == ParseError::bad_number);
    CHECK(parse_smaps("00400000-00401000 r-xp 00000000 00:00 0\nTHPeligible: 2\n").error() == ParseError::bad_number);
    CHECK(parse_smaps("junk line\n").error() == ParseError::truncated);
}

TEST_CASE("proc.decode_pagemap.reads_the_flag_bits")
{
    const auto none = decode_pagemap(0);
    CHECK_FALSE(none.present);
    CHECK_FALSE(none.swapped);
    CHECK_FALSE(none.exclusive);
    const auto resident = decode_pagemap(pagemap_entry(true, false, true, false, true));
    CHECK(resident.present);
    CHECK(resident.exclusive);
    CHECK(resident.soft_dirty);
    CHECK_FALSE(resident.file_or_shared);
    const auto swapped = decode_pagemap(pagemap_entry(false, true, false, true));
    CHECK(swapped.swapped);
    CHECK(swapped.file_or_shared);
    // Frame number bits don't leak into the flags.
    CHECK_FALSE(decode_pagemap(0x0000'0000'0012'3456U).present);
}
