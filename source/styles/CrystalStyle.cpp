// =============================================================================
// CrystalStyle.cpp — 水晶/玻璃质感频谱曲线（第一版实现）
//
// 三 pass 合成"水晶效果"：
//   pass 0 — 辉光：4 层递减粗细描边（14px@0.04 → 10px@0.07 → 6px@0.12 → 3px@0.20）
//             模拟 bloom，颜色用 primary 散开
//   pass 1 — 玻璃体：垂直渐变填充（底 primary@0.50 → 顶 secondary@0.15）
//             + 主曲线双层描边（外 2.5px@0.40 + 内 lineWidth px@0.90）
//   pass 2 — 高光：1px 亮白线沿曲线顶部，alpha=0.55，模拟玻璃边缘反光
//
// render() = 按序调用三个 renderPass()（单 Graphics 上叠加，等价于多图层合成）。
// 未来可改为真多 Image 合成以支持 blend mode / blur filter。
// =============================================================================
#include "CrystalStyle.h"
#include <cmath>
#include <algorithm>

float CrystalStyle::dbToY_ (float db, const juce::Rectangle<int>& canvas,
                              float minDb, float maxDb)
{
    const float d = juce::jlimit (minDb, maxDb, db);
    const float t = (d - minDb) / (maxDb - minDb);
    return (float) canvas.getBottom() - t * (float) canvas.getHeight();
}

void CrystalStyle::buildSmoothPath_ (juce::Path& path,
                                      const std::vector<juce::Point<float>>& P,
                                      bool closeToBottom, float yBot, float yTop) const
{
    const int n = (int) P.size();
    if (n < 2) return;

    constexpr float floorPixelTol = 0.75f;
    auto atFloor = [yBot, floorPixelTol] (const juce::Point<float>& p) noexcept
    {
        return (yBot - p.y) <= floorPixelTol;
    };

    if (closeToBottom) {
        path.startNewSubPath (P[0].x, yBot);
        path.lineTo (P[0]);
    } else {
        path.startNewSubPath (P[0]);
    }

    for (int i = 0; i < n - 1; ++i)
    {
        const auto& p0 = (i == 0)     ? P[0]     : P[i - 1];
        const auto& p1 = P[i];
        const auto& p2 = P[i + 1];
        const auto& p3 = (i + 2 >= n) ? P[n - 1] : P[i + 2];

        if (atFloor (p1) && atFloor (p2))
        {
            path.lineTo (p2.x, yBot);
            continue;
        }

        const float c1x = p1.x + (p2.x - p0.x) / 6.0f;
        const float c2x = p2.x - (p3.x - p1.x) / 6.0f;
        float c1y       = p1.y + (p2.y - p0.y) / 6.0f;
        float c2y       = p2.y - (p3.y - p1.y) / 6.0f;

        if (atFloor (p1)) c1y = p1.y;
        if (atFloor (p2)) c2y = p2.y;

        c1y = juce::jlimit (yTop, yBot, c1y);
        c2y = juce::jlimit (yTop, yBot, c2y);

        path.cubicTo (c1x, c1y, c2x, c2y, p2.x, p2.y);
    }

    if (closeToBottom)
    {
        path.lineTo (P[n - 1].x, yBot);
        path.closeSubPath();
    }
}

void CrystalStyle::buildCurvePoints_ (std::vector<juce::Point<float>>& pts,
                                       const BandFrame& frame,
                                       const juce::Rectangle<int>& canvas,
                                       const RenderParams& rp) const
{
    const int N = frame.bandCount;
    pts.resize ((size_t) N);
    auto inner = canvas.reduced (2);
    const float x0   = (float) inner.getX();
    const float xLen = (float) inner.getWidth();
    const float invN = 1.0f / (float) juce::jmax (1, N - 1);
    for (int i = 0; i < N; ++i)
    {
        const float x = x0 + (float) i * invN * xLen;
        const float y = dbToY_ (frame.db[i], canvas, rp.minDb, rp.maxDb);
        pts[(size_t) i] = { x, y };
    }
}

