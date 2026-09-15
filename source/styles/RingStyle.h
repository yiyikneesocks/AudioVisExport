// =============================================================================
// RingStyle.h — 径向频谱（柱状绕圆放射；外层实时、内层峰值双层）
//
//   band 值 → 从内半径向外的辐条长度；沿整圈按频带顺序排列（低频→顺时针）。
//   ringLayers=2 时内圈再画一圈"峰值"辐条。整体可匀速旋转（按帧累计，fps 归一）。
//   颜色复用 ColorMap（solid=primary；gradient/rainbow 按带取色）。
//   与基线轴/三边描边无关：本样式 outline 不适用（见 SpectrumMask::styleSupportsOutline）。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class RingStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "ring"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    float spinDeg_ = 0.0f;   // 累计旋转角（帧间状态）
};
