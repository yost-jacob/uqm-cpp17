cmake_minimum_required(VERSION 3.20)
project(uqm LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The CMake build currently supports Linux. Use the legacy build on other platforms.")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake -S . -B build-cmake -G Ninja")
endif()

# Keep C sources as C; new C++ sources use the full C++17 standard library.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.0)
pkg_check_modules(VORBIS REQUIRED IMPORTED_TARGET vorbisfile)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)

include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)
test_big_endian(UQM_BIG_ENDIAN)
if(UQM_BIG_ENDIAN)
    set(WORDS_BIGENDIAN "#define WORDS_BIGENDIAN 1")
endif()

# Reuse the legacy header template without writing into the source tree.
check_symbol_exists(readdir_r "dirent.h" UQM_HAVE_READDIR_R)
check_symbol_exists(setenv "stdlib.h" UQM_HAVE_SETENV)
check_symbol_exists(strupr "string.h" UQM_HAVE_STRUPR)
check_symbol_exists(strcasecmp "strings.h" UQM_HAVE_STRCASECMP_UQM)
check_symbol_exists(stricmp "string.h" UQM_HAVE_STRICMP)
check_symbol_exists(getopt_long "getopt.h" UQM_HAVE_GETOPT_LONG)
check_symbol_exists(iswgraph "wctype.h" UQM_HAVE_ISWGRAPH)
set(CMAKE_EXTRA_INCLUDE_FILES wchar.h)
check_type_size(wchar_t UQM_WCHAR_T)
check_type_size(wint_t UQM_WINT_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)
check_type_size(_Bool UQM_BOOL)
set(UQM_HAVE_WCHAR_T "${HAVE_UQM_WCHAR_T}")
set(UQM_HAVE_WINT_T "${HAVE_UQM_WINT_T}")
set(UQM_HAVE__BOOL "${HAVE_UQM_BOOL}")
foreach(feature READDIR_R SETENV STRUPR STRCASECMP_UQM STRICMP GETOPT_LONG
        ISWGRAPH WCHAR_T WINT_T _BOOL)
    if(UQM_HAVE_${feature})
        set(HAVE_${feature} "#define HAVE_${feature} 1")
    endif()
endforeach()
if(NOT UQM_HAVE_GETOPT_LONG OR NOT UQM_HAVE_STRCASECMP_UQM)
    message(FATAL_ERROR "This Linux build requires getopt_long and strcasecmp.")
endif()

set(UQM_CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sc2/content" CACHE PATH
    "Default game content directory (can also be overridden with --contentdir)")
set(CONTENTDIR "${UQM_CONTENT_DIR}")
configure_file(sc2/src/config_unix.h.in generated/config_unix.h @ONLY)

include(cmake/Sources.cmake)
add_executable(uqm ${UQM_SOURCES})
target_include_directories(uqm PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" sc2/src)
target_compile_definitions(uqm PRIVATE
    GFXMODULE_SDL SDL_DIR=SDL2 THREADLIB_SDL USE_INTERNAL_MIKMOD
    HAVE_JOYSTICK HAVE_ZIP=1 NETPLAY=NETPLAY_FULL USE_PLATFORM_ACCEL
    $<$<CONFIG:Debug>:DEBUG>)
target_link_libraries(uqm PRIVATE PkgConfig::SDL2 PkgConfig::VORBIS PNG::PNG ZLIB::ZLIB m)

include(CTest)
if(BUILD_TESTING)
    find_package(Boost 1.65 CONFIG REQUIRED COMPONENTS unit_test_framework)
    add_executable(uqm-build-tests tests/build.cpp tests/hash_tables.cpp
        tests/hash_tables_c.c tests/memory_log.cpp tests/stringbank.cpp
        tests/stringbank_c.c tests/string_resources.cpp tests/value_types.cpp
        tests/value_types_c.c tests/packet_queue.cpp tests/alarms.cpp
        sc2/src/libs/callback/alarm.cpp
        sc2/src/uqm/supermelee/netplay/packetq.cpp
        sc2/src/libs/resource/stringbank.cpp sc2/src/libs/resource/loadres.c
        sc2/src/libs/strings/getstr.cpp sc2/src/libs/strings/strings.c
        sc2/src/libs/strings/unicode.c sc2/src/libs/memory/w_memlib.c
        sc2/src/libs/math/random.c sc2/src/libs/uio/charhashtable.cpp
        sc2/src/libs/uio/memdebug.cpp)
    target_include_directories(uqm-build-tests PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated" sc2/src)
    target_link_libraries(uqm-build-tests PRIVATE Boost::unit_test_framework PkgConfig::SDL2)
    target_compile_definitions(uqm-build-tests PRIVATE BOOST_TEST_DYN_LINK NETPLAY=NETPLAY_FULL)
    add_test(NAME uqm-c-cxx-interop COMMAND uqm-build-tests)
    add_test(NAME uqm-version COMMAND uqm --version)
    add_test(NAME uqm-help COMMAND uqm --help)
    set_tests_properties(uqm-version uqm-help PROPERTIES TIMEOUT 10)
endif()
