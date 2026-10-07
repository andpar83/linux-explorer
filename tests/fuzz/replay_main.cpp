// Replays fuzz corpus files through LLVMFuzzerTestOneInput, so every build exercises the
// harnesses even without a fuzzing engine. Usage: <binary> <file-or-directory>...
#include "lxe/sys/file.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <print>
#include <span>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main(int argc, char* argv[])
{
    std::vector<std::filesystem::path> inputs;
    for (const char* arg : std::span{argv, static_cast<std::size_t>(argc)}.subspan(1)) {
        const std::filesystem::path path{arg};
        if (std::filesystem::is_directory(path)) {
            for (const auto& file : std::filesystem::recursive_directory_iterator{path}) {
                if (file.is_regular_file()) {
                    inputs.push_back(file.path());
                }
            }
        }
        else {
            inputs.push_back(path);
        }
    }
    for (const auto& input : inputs) {
        const auto content = lxe::sys::read_file(input);
        if (!content) {
            std::println(stderr, "cannot read {}: {}", input.string(), content.error().message());
            return 1;
        }
        LLVMFuzzerTestOneInput(std::bit_cast<const std::uint8_t*>(content->data()), content->size());
    }
    std::println("replayed {} inputs", inputs.size());
    return inputs.empty() ? 1 : 0;
}
