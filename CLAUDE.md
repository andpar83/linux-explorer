# linux-explorer

## Goal

A Linux counterpart of Sysinternals **Process Explorer**. The end state is a live, refreshing,
hierarchical process view (tree by parent PID) with PID, CPU %, memory, owner, command line /
description, colour-coded rows (services, own processes, new / exiting processes, elevated),
sorting, searching, and drill-down into one process: threads, open file descriptors, memory
maps, environment, sockets, cgroup, limits; plus actions on a process (signal, renice, affinity)
with confirmation. A status line shows totals (CPU, memory/commit, process count).

Data comes directly from `/proc`, `/sys` and netlink. No shelling out to `ps`, `top`, `ss`.

**UI toolkit is not decided yet** (candidates: Qt 6 GUI, or a TUI with FTXUI/ncurses). The
data-collection core must stay UI-agnostic (`lxe::proc`, `lxe::sys`, `lxe::model`) so either
can be attached. Ask before choosing the UI toolkit or adding any third-party dependency.

Development is **iterative**: small, self-contained increments, each fully tested, each
committed. Build only what the current step asks for; don't anticipate later steps in code.

## Current state

- Hello-world skeleton: CMake + presets, sanitizer/hardening/warning configuration, CTest smoke tests.
- No application code yet. Next steps (in order, subject to change): a `/proc/<pid>/stat`
  + `status` parser with unit tests, a process-tree model, a periodic sampler with CPU % deltas,
  then the first UI.

## Toolchain

| Component | What we use | Notes |
|-----------|-------------|-------|
| Compiler  | GCC 15.2 (`g++-15`) | Chosen in one place: `LXE_GCC_MAJOR` at the top of `CMakeLists.txt`, used whenever no compiler was given (presets, CLion profiles, bare `cmake -S . -B build`). An explicit `-DCMAKE_CXX_COMPILER` or `CXX` still wins; any other GCC major prints a CMake warning, older is an error. GCC 16 is installed (`g++-16`) but deliberately unused: Ubuntu ships a pre-release trunk snapshot (16.0.1 *experimental*), and CLion's Nova engine flags `std::println` as an error with its library headers. Revisit when a released GCC 16 is packaged and CLion handles it. Clang is not installed (needed later for libFuzzer / CLI clang-tidy). |
| Standard  | **C++23** (`-std=c++23`, no GNU extensions) | Chosen for consistency: compiler, CLion and clangd all fully understand it. Stick to the verified list below; no C++26 features. |
| Build     | CMake >= 3.30, `CMakePresets.json` | `cmake` is **not on PATH** on this machine. CLion's bundled copy works: `/home/andrey/Desktop/clion-2026.2.0.1/bin/cmake/linux/x64/bin/cmake` (Ninja next to it: `.../bin/ninja/linux/x64/ninja`); the path changes with CLion upgrades. Or `sudo apt install cmake ninja-build`. |
| Generator | not pinned in presets | CLion uses its bundled Ninja; the CLI uses the default (Unix Makefiles) unless `CMAKE_GENERATOR=Ninja` is exported. |
| Build dirs | `build/<preset>/` (CLI), `cmake-build-*/` (CLion) | Both git-ignored. Never build in-source. |

### Presets (`cmake --list-presets`)

| Preset | Config | What's on |
|--------|--------|-----------|
| `debug` | Debug | `_GLIBCXX_ASSERTIONS`, full warnings, `-Werror` |
| `asan` | Debug | + ASan, LSan, UBSan (incl. `float-divide-by-zero`, `float-cast-overflow`, `bounds-strict`), `pointer-compare`/`pointer-subtract`, `_GLIBCXX_DEBUG` (checked iterators), `_GLIBCXX_SANITIZE_VECTOR` |
| `tsan` | Debug | + TSan, UBSan |
| `ubsan` | Debug | + UBSan only |
| `analyze` | Debug | + GCC `-fanalyzer` (warnings not fatal: the C++ analyzer still has false positives) |
| `release` / `relwithdebinfo` | Release / RelWithDebInfo | hardening: `_FORTIFY_SOURCE=3`, stack protector, stack-clash protection, CET (`-fcf-protection=full`), `-ftrivial-auto-var-init=zero`, PIE, RELRO + `-z now`, `noexecstack` |

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

