#include "lxe/app/app.hpp"

#include "lxe/model/details.hpp"
#include "lxe/model/sampler.hpp"
#include "lxe/sys/system.hpp"
#include "lxe/sys/users.hpp"
#include "lxe/ui/process_view.hpp"
#include "lxe/ui/theme.hpp"
#include "lxe/util/scope_exit.hpp"
#include "lxe/version.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace lxe::app {
namespace {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto refresh_interval = 1s;
/// After input, keep rendering this long so hover effects, tooltips and clicks settle.
/// Otherwise the loop sleeps until the next refresh or input event and costs no CPU.
constexpr auto active_period = 750ms;
/// Frames rendered before a screenshot is taken: fonts and layout have settled by then.
constexpr int screenshot_after_frames = 3;
/// GNOME's default UI font is 11 pt, i.e. about 15 px at 96 dpi.
constexpr float base_font_size = 15.0F;
constexpr int initial_width = 1280;
constexpr int initial_height = 800;

/// The first one that exists is used: the desktop's UI font first, common fallbacks after.
constexpr std::array text_fonts{
    "/usr/share/fonts/truetype/ubuntu/UbuntuSans[wdth,wght].ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};
/// Font Awesome 4 (Ubuntu package fonts-font-awesome) for toolbar icons; optional.
constexpr const char* icon_font = "/usr/share/fonts/truetype/font-awesome/fontawesome-webfont.ttf";
/// Monospace font for addresses and permission flags; optional.
constexpr std::array mono_fonts{
    "/usr/share/fonts/truetype/ubuntu/UbuntuSansMono[wght].ttf",
    "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
};

struct WindowDeleter
{
    void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};

using WindowHandle = std::unique_ptr<SDL_Window, WindowDeleter>;

struct GlContextDeleter
{
    void operator()(SDL_GLContextState* context) const noexcept { SDL_GL_DestroyContext(context); }
};

using GlContextHandle = std::unique_ptr<SDL_GLContextState, GlContextDeleter>;

/// The few OpenGL calls the loop makes itself. Loaded through SDL, so there is no link-time
/// dependency on libGL (ImGui's renderer backend loads its own entry points the same way).
struct GlCalls
{
    using Viewport = void (*)(int, int, int, int);
    using ClearColor = void (*)(float, float, float, float);
    using Clear = void (*)(unsigned int);
    using PixelStorei = void (*)(unsigned int, int);
    using ReadPixels = void (*)(int, int, int, int, unsigned int, unsigned int, void*);

    Viewport viewport = nullptr;
    ClearColor clear_color = nullptr;
    Clear clear = nullptr;
    PixelStorei pixel_storei = nullptr;
    ReadPixels read_pixels = nullptr;

    [[nodiscard]] bool loaded() const noexcept
    {
        return viewport != nullptr && clear_color != nullptr && clear != nullptr && pixel_storei != nullptr &&
               read_pixels != nullptr;
    }
};

constexpr unsigned int gl_color_buffer_bit = 0x0000'4000;
constexpr unsigned int gl_pack_alignment = 0x0D05;
constexpr unsigned int gl_rgb = 0x1907;
constexpr unsigned int gl_unsigned_byte = 0x1401;

template <class Function>
[[nodiscard]] Function gl_function(const char* name) noexcept
{
    return std::bit_cast<Function>(SDL_GL_GetProcAddress(name));
}

[[nodiscard]] int report_sdl_error(std::string_view what)
{
    std::println(stderr, "linux-explorer: {}: {}", what, SDL_GetError());
    return 1;
}

[[nodiscard]] ui::Theme current_theme(const Options& options) noexcept
{
    if (options.theme) {
        return *options.theme;
    }
    return SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK ? ui::Theme::dark : ui::Theme::light;
}

[[nodiscard]] bool file_exists(const char* path) noexcept
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

struct Fonts
{
    bool icons = false;
    ImFont* mono = nullptr;
};

/// Loads the UI font with icon glyphs merged in, and a monospace font. Both extras are optional.
[[nodiscard]] Fonts load_fonts(ImGuiIO& io)
{
    Fonts fonts;
    const auto text_font = std::ranges::find_if(text_fonts, file_exists);
    if (text_font == text_fonts.end() || io.Fonts->AddFontFromFileTTF(*text_font, base_font_size) == nullptr) {
        io.Fonts->AddFontDefault();
    }
    if (file_exists(icon_font)) {
        ImFontConfig config;
        config.MergeMode = true;
        config.GlyphMinAdvanceX = base_font_size; // same width for every icon, so labels line up
        fonts.icons = io.Fonts->AddFontFromFileTTF(icon_font, base_font_size * 0.9F, &config) != nullptr;
    }
    if (const auto mono = std::ranges::find_if(mono_fonts, file_exists); mono != mono_fonts.end()) {
        fonts.mono = io.Fonts->AddFontFromFileTTF(*mono, base_font_size);
    }
    return fonts;
}

/// "Linux Explorer — user@host", like Process Explorer's "[host\user]" title.
[[nodiscard]] std::string window_title()
{
    std::array<char, HOST_NAME_MAX + 1> host{};
    if (::gethostname(host.data(), host.size()) != 0) {
        return "Linux Explorer";
    }
    host.back() = '\0';
    const UserId user = sys::current_user();
    return std::format(
        "Linux Explorer — {}@{}", sys::lookup_system_user(user).value_or(std::format("{}", user)), host.data()
    );
}

/// Saves the current framebuffer as a binary PPM (P6). OpenGL rows start at the bottom.
[[nodiscard]] bool save_screenshot(const GlCalls& gl, int width, int height, const std::filesystem::path& path)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    const auto row_bytes = static_cast<std::size_t>(width) * 3;
    std::vector<unsigned char> pixels(row_bytes * static_cast<std::size_t>(height));
    gl.pixel_storei(gl_pack_alignment, 1);
    gl.read_pixels(0, 0, width, height, gl_rgb, gl_unsigned_byte, pixels.data());

    std::ofstream file{path, std::ios::binary};
    file << std::format("P6\n{} {}\n255\n", width, height);
    for (int row = height - 1; row >= 0; --row) {
        const auto* start = std::to_address(pixels.begin()) + static_cast<std::size_t>(row) * row_bytes;
        file.write(std::bit_cast<const char*>(start), static_cast<std::streamsize>(row_bytes));
    }
    return file.good();
}

/// The start address of the mapping `wanted` names (hex address or path), if listed.
[[nodiscard]] std::optional<std::uint64_t> find_mapping(const model::ProcessDetails& details, std::string_view wanted)
{
    for (const auto& info : details.maps) {
        if (info.mapping.path == wanted) {
            return info.mapping.start;
        }
    }
    std::uint64_t address = 0;
    const char* const last = std::to_address(wanted.end());
    constexpr int hex = 16;
    if (const auto [end, error] = std::from_chars(wanted.data(), last, address, hex); error == std::errc{} && end == last) {
        return address;
    }
    return std::nullopt;
}

/// Where ImGui keeps column widths and order between runs (~/.local/share/linux-explorer/).
[[nodiscard]] std::string settings_path()
{
    char* dir = SDL_GetPrefPath("", "linux-explorer");
    if (dir == nullptr) {
        return {};
    }
    const util::ScopeExit free_dir{[dir] { SDL_free(dir); }};
    return std::format("{}imgui.ini", dir);
}

} // namespace

