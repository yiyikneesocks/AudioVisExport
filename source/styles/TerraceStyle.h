// =============================================================================
// TerraceStyle.h — 地形剖面：把过去 N 帧的频谱曲线层层堆叠成"山丘地形"
//
//   每帧把 frame.normalized 存入历史环形缓冲；从后（旧）往前（新）逐层绘制填充曲线，
//   每层沿 y 抬高 depthStep、向后渐隐（terraceFade），前层遮住后层形成伪 3D 层峦效果。
//   颜色复用 ColorMap（solid=primary）。非轴对齐 → styleSupportsOutline=false。
// =============================================================================
#pragma once

#include "../core/SpectrumStyle.h"
#include <vector>

class TerraceStyle : public SpectrumStyle
{
public:
    juce::String getName() const override { return "terrace"; }
    void render (juce::Graphics& g,
                 const juce::Rectangle<int>& canvas,
                 const BandFrame& frame,
                 const RenderParams& rp) override;
private:
    std::vector<std::vector<float>> history_;   // [层][带]，front = 最新
};
