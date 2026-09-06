// =============================================================================
// PngSequenceEncoder.h — 帧序列编码 + ffmpeg 合成视频（webm/mov/png-seq）
//
// 设计意图：
//   · 这是 source/core/ 引擎层的一员。
//   · 写阶段：每帧 RGBA → 翻转 Y 轴 → juce::PNGImageFormat::writeImageToStream
//   · 合成阶段：调 ffmpeg.exe 子进程把 PNG 序列 + 音频合成视频
//     - MOV（QTRLE rgba）  → ★无损 alpha 透明通道（推荐给剪辑软件，实测 pix_fmt=argb）
//     - WebM（VP9）        → 无 alpha 的小体积预览片（libvpx 未开 VP9-alpha，实测丢 alpha）
//     - PNG-seq            → 不调 ffmpeg，仅写序列帧（ARGB 100% 保真，终极方案）
//   · 子进程方式最简单，PoC 阶段用户需自备 ffmpeg.exe（可见于 PATH）
// =============================================================================
#pragma once

// JUCE module headers（PNGImageFormat 在 juce_graphics；ChildProcess 在 juce_core）
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>

#include "SpectrumParams.h"   // SpectrumParams::Encoder 枚举

class PngSequenceEncoder
{
public:
    struct Config
    {
        juce::String outputDir;             // PNG 序列输出目录（中间产物）
        juce::String baseName = "frame_";  // 文件名前缀
        int width = 1280;
        int height = 720;
        int fps = 30;
        int digits = 6;                     // frame_000001.png
        SpectrumParams::Encoder encoder = SpectrumParams::WebmVp9;   // 合流目标编码器
    };

    PngSequenceEncoder() = default;
    ~PngSequenceEncoder();

    PngSequenceEncoder (const PngSequenceEncoder&) = delete;
    PngSequenceEncoder& operator= (const PngSequenceEncoder&) = delete;

    // 初始化输出目录（自动创建 + 清空已存在同名文件）。
    bool startSession (const Config& cfg);

    // 写一帧（已经由 ProjectMRenderer::readAsImage() 处理好 Y-flip +
    // RGBA→BGRA 的 JUCE ARGB Image，直接交给 PNGImageFormat 即可）
    void writeFrame (const juce::Image& frame);

    // 完成帧序列后调：调 ffmpeg.exe 把 PNG 序列 + 音频合成 mp4
    // ffmpegPath 为空时尝试 FFMPEG_PATH 环境变量 / PATH 自动搜索
    // 返回 ffmpeg 退出码（0 = 成功）；非 0 时 stderr 已打到 stdout
    int finalizeAndMux (const juce::String& audioPath,
                       const juce::String& outputVideoPath,
                       const juce::String& ffmpegPath = {});

    int getFrameCount() const noexcept { return frameIndex_; }
    juce::String getFrameFilePath (int index) const;
    juce::String getOutputDir() const noexcept { return cfg_.outputDir; }

private:
    Config cfg_;
    int frameIndex_ = 0;

    juce::String findFfmpeg_ (const juce::String& hint) const;
    juce::StringArray buildFfmpegArgs_ (const juce::String& audioPath,
                                       const juce::String& outputVideoPath) const;
};
