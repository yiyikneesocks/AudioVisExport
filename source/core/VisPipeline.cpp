// =============================================================================
// VisPipeline.cpp — 高层编排器（v2 重写）
//
// 编排流程：
//   1) 加载音频（PcmSource，缺失则 44100Hz 静音 5s fallback）
//   2) 初始化 SpectrumCore（参数副本）
//   3) 静音 warmup 8 帧（让 FFT FIFO 与平滑态稳定）
//   4) 工厂创建 SpectrumStyle
//   5) 主循环：pcm.read → core.push + advanceTime + getBandFrame
//             → 创建 ARGB Image + style.render → enc.writeFrame → 进度回调
//   6) finalize（PNG-seq 模式不调 ffmpeg；WebM/MOV 才调）
//   7) 返回结构化结果
// =============================================================================
#include "VisPipeline.h"
#include "PcmSource.h"
#include "SpectrumCore.h"
#include "SpectrumStyle.h"
#include "PngSequenceEncoder.h"
#include "FreqMap.h"
#include "VisTransform.h"
#include "SpectrumMask.h"

#include <chrono>
#include <vector>
#include <map>
#include <iostream>

namespace
{
    // 辅助：相对路径 → 绝对 File（与 v1 locateAsset 等价，简化版）
    static juce::File locateAsset (const juce::String& rel)
    {
        if (rel.isEmpty()) return {};
        juce::File f (rel);
        if (f.exists()) return f;
        auto cwd = juce::File::getCurrentWorkingDirectory().getChildFile (rel);
        if (cwd.exists()) return cwd;
        const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        auto fromExe = exeDir.getChildFile (rel);
        if (fromExe.exists()) return fromExe;
        // 向上 5 层
        juce::File d = exeDir;
        for (int up = 0; up < 6; ++up) {
            auto cand = d.getChildFile (rel);
            if (cand.exists()) return cand;
            d = d.getParentDirectory();
        }
        return {};
    }

    // 从 SpectrumParams 构建 SpectrumStyle::RenderParams
    static SpectrumStyle::RenderParams buildRenderParams (const SpectrumParams& p)
    {
        SpectrumStyle::RenderParams rp;
        rp.width  = p.width;
        rp.height = p.height;
        rp.primary   = p.primaryColor;
        rp.secondary = p.secondaryColor;
        rp.peak      = p.peakColor;
        rp.bg        = p.bgColor;
        rp.lineWidth      = p.lineWidth;
        rp.opacity        = p.opacity;
        rp.barGapRatio    = p.barGapRatio;
        rp.barWidthRatio  = p.barWidthRatio;
        rp.drawGrid       = p.drawGrid;
        rp.drawAxisLabels = p.drawAxisLabels;
        rp.minHz          = p.minHz;
        rp.maxHz          = p.maxHz;
        rp.minDb          = p.minDb;
        rp.maxDb          = p.maxDb;
        rp.colorMap       = p.colorMap;
        rp.barParticles   = p.barParticles;
        return rp;
    }

    // 画棋盘格（预览模式用）
    static void drawCheckerboard (juce::Graphics& g, int w, int h, int cellSize = 8)
    {
        for (int y = 0; y < h; y += cellSize) {
            for (int x = 0; x < w; x += cellSize) {
                const bool light = (((x / cellSize) + (y / cellSize)) & 1) != 0;
                g.setColour (light ? juce::Colours::lightgrey : juce::Colours::white);
                g.fillRect (x, y, cellSize, cellSize);
            }
        }
    }

