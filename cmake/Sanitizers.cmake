# Sanitizer and analyzer configuration.
#
#   -DLXE_SANITIZER=address    ASan + LSan + UBSan + invalid pointer-pair checks (preset: asan)
#   -DLXE_SANITIZER=thread     TSan + UBSan                                     (preset: tsan)
#   -DLXE_SANITIZER=undefined  UBSan only                                       (preset: ubsan)
#   -DLXE_SANITIZER=none       default
#   -DLXE_LIBSTDCXX_DEBUG=ON   libstdc++ debug mode: checked iterators/containers (_GLIBCXX_DEBUG).
#                              Changes the container ABI, so only for all-from-source builds.
#   -DLXE_ENABLE_ANALYZER=ON   GCC static analyzer (-fanalyzer)                  (preset: analyze)
#
# After lxe_enable_sanitizers() the variable LXE_SANITIZER_ENV holds the runtime
# environment (ASAN_OPTIONS etc.) that tests must run with.

set(LXE_SANITIZER "none" CACHE STRING "Sanitizer set: none, address, thread, undefined")
set_property(CACHE LXE_SANITIZER PROPERTY STRINGS none address thread undefined)
option(LXE_LIBSTDCXX_DEBUG "Enable libstdc++ debug mode (_GLIBCXX_DEBUG)" OFF)
option(LXE_ENABLE_ANALYZER "Enable the GCC static analyzer (-fanalyzer)" OFF)

function(lxe_enable_sanitizers target)
    # UBSan: 'undefined' plus the checks GCC leaves out of it by default.
    set(ubsan "undefined,float-divide-by-zero,float-cast-overflow,bounds-strict")
    set(ubsan_env "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1")
    set(common_flags -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-sanitize-recover=all)

    set(flags "")
    set(env "")
    if(LXE_SANITIZER STREQUAL "address")
        set(flags
            "-fsanitize=address,leak,pointer-compare,pointer-subtract,${ubsan}"
            -fsanitize-address-use-after-scope
            ${common_flags})
        # Let ASan see container-internal overflows in std::vector.
        target_compile_definitions(${target} INTERFACE _GLIBCXX_SANITIZE_VECTOR)
        set(env
            "ASAN_OPTIONS=detect_invalid_pointer_pairs=2:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:strict_string_checks=1:detect_leaks=1:halt_on_error=1"
            "LSAN_OPTIONS=print_suppressions=0"
            "${ubsan_env}")
    elseif(LXE_SANITIZER STREQUAL "thread")
        set(flags "-fsanitize=thread,${ubsan}" ${common_flags})
        set(env "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1" "${ubsan_env}")
    elseif(LXE_SANITIZER STREQUAL "undefined")
        set(flags "-fsanitize=${ubsan}" ${common_flags})
        set(env "${ubsan_env}")
    elseif(NOT LXE_SANITIZER STREQUAL "none")
        message(FATAL_ERROR "Unknown LXE_SANITIZER='${LXE_SANITIZER}' (none|address|thread|undefined)")
    endif()

    target_compile_options(${target} INTERFACE ${flags})
    target_link_options(${target} INTERFACE ${flags})

    if(LXE_LIBSTDCXX_DEBUG)
        target_compile_definitions(${target} INTERFACE _GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC)
    endif()

    if(LXE_ENABLE_ANALYZER)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            message(FATAL_ERROR "LXE_ENABLE_ANALYZER requires GCC")
        endif()
        target_compile_options(${target} INTERFACE -fanalyzer)
    endif()

    set(LXE_SANITIZER_ENV "${env}" PARENT_SCOPE)
    if(NOT LXE_SANITIZER STREQUAL "none")
        message(STATUS "Sanitizer: ${LXE_SANITIZER} (${flags})")
    endif()
endfunction()
