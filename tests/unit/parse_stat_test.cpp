#include "lxe/proc/parse.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using lxe::ProcessId;
using lxe::proc::parse_stat;
using lxe::proc::ParseError;
using lxe::proc::ProcessState;

namespace {

constexpr std::string_view systemd_stat =
    "1 (systemd) S 0 1 1 0 -1 4194560 52005 2386196 113 1137 1081 676 7591 4029 20 0 1 0 22 23887872 3263 "
    "18446744073709551615 1 1 0 0 0 0 671173123 4096 1260 0 0 0 17 3 0 0 0 0 0 0 0 0 0 0 0 0 0\n";

/// A valid stat line with the given pid/comm prefix and fields after comm.
std::string stat_line(std::string_view head, std::string_view state_and_ppid = "S 1")
{
    return std::string{head} + " " + std::string{state_and_ppid} +
           " 1 1 0 -1 4194560 0 0 0 0 7 3 0 0 20 0 1 0 100 4096 10 18446744073709551615 0 0 0\n";
}

} // namespace

TEST_CASE("proc.parse_stat.reads_the_used_fields")
{
    const auto stat = parse_stat(systemd_stat);
    REQUIRE(stat.has_value());
    CHECK(stat->pid == ProcessId{1});
    CHECK(stat->comm == "systemd");
    CHECK(stat->state == ProcessState::sleeping);
    CHECK(stat->ppid == ProcessId{0});
    CHECK(stat->flags == 4194560U);
    CHECK(stat->utime_ticks == 1081U);
    CHECK(stat->stime_ticks == 676U);
    CHECK(stat->priority == 20);
    CHECK(stat->nice == 0);
    CHECK(stat->threads == 1);
    CHECK(stat->start_time_ticks == 22U);
    CHECK(stat->virtual_bytes == 23887872U);
    CHECK(stat->resident_pages == 3263);
    CHECK(stat->cpu_ticks() == 1757U);
    CHECK_FALSE(stat->is_kernel_thread());
}

TEST_CASE("proc.parse_stat.comm_may_contain_spaces_and_parentheses")
{
    const auto stat = parse_stat(stat_line("101 (my (weird) prog)", "R 100"));
    REQUIRE(stat.has_value());
    CHECK(stat->comm == "my (weird) prog");
    CHECK(stat->state == ProcessState::running);
    CHECK(stat->ppid == ProcessId{100});
}

TEST_CASE("proc.parse_stat.comm_ends_at_the_last_parenthesis")
{
    // A hostile name that looks like the start of the fields.
    const auto stat = parse_stat(stat_line("7 (x) Z 99)", "R 3"));
    REQUIRE(stat.has_value());
    CHECK(stat->comm == "x) Z 99");
    CHECK(stat->state == ProcessState::running);
    CHECK(stat->ppid == ProcessId{3});
}

TEST_CASE("proc.parse_stat.accepts_an_empty_comm")
{
    const auto stat = parse_stat(stat_line("5 ()"));
    REQUIRE(stat.has_value());
    CHECK(stat->comm.empty());
}

TEST_CASE("proc.parse_stat.detects_kernel_threads")
{
    const auto stat = parse_stat(
        "2 (kthreadd) S 0 0 0 0 -1 2129984 0 0 0 0 0 4 0 0 20 0 1 0 22 0 0 18446744073709551615 0 0 0 0\n"
    );
    REQUIRE(stat.has_value());
    CHECK(stat->is_kernel_thread());
}

TEST_CASE("proc.parse_stat.reads_negative_nice_and_unknown_states")
{
    const auto stat =
        parse_stat("9 (n) W 1 1 1 0 -1 0 0 0 0 0 1 1 0 0 15 -5 2 0 100 4096 10 18446744073709551615 0 0 0\n");
    REQUIRE(stat.has_value());
    CHECK(stat->nice == -5);
    CHECK(stat->priority == 15);
    CHECK(stat->state == ProcessState{'W'});
}

TEST_CASE("proc.parse_stat.rejects_malformed_input")
{
    CHECK(parse_stat("").error() == ParseError::malformed);
    CHECK(parse_stat("1 systemd S 0").error() == ParseError::malformed);
    CHECK(parse_stat("1 (x S 0 1").error() == ParseError::malformed);
    CHECK(parse_stat(stat_line("1 (x)", "SS 1")).error() == ParseError::malformed);
    CHECK(parse_stat("1 (x) S 0 1 1").error() == ParseError::truncated);
    CHECK(parse_stat(stat_line("abc (x)")).error() == ParseError::bad_number);
    CHECK(parse_stat(stat_line("0 (x)")).error() == ParseError::bad_number);
    CHECK(parse_stat(stat_line("-4 (x)")).error() == ParseError::bad_number);
    CHECK(parse_stat(stat_line("1 (x)", "S one")).error() == ParseError::bad_number);
    CHECK(parse_stat(stat_line("99999999999999999999 (x)")).error() == ParseError::bad_number);
}

TEST_CASE("proc.parse_error.has_readable_messages")
{
    const std::error_code error = ParseError::truncated;
    CHECK(error.category().name() == std::string_view{"lxe.proc.parse"});
    CHECK(error.message() == "truncated /proc content");
}
