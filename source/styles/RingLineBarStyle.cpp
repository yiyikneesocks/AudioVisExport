// =============================================================================
// RingLineBarStyle.cpp — ring:bar-line：极坐标 bar-line（斜接扇形 + 内镜像 + 间隙 + 外圈峰值）
//
//   每段外缘为"本带→邻带"半径斜弦；柱宽/间隙用与普通 bar 相同的 barWidthRatio/barGapRatio/barPitchRatio。
//   外圈峰值：**只有斜向帽、无连线**，帽角宽=段角宽、沿斜顶。内圈=实时镜像。非轴对齐→styleSupportsOutline=false。
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
    const float gapR  = baseR * juce::jlimit (0.0f, 0.5f, rp.ringMidGapRatio);
    const float oEdge = baseR + gapR, iEdge = baseR - gapR, minR = maxR * 0.02f;
    const float oScale = juce::jlimit (0.1f, 3.0f, rp.ringOuterHeightScale);
    const float iScale = juce::jlimit (0.1f, 3.0f, rp.ringInnerHeightScale);

    const float fps = juce::jmax (1.0f, rp.fps);
    spinDeg_ += rp.ringRotationDegPerSec / fps;
    if (spinDeg_ > 360.0f) spinDeg_ -= 360.0f;
    if (spinDeg_ < 0.0f)   spinDeg_ += 360.0f;
    const float twoPi = 6.2831853f, step = twoPi / (float) N;
    const float baseA = -1.5707963f + spinDeg_ * 0.0174533f;
    // 与普通 bar 相同布局模型（W→2π）
    const float pitchAng = twoPi * juce::jlimit (0.001f, 1.0f, rp.barPitchRatio);
    const float barAng   = juce::jlimit (0.02f, 2.5f, rp.barWidthRatio) * step;
    const float a0off    = (pitchAng - barAng) * 0.5f;
    const auto  aLof = [&] (int i) { return baseA + a0off + (float) i * pitchAng; };
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
        const float aL = aLof (i), aR = aL + barAng;
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
        const float aL = aLof (i), aR = aL + barAng;
        const float rL = rI (nv[(size_t) i]), rR = rI (nv[(size_t) j]);
        juce::Path q;
        q.startNewSubPath (P (aL, iEdge)); q.lineTo (P (aR, iEdge));
        q.lineTo (P (aR, rR));             q.lineTo (P (aL, rL)); q.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv[(size_t) i]).darker (0.25f) : rp.secondary.withAlpha (0.9f));
        g.fillPath (q);
    }
    // 外圈峰值帽：**只有斜向帽、无连线**。沿该段斜顶（本带→邻带峰半径弦）画一条与段同宽的帽，
    //   厚度 peakCapWidth、平头(butt) → 帽宽随半径/段宽自然变化。
    if (rp.ringOuterOn && rp.barParticles && rp.ringPeakCapOn)
    {
        const float cw = juce::jmax (1.0f, rp.peakCapWidth);
        g.setColour (cm.peakColor());
        for (int i = 0; i < N; ++i)
        {
            const int j = nx (i);
            const float aL = aLof (i), aR = aL + barAng;
            g.drawLine (juce::Line<float> (P (aL, rO (pvOf (i))), P (aR, rO (pvOf (j)))), cw);
        }
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
