// Default sanitizer runtime options, compiled into every executable of a sanitized build
// (see cmake/Sanitizers.cmake). ASAN_OPTIONS, UBSAN_OPTIONS, ... in the environment still
// override them. The runtimes look these functions up by name, hence the reserved identifiers.

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

[[gnu::used]] const char* __asan_default_options() noexcept
{
    return "detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:"
           "strict_string_checks=1:detect_leaks=1:halt_on_error=1";
}

[[gnu::used]] const char* __ubsan_default_options() noexcept
{
    return "print_stacktrace=1:halt_on_error=1";
}

[[gnu::used]] const char* __tsan_default_options() noexcept
{
    // report_mutex_bugs=0: ignoring the uninstrumented desktop libraries below hides one side
    // of lock/unlock pairs that cross library boundaries, which TSan then reports as mutex
    // misuse. Our own locking is RAII (std::scoped_lock), so the check has little to find here;
    // data-race detection is unaffected.
    return "halt_on_error=1:second_deadlock_stack=1:report_mutex_bugs=0";
}

// SDL and the desktop/graphics libraries loaded for the window: Wayland decorations go through
// GTK, fonts through fontconfig/Pango, the theme through D-Bus, rendering through the GPU driver.
// They leak at exit and use synchronisation TSan can't see (they aren't instrumented). Not ours
// to fix; a report with one of our frames in the stack and none of these is still shown.
// Names are exact library files: TSan refuses a called_from_lib entry that matches more than
// one loaded library. "<unknown module>" is code SDL unloaded (dlclose) before the leak check.
#define LXE_DESKTOP_LIBRARIES(kind) \
    kind ":libdbus-1.so\n" \
    kind ":libglib-2.0.so\n" \
    kind ":libgobject-2.0.so\n" \
    kind ":libgio-2.0.so\n" \
    kind ":libgmodule-2.0.so\n" \
    kind ":libfontconfig.so\n" \
    kind ":libfreetype.so\n" \
    kind ":libharfbuzz.so\n" \
    kind ":libpango-1.0.so\n" \
    kind ":libpangoft2-1.0.so\n" \
    kind ":libpangocairo-1.0.so\n" \
    kind ":libcairo.so\n" \
    kind ":libcairo-gobject.so\n" \
    kind ":libpixman-1.so\n" \
    kind ":libgdk_pixbuf-2.0.so\n" \
    kind ":libglycin-2.so\n" \
    kind ":librsvg-2.so\n" \
    kind ":libgtk-3.so\n" \
    kind ":libgdk-3.so\n" \
    kind ":libdecor-0.so\n" \
    kind ":libdecor-gtk.so\n" \
    kind ":libwayland-client.so\n" \
    kind ":libwayland-cursor.so\n" \
    kind ":libwayland-egl.so\n" \
    kind ":libxkbcommon.so\n" \
    kind ":libffi.so\n" \
    kind ":libSDL3.so\n" \
    kind ":libEGL.so\n" \
    kind ":libGL.so\n" \
    kind ":libGLX.so\n" \
    kind ":libGLdispatch.so\n" \
    kind ":libdrm.so\n" \
    kind ":libEGL_mesa.so\n" \
    kind ":libGLX_mesa.so\n" \
    kind ":libgallium-\n" \
    kind ":libEGL_nvidia.so\n" \
    kind ":libGLX_nvidia.so\n" \
    kind ":libnvidia-eglcore.so\n" \
    kind ":libnvidia-glcore.so\n" \
    kind ":libnvidia-glsi.so\n" \
    kind ":libnvidia-gpucomp.so\n"

[[gnu::used]] const char* __lsan_default_suppressions() noexcept
{
    return LXE_DESKTOP_LIBRARIES("leak") "leak:<unknown module>\n";
}

[[gnu::used]] const char* __tsan_default_suppressions() noexcept
{
    // called_from_lib: ignore everything TSan intercepts while inside these libraries.
    return LXE_DESKTOP_LIBRARIES("called_from_lib") LXE_DESKTOP_LIBRARIES("race")
        LXE_DESKTOP_LIBRARIES("mutex") LXE_DESKTOP_LIBRARIES("deadlock") LXE_DESKTOP_LIBRARIES("thread");
}

#undef LXE_DESKTOP_LIBRARIES

} // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
