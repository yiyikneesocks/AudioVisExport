// =============================================================================
// RingLineBarStyle.cpp — ring:bar-line：极坐标 bar-line（斜接扇形 + 内镜像 + 间隙 + 外圈峰值）
//
//   每段外缘为"本带→邻带"半径斜弦（相邻段边界半径相等→脊线连续），但角宽/间隙随
//   ringBarThicknessRatio 可调、可留隙（同 bar）。内圈为实时镜像（非峰值）。
//   外圈峰值帽/线默认开，内圈峰值默认关。非轴对齐 → styleSupportsOutline=false。
// =============================================================================
#include "RingLineBarStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>
#include <vector>

void RingLineBarStyle::render (juce::Graphics& g, const juce::Rectangle<int>& canvas,
                               const BandFrame& frame, const RenderParams& rp)
{
    const int N = frame.bandCount;
    if (N <= 1 || canvas.getWidth() < 8 || canvas.getHeight() < 8) return;

    const float cx = (float) canvas.getX() + (float) canvas.getWidth()  * 0.5f;
    const float cy = (float) canvas.getY() + (float) canvas.getHeight() * 0.5f;
    const float maxR  = std::min ((float) canvas.getWidth(), (float) canvas.getHeight()) * 0.5f * 0.92f;
    const float baseR = maxR * juce::jlimit (0.05f, 0.8f, rp.ringBaseRadiusRatio);
    const float gapR  = maxR * juce::jlimit (0.0f, 0.4f, rp.ringMidGapRatio);
    const float oEdge = baseR + gapR, iEdge = baseR - gapR, minR = maxR * 0.02f;
    const float oScale = juce::jlimit (0.1f, 3.0f, rp.ringOuterHeightScale);
    const float iScale = juce::jlimit (0.1f, 3.0f, rp.ringInnerHeightScale);

    const float fps = juce::jmax (1.0f, rp.fps);
    spinDeg_ += rp.ringRotationDegPerSec / fps;
    if (spinDeg_ > 360.0f) spinDeg_ -= 360.0f;
    if (spinDeg_ < 0.0f)   spinDeg_ += 360.0f;
    const float twoPi = 6.2831853f, step = twoPi / (float) N;
    const float barW  = juce::jlimit (0.05f, 1.0f, rp.ringBarThicknessRatio);
    const float halfGap = step * 0.5f * (1.0f - barW);
    const float baseA = -1.5707963f + spinDeg_ * 0.0174533f;
    const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };
    const auto rO = [&] (float v) { return oEdge + juce::jlimit (0.0f, 1.0f, v) * (maxR - oEdge) * oScale; };
    const auto rI = [&] (float v) { return iEdge - juce::jlimit (0.0f, 1.0f, v) * (iEdge - minR) * iScale; };

    ColorMap cm; cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();
    const float lw = juce::jmax (1.0f, rp.lineWidth);
    const float span = juce::jmax (1e-3f, rp.maxDb - rp.minDb);
    std::vector<float> nv ((size_t) N);
    for (int i = 0; i < N; ++i) nv[(size_t) i] = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
    const auto pvOf = [&] (int i) { return juce::jlimit (0.0f, 1.0f, (frame.peakDb[(size_t) i] - rp.minDb) / span); };
    const auto nx = [N] (int i) { return (i + 1) % N; };

    // 外圈斜接扇形（实时）
    if (rp.ringOuterOn)
    for (int i = 0; i < N; ++i)
    {
        const int j = nx (i);
        const float aL = baseA + (float) i * step + halfGap, aR = baseA + (float) j * step - halfGap;
        const float rL = rO (nv[(size_t) i]), rR = rO (nv[(size_t) j]);
        juce::Path q;
        q.startNewSubPath (P (aL, oEdge)); q.lineTo (P (aR, oEdge));
        q.lineTo (P (aR, rR));             q.lineTo (P (aL, rL)); q.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv[(size_t) i]) : rp.primary);
        g.fillPath (q);
    }
    // 内圈斜接扇形（实时镜像）
    if (rp.ringInnerOn)
    for (int i = 0; i < N; ++i)
    {
        const int j = nx (i);
        const float aL = baseA + (float) i * step + halfGap, aR = baseA + (float) j * step - halfGap;
        const float rL = rI (nv[(size_t) i]), rR = rI (nv[(size_t) j]);
        juce::Path q;
        q.startNewSubPath (P (aL, iEdge)); q.lineTo (P (aR, iEdge));
        q.lineTo (P (aR, rR));             q.lineTo (P (aL, rL)); q.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv[(size_t) i]).darker (0.25f) : rp.secondary.withAlpha (0.9f));
        g.fillPath (q);
    }
    // 外圈峰值线（连各带峰半径的斜接脊）
    if (rp.ringOuterOn && rp.barParticles && rp.ringPeakLineOn)
    {
        juce::Path ridge;
        for (int i = 0; i < N; ++i)
        { const auto pt = P (baseA + (float) i * step, rO (pvOf (i))); if (i==0) ridge.startNewSubPath (pt); else ridge.lineTo (pt); }
        ridge.closeSubPath();
        g.setColour (cm.peakColor()); g.strokePath (ridge, juce::PathStrokeType (lw));
    }
    // 外圈峰值帽
    if (rp.ringOuterOn && rp.barParticles && rp.ringPeakCapOn)
    {
        const float cw = juce::jmax (1.5f, rp.peakCapWidth), hw = step * 0.5f * barW;
        g.setColour (cm.peakColor());
        for (int i = 0; i < N; ++i)
        { const float a = baseA + ((float) i + 0.5f) * step, r = rO (pvOf (i));
          g.drawLine (juce::Line<float> (P (a - hw, r), P (a + hw, r)), cw); }
    }
    // 内圈峰值线（默认关）
    if (rp.ringInnerPeakOn && rp.barParticles)
    {
        juce::Path ridge;
        for (int i = 0; i < N; ++i)
        { const auto pt = P (baseA + (float) i * step, rI (pvOf (i))); if (i==0) ridge.startNewSubPath (pt); else ridge.lineTo (pt); }
        ridge.closeSubPath();
        g.setColour (cm.peakColor().withAlpha (0.7f)); g.strokePath (ridge, juce::PathStrokeType (lw));
    }
}
