#include "lxe/proc/parse.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;
using lxe::UserId;
using lxe::proc::parse_cmdline;
using lxe::proc::parse_status;
using lxe::proc::ParseError;
using Args = std::vector<std::string>;

TEST_CASE("proc.parse_status.reads_real_and_effective_uid")
{
    const auto status = parse_status("Name:\tsudo\nUmask:\t0022\nState:\tS (sleeping)\nUid:\t1000\t0\t0\t0\nGid:\t1000\n");
    REQUIRE(status.has_value());
    CHECK(status->real_uid == UserId{1000});
    CHECK(status->effective_uid == UserId{0});
}

TEST_CASE("proc.parse_status.reports_missing_or_bad_uid")
{
    CHECK(parse_status("Name:\tx\nGid:\t0\t0\t0\t0\n").error() == ParseError::missing_field);
    CHECK(parse_status("").error() == ParseError::missing_field);
    CHECK(parse_status("Uid:\t1000\n").error() == ParseError::truncated);
    CHECK(parse_status("Uid:\tme\t1000\t0\t0\n").error() == ParseError::bad_number);
    CHECK(parse_status("Uid:\t-1\t1000\t0\t0\n").error() == ParseError::bad_number);
}

TEST_CASE("proc.parse_cmdline.splits_on_nul")
{
    CHECK(parse_cmdline("/usr/bin/foo\0--bar\0baz\0"sv) == Args{"/usr/bin/foo", "--bar", "baz"});
    CHECK(parse_cmdline("/usr/bin/foo\0--bar"sv) == Args{"/usr/bin/foo", "--bar"});
}

TEST_CASE("proc.parse_cmdline.keeps_inner_empty_arguments")
{
    CHECK(parse_cmdline("a\0\0b\0"sv) == Args{"a", "", "b"});
}

TEST_CASE("proc.parse_cmdline.drops_trailing_padding")
{
    // Processes that rewrite their title leave NUL padding behind.
    CHECK(parse_cmdline("nginx: worker process\0\0\0\0\0"sv) == Args{"nginx: worker process"});
}

TEST_CASE("proc.parse_cmdline.is_empty_for_kernel_threads")
{
    CHECK(parse_cmdline(""sv).empty());
    CHECK(parse_cmdline("\0\0"sv).empty());
}
