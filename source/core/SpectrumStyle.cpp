// =============================================================================
// SpectrumStyle.cpp — 工厂实现
// =============================================================================
#include "SpectrumStyle.h"
#include "../styles/Y2KLineStyle.h"
#include "../styles/BarStyle.h"
#include "../styles/PolylineStyle.h"
#include "../styles/CrystalStyle.h"

std::unique_ptr<SpectrumStyle> SpectrumStyle::create (const juce::String& name)
{
    auto lower = name.toLowerCase().trim();
    if (lower == "y2k-line" || lower == "y2kline" || lower == "y2k")
        return std::make_unique<Y2KLineStyle>();
    if (lower == "bar" || lower == "bars")
        return std::make_unique<BarStyle>();
    if (lower == "polyline" || lower == "line" || lower == "poly")
        return std::make_unique<PolylineStyle>();
    if (lower == "crystal" || lower == "glass")
        return std::make_unique<CrystalStyle>();
    return nullptr;
}
