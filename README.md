# linux-explorer

A Linux counterpart of Sysinternals **Process Explorer**: a live, hierarchical view of every
process on the machine with CPU, memory, owner and description, and drill-down into threads,
file descriptors, memory maps, sockets and environment. Data comes straight from `/proc`
and `/sys`.

Status: early skeleton (hello world + build system). See `CLAUDE.md` for goals, rules and roadmap.

## Build

Requirements: GCC 15 (`g++-15`, selected automatically), C++23, CMake >= 3.30, optionally Ninja.

```sh
cmake --workflow --preset asan      # configure + build + test with ASan/LSan/UBSan
cmake --workflow --preset tsan      # ... with ThreadSanitizer
cmake --workflow --preset release   # hardened optimised build + tests
cmake --list-presets                # all presets
./build/asan/src/linux-explorer
```

If `cmake` isn't on PATH, CLion's bundled copy works (see `CLAUDE.md`), or
`sudo apt install cmake ninja-build`.

## CLion

Open the project directory. CLion's default `Debug` profile works out of the box. CLion also
picks up `CMakePresets.json`; enable the presets you want in *Settings | Build, Execution,
Deployment | CMake* (at least `asan`, which is how you get sanitizers inside the IDE).
`.clang-format` and `.clang-tidy` are applied by the IDE automatically.
