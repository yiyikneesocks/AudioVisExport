// =============================================================================
// RingLineStyle.h — 径向"线"谱：把各频带值连成一圈闭合极坐标曲线
//
//   ringline = 极坐标版的 polyline/y2k-line：不是分立辐条，而是一条绕圆闭合的折线轮廓。
//   外圈(实时 ringOuterOn)/内圈(峰值 ringInnerOn)各画一条闭合环；可旋转、复用 ring 几何参数、
//   线宽用 lineWidth。非轴对齐 → styleSupportsOutline=false。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class RingLineStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "ringline"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    float spinDeg_ = 0.0f;
};
