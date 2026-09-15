// =============================================================================
// RingLineBarStyle.h — 径向 bar-line（极坐标 bar-line，样式名 ring:bar-line）
//
//   每段是一个扇形条带：外缘为"本带半径→下一带半径"的斜弦，相邻段共享外缘端点
//   → 柱顶连成一条**锯齿斜接脊线**（正是直角 bar-line 的极坐标版），而非一圈闭合线。
//   外圈(实时)/内圈(峰值)独立开关；复用 ring 几何/旋转。非轴对齐 → styleSupportsOutline=false。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class RingLineBarStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "ring:bar-line"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    float spinDeg_ = 0.0f;
};
