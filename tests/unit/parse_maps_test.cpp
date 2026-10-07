#include "lxe/proc/parse.hpp"
#include "support/builders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using namespace std::string_view_literals;
using lxe::proc::parse_maps;
using lxe::proc::ParseError;
using lxe::test::mapping;

TEST_CASE("proc.parse_maps.reads_every_field")
{
    const auto maps = parse_maps("5f1e4121c000-5f1e4124d000 r-xp 00031000 103:03 49414585                  /usr/bin/bash\n");
    REQUIRE(maps.has_value());
    REQUIRE(maps->size() == 1);
    const auto& m = maps->front();
    CHECK(m.start == 0x5f1e4121c000U);
    CHECK(m.end == 0x5f1e4124d000U);
    CHECK(m.size() == 0x31000U);
    CHECK(m.readable);
    CHECK_FALSE(m.writable);
    CHECK(m.executable);
    CHECK_FALSE(m.shared);
    CHECK(m.offset == 0x31000U);
    CHECK(m.device_major == 0x103U);
    CHECK(m.device_minor == 3U);
    CHECK(m.inode == 49414585U);
    CHECK(m.path == "/usr/bin/bash");
}

TEST_CASE("proc.parse_maps.anonymous_mappings_have_no_path")
{
    // The kernel leaves a single space after the inode when there is no path.
    const auto maps = parse_maps("5f1e41394000-5f1e413a0000 rw-p 00000000 00:00 0 \n");
    REQUIRE(maps.has_value());
    CHECK(*maps == std::vector{mapping(0x5f1e41394000, 0x5f1e413a0000, {}, "rw-p")});
}

TEST_CASE("proc.parse_maps.keeps_special_names_spaces_and_deleted_markers")
{
    const auto maps = parse_maps(
        "7f0000000000-7f0000021000 rw-p 00000000 00:00 0                          [heap]\n"
        "7f0000100000-7f0000200000 rw-p 00000000 00:00 0                          [anon:scudo:primary]\n"
        "7f0000200000-7f0000300000 r--s 00000000 00:05 98765                      /dev/shm/cache (deleted)\n"
        "00400000-00401000 r-xp 00000000 103:03 1234567                           /home/alice/my prog\n"
        "ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]\n"
    );
    REQUIRE(maps.has_value());
    REQUIRE(maps->size() == 5);
    CHECK((*maps)[0].path == "[heap]");
    CHECK((*maps)[1].path == "[anon:scudo:primary]");
    CHECK((*maps)[2].path == "/dev/shm/cache (deleted)");
    CHECK((*maps)[2].shared);
    CHECK((*maps)[2].device_minor == 5U);
    CHECK((*maps)[2].inode == 98765U);
    CHECK((*maps)[3].path == "/home/alice/my prog");
    CHECK((*maps)[3].start == 0x400000U);
    CHECK((*maps)[4].path == "[vsyscall]");
    CHECK((*maps)[4].executable);
    CHECK_FALSE((*maps)[4].readable);
    CHECK((*maps)[4].start == 0xffffffffff600000U);
}

TEST_CASE("proc.parse_maps.ignores_blank_lines_and_accepts_empty_input")
{
    CHECK(parse_maps("").value().empty());
    CHECK(parse_maps("\n\n").value().empty());
    const auto maps = parse_maps("\n00400000-00401000 r--p 00000000 00:00 0\n\n");
    REQUIRE(maps.has_value());
    CHECK(maps->size() == 1);
}

TEST_CASE("proc.parse_maps.accepts_tabs_and_unpadded_lines")
{
    const auto maps = parse_maps("00400000-00401000\tr--p\t00000000\t00:00\t0\t[x]\n");
    REQUIRE(maps.has_value());
    CHECK(maps->front().path == "[x]");
}

TEST_CASE("proc.parse_maps.rejects_malformed_lines")
{
    CHECK(parse_maps("00400000-00401000 r--p 00000000 00:00\n").error() == ParseError::truncated);
    CHECK(parse_maps("00400000 r--p 00000000 00:00 0\n").error() == ParseError::malformed);
    CHECK(parse_maps("00400000-00401000 r--p 00000000 0000 0\n").error() == ParseError::malformed);
    CHECK(parse_maps("00400000-00401000 r--pp 00000000 00:00 0\n").error() == ParseError::malformed);
    CHECK(parse_maps("00400000-00401000 rwxq 00000000 00:00 0\n").error() == ParseError::malformed);
    CHECK(parse_maps("00400000-00401000 R--p 00000000 00:00 0\n").error() == ParseError::malformed);
    CHECK(parse_maps("00401000-00400000 r--p 00000000 00:00 0\n").error() == ParseError::malformed);
    CHECK(parse_maps("0040000g-00401000 r--p 00000000 00:00 0\n").error() == ParseError::bad_number);
    CHECK(parse_maps("00400000-00401000 r--p 0000000x 00:00 0\n").error() == ParseError::bad_number);
    CHECK(parse_maps("00400000-00401000 r--p 00000000 00:00 -1\n").error() == ParseError::bad_number);
    CHECK(parse_maps("00400000-00401000 r--p 00000000 00:00 99999999999999999999\n").error() == ParseError::bad_number);
    CHECK(parse_maps("1ffffffffffffffff-2ffffffffffffffff r--p 0 0:0 0\n").error() == ParseError::bad_number);
    // One bad line fails the whole file.
    CHECK_FALSE(parse_maps("00400000-00401000 r--p 00000000 00:00 0\njunk\n").has_value());
}
