// =============================================================================
// BarStyle.cpp — 传统柱状频谱图（完整实现）
//
// 每带一根柱，柱高 = normalized[i] * canvas.height。
// 柱体：半透明渐变填充（底→顶：primary 透明→primary 不透明）。
// 峰值帽：peakDb 位置画一条细水平线（peakColor）。
// 网格：可选（同 Y2KLineStyle 风格）。
// =============================================================================
#include "BarStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

float BarStyle::freqToX_ (float freqHz, const juce::Rectangle<int>& canvas,
                           float minHz, float maxHz)
{
    const float f = juce::jlimit (minHz, maxHz, freqHz);
    const float t = (std::log10 (f) - std::log10 (minHz))
                  / (std::log10 (maxHz) - std::log10 (minHz));
    return (float) canvas.getX() + t * (float) canvas.getWidth();
}

float BarStyle::normalizedToY_ (float n, const juce::Rectangle<int>& canvas)
{
    const float t = std::clamp (n, 0.0f, 1.0f);
    return (float) canvas.getBottom() - t * (float) canvas.getHeight();
}

void BarStyle::render (juce::Graphics& g,
                       const juce::Rectangle<int>& canvas,
                       const BandFrame& frame,
                       const RenderParams& rp)
{
    // 网格
    if (rp.drawGrid)
    {
        auto inner = canvas.reduced (2);
        if (! inner.isEmpty())
        {
            for (int db = (int) rp.minDb; db <= (int) rp.maxDb; db += 20)
            {
                // dB→normalized 近似：normalized = (db - minDb) / (maxDb - minDb)
                float n = (float) (db - rp.minDb) / (float) (rp.maxDb - rp.minDb);
                int y = (int) std::round (normalizedToY_ (n, canvas));
                if (y < inner.getY() || y > inner.getBottom()) continue;
                g.setColour (db == 0 ? rp.secondary.withAlpha (0.70f)
                                     : rp.secondary.withAlpha (0.22f));
                g.drawHorizontalLine (y, (float) inner.getX(), (float) inner.getRight());
            }
            const float minorFreqs[] = { 30.0f, 50.0f, 200.0f, 300.0f, 500.0f,
                                         2000.0f, 3000.0f, 5000.0f, 15000.0f };
            g.setColour (rp.secondary.withAlpha (0.14f));
            for (float f : minorFreqs)
            {
                int x = (int) std::round (freqToX_ (f, canvas, rp.minHz, rp.maxHz));
                if (x > inner.getX() && x < inner.getRight())
                    g.drawVerticalLine (x, (float) inner.getY(), (float) inner.getBottom());
            }
            const float majorFreqs[] = { 100.0f, 1000.0f, 10000.0f };
            g.setColour (rp.secondary.withAlpha (0.30f));
            for (float f : majorFreqs)
            {
                int x = (int) std::round (freqToX_ (f, canvas, rp.minHz, rp.maxHz));
                if (x > inner.getX() && x < inner.getRight())
                    g.drawVerticalLine (x, (float) inner.getY(), (float) inner.getBottom());
            }
        }
    }

    const int N = frame.bandCount;
    if (N <= 0) return;

    auto inner = canvas.reduced (2);
    if (inner.getWidth() <= 2 || inner.getHeight() <= 2) return;

    // 柱体布局：等距分布。每带占一个 slot，slot 内柱居中：
    //   gap  = slot * barGapRatio          （柱间空隙，0 = 无缝）
    //   barW = (slot - gap) * barWidthRatio（柱宽，>1 时侵入空隙 / 与相邻柱重叠）
    // 默认 gap=0.28、width=1.0 时与旧版（bar=0.72*slot）视觉完全一致。
    // v0.5.4 #25：三联动布局。slot = 带 pitch（两柱锚点间距）；
    //   间隙 gap = pitch − width（可负=重叠）；柱锚点 x 间距用 pitch。
    const float slotW = (float) inner.getWidth() / (float) N;   // #1': 恒铺满横向（pitch 语义=目标带宽%，驱动 bandCount）
    const float gap   = juce::jlimit (-2.48f, 2.48f, rp.barGapRatio) * (float) inner.getWidth() / (float) N;
    const float barW  = juce::jlimit (0.02f, 2.5f, rp.barWidthRatio) * (float) inner.getWidth() / (float) N;
    const float x0    = (float) inner.getX() + (slotW - barW) * 0.5f;
    const float yBot  = (float) inner.getBottom();
    const float yTop  = (float) inner.getY();

    // ColorMap：solid 走原 primary→secondary 竖向渐变（零回归）；gradient/rainbow 逐柱取色。
    ColorMap cm;
    cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();

    // 画柱（solid：底 primary→顶 secondary；colormap：底基色→顶基色淡）
    // v0.5.4 #4：柱以基线轴为零点上下按比例生长（a=0 退化为原底部生长）。
    const float a = juce::jlimit (0.0f, 1.0f, rp.baselineY);
    for (int i = 0; i < N; ++i)
    {
        float n = std::clamp (frame.normalized[i], 0.0f, 1.0f);
        if (n < 0.005f) continue;  // 贴底跳过

        float x = x0 + (float) i * slotW;
        float y    = normalizedToY_ (baselineTop    (n, a), canvas);   // 柱顶
        float yBtm = normalizedToY_ (baselineBottom (n, a), canvas);   // 柱底（轴下臂）
        float h = yBtm - y;
        if (h < 1.0f) continue;

        juce::Colour bottom, top;
        if (useMap)
        {
            const juce::Colour base = cm.colourForBand (i, N, n);
            bottom = base.withAlpha (0.85f);
            top    = base.withAlpha (0.40f);
        }
        else
        {
            bottom = rp.primary.withAlpha (0.85f);
            top    = rp.secondary.withAlpha (0.35f);
        }
        juce::ColourGradient grad (bottom, x, yBtm, top, x, y, false);
        g.setGradientFill (grad);
        g.fillRect (x, y, barW, h);
    }
    // 重置 gradient（JUCE 需手动清除，否则影响后续绘制）
    g.setColour (juce::Colours::white);

    // 柱顶描边（1px 不透明线，增加锐利感；colormap 时逐柱取色）
    if (! useMap) g.setColour (rp.primary);
    for (int i = 0; i < N; ++i)
    {
        float n = std::clamp (frame.normalized[i], 0.0f, 1.0f);
        if (n < 0.005f) continue;
        if (useMap) g.setColour (cm.colourForBand (i, N, n));
        float x = x0 + (float) i * slotW;
        float y = normalizedToY_ (baselineTop (n, a), canvas);
        g.drawHorizontalLine ((int) std::round (y), x, x + barW);
    }

    // 峰值帽（可选）：peakDb → normalized 近似 → Y 位置，画 2px 水平线
    if (rp.barParticles)
    {
        g.setColour (rp.peak.withAlpha (0.80f));
        for (int i = 0; i < N; ++i)
        {
            // peakDb → normalized：(peakDb - minDb) / (maxDb - minDb)
            float pn = (frame.peakDb[i] - rp.minDb) / (rp.maxDb - rp.minDb);
            pn = std::clamp (pn, 0.0f, 1.0f);
            if (pn < 0.01f) continue;
            float x = x0 + (float) i * slotW;
            float y = normalizedToY_ (pn, canvas);
            // 帽宽 = barW + gap*0.5 向两侧延伸
            float capX = x - gap * 0.25f;
            float capW = barW + gap * 0.5f;
            g.drawHorizontalLine ((int) std::round (y), capX, capX + capW);
        }
    }
}
