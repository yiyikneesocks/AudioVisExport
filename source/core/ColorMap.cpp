// =============================================================================
// ColorMap.cpp — 颜色映射实现
// =============================================================================
#include "ColorMap.h"
#include <cmath>
#include <algorithm>

void ColorMap::configure (const juce::String& name,
                          juce::Colour primary,
                          juce::Colour secondary,
                          juce::Colour peak)
{
    name_ = name.toLowerCase().trim();
    primaryColor_   = primary;
    secondaryColor_ = secondary;
    peakColor_      = peak;
}

juce::Colour ColorMap::map (float intensity) const noexcept
{
    const float t = std::clamp (intensity, 0.0f, 1.0f);

    if (name_ == "solid") {
        return primaryColor_;
    }
    if (name_ == "gradient") {
        // primary (t=0) → secondary (t=1)
        const float r = primaryColor_.getFloatRed()   * (1.0f - t) + secondaryColor_.getFloatRed()   * t;
        const float g = primaryColor_.getFloatGreen() * (1.0f - t) + secondaryColor_.getFloatGreen() * t;
        const float b = primaryColor_.getFloatBlue()  * (1.0f - t) + secondaryColor_.getFloatBlue()  * t;
        const float a = primaryColor_.getFloatAlpha() * (1.0f - t) + secondaryColor_.getFloatAlpha() * t;
        return juce::Colour::fromFloatRGBA (r, g, b, a);
    }
    if (name_ == "rainbow") {
        // HSL 色环：t=0 → 红(0°)，t=1 → 紫(300°)
        const float hue = (300.0f * t) / 360.0f;
        return juce::Colour::fromHSV (hue, 0.85f, 0.55f, primaryColor_.getFloatAlpha());
    }

    // 默认 = solid
    return primaryColor_;
}

juce::Colour ColorMap::colourForBand (int bandIdx, int bandCount, float intensity) const noexcept
{
    if (name_ == "gradient")
        return map (intensity);                                   // 按该带强度上色
    if (name_ == "rainbow")
    {
        const float t = bandCount > 1
                      ? std::clamp ((float) bandIdx / (float) (bandCount - 1), 0.0f, 1.0f)
                      : 0.0f;
        return map (t);                                           // 按频率位置铺彩虹
    }
    return primaryColor_;
}

juce::ColourGradient ColorMap::horizontalGradient (float x0, float x1, float y, float alpha) const
{
    // solid：两端同色（等价纯色，调用方一般走 isSolid 分支用 primary）
    // gradient / rainbow：沿频率轴取 8 个采样点
    juce::ColourGradient grad (map (0.0f).withAlpha (alpha), x0, y,
                               map (1.0f).withAlpha (alpha), x1, y, false);
    if (! isSolid())
        for (int k = 1; k < 8; ++k)
            grad.addColour (k / 8.0f, map (k / 8.0f).withAlpha (alpha));
    return grad;
}
