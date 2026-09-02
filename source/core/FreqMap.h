// =============================================================================
// FreqMap.h — 频率映射工具（log/linear/mel/bark）
//
// 给定频段范围 [minHz, maxHz] 和 bandCount，提供：
//   · bandCenterHz(i)       —— 第 i 个带的中心频率
//   · bandEdgesHz(i, &f0, &f1) —— 第 i 个带的几何边界（用于 band mapping）
//   · freqToBand(freqHz)    —— 频率落到哪个带（-1 = 范围外）
//
// 各 scale 的几何带边界公式：
//   · log     —— 等分 log10(f)，与 Y2Kmeter getSpectrumMagnitudesBlended 一致
//   · linear  —— 等分 f
//   · mel     —— 等分 mel(f) = 2595 * log10(1 + f/700)
//   · bark    —— 等分 bark(f)（Traunmüller 近似：z = 26.81*f/(1960+f) - 0.53）
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include "SpectrumParams.h"

class FreqMap
{
public:
    FreqMap() = default;

    // 用参数初始化。会在内部预计算每带几何边界，避免热路径重复算。
    void configure (SpectrumParams::FreqScale scale, float minHz, float maxHz, int bandCount);

    int getBandCount() const noexcept { return bandCount_; }
    float getMinHz() const noexcept { return minHz_; }
    float getMaxHz() const noexcept { return maxHz_; }
    SpectrumParams::FreqScale getScale() const noexcept { return scale_; }

    // 第 i 个带的中心频率
    float bandCenterHz (int i) const noexcept;

    // 第 i 个带的几何边界 [f0, f1]
    void bandEdgesHz (int i, float& f0, float& f1) const noexcept;

    // 频率 → 带索引（-1 = 范围外）
    int freqToBand (float freqHz) const noexcept;

    // 工具：物理频率 → mel
    static float hzToMel (float hz) noexcept;
    static float melToHz (float mel) noexcept;
    // 工具：物理频率 → bark（Traunmüller）
    static float hzToBark (float hz) noexcept;
    static float barkToHz (float z) noexcept;

private:
    SpectrumParams::FreqScale scale_ = SpectrumParams::Log;
    float minHz_ = 20.0f;
    float maxHz_ = 20000.0f;
    int   bandCount_ = 160;

    // 预计算：每带在"映射域"里的边界（log 域 / linear 域 / mel 域 / bark 域）
    // center = (edge[i] + edge[i+1]) / 2，在映射域里取中再转回 Hz。
    std::vector<float> mappedEdges_;

    // 映射域 → Hz 的反函数
    float mapForward_ (float hz) const noexcept;   // Hz → 映射域
    float mapInverse_ (float v) const noexcept;    // 映射域 → Hz
};
