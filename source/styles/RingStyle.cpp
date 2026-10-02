// =============================================================================
// RingStyle.cpp — ring:bar 径向柱（基圆 + 内外镜像 + 间隙 + 外圈峰值帽/线）
//
//   基圆半径 = ringBaseRadiusRatio*maxR。外圈从 (baseR+gap) 向外生长到 rO(nv)，
//   内圈从 (baseR-gap) 向内（实时镜像，非峰值）生长到 rI(nv)；gap=ringMidGapRatio*baseR，
//   使 nv=0 时内外不接触。角宽 ringBarThicknessRatio（<1 留柱隙）。外圈可加峰值帽(ringPeakCapOn)
//   + 峰值线(ringPeakLineOn)；内圈峰值默认关(ringInnerPeakOn)。
// =============================================================================
#include "RingStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

void RingStyle::render (juce::Graphics& g, const juce::Rectangle<int>& canvas,
                        const BandFrame& frame, const RenderParams& rp)
{
    const int N = frame.bandCount;
    if (N <= 1 || canvas.getWidth() < 8 || canvas.getHeight() < 8) return;

    const float cx = (float) canvas.getX() + (float) canvas.getWidth()  * 0.5f;
    const float cy = (float) canvas.getY() + (float) canvas.getHeight() * 0.5f;
    const float maxR  = std::min ((float) canvas.getWidth(), (float) canvas.getHeight()) * 0.5f * 0.92f;
    const float baseR = maxR * juce::jlimit (0.05f, 0.8f, rp.ringBaseRadiusRatio);
    const float gapR  = baseR * juce::jlimit (0.0f, 0.5f, rp.ringMidGapRatio);   // 间隙相对"基圆半径"（比相对 maxR 小得多）
    const float oEdge = baseR + gapR;                 // 外圈起点
    const float iEdge = baseR - gapR;                 // 内圈起点（向外看的最外）
    const float minR  = maxR * 0.02f;
    const float oScale = juce::jlimit (0.1f, 3.0f, rp.ringOuterHeightScale);
    const float iScale = juce::jlimit (0.1f, 3.0f, rp.ringInnerHeightScale);

    const float fps = juce::jmax (1.0f, rp.fps);
    spinDeg_ += rp.ringRotationDegPerSec / fps;
    if (spinDeg_ > 360.0f) spinDeg_ -= 360.0f;
    if (spinDeg_ < 0.0f)   spinDeg_ += 360.0f;

    const float twoPi = 6.2831853f, step = twoPi / (float) N;
    // v0.5.6：与普通 bar 相同的布局模型（W→2π，W/N→step）：
    //   pitchAng = 2π·barPitchRatio（锚点角距）；barAng = barWidthRatio·step（柱角宽）；gap = barGapRatio·step。
    const float baseA = -1.5707963f + spinDeg_ * 0.0174533f;
    const float pitchAng = twoPi * juce::jlimit (0.001f, 1.0f, rp.barPitchRatio);
    const float barAng   = juce::jlimit (0.02f, 2.5f, rp.barWidthRatio) * step;
    const float a0off    = (pitchAng - barAng) * 0.5f;
    const auto  aLof = [&] (int i) { return baseA + a0off + (float) i * pitchAng; };
    const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };
    const auto rO = [&] (float v) { return oEdge + juce::jlimit (0.0f, 1.0f, v) * (maxR - oEdge) * oScale; };
    const auto rI = [&] (float v) { return iEdge - juce::jlimit (0.0f, 1.0f, v) * (iEdge - minR) * iScale; };

    ColorMap cm; cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();
    const float span = juce::jmax (1e-3f, rp.maxDb - rp.minDb);
    const auto pvOf = [&] (int i) { return juce::jlimit (0.0f, 1.0f, (frame.peakDb[(size_t) i] - rp.minDb) / span); };

    // 外圈实时柱
    if (rp.ringOuterOn)
    for (int i = 0; i < N; ++i)
    {
        const float nv = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
        const float a0 = aLof (i), a1 = a0 + barAng, r1 = rO (nv);
        juce::Path q;
        q.startNewSubPath (P (a0, oEdge)); q.lineTo (P (a1, oEdge));
        q.lineTo (P (a1, r1));             q.lineTo (P (a0, r1)); q.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv) : rp.primary);
        g.fillPath (q);
    }
    // 内圈实时镜像柱
    if (rp.ringInnerOn)
    for (int i = 0; i < N; ++i)
    {
        const float nv = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
        const float a0 = aLof (i), a1 = a0 + barAng, r1 = rI (nv);
        juce::Path q;
        q.startNewSubPath (P (a0, iEdge)); q.lineTo (P (a1, iEdge));
        q.lineTo (P (a1, r1));             q.lineTo (P (a0, r1)); q.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv).darker (0.25f) : rp.secondary.withAlpha (0.9f));
        g.fillPath (q);
    }

    // 外圈峰值帽：**只有帽、无连线**；帽角宽 = 该柱角宽（半径越大，弧长越宽 → 视觉上逐渐变宽），
    //   径向厚度 = peakCapWidth（以峰半径为中线的一小段环带）。
    if (rp.ringOuterOn && rp.barParticles && rp.ringPeakCapOn)
    {
        const float cw = juce::jmax (1.0f, rp.peakCapWidth);
        g.setColour (cm.peakColor());
        for (int i = 0; i < N; ++i)
        {
            const float a0 = aLof (i), a1 = a0 + barAng;
            const float r = rO (pvOf (i)), ri = juce::jmax (0.0f, r - cw * 0.5f), ro = r + cw * 0.5f;
            juce::Path cap;
            cap.startNewSubPath (P (a0, ri)); cap.lineTo (P (a1, ri));
            cap.lineTo (P (a1, ro));          cap.lineTo (P (a0, ro)); cap.closeSubPath();
            g.fillPath (cap);
        }
    }
    // 内圈峰值（默认关）
    if (rp.ringInnerPeakOn && rp.barParticles)
    {
        juce::Path lp;
        for (int i = 0; i < N; ++i)
        { const auto pt = P (baseA + ((float) i + 0.5f) * step, rI (pvOf (i))); if (i==0) lp.startNewSubPath (pt); else lp.lineTo (pt); }
        lp.closeSubPath();
        g.setColour (cm.peakColor().withAlpha (0.7f));
        g.strokePath (lp, juce::PathStrokeType (juce::jmax (1.0f, rp.lineWidth)));
    }
}
