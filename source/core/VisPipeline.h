// =============================================================================
// VisPipeline.h — 高层编排器（v2 重写）
//
// 设计意图：
//   · 把 PcmSource + SpectrumCore + SpectrumStyle + PngSequenceEncoder 串成一条
//     "输入音频 + 参数 → 输出透明背景频谱图 PNG 序列"的同步阻塞管线。
//   · 调用方只需提供 SpectrumParams + ProgressCallback，不需要管 GL 线程生命周期
//     （v2 不再用 OpenGL，纯 juce::Graphics 渲染到 ARGB Image）。
//
// 与 v1 差异：
//   · 删 projectM / presets / textures / warmup-for-shader
//   · 用 SpectrumCore 替代 ProjectMRenderer
//   · 用 SpectrumStyle 替代 projectM preset
//   · 输出 ARGB PNG（带 alpha），默认全透明背景
//   · PNG-seq 模式不调 ffmpeg（WebM/MOV 模式才调）
//
// 后续大工程衔接：剪辑软件可绕过 VisPipeline，直接组合 PcmSource + SpectrumCore
// 按时间轴逐帧调用，自定义 style 和编码器。
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include "SpectrumParams.h"

class VisPipeline
{
public:
    struct Config
    {
        SpectrumParams params;   // 完整参数（含 audioPath / outputDir / encoder 等）
    };

    using ProgressCallback =
        std::function<void (int doneFrame, int totalFrames, double elapsedSec)>;

    // 同步阻塞执行。返回 StringPairArray：
    //   成功时含 "ok"="true"，以及 "frames_written" / "png_dir" / "elapsed_sec" 等
    //   失败时含 "ok"="false" 和 "error"
    // cb 在主渲染循环每帧后调用一次（用于 CLI 打印进度 / 后续 UI 进度条）。
    juce::StringPairArray run (const Config& cfg, ProgressCallback cb = {});

    // 单帧预览模式：跑管线到第 frameIndex 帧后只渲染一次写单 PNG。
    // 若 params.bgCheckerboardPreview=true，先画棋盘格再画谱（肉眼判断 alpha）。
    // 返回 StringPairArray：ok / error / png_path
    juce::StringPairArray previewFrame (const Config& cfg, int frameIndex,
                                        const juce::String& outPngPath);

    // 数值调试模式：跑管线到第 frameIndex 帧后 dump BandFrame 前 16 带 dB 到 stdout。
    // 返回 StringPairArray：ok / error
    juce::StringPairArray probeSpectrum (const Config& cfg, int frameIndex);
};
