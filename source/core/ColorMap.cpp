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