Sanitizers available with GCC: address, leak, undefined, thread (MemorySanitizer and HWASan
need Clang and are not available). Sanitizer runtime options (`ASAN_OPTIONS`, `TSAN_OPTIONS`,
`UBSAN_OPTIONS`) are attached to every CTest test via `LXE_SANITIZER_ENV`; when running the
binary by hand or from a CLion run configuration, set them yourself if you need e.g.
`detect_invalid_pointer_pairs`.

Flags live in `cmake/CompilerWarnings.cmake`, `cmake/Sanitizers.cmake`, `cmake/Hardening.cmake`
and are applied through the single `lxe::options` INTERFACE target. Every target links it.
Changing any flag means updating this file.

### Commands

```sh
cmake --workflow --preset asan                # configure + build + test
cmake --workflow --preset tsan
cmake --workflow --preset release
cmake --preset asan && cmake --build --preset asan -j && ctest --preset asan   # step by step
ctest --preset asan -R unit.                  # one category
./build/asan/src/linux-explorer
```

### CLion

The project is opened as a CMake project. CLion creates its own `Debug` profile
(`cmake-build-debug/`, Ninja, no compiler given), which works because `CMakeLists.txt` picks
`g++-15` itself. CLion also reads `CMakePresets.json` and lists each configure preset as a
profile, disabled by default: enable them in *Settings | Build, Execution, Deployment | CMake*
(`asan` should be the everyday profile; it is the only way to get sanitizers inside the IDE).
`.clang-format` and `.clang-tidy` are picked up by CLion's bundled clangd. `.idea/` and
`cmake-build-*/` are git-ignored. Don't add files or settings that only work from the CLI or
only from the IDE. If CLion's CMake output shows the wrong compiler or the GCC-version warning, use
*Tools | CMake | Reset Cache and Reload Project*. An error mark in the editor on code that
builds cleanly is an IDE problem: confirm with the compiler before changing code.

## C++ rules

- **Namespaces**: everything in `lxe`, one sub-namespace per area (`lxe::proc`, `lxe::sys`,
  `lxe::model`, `lxe::ui`). No `using namespace` in headers, never `using namespace std`.
- **Files**: `.hpp` / `.cpp`, `#pragma once`, headers under `src/lxe/...` and included as
  `"lxe/proc/stat.hpp"`. One class or one cohesive set of functions per file. Modules are
  planned once GCC + CMake + CLion handle them smoothly; don't convert yet.
- **Output / formatting**: `std::print`, `std::println`, `std::format`. Never `<iostream>`,
  `printf`, or string concatenation to build text.
- **Errors**: `std::expected<T, Error>` for anything that can fail at runtime (I/O, parsing,
  permissions). Exceptions only for programmer errors / unrecoverable states. `noexcept` on
  everything that cannot throw. Results are `[[nodiscard]]`; never ignore one.
- **Ownership**: no `new`/`delete`/`malloc`, no owning raw pointers. `std::unique_ptr`,
  containers, values. OS handles wrapped in RAII (`lxe::sys::unique_fd`), never a bare `int fd`
  that outlives one expression.
- **Views over copies**: `std::string_view`, `std::span`, ranges (`std::ranges::`,
  `std::views::`, `std::ranges::to`). A raw loop only where the algorithm would be less clear.
- **Types**: `enum class` always; strong types for identifiers (`ProcessId`, `ThreadId`,
  `UserId`) instead of bare integers; `std::chrono` for time; `std::filesystem::path` for
  paths; fixed-width / `std::size_t` integers; no implicit narrowing (`-Wconversion` is an error;
  use explicit `static_cast` with a reason or a checked conversion helper).
- **Compile time**: `constexpr` / `consteval` / `static_assert` whenever possible; `const` by
  default; `auto` where the type is obvious or deduced; structured bindings; designated
  initialisers; `std::optional` for absent values (never sentinels); `<=>` for ordering.
- **Concurrency**: `std::jthread` + `std::stop_token`, `std::atomic`, `std::mutex` with
  `std::scoped_lock`. No raw `pthread`. Every concurrent component gets a test that runs under the
  `tsan` preset.
- **Forbidden**: macros (other than `#pragma once` and CMake-injected config), `goto`, C-style
  casts, `reinterpret_cast` where `std::bit_cast` works, `NULL`, `typedef`, `volatile` for
  synchronisation, global mutable state, `std::endl`.
