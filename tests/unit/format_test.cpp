#include "lxe/model/format.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>

using namespace lxe::model;
using lxe::proc::ProcessState;

TEST_CASE("model.format_bytes.uses_binary_units")
{
    CHECK(format_bytes(0) == "0 B");
    CHECK(format_bytes(1023) == "1023 B");
    CHECK(format_bytes(1024) == "1.0 KiB");
    CHECK(format_bytes(1536) == "1.5 KiB");
    CHECK(format_bytes(1024ULL * 1024) == "1.0 MiB");
    CHECK(format_bytes(5ULL * 1024 * 1024 * 1024) == "5.0 GiB");
    CHECK(format_bytes(std::numeric_limits<std::uint64_t>::max()) == "16.0 EiB");
}

TEST_CASE("model.format_cpu.hides_idle_and_unknown")
{
    CHECK(format_cpu(std::nullopt).empty());
    CHECK(format_cpu(0.0).empty());
    CHECK(format_cpu(0.004) == "< 0.01");
    CHECK(format_cpu(12.5) == "12.50");
    CHECK(format_cpu(100.0) == "100.00");
}

TEST_CASE("model.state_name.names_known_states")
{
    CHECK(state_name(ProcessState::running) == "Running");
    CHECK(state_name(ProcessState::sleeping) == "Sleeping");
    CHECK(state_name(ProcessState::disk_sleep) == "Disk sleep");
    CHECK(state_name(ProcessState::zombie) == "Zombie");
    CHECK(state_name(ProcessState::idle) == "Idle");
    CHECK(state_name(ProcessState{'?'}) == "Unknown");
}

TEST_CASE("model.join_command_line.joins_with_spaces")
{
    CHECK(join_command_line(std::vector<std::string>{"/bin/ls", "-l", "my dir"}) == "/bin/ls -l my dir");
    CHECK(join_command_line(std::vector<std::string>{}).empty());
}
