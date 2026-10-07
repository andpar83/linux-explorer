# Test registration. Naming: <category>.<suite>.<case>  (categories: unit, integration, e2e, property, fuzz, bench)
#
# Every test inherits the sanitizer runtime options of the current preset and fails on any
# sanitizer report even if the exit code is 0.

set(LXE_SANITIZER_FAIL_REGEX
    "AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer|runtime error:")

# lxe_add_test(<name> COMMAND <cmd...> [PASS_REGEX <regex>])
function(lxe_add_test name)
    cmake_parse_arguments(arg "" "PASS_REGEX" "COMMAND" ${ARGN})
    add_test(NAME ${name} COMMAND ${arg_COMMAND})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "${LXE_SANITIZER_ENV}"
        FAIL_REGULAR_EXPRESSION "${LXE_SANITIZER_FAIL_REGEX}")
    if(arg_PASS_REGEX)
        set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${arg_PASS_REGEX}")
    endif()
endfunction()

# --- e2e: run the real binary ----------------------------------------------
lxe_add_test(e2e.smoke.exit_code COMMAND linux-explorer)
lxe_add_test(e2e.smoke.greeting  COMMAND linux-explorer PASS_REGEX "^Hello, linux-explorer!\n$")
