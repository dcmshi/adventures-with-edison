cmake_minimum_required(VERSION 3.20)
project(adventures_with_edison C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- third-party dependencies (downloaded at configure time, pinned) ---------
include(FetchContent)

# Nuked-OPL3: cycle-accurate OPL2/OPL3 emulator (LGPL-2.1).
FetchContent_Declare(nuked_opl3
    GIT_REPOSITORY https://github.com/nukeykt/Nuked-OPL3.git
    GIT_TAG 765ec962e473aeb767e4cba74ffdc8f588ffbfe8)
FetchContent_MakeAvailable(nuked_opl3)
add_library(nuked_opl3 STATIC ${nuked_opl3_SOURCE_DIR}/opl3.c)
target_include_directories(nuked_opl3 PUBLIC ${nuked_opl3_SOURCE_DIR})

# SDL3 for audio output (static, so there is no DLL to ship alongside).
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3)

# MinGW: link the GCC runtime statically. Otherwise the executables load
# whatever libstdc++-6.dll comes first on PATH (e.g. Git for Windows' older
# copy), which crashes on ABI mismatch.
if(MINGW)
    add_link_options(-static-libstdc++ -static-libgcc)
endif()

# Warnings for our own code only.
if(MSVC)
    set(EDISON_WARNINGS /W4)
else()
    set(EDISON_WARNINGS -Wall -Wextra)
endif()

enable_testing()
add_subdirectory(engine)
