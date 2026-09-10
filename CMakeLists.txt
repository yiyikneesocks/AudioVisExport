# =============================================================================
# AudioVisExport v2 — 透明背景频谱图视频生成器
#
# 用途：导入 wav/aiff → 自写 SpectrumCore (双路 FFT + band mapping + 平滑 + 峰值)
#       → 可插拔 SpectrumStyle 渲染到 ARGB juce::Image → 输出带 alpha 的 PNG 序列
#       （后续可加 WebM/MOV alpha 编码）。
#
# 设计意图：source/core/ 是纯函数式可复用引擎，不依赖 UI / 系统时钟 / OpenGL，
#           后续大工程（剪辑软件）可作为子目录或静态库引入。
#
# 协议：GPL-3.0
# =============================================================================
cmake_minimum_required(VERSION 3.22)
project(AudioVisExport VERSION 0.5.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows 强制静态 CRT（与 Y2Kmeter 一致，避免依赖 VC_redist）
# Note: toolchain file overrides this for cross-compilation
if (NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 默认 Release（PoC 阶段不需要 Debug 调试）
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# =============================================================================
# JUCE 8.0.12
# =============================================================================

# v0.5.4 #7：开启 JUCE 软件 MP3 解码（解码代码随 juce_audio_formats 源码编译；
#   JUCE 官方免责声明版权风险由使用者自负。需在 FetchContent_MakeAvailable 前声明，
#   使宏传入 JUCE 子目录编译）。
add_compile_definitions (JUCE_USE_MP3AUDIOFORMAT=1)

option(AVX_USE_LOCAL_JUCE "Use local JUCE checkout at third_party/JUCE" OFF)

if (AVX_USE_LOCAL_JUCE)
    set(_avx_local_juce "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE")
    if (EXISTS "${_avx_local_juce}/CMakeLists.txt")
        set(FETCHCONTENT_SOURCE_DIR_JUCE "${_avx_local_juce}" CACHE PATH "Local JUCE" FORCE)
        message(STATUS "AudioVisExport: 使用本地 JUCE → ${_avx_local_juce}")
    endif()
    unset(_avx_local_juce)
endif()

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

if (TARGET juceaide AND NOT TARGET juce::juceaide)
    add_executable(juce::juceaide ALIAS juceaide)
endif()

# juceaide CRT 适配（与 Y2Kmeter 一致，避免 LNK2001）
if (WIN32 AND TARGET juceaide)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(juceaide PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    else()
        set_target_properties(juceaide PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
endif()

# =============================================================================
# 共享引擎源（core + styles）：CLI 与 GUI 两个 target 复用
# =============================================================================
set(AVX_ENGINE_SOURCES
    source/Y2KLogging.h
    source/core/CrashReporter.h
    source/core/CrashReporter.cpp
    source/core/PcmSource.h
    source/core/PcmSource.cpp
    source/core/PngSequenceEncoder.h
    source/core/PngSequenceEncoder.cpp
    source/core/SpectrumParams.h
    source/core/SpectrumParams.cpp
    source/core/BandFrame.h
    source/core/FreqMap.h
    source/core/FreqMap.cpp
    source/core/ColorMap.h
    source/core/ColorMap.cpp
    source/core/SpectrumCore.h
    source/core/SpectrumCore.cpp
    source/core/SpectrumStyle.h
    source/core/SpectrumStyle.cpp
    source/core/SpectrumMask.h
    source/core/SpectrumMask.cpp
    source/styles/Y2KLineStyle.h
    source/styles/Y2KLineStyle.cpp
    source/styles/BarStyle.h
    source/styles/BarStyle.cpp
     source/styles/BarLineStyle.cpp
     source/styles/BarLineStyle.h
     source/styles/PolylineStyle.h
    source/styles/PolylineStyle.cpp
    source/styles/CrystalStyle.h
    source/styles/CrystalStyle.cpp
    source/core/VisPipeline.h
    source/core/VisPipeline.cpp
)

# 两个 target 的公共配置（include / defs / MSVC 选项）
function(avx_configure_target tgt)
    target_include_directories(${tgt} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}         # 让 #include "source/Y2KLogging.h" 能解析
        ${CMAKE_CURRENT_SOURCE_DIR}/source
    )
    target_compile_definitions(${tgt} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_ARA=0
        JUCE_PLUGINHOST_LV2=0
    )
    if (MSVC OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
        # MSVC-compatible mode (native MSVC or Clang-cl / Clang targeting MSVC ABI)
        target_compile_options(${tgt} PRIVATE /utf-8)
        if (MSVC)
            target_compile_options(${tgt} PRIVATE /MP)
        endif()
        # Debug 信息：/Z7 嵌入 obj（与 Y2Kmeter 一致，避免 mspdbsrv 竞争）
        target_compile_options(${tgt} PRIVATE
            $<$<CONFIG:Debug>:/Z7>
            $<$<CONFIG:RelWithDebInfo>:/Z7>
        )
        target_compile_options(${tgt} PRIVATE
            $<$<CONFIG:Release>:/O2 /Ob3 /Oi /Ot /Gy /Gw>
        )
        target_link_options(${tgt} PRIVATE
            $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
        )
        # Clang cross-compilation: disable stdlib (MSVC CRT is linked via toolchain)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CROSSCOMPILING)
            target_compile_options(${tgt} PRIVATE -nostdinc++ -fno-exceptions -fno-rtti)
            target_link_options(${tgt} PRIVATE -nostdlib++)
        endif()
    endif()
endfunction()

# =============================================================================
# Target 1：AudioVisExport（CLI）
# =============================================================================
juce_add_console_app(AudioVisExport
    PRODUCT_NAME "AudioVisExport"
    VERSION "0.5.3"
    COMPANY_NAME "AudioVisExport"
)

target_sources(AudioVisExport PRIVATE
    ${AVX_ENGINE_SOURCES}
    source/cli/CliArgs.h
    source/cli/CliArgs.cpp
    source/cli/main.cpp
)

target_link_libraries(AudioVisExport PRIVATE
    juce::juce_audio_utils      # 含 AudioFormatManager + ChildProcess（传递带 audio_formats/gui_basics）
    juce::juce_dsp              # FFT + WindowingFunction
    juce::juce_graphics         # Graphics / Path / Image (ARGB) / PNG encoder
    juce::juce_gui_basics       # Image / ColourHelpers（被 graphics 拉但显式列出更稳）
)
avx_configure_target(AudioVisExport)

# =============================================================================
# Target 2：AudioVisGUI（实时预览界面）
#
# 拖入音频 → AudioTransportSource 播放 + SpectrumCore 实时同步 →
# SpectrumStyle 实时渲染；右侧参数面板（滑块/下拉/颜色）即改即见；
# 支持一键导出 PNG 序列 / WebM / MOV（复用 VisPipeline::run）。
# =============================================================================
juce_add_gui_app(AudioVisGUI
    PRODUCT_NAME "AudioVisGUI"
    VERSION "0.5.3"
    COMPANY_NAME "AudioVisExport"
    ICON_BIG   "${CMAKE_SOURCE_DIR}/assets/icon.png"   # v0.5.4 #1: exe 图标
    ICON_SMALL "${CMAKE_SOURCE_DIR}/assets/icon.png"   # 注意：juceaide winicon 只认 png 输入（ico 会静默失败）
)

target_sources(AudioVisGUI PRIVATE
    ${AVX_ENGINE_SOURCES}
    source/gui/Main.cpp
    source/gui/MainComponent.h
    source/gui/MainComponent.cpp
    source/gui/ParamPanel.h
    source/gui/ParamPanel.cpp
    source/gui/SpectrumCanvas.h
    source/gui/SpectrumCanvas.cpp
    source/gui/WinDragCompat.h
    source/gui/WinDragCompat.cpp
)

target_link_libraries(AudioVisGUI PRIVATE
    juce::juce_audio_utils      # AudioDeviceManager + AudioTransportSource + AudioFormatManager
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra        # ColourSelector（颜色选择弹层）
)
if (WIN32)
    target_link_libraries (AudioVisGUI PRIVATE dbghelp)
    target_link_libraries (AudioVisExport PRIVATE dbghelp)
endif()
avx_configure_target(AudioVisGUI)

# =============================================================================
# （临时）对边锚定缩放数学断言测试 — v0.5.2 验证用
# =============================================================================
add_executable(vis_anchor_test EXCLUDE_FROM_ALL
    scripts/vis_anchor_test.cpp)
target_compile_definitions(vis_anchor_test PRIVATE JUCE_USE_CURL=0)
target_link_libraries(vis_anchor_test PRIVATE juce::juce_graphics juce::juce_events)

# v0.5.4 #3：峰帽/峰线跟随基线轴 + Peak caps 全样式 + 下臂描边/填充回归
add_executable(vis_peaks_test EXCLUDE_FROM_ALL
    scripts/vis_peaks_test.cpp
    source/core/SpectrumStyle.cpp
    source/core/ColorMap.cpp
    source/styles/BarStyle.cpp
    source/styles/BarLineStyle.cpp
    source/styles/Y2KLineStyle.cpp
    source/styles/PolylineStyle.cpp
    source/styles/CrystalStyle.cpp)
target_compile_definitions(vis_peaks_test PRIVATE JUCE_USE_CURL=0)
target_include_directories(vis_peaks_test PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/source)
target_link_libraries(vis_peaks_test PRIVATE juce::juce_graphics juce::juce_events)

add_executable(vis_mask_test EXCLUDE_FROM_ALL
    scripts/vis_mask_test.cpp
    source/core/SpectrumMask.cpp)
target_compile_definitions(vis_mask_test PRIVATE JUCE_USE_CURL=0)
target_include_directories(vis_mask_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(vis_mask_test PRIVATE juce::juce_graphics juce::juce_events)
