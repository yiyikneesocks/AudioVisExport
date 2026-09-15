// =============================================================================
// RingLineStyle.cpp — 闭合极坐标线谱
// =============================================================================
#include "RingLineStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

void RingLineStyle::render (juce::Graphics& g,
                            const juce::Rectangle<int>& canvas,
                            const BandFrame& frame,
                            const RenderParams& rp)
{
    const int N = frame.bandCount;
    if (N <= 1 || canvas.getWidth() < 8 || canvas.getHeight() < 8) return;

    const float cx = (float) canvas.getX() + (float) canvas.getWidth()  * 0.5f;
    const float cy = (float) canvas.getY() + (float) canvas.getHeight() * 0.5f;
    const float maxR   = std::min ((float) canvas.getWidth(), (float) canvas.getHeight()) * 0.5f * 0.92f;
    const float innerR = maxR * juce::jlimit (0.0f, 0.8f, rp.ringInnerRadiusRatio);

    const float fps = juce::jmax (1.0f, rp.fps);
    spinDeg_ += rp.ringRotationDegPerSec / fps;
    if (spinDeg_ > 360.0f) spinDeg_ -= 360.0f;
    if (spinDeg_ < 0.0f)   spinDeg_ += 360.0f;

    const float twoPi = 6.2831853f;
    const float step  = twoPi / (float) N;
    const float baseA = -1.5707963f + spinDeg_ * 0.0174533f;
    const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };

    ColorMap cm; cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const float lw = juce::jmax (1.0f, rp.lineWidth);

    if (rp.drawGrid)
    {
        g.setColour (rp.secondary.withAlpha (0.10f));
        for (float f : { 0.5f, 1.0f })
        {
            const float r = innerR + (maxR - innerR) * f;
            g.drawEllipse (juce::Rectangle<float> (cx - r, cy - r, r + r, r + r), 1.0f);
        }
    }

    // 外圈：实时闭合折线（+ 淡淡填充体现轮廓内部）
    if (rp.ringOuterOn)
    {
        juce::Path loop;
        for (int i = 0; i < N; ++i)
        {
            const float nv = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
            const auto pt  = P (baseA + (float) i * step, innerR + nv * (maxR - innerR));
            if (i == 0) loop.startNewSubPath (pt); else loop.lineTo (pt);
        }
        loop.closeSubPath();
        g.setColour (rp.secondary.withAlpha (0.10f));
        g.fillPath (loop);
        g.setColour (rp.primary);
        g.strokePath (loop, juce::PathStrokeType (lw));
    }

    // 内圈：峰值闭合折线
    if (rp.ringInnerOn && rp.barParticles)
    {
        const float span  = rp.maxDb - rp.minDb;
        const float outer = innerR * 0.90f, inr = innerR * 0.30f;
        juce::Path loop;
        for (int i = 0; i < N; ++i)
        {
            const float pn = (span > 1e-3f)
                ? juce::jlimit (0.0f, 1.0f, (frame.peakDb[(size_t) i] - rp.minDb) / span) : 0.0f;
            const auto pt = P (baseA + (float) i * step, outer - pn * (outer - inr));
            if (i == 0) loop.startNewSubPath (pt); else loop.lineTo (pt);
        }
        loop.closeSubPath();
        g.setColour (cm.peakColor().withAlpha (0.9f));
        g.strokePath (loop, juce::PathStrokeType (lw));
    }
}
