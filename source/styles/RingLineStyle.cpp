// =============================================================================
// RingLineStyle.cpp — ringline：极坐标闭合"线"谱（外实时环 + 内镜像环，留隙；外圈峰值线/帽）
// =============================================================================
#include "RingLineStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

void RingLineStyle::render (juce::Graphics& g, const juce::Rectangle<int>& canvas,
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
    const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };
    const auto rO = [&] (float v) { return oEdge + juce::jlimit (0.0f, 1.0f, v) * (maxR - oEdge) * oScale; };
    const auto rI = [&] (float v) { return iEdge - juce::jlimit (0.0f, 1.0f, v) * (iEdge - minR) * iScale; };

    ColorMap cm; cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const float lw = juce::jmax (1.0f, rp.lineWidth);
    const float span = juce::jmax (1e-3f, rp.maxDb - rp.minDb);
    const auto nvOf = [&] (int i) { return juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]); };
    const auto pvOf = [&] (int i) { return juce::jlimit (0.0f, 1.0f, (frame.peakDb[(size_t) i] - rp.minDb) / span); };
    const auto ang  = [&] (int i)  { return baseA + ((float) i + 0.5f) * step; };

    // 外圈实时闭合线（+ 淡填充）
    if (rp.ringOuterOn)
    {
        juce::Path o;
        for (int i = 0; i < N; ++i) { const auto pt = P (ang (i), rO (nvOf (i))); if (i==0) o.startNewSubPath (pt); else o.lineTo (pt); }
        o.closeSubPath();
        g.setColour (rp.secondary.withAlpha (0.10f)); g.fillPath (o);
        g.setColour (rp.primary); g.strokePath (o, juce::PathStrokeType (lw));
    }
    // 内圈实时镜像闭合线
    if (rp.ringInnerOn)
    {
        juce::Path in;
        for (int i = 0; i < N; ++i) { const auto pt = P (ang (i), rI (nvOf (i))); if (i==0) in.startNewSubPath (pt); else in.lineTo (pt); }
        in.closeSubPath();
        g.setColour (rp.secondary); g.strokePath (in, juce::PathStrokeType (lw));
    }
    // 外圈峰值线
    if (rp.ringOuterOn && rp.barParticles && rp.ringPeakLineOn)
    {
        juce::Path pl;
        for (int i = 0; i < N; ++i) { const auto pt = P (ang (i), rO (pvOf (i))); if (i==0) pl.startNewSubPath (pt); else pl.lineTo (pt); }
        pl.closeSubPath();
        g.setColour (cm.peakColor()); g.strokePath (pl, juce::PathStrokeType (lw));
    }
    // 外圈峰值帽
    if (rp.ringOuterOn && rp.barParticles && rp.ringPeakCapOn)
    {
        const float cw = juce::jmax (1.5f, rp.peakCapWidth), hw = step * 0.30f;
        g.setColour (cm.peakColor());
        for (int i = 0; i < N; ++i) { const float r = rO (pvOf (i)); g.drawLine (juce::Line<float> (P (ang (i) - hw, r), P (ang (i) + hw, r)), cw); }
    }
    // 内圈峰值线（默认关）
    if (rp.ringInnerPeakOn && rp.barParticles)
    {
        juce::Path pl;
        for (int i = 0; i < N; ++i) { const auto pt = P (ang (i), rI (pvOf (i))); if (i==0) pl.startNewSubPath (pt); else pl.lineTo (pt); }
        pl.closeSubPath();
        g.setColour (cm.peakColor().withAlpha (0.7f)); g.strokePath (pl, juce::PathStrokeType (lw));
    }
}
