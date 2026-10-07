# linux-explorer

A Linux counterpart of Sysinternals **Process Explorer**: a live, hierarchical view of every
process on the machine with CPU, memory, owner and command line, and (eventually) drill-down
into threads, file descriptors, memory maps, sockets and environment. Data comes straight from
`/proc`; the UI is Dear ImGui on SDL3 + OpenGL.

Status: early. Today it shows the process tree with PID, user, CPU %, memory, threads, state
and command line, refreshing every second, and the memory maps of the selected process in a
lower pane, in a light or dark theme that follows the desktop. See `CLAUDE.md` for goals, rules
and roadmap.

## Build

Requirements: GCC 15 (`g++-15`, selected automatically), C++23, CMake >= 3.30, SDL3.

```sh
sudo apt install libsdl3-dev fonts-font-awesome   # fonts-font-awesome is optional (toolbar icons)
cmake --workflow --preset release   # configure + build + test
./build/release/src/linux-explorer
```

Other presets: `asan`, `tsan`, `ubsan`, `debug`, `analyze` (`cmake --list-presets`). If
`cmake` isn't on PATH, CLion's bundled copy works (see `CLAUDE.md`), or
`sudo apt install cmake ninja-build`.

## Usage

```
linux-explorer                       the window
linux-explorer --print-tree          process tree as text, like pstree
linux-explorer --print-maps PID      memory maps of a process as text, like pmap ("self" works)
linux-explorer --select PID          open with that process selected
linux-explorer --theme dark          force a theme (default: follow the desktop)
linux-explorer --screenshot x.ppm    render the window to a file and exit
linux-explorer --proc-root DIR       read a recorded /proc tree instead of the live one
```

In the window: click a process to see its memory maps in the lower pane (**Ctrl+M** shows or
hides it, drag the divider to resize). **Space** pauses/resumes live updates, **F5** refreshes,
arrows and double-click expand/collapse nodes, columns can be resized, reordered and hidden
(right-click the header; the maps pane has end address, device and inode columns hidden by
default). Column layout is remembered in `~/.local/share/linux-explorer/`.

## CLion

Open the project directory. CLion's default `Debug` profile works out of the box. CLion also
picks up `CMakePresets.json`; enable the presets you want in *Settings | Build, Execution,
Deployment | CMake* (at least `asan`, which is how you get sanitizers inside the IDE).
`.clang-format` and `.clang-tidy` are applied by the IDE automatically.