    // 渲染单帧到 ARGB Image（两段式合成，保证"频谱元素自由变换"所见即所得）：
    //   1) 基础层：频谱按输出分辨率渲染到透明 ARGB 层（不带变换）
    //   2) 合成层：背景（棋盘格 / 半透明 bg）+ 用 buildVisAffine() 叠加频谱元素
    // —— 图片图层加载（按路径缓存；本管线为单线程调用，进程内共享缓存安全）——
    static juce::Image loadImageForLayer (const juce::String& path)
    {
        static std::map<juce::String, juce::Image> cache;
        auto it = cache.find (path);
        if (it != cache.end()) return it->second;

        juce::Image im;
        juce::File f (path);
        if (f.existsAsFile())
        {
            juce::FileInputStream stream (f);
            if (stream.openedOk())
                im = juce::ImageFileFormat::loadFrom (stream);   // PNG/JPEG/GIF/BMP 自动识别
        }
        cache[path] = im;
        return im;
    }

    // 图片蒙版平均色缓存（按路径；均色与帧无关，只算一次）
    static juce::Colour averageColourCached (const juce::Image& im, const juce::String& path)
    {
        static std::map<juce::String, juce::Colour> cache;
        auto it = cache.find (path);
        if (it != cache.end()) return it->second;
        const juce::Colour c = SpectrumMask::averageColour (im);
        cache[path] = c;
        return c;
    }

    // 画一张图片图层：默认铺满输出画布（与频谱同基准），再套用 VisTransform
    static void drawImageLayer (juce::Graphics& g, const ImageLayer& layer, int w, int h)
    {
        if (! layer.visible || layer.path.isEmpty()) return;
        const juce::Image im = loadImageForLayer (layer.path);
        if (im.isNull()) return;

        // 图片元素基础矩形 = 图片自然尺寸（与 GUI SpectrumCanvas 完全同源）。
        // identity 变换（set=false）= 等比 contain 居中（v0.5.1 起替代旧"强制拉伸铺满"）。
        const float ew = (float) im.getWidth();
        const float eh = (float) im.getHeight();
        const VisTransform tf = layer.transform.set
            ? layer.transform
            : makeContainTransform (ew, eh, (float) w, (float) h);

        g.saveState();
        if (layer.opacity < 1.0f)
            g.setOpacity (juce::jlimit (0.0f, 1.0f, layer.opacity));
        g.addTransform (buildVisAffine (tf));
        g.drawImageAt (im, 0, 0);
        g.restoreState();
    }

    static juce::Image renderFrame (SpectrumCore& core, SpectrumStyle& style,
                                    const SpectrumParams& p,
                                    const SpectrumStyle::RenderParams& rp,
                                    bool checkerboard)
    {
        // 频谱画框（padding 内）——渲染基础层 + 蒙版图片 fill 基准共用
        const auto canvas = juce::Rectangle<int> (
            (int) rp.paddingLeft,
            (int) rp.paddingTop,
            p.width  - (int)(rp.paddingLeft + rp.paddingRight),
            p.height - (int)(rp.paddingTop  + rp.paddingBottom));

        // —— 基础层 ——
        juce::Image base (juce::Image::ARGB, p.width, p.height, true);   // true = 清空（全透明）
        {
            juce::Graphics gb (base);
            gb.setOpacity (rp.opacity);

            BandFrame bandFrame;
            core.getBandFrame (bandFrame);
            style.render (gb, canvas, bandFrame, rp);
        }

        // —— 合成层 ——
        juce::Image img (juce::Image::ARGB, p.width, p.height, true);
        {
            juce::Graphics g (img);

            // 棋盘格预览（在透明背景下画棋盘，肉眼判断 alpha）
            if (checkerboard)
            {
                drawCheckerboard (g, p.width, p.height);
            }
            else if (rp.bg.getAlpha() > 0)
            {
                // 半透明背景（非全透明时才画）
                g.fillAll (rp.bg);
            }

            // 图片图层：频谱下方组 → 频谱元素（若存在）→ 频谱上方组
            const int N = (int) p.images.size();
            const int k = juce::jlimit (0, N, p.spectrumIndex);

            for (int i = 0; i < k; ++i)
                drawImageLayer (g, p.images[(size_t) i], p.width, p.height);

            // 频谱元素：未变换（set=false）直接铺满；已变换则按仿射合成
            //   若开启频谱蒙版：图片填轮廓（+可选描边），取代裸频谱填充层（选项 C）
            if (p.spectrumPresent)
            {
                juce::Image layer = base;
                if (p.maskImage.enabled && ! p.maskImage.path.isEmpty())
                {
                    const juce::Image im = loadImageForLayer (p.maskImage.path);
                    if (im.isValid())
                    {
                        const juce::Colour stroke = p.maskImage.strokeAutoColor
                                                  ? averageColourCached (im, p.maskImage.path)
                                                  : p.maskImage.strokeColor;
                        juce::Image masked = SpectrumMask::compose (base, im, p.maskImage, stroke);
                        if (masked.isValid()) layer = masked;
                    }
                }

                if (p.transform.set)
                {
                    g.saveState();
                    g.addTransform (buildVisAffine (p.transform));
                    g.drawImageAt (layer, 0, 0);
                    g.restoreState();
                }
                else
                {
                    g.drawImageAt (layer, 0, 0);
                }
            }

            for (int i = k; i < N; ++i)
                drawImageLayer (g, p.images[(size_t) i], p.width, p.height);
        }
        return img;
    }

