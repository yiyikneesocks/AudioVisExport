// =============================================================================
// SpectrogramStyle.h — 频谱瀑布图（时间右→左滚动的像素热力图，复古方块风）
//
//   内部持有一张滚动 ARGB 画布 acc_：每帧先把它整体左移 dpx（并按 persistence 逐帧衰减透明度
//   做拖尾），再在最右端写入"当前频谱列"。频率=纵轴（低频在下），亮度/色=能量（ColorMap），
//   cellSize 控制方块粒度。bandCount/尺寸变化 → acc_ 重置（resize 安全）。
//   非轴对齐满幅块状 → styleSupportsOutline=false（瀑布无"顶/左右边"概念）。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"

class SpectrogramStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "spectrogram"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    juce::Image acc_;                 // 滚动累积画布（canvas 尺寸）
    int lastBandCount_ = -1;
};
