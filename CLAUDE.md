# linux-explorer

## Goal

A Linux counterpart of Sysinternals **Process Explorer**. The end state is a live, refreshing,
hierarchical process view (tree by parent PID) with PID, CPU %, memory, owner, command line /
description, colour-coded rows (services, own processes, new / exiting processes, elevated),
sorting, searching, and drill-down into one process: threads, open file descriptors, memory
maps, environment, sockets, cgroup, limits; plus actions on a process (signal, renice, affinity)
with confirmation. A status line shows totals (CPU, memory, process count).

Data comes directly from `/proc`, `/sys` and netlink. No shelling out to `ps`, `top`, `ss`.
Linux only: no portability layers, no abstractions for other operating systems.

The data-collection core (`lxe::sys`, `lxe::proc`, `lxe::model`) stays UI-agnostic; the UI
(`lxe::ui`, `lxe::app`) only renders models. Ask before adding any third-party dependency.

Development is **iterative**: small, self-contained increments, each fully tested, each
committed. Build only what the current step asks for; don't anticipate later steps in code.

## Current state (2026-10-06)

Done: a `pstree`-style live tree in a window. Columns: process, PID, user, CPU %, memory (RSS),
threads, state, command line; resizable/reorderable/hideable columns; expand/collapse; pause;
refresh every second only while unpaused; own processes tinted; kernel threads dimmed; CPU
heat-map cell; tooltip with the full command line; light/dark theme following the desktop;
status bar with CPU %, memory, process and thread counts.

A lower pane (Ctrl+M, draggable splitter) shows the selected process's memory maps from
`/proc/<pid>/smaps` (falling back to `maps` when a kernel has no smaps): address, size, Rss,
permissions, offset, path, with end address, Pss, dirty, swap, anonymous, device and inode as
hidden-by-default columns. Clicking a mapping shows its pages on the right: a presence strip
drawn from `/proc/<pid>/pagemap` (resident / swapped / not present per page, sampled for
mappings over 1 GiB), counts of exclusive, file-or-shared and soft-dirty pages, and the smaps
accounting (Rss, Pss, shared/private clean/dirty, anonymous, huge pages, swap, locked,
referenced, THP eligibility, VmFlags with a tooltip explaining each code). Another user's
process gives a clear permission message: the kernel only lets you read your own processes'
memory without CAP_SYS_PTRACE. Physical page flags (`/proc/kpageflags`, the kernel's
`page-types` tool) are root-only and not read yet.

CLI: `--print-tree` (text tree), `--print-maps PID|self` (text table with Rss),
`--select PID|self`, `--select-mapping ADDR|PATH`, `--proc-root DIR` (fixtures),
`--theme light|dark`, `--screenshot FILE`.

Next steps (in order, subject to change): physical page flags from `/proc/kpageflags` when
running as root; full executable names where the kernel truncated the 15-byte name;
search/filter box; sort by column; more detail tabs for the selected process (threads, open
files, environment); CPU/memory history graphs (ImPlot); kill/renice with confirmation;
per-process icons; services/new/exiting colouring.

## UI stack: Dear ImGui + SDL3 + OpenGL 3

Chosen 2026-10-06 over Qt (too big, its own object model) and Skia (a renderer, not a toolkit).
ImGui is immediate mode: `ui::draw_main_window()` rebuilds the whole UI every frame from the
`model::Model`; the only state that survives a frame is `ui::ViewState`. Custom components are
plain functions. What to know when touching UI code:

- **Render loop** (`app.cpp`): render continuously only for a short while after input; otherwise
  sleep until the next refresh or an event. Idle CPU use must stay near zero.
- **Fonts**: ImGui 1.92 font API (`AddFontFromFileTTF` without a baked size, `style.FontSizeBase`,
  `style.FontScaleDpi`). The desktop UI font is used (Ubuntu Sans, then Noto, then DejaVu);
  Font Awesome 4 (`fonts-font-awesome`) is merged in for toolbar icons when installed.
- **Theme** (`theme.cpp`): one place for colours and spacing; light and dark palettes; meaning-
  carrying colours live in `ui::Palette`, not inline in views.
