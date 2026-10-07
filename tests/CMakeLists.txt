# Tests. Every test is named <category>.<suite>.<case> and labelled with its category:
#   unit, integration, property, ui, fuzz, e2e (default run) and bench (opt-in: -L bench).
# Sanitizer runtime options are compiled into each test executable (lxe_add_sanitizer_defaults);
# on top of that, any sanitizer report in a test's output fails it even with exit code 0.

FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
    URL_HASH SHA256=0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34
    SYSTEM
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

set(LXE_SANITIZER_FAIL_REGEX
    "AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer|runtime error:")

set(LXE_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
configure_file(test_paths.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/test_paths.hpp" @ONLY)
add_library(lxe_test_support INTERFACE)
target_include_directories(lxe_test_support INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(lxe_test_support INTERFACE lxe_core)

# lxe_add_test(<name> COMMAND <cmd...> [PASS_REGEX <regex>] [LABELS <labels...>])
function(lxe_add_test name)
    cmake_parse_arguments(arg "" "PASS_REGEX" "COMMAND;LABELS" ${ARGN})
    add_test(NAME ${name} COMMAND ${arg_COMMAND})
    set_tests_properties(${name} PROPERTIES FAIL_REGULAR_EXPRESSION "${LXE_SANITIZER_FAIL_REGEX}")
    if(arg_PASS_REGEX)
        set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${arg_PASS_REGEX}")
    endif()
    if(arg_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${arg_LABELS}")
    endif()
endfunction()

# lxe_add_catch_tests(<category> SOURCES <files...> [LIBRARIES <libs...>])
# Builds <category>_tests and registers each TEST_CASE as "<category>.<test case name>".
function(lxe_add_catch_tests category)
    cmake_parse_arguments(arg "" "" "SOURCES;LIBRARIES" ${ARGN})
    set(target ${category}_tests)
    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE lxe_test_support Catch2::Catch2WithMain ${arg_LIBRARIES})
    lxe_add_sanitizer_defaults(${target})
    catch_discover_tests(${target}
        TEST_PREFIX "${category}."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            LABELS ${category}
            FAIL_REGULAR_EXPRESSION "${LXE_SANITIZER_FAIL_REGEX}"
    )
endfunction()

lxe_add_catch_tests(unit SOURCES
    unit/format_test.cpp
    unit/maps_format_test.cpp
    unit/page_summary_test.cpp
    unit/parse_maps_test.cpp
    unit/parse_smaps_test.cpp
    unit/parse_stat_test.cpp
    unit/parse_status_cmdline_test.cpp
    unit/parse_system_test.cpp
    unit/process_tree_test.cpp
    unit/sampler_test.cpp
    unit/text_tree_test.cpp
    unit/user_names_test.cpp
)

lxe_add_catch_tests(integration SOURCES
    integration/maps_test.cpp
    integration/proc_fs_fixture_test.cpp
    integration/proc_fs_live_test.cpp
    integration/read_file_test.cpp
)

lxe_add_catch_tests(property SOURCES
    property/parsers_property_test.cpp
    property/tree_property_test.cpp
)

lxe_add_catch_tests(ui
    SOURCES ui/process_view_test.cpp
    LIBRARIES lxe_ui imgui::null
)

# --- fuzz: replay the corpus through the libFuzzer harness ------------------
add_executable(fuzz_proc_parsers fuzz/proc_parsers_fuzz.cpp fuzz/replay_main.cpp)
target_link_libraries(fuzz_proc_parsers PRIVATE lxe_core)
lxe_add_sanitizer_defaults(fuzz_proc_parsers)
lxe_add_test(fuzz.proc_parsers.replay_corpus
    COMMAND fuzz_proc_parsers "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/proc_parsers"
    PASS_REGEX "replayed [1-9][0-9]* inputs"
    LABELS fuzz
)

# --- bench: built everywhere, run only on request (-L bench) ----------------
add_executable(bench_tests bench/proc_bench.cpp)
target_link_libraries(bench_tests PRIVATE lxe_test_support Catch2::Catch2WithMain)
lxe_add_sanitizer_defaults(bench_tests)
lxe_add_test(bench.all COMMAND bench_tests --benchmark-samples 5 LABELS bench)

# --- e2e: the real binary ----------------------------------------------------
lxe_add_test(e2e.cli.help COMMAND linux-explorer --help PASS_REGEX "^Usage: linux-explorer" LABELS e2e)
lxe_add_test(e2e.cli.version COMMAND linux-explorer --version
    PASS_REGEX "^linux-explorer ${PROJECT_VERSION}\n$" LABELS e2e)
lxe_add_test(e2e.cli.rejects_unknown_arguments COMMAND linux-explorer --bogus
    PASS_REGEX "unrecognized argument '--bogus'" LABELS e2e)
lxe_add_test(e2e.cli.print_tree_of_fixture
    COMMAND linux-explorer --print-tree --proc-root "${LXE_FIXTURES_DIR}/proc"
    PASS_REGEX "^systemd\\(1\\)\n├─agetty\\(50\\)\n├─sshd\\(60\\)\n│ └─sshd\\(61\\)\n└─bash\\(100\\)\n  └─my \\(weird\\) prog\\(101\\)\nkthreadd\\(2\\)\n└─kworker/0:0H\\(3\\)\n$"
    LABELS e2e)
lxe_add_test(e2e.cli.print_tree_shows_itself COMMAND linux-explorer --print-tree
    PASS_REGEX "linux-explorer\\([0-9]+\\)" LABELS e2e)
lxe_add_test(e2e.cli.print_maps_of_fixture_without_smaps
    COMMAND linux-explorer --print-maps 101 --proc-root "${LXE_FIXTURES_DIR}/proc"
    PASS_REGEX "^Address +Size +Rss Perms Offset +Path\n0000000000400000 +4\\.0 KiB +r-xp +00000000 /home/alice/my prog\n.*00007f0000200000 +1\\.0 MiB +r--s +00000000 /dev/shm/cache \\(deleted\\)\n.*ffffffffff600000 +4\\.0 KiB +--xp +00000000 \\[vsyscall\\]\n8 mappings, 2\\.3 MiB mapped, 0 B resident\n$"
    LABELS e2e)
lxe_add_test(e2e.cli.print_maps_of_fixture_with_smaps
    COMMAND linux-explorer --print-maps 100 --proc-root "${LXE_FIXTURES_DIR}/proc"
    PASS_REGEX "^Address +Size +Rss Perms Offset +Path\n0000558367a0a000 +196\\.0 KiB +196\\.0 KiB r--p +00000000 /usr/bin/bash\n.*33 mappings, [0-9.]+ MiB mapped, [0-9.]+ MiB resident\n$"
    LABELS e2e)
lxe_add_test(e2e.cli.print_maps_of_itself COMMAND linux-explorer --print-maps self
    PASS_REGEX "r-xp +[0-9a-f]+ .*linux-explorer\n.*\\[stack\\]\n.*[0-9]+ mappings, .* resident" LABELS e2e)
lxe_add_test(e2e.cli.print_maps_rejects_bad_pid COMMAND linux-explorer --print-maps abc
    PASS_REGEX "'abc' is not a process id" LABELS e2e)
lxe_add_test(e2e.cli.print_maps_of_missing_process
    COMMAND linux-explorer --print-maps 999 --proc-root "${LXE_FIXTURES_DIR}/proc"
    PASS_REGEX "cannot read maps of process 999: No such file or directory" LABELS e2e)
