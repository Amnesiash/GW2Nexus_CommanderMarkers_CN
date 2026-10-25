# MinGW toolchain file for building on Windows with the WinLibs
# (MinGW-W64 UCRT / POSIX / SEH) distribution installed via winget.
#
# The repo's own cmake/mingw-w64-toolchain.cmake targets the Linux
# cross-compiler prefix `x86_64-w64-mingw32-*` and a MSYS2 root path. This
# native Windows build uses the unprefixed compiler names inside the WinLibs
# `mingw64\bin` directory, so it lives beside the repo and is passed explicitly:
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/winlibs-native-toolchain.cmake
#
# Override the compiler location with -DMINGW_BIN=<path-to-mingw64\bin> when the
# toolchain is not in the default winget package directory.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT DEFINED MINGW_BIN)
    set(MINGW_BIN "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
        CACHE PATH "Directory holding the MinGW-W64 executables")
endif()

file(TO_CMAKE_PATH "${MINGW_BIN}" MINGW_BIN)

# WinLibs ships unprefixed names (gcc.exe / g++.exe / windres.exe) plus the
# prefixed aliases; the unprefixed pair is what actually exists in mingw64\bin.
find_program(MINGW_C_COMPILER NAMES gcc x86_64-w64-mingw32-gcc HINTS "${MINGW_BIN}" REQUIRED)
find_program(MINGW_CXX_COMPILER NAMES g++ x86_64-w64-mingw32-g++ HINTS "${MINGW_BIN}" REQUIRED)
find_program(MINGW_RC_COMPILER NAMES windres x86_64-w64-mingw32-windres HINTS "${MINGW_BIN}" REQUIRED)
find_program(MINGW_AR NAMES ar x86_64-w64-mingw32-ar HINTS "${MINGW_BIN}")
find_program(MINGW_RANLIB NAMES ranlib x86_64-w64-mingw32-ranlib HINTS "${MINGW_BIN}")

set(CMAKE_C_COMPILER "${MINGW_C_COMPILER}")
set(CMAKE_CXX_COMPILER "${MINGW_CXX_COMPILER}")
set(CMAKE_RC_COMPILER "${MINGW_RC_COMPILER}")
if(MINGW_AR)
    set(CMAKE_AR "${MINGW_AR}")
endif()
if(MINGW_RANLIB)
    set(CMAKE_RANLIB "${MINGW_RANLIB}")
endif()

# Native Windows toolchain: no sysroot to re-root searches into.
set(CMAKE_FIND_ROOT_PATH "${MINGW_BIN}/../")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
