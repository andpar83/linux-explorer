// Generated inputs for the /proc parsers: round trips of valid content, and no crash or UB on
// arbitrary bytes. The random seed is printed by Catch2 (--rng-seed reproduces a run).
#include "lxe/proc/parse.hpp"

#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <format>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using lxe::ProcessId;
using lxe::proc::ProcessState;
using lxe::proc::StatInfo;

namespace {

class Generator
{
public:
    Generator()
    : rng_{Catch::getSeed()}
    {}

    template <std::integral T>
    T number(T low = std::numeric_limits<T>::min(), T high = std::numeric_limits<T>::max())
    {
        return std::uniform_int_distribution<T>{low, high}(rng_);
    }

    char pick(std::string_view chars) { return chars[number<std::size_t>(0, chars.size() - 1)]; }

    std::string text(std::string_view alphabet, std::size_t max_length)
    {
        std::string out(number<std::size_t>(0, max_length), ' ');
        for (char& c : out) {
            c = pick(alphabet);
        }
        return out;
    }

    std::string bytes(std::size_t max_length)
    {
        std::string out(number<std::size_t>(0, max_length), '\0');
        for (char& c : out) {
            // Mostly characters that matter to the parsers, sometimes any byte.
            c = number(0, 3) == 0 ? static_cast<char>(number<int>(-128, 127)) : pick(" ()\n\t:0123456789-SRZ\0kB");
        }
        return out;
    }

private:
    std::mt19937_64 rng_;
};

constexpr int iterations = 2000;

} // namespace

TEST_CASE("proc.parse_stat.round_trips_generated_lines")
{
    Generator gen;
    for (int i = 0; i < iterations; ++i) {
        StatInfo expected;
        expected.pid = ProcessId{gen.number<pid_t>(1, 4'194'304)};
        expected.comm = gen.text("abcXYZ019 ()):/-_.\t\n", 64);
        expected.state = ProcessState{gen.pick("RSDZTtXIP")};
        expected.ppid = ProcessId{gen.number<pid_t>(0, 4'194'304)};
        expected.flags = gen.number<std::uint32_t>();
        expected.utime_ticks = gen.number<std::uint64_t>();
        expected.stime_ticks = gen.number<std::uint64_t>();
        expected.priority = gen.number<std::int64_t>(-100, 139);
        expected.nice = gen.number<std::int64_t>(-20, 19);
        expected.threads = gen.number<std::int64_t>(1, 100'000);
        expected.start_time_ticks = gen.number<std::uint64_t>();
        expected.virtual_bytes = gen.number<std::uint64_t>();
        expected.resident_pages = gen.number<std::int64_t>(0, std::numeric_limits<std::int64_t>::max());

        const std::string line = std::format(
            "{} ({}) {} {} 1 1 0 -1 {} 0 0 0 0 {} {} 0 0 {} {} {} 0 {} {} {} 18446744073709551615 1 1 0 0 0\n",
            expected.pid,
            expected.comm,
            std::to_underlying(expected.state),
            expected.ppid,
            expected.flags,
            expected.utime_ticks,
            expected.stime_ticks,
            expected.priority,
            expected.nice,
            expected.threads,
            expected.start_time_ticks,
            expected.virtual_bytes,
            expected.resident_pages
        );
        INFO("line: " << line);
        const auto parsed = lxe::proc::parse_stat(line);
        REQUIRE(parsed.has_value());
        REQUIRE(*parsed == expected);
    }
}

TEST_CASE("proc.parse_cmdline.round_trips_generated_arguments")
{
    Generator gen;
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::string> args(gen.number<std::size_t>(0, 8));
        for (auto& arg : args) {
            arg = gen.text("ab -=/\"' ", 12);
        }
        if (!args.empty() && args.back().empty()) {
            args.back() = "x"; // trailing empty arguments are indistinguishable from padding
        }
        std::string text;
        for (const auto& arg : args) {
            text += arg;
            text += '\0';
        }
        REQUIRE(lxe::proc::parse_cmdline(text) == args);
    }
}

TEST_CASE("proc.parsers.survive_arbitrary_bytes")
{
    Generator gen;
    for (int i = 0; i < iterations * 5; ++i) {
        const std::string input = gen.bytes(300);
        INFO("input size: " << input.size());
        if (const auto stat = lxe::proc::parse_stat(input)) {
            CHECK(input.find(stat->comm) != std::string::npos);
            CHECK(std::to_underlying(stat->pid) > 0);
        }
        std::ignore = lxe::proc::parse_status(input);
        std::ignore = lxe::proc::parse_cmdline(input);
        std::ignore = lxe::proc::parse_cpu_times(input);
        std::ignore = lxe::proc::parse_meminfo(input);
        std::ignore = lxe::proc::parse_pid(input);
    }
}
