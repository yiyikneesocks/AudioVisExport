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
#include "../core/ColorMap.h"
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

std::vector<juce::Point<float>> CrystalStyle::buildMirrorPoints_ (
        const std::vector<juce::Point<float>>& up, float a,
        const juce::Rectangle<int>& canvas, const RenderParams& rp)
{
    // 下臂点 = baselineBottom(nv)：由上臂点 y 反推 nv（同一像素映射），再映射下臂
    auto inner = canvas.reduced (2);
    const float bottom = (float) inner.getBottom();
    const float H = (float) canvas.getHeight();
    const float span = juce::jmax (1.0f, rp.maxDb - rp.minDb);
    std::vector<juce::Point<float>> dn (up.size());
    for (size_t i = 0; i < up.size(); ++i)
    {
        const float nrm = juce::jlimit (0.0f, 1.0f, (bottom - up[i].getY()) / H / juce::jmax (0.001f, 1.0f - a));
        dn[i] = juce::Point<float> (up[i].getX(), bottom - SpectrumStyle::baselineBottom (nrm, a) * H);
    }
    return dn;
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
    // v0.5.4 #3(2)：a>0 时曲线按基线轴重映射（轴零点向上臂生长）；a=0 原版
    const float aC = juce::jlimit (0.0f, 1.0f, rp.baselineY);
    for (int i = 0; i < N; ++i)
    {
        const float x = x0 + (float) i * invN * xLen;
        float y = dbToY_ (frame.db[i], canvas, rp.minDb, rp.maxDb);
        if (aC > 0.001f)
        {
            const float nrm = juce::jlimit (0.0f, 1.0f, (frame.db[i] - rp.minDb) / (rp.maxDb - rp.minDb));
            y = (float) inner.getBottom() - SpectrumStyle::baselineTop (nrm, aC) * (float) canvas.getHeight();
        }
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
    // v0.5.4 #4：填充闭合底边 = 基线轴（a=0 退化为画布底）
    const float yBot = (float) inner.getBottom()
                     - juce::jlimit (0.0f, 1.0f, rp.baselineY) * (float) canvas.getHeight();

    // ---- pass 0: 真辉光 bloom（离屏描曲线 → 高斯模糊 → 叠加回主画布）----
    if (pass == 0)
    {
        // v0.5.3: 用 JUCE 真高斯模糊替换旧版 4 层递减描边模拟。
        //   离屏画一条实心主曲线 → applyGaussianBlurEffect 扩散 → 半透明叠回，得柔光晕。
        const float radius = 7.0f;
        const int   pad    = (int) std::ceil (radius * 3.0f);

        juce::Rectangle<int> region (canvas.getX() - pad, canvas.getY() - pad,
                                     canvas.getWidth() + pad * 2,
                                     canvas.getHeight() + pad * 2);
        const int gw = juce::jmax (1, region.getWidth());
        const int gh = juce::jmax (1, region.getHeight());

        juce::Image glow (juce::Image::ARGB, gw, gh, true);
        {
            juce::Graphics gg (glow);
            gg.setColour (rp.primary);
            gg.addTransform (juce::AffineTransform::translation (-region.getX(), -region.getY()));
            juce::Path curvePath;
            buildSmoothPath_ (curvePath, pts, false, yBot, yTop);
            gg.strokePath (curvePath, juce::PathStrokeType (juce::jmax (3.0f, rp.lineWidth * 2.0f)));
            // #3(2)：a>0 时下臂曲线同样辉光
            const float aG = juce::jlimit (0.0f, 1.0f, rp.baselineY);
            if (aG > 0.001f)
            {
                auto dnPts = buildMirrorPoints_ (pts, aG, canvas, rp);
                juce::Path dnPath;
                dnPath.startNewSubPath (dnPts[0]);
                for (size_t i = 1; i < dnPts.size(); ++i) dnPath.lineTo (dnPts[i]);
                gg.strokePath (dnPath, juce::PathStrokeType (juce::jmax (3.0f, rp.lineWidth * 2.0f)));
            }
            auto pd = glow.getPixelData();
            if (pd != nullptr)
                pd->applyGaussianBlurEffect (radius);
        }
        g.saveState();
        g.setOpacity (0.55);
        g.drawImageAt (glow, region.getX(), region.getY());
        g.restoreState();
        return;
    }

    // ---- pass 1: 玻璃体 glass body ----
    if (pass == 1)
    {
        // 半透明渐变填充（#6 lineOnly → 跳过玻璃体）
        if (! rp.lineOnly)
        {
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
            // #3(2)：a>0 时下臂玻璃体填充
            const float aF = juce::jlimit (0.0f, 1.0f, rp.baselineY);
            if (aF > 0.001f)
            {
                auto dnPts = buildMirrorPoints_ (pts, aF, canvas, rp);
                juce::Path fillDn;
                fillDn.startNewSubPath (dnPts[0].getX(), yBot);
                fillDn.lineTo (dnPts[0]);
                for (size_t i = 1; i < dnPts.size(); ++i) fillDn.lineTo (dnPts[i]);
                fillDn.lineTo (dnPts.back().getX(), yBot);
                fillDn.closeSubPath();
                g.setGradientFill (grad);
                g.fillPath (fillDn);
            }
            g.setColour (juce::Colours::white);  // 清除 gradient
        }

        // 主曲线双层描边（colormap 时沿频率横向取色）
        ColorMap cm;
        cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
        const bool useMap  = ! cm.isSolid();
        const float gx0    = (float) inner.getX();
        const float gx1    = (float) inner.getRight();

        juce::Path curvePath;
        buildSmoothPath_ (curvePath, pts, false, yBot, yTop);

        if (useMap) g.setGradientFill (cm.horizontalGradient (gx0, gx1, yBot, 0.40f));
        else        g.setColour (rp.primary.withAlpha (0.40f));
        g.strokePath (curvePath, juce::PathStrokeType (2.5f));
        if (useMap) g.setGradientFill (cm.horizontalGradient (gx0, gx1, yBot, 0.90f));
        else        g.setColour (rp.primary.withAlpha (0.90f));
        g.strokePath (curvePath, juce::PathStrokeType (rp.lineWidth));
        g.setColour (juce::Colours::white);   // 清渐变
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
