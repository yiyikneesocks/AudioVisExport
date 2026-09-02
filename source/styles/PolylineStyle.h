// =============================================================================
// PolylineStyle.h — 折线形频谱图（点 + 斜直线连接）
//
// 与 Y2KLineStyle 的贝塞尔曲线对照：直接 lineTo 连接，无 cubicTo 平滑。
// 可选在每个采样点画 1px 圆点。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class PolylineStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "polyline"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    static float freqToX_ (float freqHz, const juce::Rectangle<int>& canvas,
                           float minHz, float maxHz);
    static float dbToY_ (float db, const juce::Rectangle<int>& canvas,
                         float minDb, float maxDb);
};
