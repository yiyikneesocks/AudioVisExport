// =============================================================================
// BarMirrorStyle.cpp — 上下镜像柱状频谱（实现）
//
// 中线 yMid = inner 垂直中心；每带上柱 [yMid-h, yMid]、下柱 [yMid, yMid+h]。
// h = normalized[i] * 半高。布局参数（gap / barW）与 BarStyle 完全一致。
// =============================================================================
#include "BarMirrorStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

float BarMirrorStyle::normalizedToHalf_ (float n, const juce::Rectangle<int>& canvas)
{
    auto inner = canvas.reduced (2);
    const float t = std::clamp (n, 0.0f, 1.0f);
    return t * (float) inner.getHeight() * 0.5f;   // 半高内映射
}

void BarMirrorStyle::render (juce::Graphics& g,
                             const juce::Rectangle<int>& canvas,
                             const BandFrame& frame,
                             const RenderParams& rp)
{
    const int N = frame.bandCount;
    if (N <= 0) return;

    auto inner = canvas.reduced (2);
    if (inner.getWidth() <= 2 || inner.getHeight() <= 2) return;

    // v0.5.4 #25：三联动布局（与 BarStyle 同式）
    const float slotW = (float) inner.getWidth() / (float) N * juce::jlimit (0.05f, 2.5f, rp.barPitchRatio);
    const float gap   = juce::jlimit (-2.48f, 2.48f, rp.barGapRatio) * (float) inner.getWidth() / (float) N;
    const float barW  = juce::jlimit (0.02f, 2.5f, rp.barWidthRatio) * (float) inner.getWidth() / (float) N;
    const float x0    = (float) inner.getX() + (slotW - barW) * 0.5f;
    const float yMid  = (float) inner.getCentreY();

    ColorMap cm;
    cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();

    for (int i = 0; i < N; ++i)
    {
        const float n = std::clamp (frame.normalized[i], 0.0f, 1.0f);
        if (n < 0.005f) continue;

        const float x = x0 + (float) i * slotW;
        const float h = normalizedToHalf_ (n, canvas);
        if (h < 1.0f) continue;

        const juce::Colour base = useMap ? cm.colourForBand (i, N, n) : rp.primary;
        const juce::Colour near = base.withAlpha (0.90f);           // 靠中线：实
        const juce::Colour far  = useMap ? base.withAlpha (0.35f)
                                         : rp.secondary.withAlpha (0.35f);   // 远端：淡

        // 上半（向上生长：底@中线=base，顶=far）
        juce::ColourGradient up (near, 0.0f, yMid, far, 0.0f, yMid - h, false);
        g.setGradientFill (up);
        g.fillRect (x, yMid - h, barW, h);

        // 下半（向下生长：顶@中线=base，底=far）
        juce::ColourGradient dn (near, 0.0f, yMid, far, 0.0f, yMid + h, false);
        g.setGradientFill (dn);
        g.fillRect (x, yMid, barW, h);
    }
    g.setColour (juce::Colours::white);

    // 峰值帽（可选，上下对称）
    if (rp.barParticles)
    {
        for (int i = 0; i < N; ++i)
        {
            float pn = (frame.peakDb[i] - rp.minDb) / (rp.maxDb - rp.minDb);
            pn = std::clamp (pn, 0.0f, 1.0f);
            if (pn < 0.01f) continue;

            const float x    = x0 + (float) i * slotW;
            const float hp   = normalizedToHalf_ (pn, canvas);
            const float capX = x - gap * 0.25f;
            const float capW = barW + gap * 0.5f;
            g.setColour (rp.peak.withAlpha (0.80f));
            g.drawHorizontalLine ((int) std::round (yMid - hp), capX, capX + capW);   // 上
            g.drawHorizontalLine ((int) std::round (yMid + hp), capX, capX + capW);   // 下
        }
    }
}
