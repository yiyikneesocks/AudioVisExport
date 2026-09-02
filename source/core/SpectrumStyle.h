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

        // 元素开关
        bool  drawGrid        = true;
        bool  drawAxisLabels  = true;

        // 镜像 SpectrumParams::freq / dynamic 给画轴用
        float minHz = 20.0f,  maxHz = 20000.0f;
        float minDb = -80.0f, maxDb = 0.0f;

        // colorMap 名（若 style 想按强度上色）
        juce::String colorMap = "solid";
    };

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
