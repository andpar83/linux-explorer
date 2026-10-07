# Sanitizer and analyzer configuration.
#
#   -DLXE_SANITIZER=address    ASan + LSan + UBSan                              (preset: asan)
#   -DLXE_SANITIZER=thread     TSan + UBSan                                     (preset: tsan)
#   -DLXE_SANITIZER=undefined  UBSan only                                       (preset: ubsan)
#   -DLXE_SANITIZER=none       default
#   -DLXE_LIBSTDCXX_DEBUG=ON   libstdc++ debug mode: checked iterators/containers (_GLIBCXX_DEBUG).
#                              Changes the container ABI, so it is applied build-wide.
#   -DLXE_ENABLE_ANALYZER=ON   GCC static analyzer (-fanalyzer) on non-test code      (preset: analyze)
#
# Sanitizer runtime options are compiled into every executable (src/sanitizer_defaults.cpp,
# added by lxe_add_sanitizer_defaults), so they apply under ctest, in CLion run configurations
# and on the command line alike. ASAN_OPTIONS etc. in the environment still override them.

set(LXE_SANITIZER "none" CACHE STRING "Sanitizer set: none, address, thread, undefined")
set_property(CACHE LXE_SANITIZER PROPERTY STRINGS none address thread undefined)
option(LXE_LIBSTDCXX_DEBUG "Enable libstdc++ debug mode (_GLIBCXX_DEBUG)" OFF)
option(LXE_ENABLE_ANALYZER "Enable the GCC static analyzer (-fanalyzer)" OFF)

# Applies the selected sanitizer to every target defined after this call (directory scope).
function(lxe_enable_sanitizers)
    # UBSan: 'undefined' plus the checks GCC leaves out of it by default.
    set(ubsan "undefined,float-divide-by-zero,float-cast-overflow,bounds-strict")
    set(common_flags -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-sanitize-recover=all)

    set(flags "")
    if(LXE_SANITIZER STREQUAL "address")
        # Not included: pointer-compare/pointer-subtract. Their runtime check scans the shadow
        # memory between the two pointers, and _GLIBCXX_SANITIZE_VECTOR below poisons a vector's
        # spare capacity, so every vector::insert is reported. Overflow detection inside vectors
        # is the more valuable of the two for this code base.
        set(flags
            "-fsanitize=address,leak,${ubsan}"
            -fsanitize-address-use-after-scope
            ${common_flags})
        # Let ASan see container-internal overflows in std::vector.
        add_compile_definitions(_GLIBCXX_SANITIZE_VECTOR)
    elseif(LXE_SANITIZER STREQUAL "thread")
        set(flags "-fsanitize=thread,${ubsan}" ${common_flags})
    elseif(LXE_SANITIZER STREQUAL "undefined")
        set(flags "-fsanitize=${ubsan}" ${common_flags})
    elseif(NOT LXE_SANITIZER STREQUAL "none")
        message(FATAL_ERROR "Unknown LXE_SANITIZER='${LXE_SANITIZER}' (none|address|thread|undefined)")
    endif()

    add_compile_options(${flags})
    add_link_options(${flags})

    if(LXE_LIBSTDCXX_DEBUG)
        add_compile_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC)
    endif()

    if(NOT LXE_SANITIZER STREQUAL "none")
        message(STATUS "Sanitizer: ${LXE_SANITIZER} (${flags})")
    endif()
endfunction()

# Creates the INTERFACE target lxe::analyzer, which carries -fanalyzer when LXE_ENABLE_ANALYZER
# is on and nothing otherwise. Only src/ targets link it (PRIVATE, so it doesn't propagate):
# the analyzer produces hundreds of false positives inside test-framework macros, which would
# bury real findings.
function(lxe_add_analyzer_target)
    add_library(lxe_analyzer INTERFACE)
    add_library(lxe::analyzer ALIAS lxe_analyzer)
    if(NOT LXE_ENABLE_ANALYZER)
        return()
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "LXE_ENABLE_ANALYZER requires GCC")
    endif()
    target_compile_options(lxe_analyzer INTERFACE -fanalyzer)
endfunction()

# Compiles the default sanitizer runtime options into an executable (no-op without sanitizers).
function(lxe_add_sanitizer_defaults target)
    if(NOT LXE_SANITIZER STREQUAL "none")
        target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/sanitizer_defaults.cpp")
    endif()
endfunction()