int run(const proc::ProcFs& fs, const Options& options)
{
    // What the desktop sees: window title group, dock icon and inhibitor names. The identifier
    // matches packaging/linux-explorer.desktop so GNOME can pair the window with the entry.
    SDL_SetAppMetadata("Linux Explorer", std::string{version}.c_str(), "linux-explorer");
    // SDL disables the screensaver by default, as games want; a monitor must let the machine
    // blank and sleep.
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return report_sdl_error("cannot initialise SDL");
    }
    const util::ScopeExit quit_sdl{[] { SDL_Quit(); }};

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (scale <= 0.0F) {
        scale = 1.0F;
    }
    const auto scaled = [scale](int size) { return static_cast<int>(static_cast<float>(size) * scale); };

    const std::string title = window_title();
    const WindowHandle window{SDL_CreateWindow(
        title.c_str(),
        scaled(initial_width),
        scaled(initial_height),
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
    )};
    if (!window) {
        return report_sdl_error("cannot create the window");
    }
    const GlContextHandle gl_context{SDL_GL_CreateContext(window.get())};
    if (!gl_context) {
        return report_sdl_error("cannot create an OpenGL 3 context");
    }
    SDL_GL_MakeCurrent(window.get(), gl_context.get());
    SDL_GL_SetSwapInterval(1);
    const GlCalls gl{
        .viewport = gl_function<GlCalls::Viewport>("glViewport"),
        .clear_color = gl_function<GlCalls::ClearColor>("glClearColor"),
        .clear = gl_function<GlCalls::Clear>("glClear"),
        .pixel_storei = gl_function<GlCalls::PixelStorei>("glPixelStorei"),
        .read_pixels = gl_function<GlCalls::ReadPixels>("glReadPixels"),
    };
    if (!gl.loaded()) {
        return report_sdl_error("cannot load OpenGL functions");
    }
    SDL_SetWindowPosition(window.get(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window.get());

    // ImGui keeps a pointer to the settings path until the context is destroyed, so it lives longer.
    const std::string settings = settings_path();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    const util::ScopeExit destroy_imgui{[] { ImGui::DestroyContext(); }};
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = settings.empty() ? nullptr : settings.c_str();

    const Fonts fonts = load_fonts(io);
    ui::ViewConfig config{
        .current_user = sys::current_user(),
        .palette = ui::apply_theme(current_theme(options), scale, base_font_size),
        .icons = fonts.icons,
        .mono_font = fonts.mono,
    };

    if (!ImGui_ImplSDL3_InitForOpenGL(window.get(), gl_context.get())) {
        return report_sdl_error("cannot initialise the ImGui SDL3 backend");
    }
    const util::ScopeExit shutdown_platform{[] { ImGui_ImplSDL3_Shutdown(); }};
    if (!ImGui_ImplOpenGL3_Init(nullptr)) {
        return report_sdl_error("cannot initialise the ImGui OpenGL backend");
    }
    const util::ScopeExit shutdown_renderer{[] { ImGui_ImplOpenGL3_Shutdown(); }};

    const std::uint64_t page_bytes = sys::page_size();
    model::Sampler sampler{page_bytes};
    model::Model model;
    model::ProcessDetails details;
    ui::ViewState state{.paused = false, .selected = options.selected};
    // Details are read only while the pane is visible; the pane shows what `details` says.
    const auto wanted_details = [&] {
        return model::DetailsRequest{
            .pid = state.show_details ? state.selected : std::nullopt,
            .mapping = state.show_details ? state.selected_mapping : std::nullopt,
        };
    };
    const auto refresh = [&] {
        if (auto snapshot = fs.read_snapshot()) {
            model = sampler.update(*snapshot);
        }
        else {
            std::println(stderr, "linux-explorer: cannot read {}: {}", fs.root().string(), snapshot.error().message());
        }
        details = model::load_details(fs, wanted_details(), page_bytes);
    };
    refresh();
    if (options.selected_mapping) {
        state.selected_mapping = find_mapping(details, *options.selected_mapping);
        details = model::load_details(fs, wanted_details(), page_bytes);
    }

    auto next_refresh = Clock::now() + refresh_interval;
    auto active_until = Clock::now() + active_period;
    int frames = 0;
    int exit_code = 0;
    bool running = true;
    while (running) {
        SDL_Event event{};
        bool have_event = false;
        if (const auto now = Clock::now(); now < active_until || options.screenshot) {
            have_event = SDL_PollEvent(&event);
        }
        else if (state.paused) {
            have_event = SDL_WaitEvent(&event);
        }
        else {
            const auto wait = std::chrono::ceil<std::chrono::milliseconds>(std::max(next_refresh - now, Clock::duration{}));
            have_event = SDL_WaitEventTimeout(&event, static_cast<Sint32>(wait.count()));
        }
        while (have_event) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(window.get()))) {
                running = false;
            }
            if (event.type == SDL_EVENT_SYSTEM_THEME_CHANGED) {
                config.palette = ui::apply_theme(current_theme(options), scale, base_font_size);
            }
            active_until = Clock::now() + active_period;
            have_event = SDL_PollEvent(&event);
        }

        if (!state.paused && Clock::now() >= next_refresh) {
            refresh();
            next_refresh = Clock::now() + refresh_interval;
        }
        if ((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_MINIMIZED) != 0) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ui::FrameRequests requests = ui::draw_main_window(model, details, state, config);
        ImGui::Render();
        if (const auto wanted = wanted_details(); details.pid != wanted.pid || details.selected_mapping != wanted.mapping) {
            details = model::load_details(fs, wanted, page_bytes); // selection changed: show it right away
        }

        int pixel_width = 0;
        int pixel_height = 0;
        SDL_GetWindowSizeInPixels(window.get(), &pixel_width, &pixel_height);
        gl.viewport(0, 0, pixel_width, pixel_height);
        const ImVec4 background = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        gl.clear_color(background.x, background.y, background.z, 1.0F);
        gl.clear(gl_color_buffer_bit);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (options.screenshot && ++frames >= screenshot_after_frames) {
            if (!save_screenshot(gl, pixel_width, pixel_height, *options.screenshot)) {
                std::println(stderr, "linux-explorer: cannot write {}", options.screenshot->string());
                exit_code = 1;
            }
            running = false;
        }
        SDL_GL_SwapWindow(window.get());

        if (requests.refresh_now) {
            refresh();
            next_refresh = Clock::now() + refresh_interval;
            active_until = Clock::now() + active_period;
        }
    }
    return exit_code;
}

} // namespace lxe::app
