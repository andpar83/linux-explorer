configure_file(lxe/version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/lxe/version.hpp" @ONLY)

# Data collection and model: /proc parsing, process tree, CPU sampling, formatting.
add_library(lxe_core STATIC
    lxe/sys/file.cpp
    lxe/sys/system.cpp
    lxe/sys/users.cpp
    lxe/proc/parse.cpp
    lxe/proc/proc_fs.cpp
    lxe/model/details.cpp
    lxe/model/format.cpp
    lxe/model/process_tree.cpp
    lxe/model/sampler.cpp
    lxe/model/text_maps.cpp
    lxe/model/text_tree.cpp
)
target_include_directories(lxe_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(lxe_core PUBLIC lxe::options PRIVATE lxe::analyzer)

# ImGui views of the model. Depends on ImGui core only, so tests can drive it headlessly.
add_library(lxe_ui STATIC
    lxe/ui/process_view.cpp
    lxe/ui/theme.cpp
)
target_link_libraries(lxe_ui PUBLIC lxe_core imgui::imgui PRIVATE lxe::analyzer)

# The application: SDL3 window + OpenGL rendering, and the command-line modes.
add_executable(linux-explorer
    main.cpp
    lxe/app/app.cpp
)
target_link_libraries(linux-explorer PRIVATE lxe_ui imgui::sdl3_opengl3 lxe::analyzer)
lxe_add_sanitizer_defaults(linux-explorer)
