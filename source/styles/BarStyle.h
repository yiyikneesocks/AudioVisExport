// =============================================================================
// BarStyle.h — 传统柱状频谱图（每带一柱）
//
// 每带宽度 = bandWidthPx，间距 = bandGapPx；柱顶可加 cap 块。
// 高度 = normalized[i] * canvas.height。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class BarStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "bar"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    static float freqToX_ (float freqHz, const juce::Rectangle<int>& canvas,
                           float minHz, float maxHz);
    static float normalizedToY_ (float n, const juce::Rectangle<int>& canvas);
};
