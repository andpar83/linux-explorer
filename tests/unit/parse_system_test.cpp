#include "lxe/proc/parse.hpp"

#include <catch2/catch_test_macros.hpp>

using lxe::ProcessId;
using lxe::proc::parse_cpu_times;
using lxe::proc::parse_meminfo;
using lxe::proc::parse_pid;
using lxe::proc::ParseError;

TEST_CASE("proc.parse_cpu_times.sums_the_aggregate_line_without_guest_time")
{
    const auto cpu = parse_cpu_times("cpu  100 20 30 400 50 6 7 8 900 1000\ncpu0 1 2 3 4 5 6 7 8 9 10\n");
    REQUIRE(cpu.has_value());
    CHECK(cpu->total_ticks == 100U + 20 + 30 + 400 + 50 + 6 + 7 + 8);
    CHECK(cpu->idle_ticks == 450U);
}

TEST_CASE("proc.parse_cpu_times.accepts_old_four_field_lines")
{
    const auto cpu = parse_cpu_times("cpu 1 2 3 4\n");
    REQUIRE(cpu.has_value());
    CHECK(cpu->total_ticks == 10U);
    CHECK(cpu->idle_ticks == 4U);
}

TEST_CASE("proc.parse_cpu_times.rejects_bad_input")
{
    CHECK(parse_cpu_times("intr 1 2 3\n").error() == ParseError::missing_field);
    CHECK(parse_cpu_times("cpu 1 2\n").error() == ParseError::truncated);
    CHECK(parse_cpu_times("cpu 1 x 3 4\n").error() == ParseError::bad_number);
    CHECK(parse_cpu_times("cpu 18446744073709551615 1 0 0\n").error() == ParseError::bad_number);
}

TEST_CASE("proc.parse_meminfo.reads_total_and_available")
{
    const auto memory = parse_meminfo("MemTotal:       16318048 kB\nMemFree:  1 kB\nMemAvailable:    8159024 kB\n");
    REQUIRE(memory.has_value());
    CHECK(memory->total_bytes == 16318048ULL * 1024);
    CHECK(memory->available_bytes == 8159024ULL * 1024);
}

TEST_CASE("proc.parse_meminfo.falls_back_to_memfree")
{
    const auto memory = parse_meminfo("MemTotal: 1000 kB\nMemFree: 300 kB\n");
    REQUIRE(memory.has_value());
    CHECK(memory->available_bytes == 300U * 1024);
}

TEST_CASE("proc.parse_meminfo.rejects_bad_input")
{
    CHECK(parse_meminfo("MemFree: 300 kB\n").error() == ParseError::missing_field);
    CHECK(parse_meminfo("MemTotal: 1000 kB\n").error() == ParseError::missing_field);
    CHECK(parse_meminfo("MemTotal:\nMemAvailable: 1 kB\n").error() == ParseError::truncated);
    CHECK(parse_meminfo("MemTotal: 18446744073709551615 kB\nMemAvailable: 1 kB\n").error() == ParseError::bad_number);
}

TEST_CASE("proc.parse_pid.accepts_only_positive_numbers")
{
    CHECK(parse_pid("1234") == ProcessId{1234});
    CHECK_FALSE(parse_pid("0").has_value());
    CHECK_FALSE(parse_pid("-1").has_value());
    CHECK_FALSE(parse_pid("self").has_value());
    CHECK_FALSE(parse_pid("").has_value());
    CHECK_FALSE(parse_pid("12a").has_value());
    CHECK_FALSE(parse_pid("99999999999").has_value());
}
