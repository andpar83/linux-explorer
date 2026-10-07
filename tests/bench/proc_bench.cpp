// Micro-benchmarks of the hot paths. Not part of the default test run (label "bench"):
//   ctest --test-dir build/release -L bench --output-on-failure
//   ./build/release/tests/bench_tests          (full sampling)
#include "lxe/model/sampler.hpp"
#include "lxe/model/text_tree.hpp"
#include "lxe/proc/proc_fs.hpp"
#include "lxe/sys/system.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

TEST_CASE("proc.read_snapshot")
{
    const lxe::proc::ProcFs fs;
    BENCHMARK("read_snapshot(/proc)")
    {
        return fs.read_snapshot();
    };
}

TEST_CASE("proc.parse_stat")
{
    constexpr std::string_view line =
        "1 (systemd) S 0 1 1 0 -1 4194560 52005 2386196 113 1137 1081 676 7591 4029 20 0 1 0 22 23887872 3263 "
        "18446744073709551615 1 1 0 0 0 0 671173123 4096 1260 0 0 0 17 3 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
    BENCHMARK("parse_stat")
    {
        return lxe::proc::parse_stat(line);
    };
}

TEST_CASE("proc.parse_maps")
{
    // 20k mappings: the order of magnitude of a big browser process.
    std::string text;
    for (std::uint64_t i = 0; i < 20'000; ++i) {
        text += std::format(
            "{:x}-{:x} r-xp {:08x} 103:03 49414585                  /usr/lib/x86_64-linux-gnu/libc.so.6\n",
            0x7f0000000000U + i * 0x2000U,
            0x7f0000001000U + i * 0x2000U,
            i * 0x1000U
        );
    }
    BENCHMARK("parse_maps (20k lines)")
    {
        return lxe::proc::parse_maps(text);
    };
}

TEST_CASE("model.sampler_and_text_tree")
{
    const auto snapshot = lxe::proc::ProcFs{}.read_snapshot().value();
    lxe::model::Sampler sampler{lxe::sys::page_size()};
    BENCHMARK("Sampler::update")
    {
        return sampler.update(snapshot);
    };
    const auto model = sampler.update(snapshot);
    BENCHMARK("render_text_tree")
    {
        return lxe::model::render_text_tree(model);
    };
}