- **ImGui limits**: tree nesting deeper than 31 levels is undefined behaviour inside ImGui
  (32-bit depth mask), so `process_view.cpp` caps nesting at 24 and flattens the rest. ImGui
  1.92.9 also computes `1 << (depth - 1)` for a clipped root node, so rows are drawn inside an
  indent-neutral dummy tree level. Keep both workarounds until a fixed ImGui release is pinned.
- **Third-party C-isms stay out of our code**: no `IM_COL32`, `IM_ASSERT`, `ImVector` in
  `lxe::` code; use the `rgba()` helper and standard containers. ImGui/SDL headers are SYSTEM
  includes so their warnings don't apply to us.
- **Headless tests**: `tests/support/headless_imgui.hpp` runs real view code through ImGui's
  null backend (no window, no GPU). Set a small display size to exercise clipping.
- **Long flat lists** go through `ImGuiListClipper` (only visible rows are submitted); trees
  can't be clipped that way, they use the nesting cap above.
- **Panes**: the lower pane is a child window of fixed height so the status bar never moves;
  the splitter is an `InvisibleButton` whose drag delta changes `ViewState::details_height`.
- **Details loading** (`app.cpp`): `model::load_details` runs on every refresh and immediately
  when the selection changes; nothing is read while the pane is hidden. smaps costs the kernel
  a page-table walk (about 50 ms for 8000 mappings); pagemap reads are capped per mapping
  (`plan_page_sample`), so a terabyte-sized virtual mapping still costs a bounded 2 MiB read.
- **Custom drawing**: the presence strip (`draw_presence_strip`) is the pattern for graphics:
  an `InvisibleButton` reserves the space and handles hover, the window draw list paints it,
  one column per pixel aggregating the model's buckets.
- **Looking at it**: `linux-explorer --screenshot /tmp/x.ppm [--theme dark]` renders a few
  frames to a PPM and exits. Convert with Python PIL if a PNG is needed.
- Settings (column widths/order) persist in `~/.local/share/linux-explorer/imgui.ini`.

## Toolchain

| Component | What we use | Notes |
|-----------|-------------|-------|
| Compiler  | GCC 15.2 (`g++-15`) | Chosen in one place: `LXE_GCC_MAJOR` at the top of `CMakeLists.txt`, used whenever no compiler was given (presets, CLion profiles, bare `cmake -S . -B build`). An explicit `-DCMAKE_CXX_COMPILER` or `CXX` still wins; any other GCC major prints a CMake warning, older is an error. GCC 16 is installed (`g++-16`) but deliberately unused: Ubuntu ships a pre-release trunk snapshot (16.0.1 *experimental*), and CLion's Nova engine flags `std::println` as an error with its library headers. Revisit when a released GCC 16 is packaged and CLion handles it. Clang is not installed (needed later for libFuzzer / CLI clang-tidy). |
| Standard  | **C++23** (`-std=c++23`, no GNU extensions) | Chosen for consistency: compiler, CLion and clangd all fully understand it. Stick to the verified list below; no C++26 features. |
| Build     | CMake >= 3.30, `CMakePresets.json` | `cmake` is **not on PATH** on this machine. CLion's bundled copy works: `/home/andrey/Desktop/clion-2026.2.0.1/bin/cmake/linux/x64/bin/cmake` (Ninja next to it: `.../bin/ninja/linux/x64/ninja`); the path changes with CLion upgrades. Or `sudo apt install cmake ninja-build`. |
| Generator | not pinned in presets | CLion uses its bundled Ninja; the CLI uses the default (Unix Makefiles) unless `CMAKE_GENERATOR=Ninja` is exported. |
| SDL3      | system package: `sudo apt install libsdl3-dev` | Window, input, GL context, system theme. `find_package(SDL3 CONFIG)`; a copy elsewhere is found with `SDL3_ROOT=<prefix>`. |
| Dear ImGui | 1.92.9b, `FetchContent` with SHA256 (`cmake/Dependencies.cmake`) | Built here as `imgui::imgui`, `imgui::sdl3_opengl3` (app), `imgui::null` (tests). Bump URL and hash together. |
| Catch2    | 3.16.0, `FetchContent` with SHA256 (`tests/CMakeLists.txt`) | Unit/integration/property/UI tests and benchmarks. |
| Fonts/icons | runtime, optional | Ubuntu Sans / Noto / DejaVu for text; `fonts-font-awesome` for toolbar icons. Missing fonts fall back to ImGui's built-in font and text-only buttons. |
| Build dirs | `build/<preset>/` (CLI), `cmake-build-*/` (CLion) | Both git-ignored. Never build in-source. |

