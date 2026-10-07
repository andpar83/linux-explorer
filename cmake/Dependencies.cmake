# Third-party dependencies.
#
#   SDL3       system package (sudo apt install libsdl3-dev): window, input, OpenGL context.
#   Dear ImGui pinned release, downloaded at configure time and built here (upstream has no
#              CMake build). Compiled with the project's sanitizer flags, without its warnings.
#   Catch2     pinned release, downloaded in tests/CMakeLists.txt when tests are enabled.
#
# Bumping a pinned version: change the URL and the SHA256 together.

include(FetchContent)

find_package(SDL3 3.2 CONFIG)
if(NOT SDL3_FOUND)
    message(FATAL_ERROR "SDL3 development files not found. Install them with:\n  sudo apt install libsdl3-dev")
endif()
# SDL3's package config marks its headers non-SYSTEM on purpose; ours treats every third-party
# header as SYSTEM so our -Werror warning set never fires inside them.
set_property(TARGET SDL3::Headers PROPERTY SYSTEM 1)

FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
    URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
)
FetchContent_MakeAvailable(imgui)

# Core library. SYSTEM include dirs keep ImGui's headers out of our -Werror warning set.
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
add_library(imgui::imgui ALIAS imgui)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR})
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# Platform + renderer backend used by the application: SDL3 window/input, OpenGL 3 rendering.
# The OpenGL backend loads GL itself at runtime, so no GL development package is needed.
add_library(imgui_sdl3_opengl3 STATIC
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui::sdl3_opengl3 ALIAS imgui_sdl3_opengl3)
target_include_directories(imgui_sdl3_opengl3 SYSTEM PUBLIC ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_sdl3_opengl3 PUBLIC imgui SDL3::SDL3 PRIVATE ${CMAKE_DL_LIBS})

# Headless backend for UI tests: no window, no GPU.
add_library(imgui_null STATIC ${imgui_SOURCE_DIR}/backends/imgui_impl_null.cpp)
add_library(imgui::null ALIAS imgui_null)
target_include_directories(imgui_null SYSTEM PUBLIC ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_null PUBLIC imgui)
