cmake_minimum_required(VERSION 3.30...4.3)

# The project standardises on GCC 15 + C++23: the newest stable GCC on this distro, fully
# understood by CLion's code engine. This is the only place that picks the compiler.
# It applies when the caller hasn't chosen one (presets, CLion profiles, bare `cmake -S . -B build`);
# an explicit -DCMAKE_CXX_COMPILER or CXX env var still wins. Must stay above project(),
# which is where compiler detection happens.
set(LXE_GCC_MAJOR 15)
if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    find_program(LXE_DEFAULT_CXX NAMES g++-${LXE_GCC_MAJOR} NO_CACHE)
    if(LXE_DEFAULT_CXX)
        set(CMAKE_CXX_COMPILER "${LXE_DEFAULT_CXX}")
    endif()
endif()

project(linux_explorer
    VERSION 0.1.1
    DESCRIPTION "Process Explorer-style live system inspector for Linux"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset: cmake --preset asan")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    string(REGEX MATCH "^[0-9]+" lxe_gcc_found_major "${CMAKE_CXX_COMPILER_VERSION}")
    if(lxe_gcc_found_major LESS LXE_GCC_MAJOR)
        message(FATAL_ERROR "GCC >= ${LXE_GCC_MAJOR} is required (found ${CMAKE_CXX_COMPILER_VERSION} at ${CMAKE_CXX_COMPILER})")
    elseif(NOT lxe_gcc_found_major EQUAL LXE_GCC_MAJOR)
        # Allowed for experiments, but loud: usually a stale cache from an older setup.
        message(WARNING "Building with GCC ${CMAKE_CXX_COMPILER_VERSION}; the project standardises on GCC ${LXE_GCC_MAJOR}. "
            "If you didn't choose this on purpose, delete the build directory "
            "(CLion: Tools | CMake | Reset Cache and Reload Project).")
    endif()
endif()

# ---- Language -------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++ modules yet; scanning needs Ninja and slows every compile. Flip when modules are adopted.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---- Options --------------------------------------------------------------
option(LXE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(LXE_BUILD_TESTS "Build and register tests" ${PROJECT_IS_TOP_LEVEL})

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(Hardening)

# ---- Build-wide instrumentation --------------------------------------------
# Applies to every target, third-party code included: sanitizers want the whole program
# instrumented, and libstdc++ debug mode changes container layouts, so mixing is unsafe.
# Bounds/precondition checks inside libstdc++ (operator[], iterators...). Cheap; on in every config.
add_compile_definitions(_GLIBCXX_ASSERTIONS)
lxe_enable_sanitizers()
lxe_enable_hardening()

# ---- Third-party dependencies ----------------------------------------------
include(Dependencies)

# ---- Project-only settings -------------------------------------------------
# Warnings apply to our code only; every project target links lxe::options. The static
# analyzer (lxe::analyzer) is linked PRIVATE by src/ targets only.
add_library(lxe_options INTERFACE)
add_library(lxe::options ALIAS lxe_options)
target_compile_features(lxe_options INTERFACE cxx_std_23)
lxe_set_warnings(lxe_options)
lxe_add_analyzer_target()

add_subdirectory(src)

if(LXE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(Packaging)
endif()
