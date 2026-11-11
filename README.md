# UQM C++17 modernization

The Ur-Quan Masters, with an incremental path to C++17. Gameplay sources
still compile as C; migrated utilities compile as C++17 with the standard library.
The original build remains available in `sc2/`.

## Build on Ubuntu with CMake and Ninja

Install dependencies (Ubuntu 22.04 or newer):

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build pkg-config \
    libsdl2-dev libpng-dev libvorbis-dev zlib1g-dev libboost-test-dev nlohmann-json3-dev
```

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
./build-cmake/debug/uqm
```

Use `release` in place of `debug` for an optimized build. CMake 3.20 or newer
is required. GCC is the supported compiler; existing C files use GNU C99.
The CMake build currently targets Linux and SDL2, using SDL threads, MixSDL
audio, the bundled MikMod decoder, Vorbis, ZIP content, joystick support,
platform acceleration, and IPv4/IPv6 netplay.

The repository includes the game content. The executable uses its absolute
`sc2/content` path by default and can be launched from any working directory.
For another content location, configure with
`-DUQM_CONTENT_DIR=/path/to/content`, or run with
`--contentdir=/path/to/content`. Reconfigure after moving the checkout.
This task provides a development build; installable packages are not yet defined.

Tests include Boost.Test checks that call the existing C API from C++17,
hash-table and memory-debugger checks, string-table loading and ownership,
packet queues, alarm scheduling, and executable version/help smoke checks. To build only the game without
Boost, configure with `-DBUILD_TESTING=OFF`.

In CLion on Linux, open the repository root and select the `debug` or `release`
CMake preset with a GCC toolchain. Build and run the `uqm` target; no custom
working directory is required. On Windows, use an Ubuntu WSL toolchain.

## Legacy build

The legacy build scripts and `Makeinfo` files are preserved:

```sh
cd sc2
./build.sh uqm -j4
./uqm-debug
```

Accept the default configuration for SDL2 and a Debug build. See `sc2/INSTALL`
for other legacy options. The legacy build also requires GCC with C++17 and
the C++ standard library. For an existing legacy build, run
`./build.sh uqm clean` and configure again after this source migration;
old dependency files reference removed headers. CMake writes generated configuration
and object files under `build-cmake/`, separately from the legacy files.
When adding or removing source files, update both `cmake/Sources.cmake` and
the appropriate legacy `Makeinfo` until the legacy build is retired.

For a runtime check, launch the game, navigate the main menu, enter Super-Melee
and start a match, then exit. Audio hardware and interactive gameplay need
manual verification on a desktop; the command-line tests do not cover them.

## Modernization progress

The [runtime encapsulation roadmap](docs/design/encapsulation-roadmap.md) records
current owners, remaining legacy boundaries, and the next steps in the active
exploration → encounter → conversation/combat work. Persistent escorts now belong
to the player's [Fleet](docs/design/fleet-ownership.md); combat crew updates use
stable vessel IDs instead of matching queue order.

Resource string tables (priority 8) use per-entry standard string storage and
typed name lookup, retaining the C API and binary byte lengths.

Math modernization (priorities 9–11) uses `std::sqrt`, `std::cos`, and
`std::atan2`, with constexpr angle conversions available to C++ callers.
Integer roots retain floor semantics. Projected components now round to nearest;
angles still have 64 clockwise steps from up and 16 facings, and the zero-vector
angle sentinel remains 64. Exact cardinal directions are preserved.

The duplicated random generators share one implementation with value-owned
contexts. `std::minstd_rand0` was evaluated and differs from the original stream
at draw 15 for seed 12345. Retaining the small shared unsigned recurrence avoids
changing planet generation, seed restoration, and netplay checksums.

Game queues (priority 12) own fixed-size vectors of their eight concrete C game
object types. Capacity never grows; the 16-encounter gameplay limit, stable
handles, intrusive traversal order, and last-in first-out slot reuse are retained.
Queue macros and the unused unbounded-storage variant are removed. C owners
explicitly initialize/release pools; save-slot previews swap ownership instead of
copying an owning queue. External game resources still need their normal cleanup
before a pool is destroyed. All twelve items in the initial priority list are
implemented.

## Data-driven star map

The star catalogue and its prefix/postfix names are loaded at startup from
[`sc2/content/base/starmap.json`](sc2/content/base/starmap.json). `StarMap` owns a
vector of `StarDesc` objects, each with its own strings. Edit names and restart
without rebuilding. Existing enum values, catalogue identities and saved star
fields are retained. See [the editing and ownership guide](docs/starmap.md).

JSON loading uses the header-only nlohmann/json library (`nlohmann-json3-dev` on
Ubuntu), also required for the legacy build.

Ship balance values are editable in [`sc2/content/base/ships.json`](sc2/content/base/ships.json).
See [ship tuning](docs/ship-tuning.md) for fields, units, validation and module-derived behavior.
