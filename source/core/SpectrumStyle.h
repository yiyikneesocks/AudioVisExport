// =============================================================================
// SpectrumStyle.h — 频谱样式抽象基类 + 工厂
//
// 设计意图：
//   · 每个样式 = 一个 cpp 文件，实现 render() 即可。
//   · 可插拔：SpectrumStyle::create(name) 返回具体子类。
//   · 多 pass 钩子为未来"半透明水晶效果"留路：style 声明 getNumPasses() > 1，
//     调用方按 pass 顺序渲染到独立 ARGB Image，最后合成。
//
// 后续大工程衔接：剪辑软件可在 timeline 里给每段频谱图选不同 style + 参数。
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <memory>
#include "core/BandFrame.h"
#include "core/SpectrumParams.h"

class SpectrumStyle
{
public:
    // 渲染参数（由 VisPipeline 从 SpectrumParams 镜像出来，避免 style 直接依赖完整参数结构）
    struct RenderParams
    {
        int width  = 1280;
        int height = 720;

        // 颜色（bgColor 默认 0x00000000 全透明）
        juce::Colour primary   { 0xffec4899 };
        juce::Colour secondary { 0xfff9a8d4 };
        juce::Colour peak      { 0xffbe185d };
        juce::Colour bg        { 0x00000000 };

        // 几何（边距，单位 px）
        float paddingLeft = 32.0f, paddingRight = 6.0f;
        float paddingTop  = 4.0f,  paddingBottom = 16.0f;

        // 描线
        float lineWidth = 1.4f;
        float opacity   = 1.0f;

        // BarStyle 专用：柱宽 / 柱间隙（相对每带 slot 宽度的比例）
        float barPitchRatio = 1.0f;   // v0.5.4 #25: 两柱锚点间距（×slot）；gap = pitch − width
        float barGapRatio   = 0.28f;  // 柱间空隙比例（可负 = 重叠）
        float barWidthRatio = 0.72f;  // 柱宽（×slot）
        float fps           = 30.0f;  // #2 峰帽动画用（下落/拉拽按帧积分）
        float baselineY = 0.0f;         // v0.5.4 #4 基线轴（0=底, 0.5=镜像）
        float peakDecayDbPerSec        = 12.0f;  // #2: 峰帽下落速度（dB/s，与 core 同源）
        float peakDecayAccelDbPerSec2  = 0.0f;   // #2: 峰帽下落加速度（dB/s²）
        bool  barParticles  = true;   // bar / bar-line：峰值帽（缓慢下落的小横线）开关

        // 元素开关
        bool  drawGrid        = true;
        bool  drawAxisLabels  = true;

        // 镜像 SpectrumParams::freq / dynamic 给画轴用
        float minHz = 20.0f,  maxHz = 20000.0f;
        float minDb = -80.0f, maxDb = 0.0f;

        // colorMap 名（若 style 想按强度上色）
        juce::String colorMap = "solid";
    };

    // ---- v0.5.4 #4 基线轴 ----
    // 值 n（0..1）在轴位 a（0=底,1=顶）下的显示区间（归一化画布高）：
    //   柱以轴为零点上下按比例生长：上臂 n×(1−a)，下臂 n×a；
    //   n=1 恒跨满 [0,1]（"上到100下到0"）；a=0.5 即镜像；n=0 高度 0（贴轴不可见）。
    inline float baselineTop    (float n, float a) noexcept { return a + (1.0f - a) * n; }
    inline float baselineBottom (float n, float a) noexcept { return a * (1.0f - n); }

    virtual ~SpectrumStyle() = default;

    // 样式名（与工厂 create 参数一致）
    virtual juce::String getName() const = 0;

    // 单 pass 入口。Graphics 已经绑到目标 ARGB Image。
    // 画布矩形是去掉边距后的有效绘制区域（由调用方算好后传入）。
    virtual void render (juce::Graphics& g,
                         const juce::Rectangle<int>& canvas,
                         const BandFrame& frame,
                         const RenderParams& rp) = 0;

    // 多 pass 钩子（未来水晶效果）。默认 = 单 pass。
    //   pass 0: 背景/底纹
    //   pass 1: 主体
    //   pass 2: 高光/水晶叠加
    // 调用方根据 getNumPasses() 决定渲染几次（每次给独立 ARGB Image，最后合成）。
    virtual int getNumPasses() const noexcept { return 1; }
    virtual void renderPass (juce::Graphics& g, int pass,
                             const juce::Rectangle<int>& canvas,
                             const BandFrame& frame,
                             const RenderParams& rp)
    {
        juce::ignoreUnused (pass);
        render (g, canvas, frame, rp);
    }

    // 工厂：name=y2k-line|bar|polyline → 具体子类
    // 未知 name 返回 nullptr（调用方报错）。
    static std::unique_ptr<SpectrumStyle> create (const juce::String& name);
};
