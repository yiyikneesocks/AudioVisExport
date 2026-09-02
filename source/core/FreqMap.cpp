// =============================================================================
// FreqMap.cpp — 频率映射工具实现
// =============================================================================
#include "FreqMap.h"
#include <cmath>
#include <algorithm>

// -----------------------------------------------------------------------------
// mel / bark 转换公式
// -----------------------------------------------------------------------------
float FreqMap::hzToMel (float hz) noexcept
{
    return 2595.0f * std::log10 (1.0f + hz / 700.0f);
}

float FreqMap::melToHz (float mel) noexcept
{
    return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f);
}

float FreqMap::hzToBark (float hz) noexcept
{
    // Traunmüller 1990 近似（数值稳定，比 Schroeder 简单）
    float z = 26.81f * hz / (1960.0f + hz) - 0.53f;
    if (z < 2.0f)    z += 0.15f * (2.0f - z);
    if (z > 20.1f)   z += 0.22f * (z - 20.1f);
    return z;
}

float FreqMap::barkToHz (float z) noexcept
{
    // Traunmüller 反函数（近似）
    if (z < 2.0f)    z = (z - 0.3f) / 0.85f;
    if (z > 20.1f)   z = (z - 4.422f) / 1.0f;  // 简化反函数，精度足够
    float hz = 1960.0f * (z + 0.53f) / (26.28f - z);
    return hz;
}

// -----------------------------------------------------------------------------
// 内部：Hz ↔ 映射域
// -----------------------------------------------------------------------------
float FreqMap::mapForward_ (float hz) const noexcept
{
    switch (scale_) {
        case SpectrumParams::Log:    return std::log10 (std::max (hz, 1e-9f));
        case SpectrumParams::Linear: return hz;
        case SpectrumParams::Mel:    return hzToMel (hz);
        case SpectrumParams::Bark:   return hzToBark (hz);
    }
    return std::log10 (std::max (hz, 1e-9f));
}

float FreqMap::mapInverse_ (float v) const noexcept
{
    switch (scale_) {
        case SpectrumParams::Log:    return std::pow (10.0f, v);
        case SpectrumParams::Linear: return v;
        case SpectrumParams::Mel:    return melToHz (v);
        case SpectrumParams::Bark:   return barkToHz (v);
    }
    return std::pow (10.0f, v);
}

// -----------------------------------------------------------------------------
// configure：预计算每带几何边界
// -----------------------------------------------------------------------------
void FreqMap::configure (SpectrumParams::FreqScale scale, float minHz, float maxHz, int bandCount)
{
    scale_ = scale;
    minHz_ = std::max (1e-3f, minHz);
    maxHz_ = std::max (minHz_ * 2.0f, maxHz);
    bandCount_ = std::max (1, bandCount);

    const float vMin = mapForward_ (minHz_);
    const float vMax = mapForward_ (maxHz_);
    const float step = (vMax - vMin) / static_cast<float> (bandCount_);

    mappedEdges_.resize (bandCount_ + 1);
    for (int i = 0; i <= bandCount_; ++i)
        mappedEdges_[i] = vMin + step * static_cast<float> (i);
}

// -----------------------------------------------------------------------------
// 带中心频率
// -----------------------------------------------------------------------------
float FreqMap::bandCenterHz (int i) const noexcept
{
    if (i < 0 || i >= bandCount_ || mappedEdges_.empty()) return 0.0f;
    const float v = 0.5f * (mappedEdges_[i] + mappedEdges_[i + 1]);
    return mapInverse_ (v);
}

// -----------------------------------------------------------------------------
// 带几何边界
// -----------------------------------------------------------------------------
void FreqMap::bandEdgesHz (int i, float& f0, float& f1) const noexcept
{
    if (i < 0 || i >= bandCount_ || mappedEdges_.empty()) { f0 = f1 = 0.0f; return; }
    f0 = mapInverse_ (mappedEdges_[i]);
    f1 = mapInverse_ (mappedEdges_[i + 1]);
}

// -----------------------------------------------------------------------------
// 频率 → 带索引
// -----------------------------------------------------------------------------
int FreqMap::freqToBand (float freqHz) const noexcept
{
    if (freqHz < minHz_ || freqHz > maxHz_) return -1;
    const float v = mapForward_ (freqHz);
    const float vMin = mapForward_ (minHz_);
    const float vMax = mapForward_ (maxHz_);
    if (vMax <= vMin) return 0;
    const float t = (v - vMin) / (vMax - vMin);
    int idx = static_cast<int> (t * static_cast<float> (bandCount_));
    if (idx >= bandCount_) idx = bandCount_ - 1;
    if (idx < 0) idx = 0;
    return idx;
}
