// =============================================================================
// BarLineStyle.h — bar-line 样式：斜面柱顶（v0.5.0 重构）
//
// 每根柱是一个五边形梯形：底边水平、左右边垂直、顶边斜线。
// 柱顶边缘高度 = 相邻两带归一化值的插值中点——相邻柱高度不同时，
// 柱顶自然呈现斜面（如左柱 50、右柱 65，则柱顶从 50 斜升到 65）；
// 相邻柱同高时顶边水平。柱间 gap 保留（斜面在各自柱宽内闭合，
// 不跨 gap 填充）。首柱左缘 / 末柱右缘用自身值，不悬空。
//
// 峰值帽（v0.5.0 状态机 v2，受 rp.barParticles 开关控制）：不再是水平线——
// 每带一个"形状捕获器"：
//   · 仅在峰值刷新时（peakDb 上升 = 顶到新峰）捕获帽形状 = 当时柱顶斜率
//     （端点偏移 = 当时柱顶缘 y − 当时峰值 y，含斜率）；
//   · 峰值持平或下落 → 冻结形状，帽位置随 y(peakDb) 移动：
//     保持期顶部下沉 → 斜线帽悬停在柱顶上方清晰可见；
//     下落期整条斜线刚性下落（斜率保持到下次顶到新峰）。
// 帽 x 范围伸出柱两侧各 gap/4（沿用 v0.4 可见性），端点 y 沿斜率线性外推。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"
#include <vector>

class BarLineStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "bar-line"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    static float normalizedToY_ (float n, const juce::Rectangle<int>& canvas);

    // 峰值帽形状状态（每带）：lastPeakDb 检测下落；capOffL/R 冻结的端点偏移
    // （相对 y(peakDb) 的像素偏移，捕获于峰值最近一次"顶到"的时刻）
    std::vector<float> lastPeakDb_;
    std::vector<float> capOffL_, capOffR_;
};