// -----------------------------------------------------------------------------
// render = 三个 pass 按序在同一 Graphics 上叠加
// -----------------------------------------------------------------------------
void CrystalStyle::render (juce::Graphics& g,
                            const juce::Rectangle<int>& canvas,
                            const BandFrame& frame,
                            const RenderParams& rp)
{
    for (int pass = 0; pass < getNumPasses(); ++pass)
        renderPass (g, pass, canvas, frame, rp);
}

// -----------------------------------------------------------------------------
// renderPass
// -----------------------------------------------------------------------------
void CrystalStyle::renderPass (juce::Graphics& g, int pass,
                                const juce::Rectangle<int>& canvas,
                                const BandFrame& frame,
                                const RenderParams& rp)
{
    const int N = frame.bandCount;
    if (N <= 1) return;

    auto inner = canvas.reduced (2);
    if (inner.getWidth() <= 2 || inner.getHeight() <= 2) return;

    std::vector<juce::Point<float>> pts;
    buildCurvePoints_ (pts, frame, canvas, rp);

    const float yTop = (float) inner.getY();
    const float yBot = (float) inner.getBottom();

    // ---- pass 0: 辉光 glow ----
    if (pass == 0)
    {
        juce::Path curvePath;
        buildSmoothPath_ (curvePath, pts, false, yBot, yTop);

        // 4 层递减粗细描边，模拟 bloom
        const struct { float width; float alpha; } layers[] = {
            { 14.0f, 0.04f },
            { 10.0f, 0.07f },
            {  6.0f, 0.12f },
            {  3.0f, 0.20f },
        };
        for (const auto& lyr : layers)
        {
            g.setColour (rp.primary.withAlpha (lyr.alpha));
            g.strokePath (curvePath, juce::PathStrokeType (lyr.width));
        }
        return;
    }

    // ---- pass 1: 玻璃体 glass body ----
    if (pass == 1)
    {
        // 半透明渐变填充
        juce::Path fillPath;
        buildSmoothPath_ (fillPath, pts, true, yBot, yTop);

        juce::ColourGradient grad (rp.primary.withAlpha (0.50f),
                                   0.0f, yBot,
                                   rp.secondary.withAlpha (0.15f),
                                   0.0f, yTop,
                                   false);
        grad.point1 = juce::Point<float> ((float) inner.getX(), yBot);
        grad.point2 = juce::Point<float> ((float) inner.getX(), yTop);
        g.setGradientFill (grad);
        g.fillPath (fillPath);
        g.setColour (juce::Colours::white);  // 清除 gradient

        // 主曲线双层描边
        juce::Path curvePath;
        buildSmoothPath_ (curvePath, pts, false, yBot, yTop);

        g.setColour (rp.primary.withAlpha (0.40f));
        g.strokePath (curvePath, juce::PathStrokeType (2.5f));
        g.setColour (rp.primary.withAlpha (0.90f));
        g.strokePath (curvePath, juce::PathStrokeType (rp.lineWidth));
        return;
    }

    // ---- pass 2: 高光 highlight ----
    if (pass == 2)
    {
        // 1px 亮白线沿曲线顶部
        juce::Path curvePath;
        buildSmoothPath_ (curvePath, pts, false, yBot, yTop);

        // 高光色：primary 的亮度提升版（混入白色）
        juce::Colour highlight = rp.primary.brighter (2.5f).withAlpha (0.55f);
        g.setColour (highlight);
        g.strokePath (curvePath, juce::PathStrokeType (1.0f));

        // 峰值虚线（与 Y2KLineStyle 一致，保持功能对等）
        std::vector<juce::Point<float>> peakPts ((size_t) N);
        const float x0   = (float) inner.getX();
        const float xLen = (float) inner.getWidth();
        const float invN = 1.0f / (float) juce::jmax (1, N - 1);
        for (int i = 0; i < N; ++i)
        {
            const float x = x0 + (float) i * invN * xLen;
            const float y = dbToY_ (frame.peakDb[i], canvas, rp.minDb, rp.maxDb);
            peakPts[(size_t) i] = { x, y };
        }
        juce::Path peakPath;
        buildSmoothPath_ (peakPath, peakPts, false, yBot, yTop);

        juce::Path dashedPeak;
        const float dashes[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashedPeak, peakPath, dashes, 2);
        g.setColour (rp.peak.withAlpha (0.60f));
        g.fillPath (dashedPeak);
        return;
    }
}
