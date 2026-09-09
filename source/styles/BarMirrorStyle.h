// =============================================================================
// BarMirrorStyle.h — 上下镜像柱状频谱（DJ 风格双频谱）
//
// 每带一根柱，从中线向上下两侧对称生长（高度 = normalized[i] * 半高）。
// 上半：向上生长；下半：镜像向下。colormap（gradient/rainbow）逐柱取色，
// solid 走 primary(近中线)→secondary(远端) 竖向渐变。峰值帽上下各一条。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class BarMirrorStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "bar-mirror"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    static float normalizedToHalf_ (float n, const juce::Rectangle<int>& canvas);
};
