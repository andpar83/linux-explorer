// libFuzzer harness for every parser of untrusted /proc content. With Clang it builds with
// -fsanitize=fuzzer; with GCC it is linked to replay_main.cpp and replays the corpus as a test.
#include "lxe/proc/parse.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>

// NOLINTNEXTLINE(readability-identifier-naming): the name is fixed by libFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string_view text{std::bit_cast<const char*>(data), size};
    if (const auto stat = lxe::proc::parse_stat(text)) {
        // A parsed command name always comes from the input.
        if (text.find(stat->comm) == std::string_view::npos) {
            __builtin_trap();
        }
    }
    std::ignore = lxe::proc::parse_status(text);
    std::ignore = lxe::proc::parse_cmdline(text);
    std::ignore = lxe::proc::parse_cpu_times(text);
    std::ignore = lxe::proc::parse_meminfo(text);
    std::ignore = lxe::proc::parse_pid(text);
    if (const auto smaps = lxe::proc::parse_smaps(text)) {
        for (const auto& entry : *smaps) {
            if (entry.mapping.end < entry.mapping.start) {
                __builtin_trap();
            }
        }
    }
    if (const auto maps = lxe::proc::parse_maps(text)) {
        for (const auto& mapping : *maps) {
            if (mapping.end < mapping.start || text.find(mapping.path) == std::string_view::npos) {
                __builtin_trap();
            }
        }
    }
    return 0;
}
