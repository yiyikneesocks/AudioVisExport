# CMake toolchain file for cross-compiling to Windows x86_64 using
# Clang (MSVC ABI) + lld-link + xwin Windows SDK
#
# Prerequisites:
#   1. clang + lld installed (sudo apt-get install clang lld)
#   2. xwin installed (cargo install xwin)
#   3. Windows SDK downloaded (xwin --accept-license --arch x86_64 --sdk-version 10.0.22621 splat --output ~/.xwin-sysroot)
#
# Usage:
#   cmake -S . -B build_win \
#     -G Ninja \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang-cl-msvc.cmake \
#     -DAVX_USE_LOCAL_JUCE=ON

# === Target platform ===
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Force Release mode (xwin sysroot only has Release CRT)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Static CRT (no VC_redist dependency)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)

# === xwin sysroot path ===
# Adjust this if you installed xwin to a different location
set(XWIN_SYSROOT "$ENV{HOME}/.xwin-sysroot" CACHE PATH "xwin sysroot directory")

# === Compilers ===
# Use clang-cl wrapper for MSVC-compatible mode
# The wrapper invokes clang with --target=x86_64-w64-windows-msvc
set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../../scripts/clang-cl-wrapper.sh")
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../../scripts/clang-cl-wrapper.sh")

# === Resource compiler ===
# Use llvm-rc for Windows resource files (.rc) — prefer LLVM 18
find_program(LLVM_RC llvm-rc-18)
if (NOT LLVM_RC)
    find_program(LLVM_RC llvm-rc-14)
endif()
if (NOT LLVM_RC)
    find_program(LLVM_RC llvm-rc)
endif()
if (NOT LLVM_RC)
    # Fallback to MinGW windres (works for basic .rc files)
    set(LLVM_RC x86_64-w64-mingw32-windres)
endif()
set(CMAKE_RC_COMPILER "${LLVM_RC}")

# === Linker ===
set(CMAKE_LINKER lld-link-18)
set(CMAKE_AR llvm-ar-18)
set(CMAKE_RANLIB llvm-ranlib-18)

# === Include paths ===
# Order matters: versioned SDK (original casing) → non-versioned (lowercase symlinks)
# xwin splat creates non-versioned dirs with lowercase symlinks, but JUCE includes
# headers with original Windows casing (e.g., <Dbghelp.h> not <dbghelp.h>).
# Adding the versioned SDK directories FIRST ensures case-sensitive lookups work.
set(XWIN_CRT_INCLUDE   "${XWIN_SYSROOT}/crt/include")
set(XWIN_SDK_UM_VER    "${XWIN_SYSROOT}/sdk/include/10.0.22621/um")
set(XWIN_SDK_SHARED_VER "${XWIN_SYSROOT}/sdk/include/10.0.22621/shared")
set(XWIN_SDK_UCRT_VER  "${XWIN_SYSROOT}/sdk/include/10.0.22621/ucrt")
set(XWIN_SDK_UM        "${XWIN_SYSROOT}/sdk/include/um")
set(XWIN_SDK_SHARED    "${XWIN_SYSROOT}/sdk/include/shared")
set(XWIN_SDK_UCRT      "${XWIN_SYSROOT}/sdk/include/ucrt")
set(XWIN_SDK_WINRT     "${XWIN_SYSROOT}/sdk/include/winrt")

# === Library paths ===
set(XWIN_CRT_LIB       "${XWIN_SYSROOT}/crt/lib/x86_64")
set(XWIN_SDK_LIB_UM    "${XWIN_SYSROOT}/sdk/lib/um/x86_64")
set(XWIN_SDK_LIB_UCRT  "${XWIN_SYSROOT}/sdk/lib/ucrt/x86_64")

# === Compiler flags ===
# Include order (all via -isystem so they don't override user flags):
#   1. clang resource include — provides real intrinsic headers (emmintrin.h etc.)
#      that inline to actual SSE instructions. The MSVC CRT headers only declare
#      extern functions (relying on MSVC's hardcoded intrinsic recognition),
#      which otherwise become undefined symbols at link time.
#   2. versioned SDK dirs (original casing) — resolve JUCE's <Dbghelp.h> etc.
#   3. non-versioned dirs (lowercase symlinks) — fallback
#   4. CRT + winrt
execute_process(COMMAND clang-18 -print-resource-dir OUTPUT_VARIABLE XWIN_CLANG_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_C_FLAGS_INIT
  "-fms-compatibility -fms-extensions -fms-compatibility-version=19.29 \
   -isystem ${XWIN_CLANG_RESOURCE_DIR}/include \
   -isystem ${XWIN_SDK_UM_VER} \
   -isystem ${XWIN_SDK_SHARED_VER} \
   -isystem ${XWIN_SDK_UCRT_VER} \
   -isystem ${XWIN_SDK_UM} \
   -isystem ${XWIN_SDK_SHARED} \
   -isystem ${XWIN_SDK_UCRT} \
   -isystem ${XWIN_CRT_INCLUDE} \
   -isystem ${XWIN_SDK_WINRT}"
)

set(CMAKE_CXX_FLAGS_INIT
  "-fms-compatibility -fms-extensions -fms-compatibility-version=19.29 \
   -isystem ${XWIN_CLANG_RESOURCE_DIR}/include \
   -isystem ${XWIN_SDK_UM_VER} \
   -isystem ${XWIN_SDK_SHARED_VER} \
   -isystem ${XWIN_SDK_UCRT_VER} \
   -isystem ${XWIN_SDK_UM} \
   -isystem ${XWIN_SDK_SHARED} \
   -isystem ${XWIN_SDK_UCRT} \
   -isystem ${XWIN_CRT_INCLUDE} \
   -isystem ${XWIN_SDK_WINRT}"
)

# RC (resource compiler) needs SDK include paths too — llvm-rc preprocesses
# .rc files which #include <windows.h> etc.
set(CMAKE_RC_FLAGS_INIT
  "-I${XWIN_CRT_INCLUDE} \
   -I${XWIN_SDK_UM_VER} \
   -I${XWIN_SDK_SHARED_VER} \
   -I${XWIN_SDK_UCRT_VER} \
   -I${XWIN_SDK_UM} \
   -I${XWIN_SDK_SHARED} \
   -I${XWIN_SDK_UCRT}"
)

# === Linker flags ===
# /LIBPATH: tells lld-link where to find .lib files
set(CMAKE_EXE_LINKER_FLAGS_INIT
  "-fuse-ld=lld-link-18 \
   -Wl,/LIBPATH:${XWIN_CRT_LIB} \
   -Wl,/LIBPATH:${XWIN_SDK_LIB_UM} \
   -Wl,/LIBPATH:${XWIN_SDK_LIB_UCRT}"
)

set(CMAKE_SHARED_LINKER_FLAGS_INIT
  "-fuse-ld=lld-link-18 \
   -Wl,/LIBPATH:${XWIN_CRT_LIB} \
   -Wl,/LIBPATH:${XWIN_SDK_LIB_UM} \
   -Wl,/LIBPATH:${XWIN_SDK_LIB_UCRT}"
)

# === Search path configuration ===
set(CMAKE_FIND_ROOT_PATH "${XWIN_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# === Windows platform macros ===
# _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH:
#   xwin downloads the latest MSVC STL (v143) which nominally requires Clang 19+;
#   Clang 18 works in practice — verified with a full STL feature test
#   (variant/vector/sort/map/unique_ptr compile+link OK).
add_compile_definitions(
  _WIN32
  WINVER=0x0A00
  _WIN32_WINNT=0x0A00
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
)
