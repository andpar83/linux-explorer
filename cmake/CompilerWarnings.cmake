# Project-wide warning set. Applied through the lxe::options INTERFACE target.

function(lxe_set_warnings target)
    set(common_warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wold-style-cast
        -Wcast-align
        -Wcast-qual
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wsuggest-override
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wmisleading-indentation
        -Wunused
        -Wextra-semi
    )
    set(gcc_warnings
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    target_compile_options(${target} INTERFACE
        ${common_warnings}
        "$<$<CXX_COMPILER_ID:GNU>:${gcc_warnings}>"
        "$<$<BOOL:${LXE_WARNINGS_AS_ERRORS}>:-Werror>"
    )
endfunction()
