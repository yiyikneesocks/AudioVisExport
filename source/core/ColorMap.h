// =============================================================================
// ColorMap.h — colorMap 工具：把 [0,1] 强度映射到颜色
//
// 当前实现：
//   · solid      —— 返回固定 primaryColor
//   · gradient   —— primaryColor ↔ secondaryColor 线性插值
//   · rainbow    —— HSL 色环
// 后续可加 magma/viridis 等科学配色
// =============================================================================
#pragma once

#include <juce_graphics/juce_graphics.h>
#include "SpectrumParams.h"

class ColorMap
{
public:
    ColorMap() = default;

    // 用参数初始化（取 visual.* 颜色）
    void configure (const juce::String& name,
                    juce::Colour primary,
                    juce::Colour secondary,
                    juce::Colour peak);

    // 强度 [0,1] → 颜色
    juce::Colour map (float intensity) const noexcept;

    // 峰值专用颜色
    juce::Colour peakColor() const noexcept { return peakColor_; }

    // 当前 colorMap 名
    juce::String getName() const noexcept { return name_; }

private:
    juce::String name_ { "solid" };
    juce::Colour primaryColor_   { 0xffec4899 };
    juce::Colour secondaryColor_ { 0xfff9a8d4 };
    juce::Colour peakColor_      { 0xffbe185d };
};
