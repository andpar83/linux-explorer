cmake_minimum_required(VERSION 3.30...4.3)

project(linux_explorer
    VERSION 0.1.0
    DESCRIPTION "Process Explorer-style live system inspector for Linux"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset: cmake --preset asan")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    message(FATAL_ERROR "GCC >= 15 is required for C++26 (found ${CMAKE_CXX_COMPILER_VERSION})")
endif()

# ---- Language -------------------------------------------------------------
set(CMAKE_CXX_STANDARD 26)
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

# One INTERFACE target carries every project-wide flag; every real target links it.
add_library(lxe_options INTERFACE)
add_library(lxe::options ALIAS lxe_options)
target_compile_features(lxe_options INTERFACE cxx_std_26)
# Bounds/precondition checks inside libstdc++ (operator[], iterators...). Cheap; on in every config.
target_compile_definitions(lxe_options INTERFACE _GLIBCXX_ASSERTIONS)
lxe_set_warnings(lxe_options)
lxe_enable_sanitizers(lxe_options)
lxe_enable_hardening(lxe_options)

add_subdirectory(src)

if(LXE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