- **Linux interface**: all `/proc` / `/sys` / syscall access is isolated in `lxe::sys` and
  `lxe::proc`; `errno` becomes `std::error_code` inside an `std::expected`; **all `/proc`
  content is untrusted input** (hostile process names, kernel version differences, races with
  exiting processes) and parsers must never crash or UB on it.
- **Privileges**: never require root. Features that need privileges degrade gracefully and
  are testable unprivileged by injecting the data source.
- **Naming** (enforced by `.clang-tidy`): types, concepts, template parameters `PascalCase`;
  functions, variables, namespaces, files, constants `snake_case`; private/protected members
  `snake_case_`. No Hungarian notation, no abbreviations that need explaining.
- **Formatting**: `.clang-format` is the law (4 spaces, 120 columns, braces on their own line
  for functions/types, attached for control flow). Run it before committing.
- **Comments**: explain *why*, not *what*. `///` Doxygen on public API. No commented-out code.

## Testing policy: every kind of test, always

Every change ships with tests. Test names are `<category>.<suite>.<case>` so a category can be
selected with `ctest -R '^unit\.'`. All categories run under every sanitizer preset.

| Category | What | How |
|----------|------|-----|
| `unit` | One function/class in isolation: `/proc` file parsers fed fixture strings, formatting, model logic, strong types. | Catch2 v3 (to be added via `FetchContent` in the next step). Until then, CTest. |
| `integration` | Several components against real but controlled input: a recorded `/proc` tree under `tests/fixtures/`, or the test process inspecting *itself* (own PID, threads, fds are deterministic). | Catch2 + fixture files. |
| `e2e` | The real `linux-explorer` binary run with arguments/env; assert exit code and output. | `lxe_add_test()` in `tests/CMakeLists.txt` (exists today). |
| `property` | Generated inputs for parsers and formatters: round-trips, invariants, no crash on arbitrary bytes. | Catch2 `GENERATE` now; rapidcheck when it earns its place. |
| `fuzz` | A libFuzzer harness for every parser that consumes untrusted bytes. Harnesses must also compile under GCC as plain tests that replay `tests/fuzz/corpus/*`. | Clang/libFuzzer when available. |
| `bench` | Micro-benchmarks for hot paths (full `/proc` scan, tree rebuild). Must build on every preset; excluded from the default `ctest` run via the `bench` label. | Catch2 `BENCHMARK` or nanobench. |
| sanitizers | Not a category: it's *how* the suite runs. `asan` and `tsan` are mandatory before every commit. | Presets. |
| static analysis | `-Werror` warning set, `.clang-tidy`, `-fanalyzer` (`analyze` preset). | CLion inspections / CLI. |

Rules:

- A bug fix comes with a regression test that fails before the fix and passes after.
- Tests are deterministic: no sleeps for synchronisation, no dependence on other processes on
  the machine, no root, no network. Time and the process table are injected, not global.
- Any sanitizer report in a test's output fails the test even with exit code 0
  (`FAIL_REGULAR_EXPRESSION` in `lxe_add_test`); keep that behaviour in new helpers.
- Never skip, disable, weaken or `-Wno-` anything to get green. Fix the code, or stop and ask.
- Test code follows the same C++ rules as production code.

## Definition of done for every change

1. `cmake --workflow --preset asan` passes.
2. `cmake --workflow --preset tsan` passes.
3. `cmake --workflow --preset release` passes (hardened, `-Werror`).
4. New behaviour has tests in the matching categories; `CLAUDE.md` is updated if rules,
   toolchain, layout or roadmap changed.
5. Commit.

## Repository layout

```
CMakeLists.txt        project root: language level, options, lxe::options target
CMakePresets.json     all configure/build/test/workflow presets
cmake/                CompilerWarnings.cmake, Sanitizers.cmake, Hardening.cmake
src/                  application code (src/main.cpp; libraries under src/lxe/<area>/)
tests/                tests, grouped by category; fixtures under tests/fixtures/
CLAUDE.md             this file; README.md: short public overview
```

## Git

- Commit messages: imperative subject <= 72 chars, blank line, body explaining *why*.
- One logical change per commit; every commit builds and passes the definition of done.
- Never commit build output, IDE state, or secrets. `.gitignore` covers `build/`,
  `cmake-build-*/`, `.idea/`.

## Ask before

- Choosing the UI toolkit or adding any dependency.
- Changing compiler flags, sanitizer sets or the C++ standard.
- Anything that needs root, installs packages, or touches processes other than the test's own.
