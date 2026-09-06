# CMake toolchain file for cross-compiling to Windows x86_64 using mingw-w64
# Usage: cmake -S . -B build_win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-w64-mingw32.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cross-compilers — 必须用 posix 线程模型（win32 模型缺 _GLIBCXX_HAS_GTHREADS，std::mutex 等未定义）
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)

# Search paths (target environment only)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Ensure Windows platform macros are defined for JUCE
add_compile_definitions(_WIN32 WINVER=0x0A00 _WIN32_WINNT=0x0A00)
