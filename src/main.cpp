#include "lxe/app/app.hpp"
#include "lxe/model/details.hpp"
#include "lxe/model/sampler.hpp"
#include "lxe/model/text_maps.hpp"
#include "lxe/model/text_tree.hpp"
#include "lxe/proc/proc_fs.hpp"
#include "lxe/sys/system.hpp"
#include "lxe/ui/theme.hpp"
#include "lxe/version.hpp"

#include <unistd.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <print>
#include <span>
#include <string_view>

namespace {

constexpr std::string_view usage = R"(Usage: linux-explorer [OPTION]...
Show the running processes as a live tree in a window.

Options:
      --print-tree       print the process tree as text (like pstree) and exit
      --print-maps PID   print the memory maps of process PID ("self" for this one) and exit
      --proc-root DIR    read processes from DIR instead of /proc
      --select PID       open with process PID ("self" for this one) selected
      --select-mapping M also select mapping M of it: a hex start address or a path like "[heap]"
      --theme light|dark colour theme (default: follow the desktop setting)
      --screenshot FILE  open the window, save it to FILE (binary PPM) and exit
      --version          print the version and exit
  -h, --help             show this help and exit
)";

struct Options
{
    bool print_tree = false;
    std::optional<lxe::ProcessId> print_maps;
    std::filesystem::path proc_root = "/proc";
    lxe::app::Options app;
};

int print_maps(const lxe::proc::ProcFs& fs, lxe::ProcessId pid)
{
    const auto maps = lxe::model::load_mappings(fs, pid);
    if (!maps) {
        std::println(stderr, "linux-explorer: cannot read maps of process {}: {}", pid, maps.error().message());
        return 1;
    }
    std::print("{}", lxe::model::render_maps_text(*maps));
    return 0;
}

int print_tree(const lxe::proc::ProcFs& fs)
{
    const auto snapshot = fs.read_snapshot();
    if (!snapshot) {
        std::println(stderr, "linux-explorer: cannot read {}: {}", fs.root().string(), snapshot.error().message());
        return 1;
    }
    lxe::model::Sampler sampler{lxe::sys::page_size()};
    std::print("{}", lxe::model::render_text_tree(sampler.update(*snapshot)));
    return 0;
}

int run(std::span<char* const> args)
{
    Options options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg{args[i]};
        if (arg == "-h" || arg == "--help") {
            std::print("{}", usage);
            return 0;
        }
        if (arg == "--version") {
            std::println("linux-explorer {}", lxe::version);
            return 0;
        }
        if (arg == "--print-tree") {
            options.print_tree = true;
            continue;
        }
        if ((arg == "--print-maps" || arg == "--select") && i + 1 < args.size()) {
            const std::string_view pid{args[++i]};
            const auto parsed = pid == "self" ? std::optional{lxe::ProcessId{::getpid()}} : lxe::proc::parse_pid(pid);
            if (!parsed) {
                std::println(stderr, "linux-explorer: '{}' is not a process id", pid);
                return 2;
            }
            (arg == "--select" ? options.app.selected : options.print_maps) = parsed;
            continue;
        }
        if (arg == "--proc-root" && i + 1 < args.size()) {
            options.proc_root = args[++i];
            continue;
        }
        if (arg == "--screenshot" && i + 1 < args.size()) {
            options.app.screenshot = args[++i];
            continue;
        }
        if (arg == "--select-mapping" && i + 1 < args.size()) {
            options.app.selected_mapping = args[++i];
            continue;
        }
        if (arg == "--theme" && i + 1 < args.size()) {
            const std::string_view theme{args[++i]};
            if (theme == "light") {
                options.app.theme = lxe::ui::Theme::light;
                continue;
            }
            if (theme == "dark") {
                options.app.theme = lxe::ui::Theme::dark;
                continue;
            }
            std::println(stderr, "linux-explorer: unknown theme '{}' (light or dark)", theme);
            return 2;
        }
        std::println(stderr, "linux-explorer: unrecognized argument '{}'", arg);
        std::print(stderr, "{}", usage);
        return 2;
    }

    const lxe::proc::ProcFs fs{options.proc_root};
    if (options.print_maps) {
        return print_maps(fs, *options.print_maps);
    }
    return options.print_tree ? print_tree(fs) : lxe::app::run(fs, options.app);
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        return run(std::span<char* const>{argv, static_cast<std::size_t>(argc)});
    }
    catch (const std::exception& error) {
        std::println(stderr, "linux-explorer: fatal error: {}", error.what());
        return 1;
    }
}
