# UQM C++17 modernization

The Ur-Quan Masters, with an incremental path to C++17. Existing game sources
still compile as C; C++ sources compile as C++17 with the standard library.
The original build remains available in `sc2/`.

## Build on Ubuntu with CMake and Ninja

Install dependencies (Ubuntu 22.04 or newer):

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build pkg-config \
    libsdl2-dev libpng-dev libvorbis-dev zlib1g-dev libboost-test-dev
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
plus executable version/help smoke checks. To build only the game without
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
for other legacy options and platforms. CMake writes generated configuration
and object files under `build-cmake/`, separately from the legacy files.
When adding or removing source files, update both `cmake/Sources.cmake` and
the appropriate legacy `Makeinfo` until the legacy build is retired.

For a runtime check, launch the game, navigate the main menu, enter Super-Melee
and start a match, then exit. Audio hardware and interactive gameplay need
manual verification on a desktop; the command-line tests do not cover them.