    // 把音频推进到第 targetFrameIndex 帧（含 warmup）
    // 返回 false = 失败（errOut 填错误）
    static bool advanceToFrame (PcmSource& pcm, bool hasAudio, double sr,
                                SpectrumCore& core, const SpectrumParams& p,
                                int targetFrameIndex, juce::String& errOut)
    {
        errOut.clear();
        const int spf = std::max<int>(1, (int)(sr / p.fps));

        // warmup：8 帧静音让 FFT FIFO 与平滑态稳定
        const int warmup = 8;
        {
            std::vector<float> zeros(spf * 2, 0.0f);
            for (int w = 0; w < warmup; ++w) {
                core.pushInterleavedStereo (zeros.data(), spf);
                core.advanceTime (1.0 / p.fps);
            }
        }

        // 推到第 targetFrameIndex 帧
        std::vector<float> frameBuf(spf * 2, 0.0f);
        for (int i = 0; i < targetFrameIndex; ++i) {
            int got = 0;
            if (hasAudio)
                got = pcm.readInterleavedStereo ((int64_t) i * spf, spf, frameBuf.data());
            if (got < spf) std::fill (frameBuf.begin() + got * 2, frameBuf.end(), 0.0f);
            core.pushInterleavedStereo (frameBuf.data(), spf);
            core.advanceTime (1.0 / p.fps);
        }
        return true;
    }
}

