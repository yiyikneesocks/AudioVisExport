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

    // 色彩调整（v0.5.4 #4）：亮度/对比度/饱和度（1.0=原图，范围 0..2）。
    // 预乘安全：解预乘 → sRGB 空间调整 → 再预乘。全为 1.0 时直接返回原图（零开销）。
    juce::Image adjustedImage (const juce::Image& img,
                               float brightness, float contrast, float saturation);

    // 逐帧共用入口：按 path+三参数 做 LRU-1 缓存的 adjustedImage（滑条拖动只重算一次）。
    // GUI/导出线程共用（双检查锁）。调用方应把结果同时喂给 compose 与 averageColour。
    juce::Image adjustedImageCached (const juce::Image& img, const MaskImageLayer& cfg);

    // 通用键控版（v0.5.4 #6：普通图片图层也用同一套调整/缓存）
    juce::Image adjustedImageCached (const juce::Image& img,
                                     const juce::String& pathKey,
                                     float brightness, float contrast, float saturation);

    // 生成蒙版合成图（输出分辨率 W×H 的 ARGB）：
    //   · 图片定位由 cfg.transform 决定（base/输出坐标系）：
    //       - set=false → 与其他图片图层一致：等比 contain 适配输出画布并居中（v0.5.4 #3）
    //       - set=true  → 按 buildVisAffine(cfg.transform) 把图片本地矩形(0,0,iw,ih)映射过去（可独立缩放/拉伸/旋转/平移）
    //   · 结果 alpha 再被 base 的轮廓 alpha 裁剪（gap / 无电平处不显示图片）
    //   · 若 cfg.strokeEnabled：沿轮廓内侧勾边，四边可独立开关与厚度
    // base / image 都应是已加载好的位图；resolvedStroke 为最终描边色（avg 或手动）。
    // 返回 null 图片 = 无有效轮廓或参数为空。
    juce::Image compose (const juce::Image& base,
                         const juce::Image& image,
                         const MaskImageLayer& cfg,
                         juce::Colour resolvedStroke);

    // ---- v0.5.5 INBOX #5：描边调色板（实时平均色）+ 预览节流/插值 ----

    //   一条描边的**颜色计划**：uniform 或**逐列**（bar 系用列段索引查色）。
    //   列段由 base alpha 自动识别（style-agnostic：任何柱之间无 alpha 的空隙即分界，
    //   柱与柱重叠 gap=0 时自动并成一段 → 语义降级为整块一色，与"可视范围平均"直觉一致）。
    struct StrokePlan
    {
        bool perColumn = false;                    // false → 整块一色（uniform / image 模式）
        juce::Colour uniform { 0xffffffff };
        // 段（bar 列区间）表：perBar 时逐段平均色；perColumn=true 时下面三组等长
        std::vector<int>          segStart, segEnd;
        std::vector<juce::Colour> segColour;
        int colWidth = 0;                          // 铺 colColour 用的 W
        std::vector<juce::Colour> colColour;       // colWidth 大小；perColumn=true 时有效
    };

    //   对**已 clip 好**的 out（step 3 之后的位图）算计划。用**预乘像素"sum(premult)/sum(alpha)"**
    //   技巧直接得到 alpha 加权未预乘平均色（数学等价、零解预乘开销）。
    //   mode == "image" 时退回 cfg.strokeColor（v0.5.4 行为，加载时算过一次）。
    StrokePlan makeStrokePlan (const juce::Image& out,
                               const MaskImageLayer& cfg,
                               juce::Colour fallbackUniform) noexcept;

    //   GUI 预览专用的节流 + 指数插值缓存。**只影响预览**：导出/离线渲染每帧真算，保精确。
    //   · update()：若距上次计算 ≥ 1/previewFps 秒 → 重算 target；再按真实 dt 把 shown
    //     向 target 做**颜色空间线性逼近**（用户说的"平滑渐变到下一秒"），返回可直接
    //     喂给 composeWithPlan 的当前插值计划。
    //   · 时间以秒计；GUI 每 paint tick 传入单调递增的 nowSec。
    class PreviewPaletteCache
    {
    public:
        const StrokePlan& update (double nowSec,
                                  const StrokePlan& freshTarget,
                                  const MaskImageLayer& cfg);
        void reset() noexcept { primed = false; shown = {}; target = {}; lastCompute = lastLerp = -1e9; }
    private:
        bool primed = false;
        double lastCompute = -1e9, lastLerp = -1e9;
        StrokePlan target, shown;
        std::vector<juce::Colour> prevSeg;   // 上一帧逐段色（lerp 起点）
    };

    //   带缓存版 compose（GUI 预览用）：step 1-3 同旧 → 描边前对"无描边的 out"现算 fresh 计划
    //   → 交给 cache 做节流+插值 → 用插值结果画描边。cache=nullptr = 导出/离线：每帧真算（精确）。
    //   ⚠️ 顺序关键：计划必须在描边**之前**取，否则白描边像素会污染平均色。
    juce::Image composeWithPlan (const juce::Image& base,
                                 const juce::Image& image,
                                 const MaskImageLayer& cfg,
                                 juce::Colour resolvedStroke,
                                 PreviewPaletteCache* cache,
                                 double nowSec);
}
