// =============================================================================
// SpectrumStyle.cpp — 工厂实现
// =============================================================================
#include "SpectrumStyle.h"
#include "../styles/Y2KLineStyle.h"
#include "../styles/BarStyle.h"
#include "../styles/BarLineStyle.h"
#include "../styles/PolylineStyle.h"
#include "../styles/CrystalStyle.h"

std::unique_ptr<SpectrumStyle> SpectrumStyle::create (const juce::String& name)
{
    auto lower = name.toLowerCase().trim();
    if (lower == "y2k-line" || lower == "y2kline" || lower == "y2k")
        return std::make_unique<Y2KLineStyle>();
    if (lower == "bar" || lower == "bars")
        return std::make_unique<BarStyle>();
    if (lower == "bar-line" || lower == "barline")
        return std::make_unique<BarLineStyle>();
    // v0.5.4 #3.1：bar-mirror 已删除 —— 基线轴（Baseline=50%）+ 双侧峰帽的 bar 是其等价替代；
    //   旧 JSON / --style 仍接受这两个别名并静默映射到 bar，避免预设失效。
    if (lower == "bar-mirror" || lower == "barmirror" || lower == "mirror")
        return std::make_unique<BarStyle>();
    if (lower == "polyline" || lower == "line" || lower == "poly")
        return std::make_unique<PolylineStyle>();
    if (lower == "crystal" || lower == "glass")
        return std::make_unique<CrystalStyle>();
    return nullptr;
}
