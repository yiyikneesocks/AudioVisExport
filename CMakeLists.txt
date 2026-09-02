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
project(AudioVisExport VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows 强制静态 CRT（与 Y2Kmeter 一致，避免依赖 VC_redist）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 默认 Release（PoC 阶段不需要 Debug 调试）
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# =============================================================================
# JUCE 8.0.12
# =============================================================================
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
# 可执行 target
#
# 用 juce_add_console_app（而非裸 add_executable），让 JUCE 自动生成 JuceHeader.h
# 并处理 Windows GUI 子系统、DLL manifest 等细节。
# =============================================================================
juce_add_console_app(AudioVisExport
    PRODUCT_NAME "AudioVisExport"
    VERSION "0.2.0"
    COMPANY_NAME "AudioVisExport"
)

target_sources(AudioVisExport PRIVATE
    # 复用（不动）
    source/Y2KLogging.h
    source/core/PcmSource.h
    source/core/PcmSource.cpp
    source/core/PngSequenceEncoder.h
    source/core/PngSequenceEncoder.cpp

    # v2 新增：参数系统
    source/core/SpectrumParams.h
    source/core/SpectrumParams.cpp
    source/core/BandFrame.h
    source/core/FreqMap.h
    source/core/FreqMap.cpp
    source/core/ColorMap.h
    source/core/ColorMap.cpp

    # v2 新增：频谱分析引擎
    source/core/SpectrumCore.h
    source/core/SpectrumCore.cpp

    # v2 新增：样式系统
    source/core/SpectrumStyle.h
    source/core/SpectrumStyle.cpp
    source/styles/Y2KLineStyle.h
    source/styles/Y2KLineStyle.cpp
    source/styles/BarStyle.h
    source/styles/BarStyle.cpp
    source/styles/PolylineStyle.h
    source/styles/PolylineStyle.cpp
    source/styles/CrystalStyle.h
    source/styles/CrystalStyle.cpp

    # v2 新增：CLI 参数解析
    source/cli/CliArgs.h
    source/cli/CliArgs.cpp

    # 重写
    source/core/VisPipeline.h
    source/core/VisPipeline.cpp
    source/cli/main.cpp
)

target_include_directories(AudioVisExport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}         # 让 #include "source/Y2KLogging.h" 能解析
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(AudioVisExport PRIVATE
    juce::juce_audio_utils      # 含 AudioFormatManager + ChildProcess（传递带 audio_formats/gui_basics）
    juce::juce_dsp              # FFT + WindowingFunction
    juce::juce_graphics         # Graphics / Path / Image (ARGB) / PNG encoder
    juce::juce_gui_basics       # Image / ColourHelpers（被 graphics 拉但显式列出更稳）
)

target_compile_definitions(AudioVisExport PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_ARA=0
    JUCE_PLUGINHOST_LV2=0
)

if (MSVC)
    target_compile_options(AudioVisExport PRIVATE /utf-8 /MP)
    # Debug 信息：/Z7 嵌入 obj（与 Y2Kmeter 一致，避免 mspdbsrv 竞争）
    target_compile_options(AudioVisExport PRIVATE
        $<$<CONFIG:Debug>:/Z7>
        $<$<CONFIG:RelWithDebInfo>:/Z7>
    )
    target_compile_options(AudioVisExport PRIVATE
        $<$<CONFIG:Release>:/O2 /Ob3 /Oi /Ot /Gy /Gw>
    )
    target_link_options(AudioVisExport PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
    )
endif()
