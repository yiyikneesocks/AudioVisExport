// =============================================================================
// PngSequenceEncoder.h — 帧序列编码 + ffmpeg 合成 mp4
//
// 设计意图：
//   · 这是 source/core/ 引擎层的一员。
//   · 写阶段：每帧 RGBA → 翻转 Y 轴 → juce::PNGImageFormat::writeImageToStream
//   · 合成阶段：调 ffmpeg.exe 子进程把 PNG 序列 + 音频合成 mp4
//   · 子进程方式最简单，PoC 阶段用户需自备 ffmpeg.exe
// =============================================================================
#pragma once

// JUCE module headers（PNGImageFormat 在 juce_graphics；ChildProcess 在 juce_core）
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>

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