// =============================================================================
// run：完整导出
// =============================================================================
juce::StringPairArray VisPipeline::run (const Config& cfg, ProgressCallback cb)
{
    const auto& p = cfg.params;
    juce::StringPairArray result;
    auto t0 = std::chrono::steady_clock::now();
    auto elapsed = [&]() -> double {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - t0).count();
    };

    // 1) 加载音频
    PcmSource pcm;
    bool hasAudio = false;
    juce::File audioFile = locateAsset (p.audioPath);
    if (audioFile.existsAsFile() && pcm.load (audioFile.getFullPathName())) {
        hasAudio = true;
    }
    const double sr = hasAudio ? pcm.getSampleRate() : 44100.0;
    const int64_t pcmTotal = hasAudio ? pcm.getNumSamples() : (int64_t)(sr * 5.0);
    const int spf = std::max<int>(1, (int)(sr / p.fps));
    const int64_t totalFrames = (pcmTotal + spf - 1) / spf;

    // 2) 初始化 SpectrumCore
    SpectrumCore core (p);
    core.setSampleRate (sr);

    // 3) warmup
    {
        std::vector<float> zeros(spf * 2, 0.0f);
        for (int w = 0; w < 8; ++w) {
            core.pushInterleavedStereo (zeros.data(), spf);
            core.advanceTime (1.0 / p.fps);
        }
    }

    // 4) 创建 style
    auto style = SpectrumStyle::create (p.style);
    if (! style) {
        result.set ("ok", "false");
        result.set ("error", "unknown style: " + p.style);
        return result;
    }
    auto rp = buildRenderParams (p);

    // 5) 初始化 PngSequenceEncoder
    PngSequenceEncoder enc;
    PngSequenceEncoder::Config ecfg;
    ecfg.outputDir = p.outputDir;
    ecfg.baseName  = p.baseName;
    ecfg.width = p.width;
    ecfg.height = p.height;
    ecfg.fps = (int) p.fps;
    ecfg.digits = p.digits;
    ecfg.encoder = p.encoder;
    if (! enc.startSession (ecfg)) {
        result.set ("ok", "false");
        result.set ("error", "PngSequenceEncoder::startSession failed: " + p.outputDir);
        return result;
    }

    // 6) 主循环
    std::vector<float> frameBuf(spf * 2, 0.0f);
    int done = 0;
    for (int64_t i = 0; i < totalFrames; ++i) {
        int got = 0;
        if (hasAudio)
            got = pcm.readInterleavedStereo (i * spf, spf, frameBuf.data());
        if (got < spf) std::fill (frameBuf.begin() + got * 2, frameBuf.end(), 0.0f);

        core.pushInterleavedStereo (frameBuf.data(), spf);
        core.advanceTime (1.0 / p.fps);

        // 渲染
        auto img = renderFrame (core, *style, p, rp, false);
        enc.writeFrame (img);
        ++done;
        if (cb) cb (done, (int) totalFrames, elapsed());
    }

    // 7) finalize（PNG-seq 模式不调 ffmpeg；WebM/MOV 才调）
    int muxExit = 0;
    juce::String videoPath = p.outputVideoPath;
    if (p.encoder != SpectrumParams::PngSeq)
    {
        // 自动补全视频输出路径（GUI / CLI 都不强制用户手填：
        //   <outputDir>/<音频名>_vis.<webm|mov>）
        if (videoPath.isEmpty())
        {
            juce::String base = audioFile.getFileNameWithoutExtension();
            if (base.isEmpty()) base = "visual";
            const juce::String ext = (p.encoder == SpectrumParams::MovQtrle) ? ".mov" : ".webm";
            videoPath = juce::File (p.outputDir).getChildFile (base + "_vis" + ext).getFullPathName();
        }
        juce::String audioPathForMux = hasAudio ? audioFile.getFullPathName() : juce::String();
        muxExit = enc.finalizeAndMux (audioPathForMux, videoPath, p.ffmpegPath);
    }

    // 8) 结果
    result.set ("ok", "true");
    result.set ("frames_written", juce::String (done));
    result.set ("png_dir", enc.getOutputDir());
    result.set ("elapsed_sec", juce::String (elapsed(), 2));
    if (p.encoder != SpectrumParams::PngSeq) {
        result.set ("video_path", videoPath);
        result.set ("video_status", muxExit == 0 ? "ok" : ("ffmpeg exit " + juce::String (muxExit)));
    } else {
        result.set ("video_status", "skipped (png-seq mode)");
    }
    return result;
}