### Presets (`cmake --list-presets`)

| Preset | Config | What's on |
|--------|--------|-----------|
| `debug` | Debug | `_GLIBCXX_ASSERTIONS`, full warnings, `-Werror` |
| `asan` | Debug | + ASan, LSan, UBSan (incl. `float-divide-by-zero`, `float-cast-overflow`, `bounds-strict`), `_GLIBCXX_DEBUG` (checked iterators), `_GLIBCXX_SANITIZE_VECTOR`. Not `pointer-compare`/`pointer-subtract`: their check and the vector annotations contradict each other (see `cmake/Sanitizers.cmake`). |
| `tsan` | Debug | + TSan, UBSan |
| `ubsan` | Debug | + UBSan only |
| `analyze` | Debug | + GCC `-fanalyzer` on `src/` targets only (`lxe::analyzer`, linked PRIVATE; in tests it drowns in Catch2 macros). Warnings are not fatal. Known false positives, ignore: two `leak of ... _Hash_node ... allocate` reports from `std::unordered_map` in `Sampler` (the analyzer doesn't follow libstdc++ hashtable ownership). Anything else is worth a look. |
| `release` / `relwithdebinfo` | Release / RelWithDebInfo | hardening: `_FORTIFY_SOURCE=3`, stack protector, stack-clash protection, CET (`-fcf-protection=full`), `-ftrivial-auto-var-init=zero`, PIE, RELRO + `-z now`, `noexecstack` |

Sanitizers and hardening are applied **build-wide** (third-party code included); warnings and the
analyzer only to our targets through `lxe::options`. Sanitizer runtime options and LSan/TSan
suppressions are compiled into every executable (`src/sanitizer_defaults.cpp`), so they apply
under ctest, in CLion run configurations and on the command line alike; environment variables
still override them. The suppressions cover desktop libraries loaded for the window (GTK via
libdecor, fontconfig, Pango, D-Bus, GPU driver): they leak at exit and aren't instrumented.
A report with one of our frames and none of theirs is still shown.

The warning set is in `cmake/CompilerWarnings.cmake`. One exclusion: `-Wno-missing-field-initializers`,
because GCC fires it on designated initializers that rely on default member initializers,
which is our idiom. Changing any flag means updating this file.

### C++23 in GCC 15: what's available (verified with feature-test macros, 2026-10-06)

Use these freely; prefer them over older idioms.

- **Language**: deducing `this` (explicit object parameter), `if consteval`, multidimensional
  `operator[]`, `auto(x)` decay-copy, `static operator()`, `uz`/`z` literal suffixes,
  `\N{...}` named character escapes, implicit move on return, relaxed `constexpr`, `[[assume]]`.
- **Output and text**: `std::print`, `std::println`, `std::format` (incl. formatting of ranges
  and tuples), `std::string::contains`, `resize_and_overwrite`, `std::spanstream`.
- **Error handling and vocabulary**: `std::expected` (incl. monadic `and_then`/`transform`/
  `or_else`), monadic `std::optional`, `std::unreachable`, `std::to_underlying`, `std::byteswap`,
  `std::move_only_function`, `std::bind_back`, `std::invoke_r`, `std::forward_like`,
  `std::out_ptr`/`inout_ptr` (wrapping C APIs that return handles through a pointer).
- **Containers**: `std::flat_map`, `std::flat_set`, construction/insertion from ranges
  (`std::from_range`, `insert_range`, `append_range`).
- **Ranges**: `std::ranges::to`, `std::generator`, `views::zip`, `zip_transform`, `adjacent`,
  `enumerate`, `chunk`, `chunk_by`, `slide`, `stride`, `join_with`, `cartesian_product`,
  `repeat`, `as_const`, `as_rvalue`; algorithms `ranges::fold_left`/`fold_right`, `contains`,
  `find_last`, `iota`.
- **Diagnostics**: `<stacktrace>` works but needs linking `stdc++exp`; add it to the target
  that uses it, not globally.

Not available in GCC 15, don't use: `std::mdspan` (no header in GCC 15), `ranges::starts_with`/
`ends_with` (use `std::string_view::starts_with` or `ranges::mismatch`), `std::start_lifetime_as`,
`constexpr` `<cmath>`. C++20 basics (`std::jthread`, `std::span`, `std::bit_cast`,
`std::source_location`, concepts, coroutines, `<=>`, `std::chrono` calendars) are all there.
C++20 modules and `import std` are not used.

### Commands

```sh
sudo apt install libsdl3-dev fonts-font-awesome   # once
cmake --workflow --preset asan                     # configure + build + test
cmake --workflow --preset tsan
cmake --workflow --preset release
cmake --preset asan && cmake --build --preset asan -j && ctest --preset asan   # step by step
ctest --preset asan -L unit                        # one category: unit, integration, property, ui, fuzz, e2e
ctest --preset release -L bench                    # benchmarks (excluded from the default run)
./build/asan/src/linux-explorer                    # the window, under sanitizers
./build/release/src/linux-explorer --print-tree    # text tree
./build/release/src/linux-explorer --print-maps self
./build/release/src/linux-explorer --select self --select-mapping '[heap]' --screenshot /tmp/lxe.ppm --theme dark
```

### CLion

The project is opened as a CMake project. CLion creates its own `Debug` profile
(`cmake-build-debug/`, Ninja, no compiler given), which works because `CMakeLists.txt` picks
`g++-15` itself. CLion also reads `CMakePresets.json` and lists each configure preset as a
profile, disabled by default: enable them in *Settings | Build, Execution, Deployment | CMake*
(`asan` should be the everyday profile; it is the only way to get sanitizers inside the IDE).
CLion uses its Nova (ReSharper C++) engine for highlighting and its bundled clangd for
`.clang-tidy`; `.clang-format` is applied by the formatter. `.idea/` and `cmake-build-*/` are
git-ignored. Don't add files or settings that only work from the CLI or only from the IDE.
If CLion's CMake output shows the wrong compiler or the GCC-version warning, use
*Tools | CMake | Reset Cache and Reload Project*. An error mark in the editor on code that
builds cleanly is an IDE problem: confirm with the compiler before changing code.

## C++ rules

- **Namespaces**: everything in `lxe`, one sub-namespace per area (`lxe::sys`, `lxe::proc`,
  `lxe::model`, `lxe::ui`, `lxe::app`, `lxe::util`). No `using namespace` in headers, never
  `using namespace std`.
- **Files**: `.hpp` / `.cpp`, `#pragma once`, headers under `src/lxe/...` and included as
  `"lxe/proc/parse.hpp"`. One class or one cohesive set of functions per file. Modules are
  planned once GCC + CMake + CLion handle them smoothly; don't convert yet.
- **Output / formatting**: `std::print`, `std::println`, `std::format`. Never `<iostream>`,
  `printf`, or string concatenation to build text. ImGui's `Text("%s", s)` style is confined
  to `lxe::ui` and wrapped (`text()` helper takes a `string_view`).
- **Errors**: `std::expected<T, std::error_code>` for anything that can fail at runtime (I/O,
  parsing, permissions); own error enums get an `std::error_category` (see `proc::ParseError`).
  Exceptions only for programmer errors / unrecoverable states. `noexcept` on everything that
  cannot throw. Results are `[[nodiscard]]`; never ignore one (`std::ignore =` when the value is
  deliberately unused, with a reason nearby).
- **Ownership**: no `new`/`delete`/`malloc`, no owning raw pointers. `std::unique_ptr` (with a
  deleter for C handles, see `app.cpp`), containers, values. OS handles wrapped in RAII
  (`sys::UniqueFd`), never a bare `int fd` that outlives one expression. Paired C init/shutdown
  calls use `util::ScopeExit`.
- **Views over copies**: `std::string_view`, `std::span`, ranges (`std::ranges::`,
  `std::views::`, `std::ranges::to`). A raw loop only where the algorithm would be less clear.
- **Types**: `enum class` always; strong types for identifiers (`ProcessId`, `UserId` in
  `lxe/ids.hpp`) instead of bare integers; `std::chrono` for time; `std::filesystem::path` for
  paths; fixed-width / `std::size_t` integers; no implicit narrowing (`-Wconversion` is an error;
  use explicit `static_cast` with a reason or a checked conversion helper).
- **Compile time**: `constexpr` / `consteval` / `static_assert` whenever possible; `const` by
  default; `auto` where the type is obvious or deduced; structured bindings; designated
  initialisers (members may be omitted when their default is meant); `std::optional` for
  absent values (never sentinels); `<=>` / defaulted `==` for comparisons.
- **Recursion**: none over data whose depth the machine controls (process trees, directory
  trees). Use an explicit stack; tests feed chains thousands deep.
- **Concurrency**: `std::jthread` + `std::stop_token`, `std::atomic`, `std::mutex` with
  `std::scoped_lock`. No raw `pthread`. Every concurrent component gets a test that runs under the
  `tsan` preset. The UI is single-threaded; data collection may move to a worker thread later.
- **Forbidden**: macros (other than `#pragma once`, CMake-injected config and the one in
  `sanitizer_defaults.cpp`), `goto`, C-style casts, `reinterpret_cast` where `std::bit_cast`
  works, `NULL`, `typedef`, `volatile` for synchronisation, global mutable state, `std::endl`.
- **Linux interface**: all `/proc` / `/sys` / syscall access is isolated in `lxe::sys` and
  `lxe::proc`; `errno` becomes `std::error_code` inside an `std::expected`; **all `/proc`
  content is untrusted input** (hostile process names, kernel version differences, races with
  exiting processes, parent cycles) and parsers must never crash or UB on it. Reads are capped
  (`sys::read_file` limits).
- **Privileges**: never require root. Features that need privileges degrade gracefully and
  are testable unprivileged by injecting the data source (`proc::ProcFs` takes a root path,
  `sys::UserNames` takes a lookup function).
- **Naming** (enforced by `.clang-tidy`): types, concepts, template parameters `PascalCase`;
  functions, variables, namespaces, files, constants `snake_case`; private/protected members
  `snake_case_`. No Hungarian notation, no abbreviations that need explaining.
- **Formatting**: `.clang-format` is the law (4 spaces, 120 columns, braces on their own line
  for functions/types, attached for control flow). Run it before committing.
- **Comments**: explain *why*, not *what*. `///` Doxygen on public API. No commented-out code.

## Testing policy: every kind of test, always

Every change ships with tests. Tests are named `<category>.<suite>.<case>` and labelled with
their category, so one category runs with `ctest -L unit`. All categories run under every
sanitizer preset. Helpers live in `tests/support/`; fixtures in `tests/fixtures/` (a recorded
`/proc` tree with a hostile command name, a zombie, a kernel thread and a vanished process).

| Category | What | How |
|----------|------|-----|
| `unit` | One function/class in isolation: `/proc` parsers fed strings, tree building, CPU sampling arithmetic, formatting, strong types. | Catch2 (`tests/unit/`), registered via `lxe_add_catch_tests`. |
| `integration` | Several components against real but controlled input: the fixture `/proc` tree, the live `/proc` of the test process itself (own PID, threads, fds are deterministic), real files. | Catch2 (`tests/integration/`). |
| `property` | Generated inputs: round trips of valid `/proc` content, random parent links, arbitrary bytes into every parser. Seeded by Catch2 (`--rng-seed` reproduces). | Catch2 (`tests/property/`). |
| `ui` | The real view code driven headlessly through ImGui's null backend: themes, shortcuts, expand/collapse, empty/deep/clipped trees, the details pane and pages panel in every state. | Catch2 + `tests/support/headless_imgui.hpp` (`tests/ui/`). |
| `fuzz` | A libFuzzer harness per untrusted-input parser (`tests/fuzz/*_fuzz.cpp`). Without Clang it is linked to `replay_main.cpp` and replays `tests/fuzz/corpus/*` as a test; add a corpus file for every crash or odd input found. | `fuzz_proc_parsers` target. |
| `e2e` | The real `linux-explorer` binary: CLI options, text tree of the fixture, text tree of the live system. | `lxe_add_test()` in `tests/CMakeLists.txt`. |
| `bench` | Micro-benchmarks of hot paths (`/proc` scan, `parse_stat`, sampling, text tree). Built everywhere, run only with `-L bench`. | Catch2 `BENCHMARK` (`tests/bench/`). |
| sanitizers | Not a category: it's *how* the suite runs. `asan` and `tsan` are mandatory before every commit; also run the window itself under `asan` (`--screenshot`) when UI or app code changed. | Presets. |
| static analysis | `-Werror` warning set, `.clang-tidy`, `-fanalyzer` (`analyze` preset). | CLion inspections / CLI. |

Rules:

- A bug fix comes with a regression test that fails before the fix and passes after.
- Tests are deterministic: no sleeps for synchronisation, no dependence on other processes on
  the machine, no root, no network, no display. Time and the process table are injected.
- Any sanitizer report in a test's output fails the test even with exit code 0
  (`FAIL_REGULAR_EXPRESSION`); keep that behaviour in new helpers.
- Never skip, disable, weaken or `-Wno-` anything to get green. Fix the code, or stop and ask.
  Suppressions for third-party code are the one exception and live in `sanitizer_defaults.cpp`
  with a reason.
- Test code follows the same C++ rules as production code.

## Definition of done for every change

1. `cmake --workflow --preset asan` passes.
2. `cmake --workflow --preset tsan` passes.
3. `cmake --workflow --preset release` passes (hardened, `-Werror`).
4. UI or app code changed: run `./build/asan/src/linux-explorer --screenshot /tmp/lxe.ppm`
   (exit 0, no sanitizer output) and look at the picture in both themes. For the `tsan` build
   use Mesa's software renderer: `__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
   LIBGL_ALWAYS_SOFTWARE=1 ./build/tsan/src/linux-explorer --screenshot /tmp/lxe.ppm`. The
   NVIDIA driver creates threads TSan doesn't track and the process dies inside TSan's runtime.
5. New behaviour has tests in the matching categories; `CLAUDE.md` is updated if rules,
   toolchain, layout or roadmap changed.
6. Commit.

## Repository layout

```
CMakeLists.txt          root: compiler choice, language level, build-wide instrumentation, lxe::options
CMakePresets.json       configure/build/test/workflow presets
cmake/                  CompilerWarnings, Sanitizers, Hardening, Dependencies (SDL3, ImGui)
src/main.cpp            CLI entry point and option parsing
src/sanitizer_defaults.cpp   sanitizer runtime options + suppressions, compiled into sanitized builds
src/lxe/ids.hpp         ProcessId, UserId
src/lxe/sys/            Linux primitives: UniqueFd, read_file, users, page size
src/lxe/proc/           /proc parsers (parse.*) and the ProcFs reader (proc_fs.*)
src/lxe/model/          ProcessEntry/ProcessTree/Model, Sampler (CPU deltas), ProcessDetails (maps, pages), formatting, text output
src/lxe/ui/             theme (palettes, style) and process_view (the main window)
src/lxe/app/            SDL3 window, OpenGL context, render loop, fonts, screenshot
src/lxe/util/           ScopeExit
tests/                  support/, unit/, integration/, property/, ui/, fuzz/ (+corpus/), bench/, fixtures/
CLAUDE.md               this file; README.md: short public overview
```

## Git

- Commit messages: imperative subject <= 72 chars, blank line, body explaining *why*.
- One logical change per commit; every commit builds and passes the definition of done.
- Never commit build output, IDE state, or secrets. `.gitignore` covers `build/`,
  `cmake-build-*/`, `.idea/`.

## Ask before

- Adding any dependency, or bumping a pinned one.
- Changing compiler flags, sanitizer sets or the C++ standard.
- Anything that needs root, installs packages, or touches processes other than the test's own.
