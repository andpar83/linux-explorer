# Security hardening for non-Debug builds without sanitizers (sanitizers and
# _FORTIFY_SOURCE interfere with each other, and -O0 can't fortify anyway).

option(LXE_HARDENING "Enable hardening flags in non-Debug, non-sanitized builds" ON)

function(lxe_enable_hardening target)
    if(NOT LXE_HARDENING OR NOT LXE_SANITIZER STREQUAL "none")
        return()
    endif()

    set(on "$<NOT:$<CONFIG:Debug>>")
    set(compile_flags
        -fstack-protector-strong
        -fstack-clash-protection
        -ftrivial-auto-var-init=zero
        -fPIE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        list(APPEND compile_flags -fcf-protection=full)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        list(APPEND compile_flags -mbranch-protection=standard)
    endif()
    set(link_flags -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)

    target_compile_definitions(${target} INTERFACE "$<${on}:_FORTIFY_SOURCE=3>")
    target_compile_options(${target} INTERFACE "$<${on}:${compile_flags}>")
    target_link_options(${target} INTERFACE "$<${on}:${link_flags}>")
endfunction()