// =============================================================================
// previewFrame：单帧预览
// =============================================================================
juce::StringPairArray VisPipeline::previewFrame (const Config& cfg, int frameIndex,
                                                  const juce::String& outPngPath)
{
    const auto& p = cfg.params;
    juce::StringPairArray result;

    // 加载音频
    PcmSource pcm;
    bool hasAudio = false;
    juce::File audioFile = locateAsset (p.audioPath);
    if (audioFile.existsAsFile() && pcm.load (audioFile.getFullPathName())) {
        hasAudio = true;
    }
    const double sr = hasAudio ? pcm.getSampleRate() : 44100.0;

    // SpectrumCore
    SpectrumCore core (p);
    core.setSampleRate (sr);

    // 推到第 frameIndex 帧
    juce::String err;
    if (! advanceToFrame (pcm, hasAudio, sr, core, p, frameIndex, err)) {
        result.set ("ok", "false");
        result.set ("error", err);
        return result;
    }

    // style
    auto style = SpectrumStyle::create (p.style);
    if (! style) {
        result.set ("ok", "false");
        result.set ("error", "unknown style: " + p.style);
        return result;
    }
    auto rp = buildRenderParams (p);

    // 渲染
    auto img = renderFrame (core, *style, p, rp, p.bgCheckerboardPreview);

    // 写 PNG
    juce::File outFile (outPngPath);
    outFile.deleteFile();
    auto ostream = outFile.createOutputStream();
    if (! ostream) {
        result.set ("ok", "false");
        result.set ("error", "cannot open for write: " + outPngPath);
        return result;
    }
    juce::PNGImageFormat pngFmt;
    if (! pngFmt.writeImageToStream (img, *ostream)) {
        result.set ("ok", "false");
        result.set ("error", "PNG write failed");
        return result;
    }
    ostream->flush();

    result.set ("ok", "true");
    result.set ("png_path", outPngPath);
    result.set ("frame_index", juce::String (frameIndex));
    result.set ("size_bytes", juce::String (outFile.getSize()));
    return result;
}

// =============================================================================
// probeSpectrum：数值调试
// =============================================================================
juce::StringPairArray VisPipeline::probeSpectrum (const Config& cfg, int frameIndex)
{
    const auto& p = cfg.params;
    juce::StringPairArray result;

    PcmSource pcm;
    bool hasAudio = false;
    juce::File audioFile = locateAsset (p.audioPath);
    if (audioFile.existsAsFile() && pcm.load (audioFile.getFullPathName())) {
        hasAudio = true;
    }
    const double sr = hasAudio ? pcm.getSampleRate() : 44100.0;

    SpectrumCore core (p);
    core.setSampleRate (sr);

    juce::String err;
    if (! advanceToFrame (pcm, hasAudio, sr, core, p, frameIndex, err)) {
        result.set ("ok", "false");
        result.set ("error", err);
        return result;
    }

    BandFrame bandFrame;
    core.getBandFrame (bandFrame);

    // dump 前 16 带
    std::cout << "=== Probe Spectrum: frame " << frameIndex << " ===\n";
    std::cout << "  bandCount = " << bandFrame.bandCount << "\n";
    std::cout << "  first 16 bands (idx / centerHz / db / peakDb / normalized):\n";
    const int showCount = std::min (16, bandFrame.bandCount);
    for (int i = 0; i < showCount; ++i) {
        // 中心频率（用 FreqMap 算）
        FreqMap fm;
        fm.configure (p.freqScale, p.minHz, p.maxHz, p.bandCount);
        float centerHz = fm.bandCenterHz (i);
        std::cout << "    [" << i << "] "
                  << "f=" << centerHz << "Hz  "
                  << "db=" << bandFrame.db[i] << "  "
                  << "peak=" << bandFrame.peakDb[i] << "  "
                  << "norm=" << bandFrame.normalized[i] << "\n";
    }

    // 也 dump 原始 FFT mag（前 16 bin）
    std::vector<float> magHi;
    core.getRawMagnitudesHi (magHi);
    std::cout << "  raw FFT mag (first 16 bins, fftSize=" << p.fftSize() << "):\n";
    const int showMag = std::min (16, (int) magHi.size());
    for (int i = 0; i < showMag; ++i) {
        double binHz = (sr / 2.0) * (double) i / (double) (magHi.size());
        std::cout << "    bin[" << i << "] " << binHz << "Hz  mag=" << magHi[i] << "\n";
    }

    result.set ("ok", "true");
    result.set ("frame_index", juce::String (frameIndex));
    result.set ("band_count", juce::String (bandFrame.bandCount));
    return result;
}
