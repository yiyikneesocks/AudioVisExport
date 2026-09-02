// =============================================================================
// CrystalStyle.h — 水晶/玻璃质感频谱曲线（第一版）
//
// 验证"多 pass 可插拔"架构（SpectrumStyle::getNumPasses / renderPass）。
// 三层 pass：
//   pass 0 — 辉光（glow）：多层粗描边模拟 bloom，primary 色散光
//   pass 1 — 玻璃体（glass body）：垂直渐变半透明填充 + 主曲线描边
//   pass 2 — 高光（highlight）：顶部亮线模拟玻璃反光
//
// 曲线使用 Catmull-Rom → Bezier 平滑（同 Y2KLineStyle）。
//
// 后续扩展路：
//   · 真 blur（GaussianBlur ImageFilter）替换多层粗描边
//   · 折射/色散（RGB 分别偏移）
//   · 粒子/光斑沿曲线流动
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class CrystalStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "crystal"; }

    // 多 pass 声明
    int getNumPasses() const noexcept override { return 3; }

    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;

    void renderPass (juce::Graphics& g, int pass,
                     const juce::Rectangle<int>& canvas,
                     const BandFrame& frame,
                     const RenderParams& rp) override;

private:
    static float dbToY_ (float db, const juce::Rectangle<int>& canvas,
                         float minDb, float maxDb);

    // Catmull-Rom → Bezier 平滑路径（同 Y2KLineStyle 实现）
    void buildSmoothPath_ (juce::Path& path,
                           const std::vector<juce::Point<float>>& P,
                           bool closeToBottom, float yBot, float yTop) const;

    // 采样点 → 像素坐标（供三个 pass 共用）
    void buildCurvePoints_ (std::vector<juce::Point<float>>& pts,
                            const BandFrame& frame,
                            const juce::Rectangle<int>& canvas,
                            const RenderParams& rp) const;
};
