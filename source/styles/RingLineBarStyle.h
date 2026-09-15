// =============================================================================
// RingLineBarStyle.h — 径向"柱+连线"：放射柱 + 柱顶连成一圈闭合线（极坐标版 bar-line）
//
//   = RingStyle 的辐条柱（外实时/内峰值，独立开关）之上，再沿外圈各柱顶描一条闭合折线。
//   复用 ring 几何参数 + 旋转；线宽用 lineWidth。非轴对齐 → styleSupportsOutline=false。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class RingLineBarStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "ringlinebar"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    float spinDeg_ = 0.0f;
};
