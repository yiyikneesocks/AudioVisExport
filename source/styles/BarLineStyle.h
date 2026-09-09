// =============================================================================
// BarLineStyle.h — bar-line 样式：斜面柱顶（v0.5.0 重构）
//
// 每根柱是一个五边形梯形：底边水平、左右边垂直、顶边斜线。
// 柱顶边缘高度 = 相邻两带归一化值的插值中点——相邻柱高度不同时，
// 柱顶自然呈现斜面（如左柱 50、右柱 65，则柱顶从 50 斜升到 65）；
// 相邻柱同高时顶边水平。柱间 gap 保留（斜面在各自柱宽内闭合，
// 不跨 gap 填充）。首柱左缘 / 末柱右缘用自身值，不悬空。
//
// 峰值帽 v3 连续折线（v0.5.4 #2，受 rp.barParticles 开关控制）：
//   所有带的帽顶点连成**一条连续折线**（相邻柱共享拐点 = 头部接尾部），
//   每个拐点恒 ≥ 该处柱顶（柱顶上来 → 立即被顶上去并改变形状）；
//   下落按帧积分：高处落得快（+accel×高度），同时受邻带拉拽
//   （向邻域均值靠拢 → 整体有"变平直"趋势，下坠可被邻带拽慢/提前/反向）；
//   拉拽后再 clamp 回柱顶上方 —— 第一原则（帽永不低于柱顶）最高优先。
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

    // 峰值帽折线状态（每带一个拐点，归一化高度）；随带数变化重建
    std::vector<float> capN_;
};
