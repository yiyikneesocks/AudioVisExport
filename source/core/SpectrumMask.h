// =============================================================================
// SpectrumMask.h — 频谱蒙版图片（v0.5.4）
//
// 思路（style-agnostic）：频谱样式已把可见形状渲染到一张带 alpha 的 ARGB 层 base。
//   直接把 base 的 alpha 当作"窗口形状"——图片只在该形状内可见。
//   · bar / bar-mirror：柱体各自的 alpha → gap 天然透明 → 图片在 gap 处不显示；
//   · line 系：整块填色的 alpha 轮廓 → 图片填满"顶线到中线"的闭合区。
//   不依赖任何具体样式几何代码，未来加样式自动生效。
//
// 与导出管线 VisPipeline::renderFrame 和 GUI SpectrumCanvas::paint 共用同一函数，
//   保证"预览即所得"。
// =============================================================================
#pragma once

#include <juce_graphics/juce_graphics.h>
#include "SpectrumParams.h"

namespace SpectrumMask
{
    // 计算一张图片的平均色（按其自身 alpha 加权；全透明则返回不透明灰）。
    juce::Colour averageColour (const juce::Image& img);

    // 生成蒙版合成图（输出分辨率 W×H 的 ARGB）：
    //   · 图片定位由 cfg.transform 决定（base/输出坐标系）：
    //       - set=false → 与其他图片图层一致：等比 contain 适配输出画布并居中（v0.5.4 #3）
    //       - set=true  → 按 buildVisAffine(cfg.transform) 把图片本地矩形(0,0,iw,ih)映射过去（可独立缩放/拉伸/旋转/平移）
    //   · 结果 alpha 再被 base 的轮廓 alpha 裁剪（gap / 无电平处不显示图片）
    //   · 若 cfg.strokeEnabled：沿轮廓内侧勾一圈 strokeColor 描边
    // base / image 都应是已加载好的位图；resolvedStroke 为最终描边色（avg 或手动）。
    // 返回 null 图片 = 无有效轮廓或参数为空。
    juce::Image compose (const juce::Image& base,
                         const juce::Image& image,
                         const MaskImageLayer& cfg,
                         juce::Colour resolvedStroke);
}
