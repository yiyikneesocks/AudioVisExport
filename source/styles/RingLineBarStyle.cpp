// =============================================================================
// RingLineBarStyle.cpp — 柱 + 柱顶闭合线（极坐标 bar-line）
// =============================================================================
#include "RingLineBarStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>
#include <vector>

void RingLineBarStyle::render (juce::Graphics& g,
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
    const float thick = step * 0.5f * juce::jlimit (0.05f, 1.0f, rp.ringBarThicknessRatio);
    const float baseA = -1.5707963f + spinDeg_ * 0.0174533f;
    const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };

    ColorMap cm; cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();
    const float lw = juce::jmax (1.0f, rp.lineWidth);

    std::vector<float> nv ((size_t) N), rOut ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        nv[(size_t) i]    = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
        rOut[(size_t) i]  = innerR + nv[(size_t) i] * (maxR - innerR);
    }

    if (rp.drawGrid)
    {
        g.setColour (rp.secondary.withAlpha (0.10f));
        for (float f : { 0.5f, 1.0f })
        { const float r = innerR + (maxR - innerR) * f; g.drawEllipse (juce::Rectangle<float> (cx - r, cy - r, r + r, r + r), 1.0f); }
    }

    // 外层柱（实时）
    if (rp.ringOuterOn)
    for (int i = 0; i < N; ++i)
    {
        const float a = baseA + (float) i * step, a0 = a - thick, a1 = a + thick, r1 = rOut[(size_t) i];
        juce::Path quad;
        quad.startNewSubPath (P (a0, innerR)); quad.lineTo (P (a1, innerR));
        quad.lineTo (P (a1, r1));               quad.lineTo (P (a0, r1)); quad.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv[(size_t) i]) : rp.primary);
        g.fillPath (quad);
    }

    // 内层柱（峰值）
    if (rp.ringInnerOn && rp.barParticles)
    {
        const float span = rp.maxDb - rp.minDb, outer = innerR * 0.90f, inr = innerR * 0.30f;
        for (int i = 0; i < N; ++i)
        {
            const float pn = (span > 1e-3f) ? juce::jlimit (0.0f, 1.0f, (frame.peakDb[(size_t) i] - rp.minDb) / span) : 0.0f;
            const float a = baseA + (float) i * step, a0 = a - thick, a1 = a + thick;
            const float r0 = outer, r1 = outer - pn * (outer - inr);
            juce::Path quad;
            quad.startNewSubPath (P (a0, r0)); quad.lineTo (P (a1, r0));
            quad.lineTo (P (a1, r1));          quad.lineTo (P (a0, r1)); quad.closeSubPath();
            g.setColour (cm.peakColor().withAlpha (0.9f));
            g.fillPath (quad);
        }
    }

    // 柱顶闭合线（连起各柱顶，极坐标 bar-line 的"线"）
    if (rp.ringOuterOn)
    {
        juce::Path ridge;
        for (int i = 0; i < N; ++i)
        {
            const auto pt = P (baseA + (float) i * step, rOut[(size_t) i]);
            if (i == 0) ridge.startNewSubPath (pt); else ridge.lineTo (pt);
        }
        ridge.closeSubPath();
        g.setColour (rp.secondary);
        g.strokePath (ridge, juce::PathStrokeType (lw));
    }
}
