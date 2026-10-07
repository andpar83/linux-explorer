add_executable(linux-explorer
    main.cpp
)
target_link_libraries(linux-explorer PRIVATE lxe::options)
