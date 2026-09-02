// =============================================================================
// Y2KLineStyle.h — Y2Kmeter 风格频谱曲线（第一实现，Step 3 完整搬运）
//
// 完全照搬 Y2Kmeter source/ui/modules/SpectrumModule.cpp 的绘制逻辑：
//   · 对数频率轴（默认 20Hz-20kHz）
//   · dBFS 纵轴（默认 -80~0）
//   · 三层：实时半透明 + 平滑主曲线 + 虚线峰值保持
//   · Catmull-Rom → Bezier 平滑路径
//   · 双层描边（外粗半透明 + 内细不透明）
//   · Slope 补偿已在 SpectrumCore 内做（不在 style 重复）
//
// 仅替换：PinkXP 颜色 → RenderParams 颜色；删 hover ruler / PinkXP 字体依赖
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class Y2KLineStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "y2k-line"; }

    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;

private:
    // 频率 → X 像素坐标（对数，照搬 SpectrumModule.cpp:92-98）
    static float freqToX_ (float freqHz, const juce::Rectangle<int>& canvas,
                           float minHz, float maxHz);
    // dBFS → Y 像素坐标
    static float dbToY_ (float db, const juce::Rectangle<int>& canvas,
                         float minDb, float maxDb);
    // Catmull-Rom → Bezier 平滑路径（照搬 SpectrumModule.cpp:501-560）
    void buildSmoothPath_ (juce::Path& path,
                           const std::vector<juce::Point<float>>& P,
                           bool closeToBottom, float yBot, float yTop) const;
    // 网格 + 坐标轴标签
    void drawGrid_ (juce::Graphics& g, const juce::Rectangle<int>& canvas,
                    const RenderParams& rp) const;
    void drawAxisLabels_ (juce::Graphics& g, const juce::Rectangle<int>& canvas,
                          const RenderParams& rp) const;
};
