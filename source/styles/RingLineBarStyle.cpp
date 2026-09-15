// =============================================================================
// RingLineBarStyle.cpp — 极坐标 bar-line（斜接扇形条带，柱顶连续）
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
    const float baseA = -1.5707963f + spinDeg_ * 0.0174533f;
    const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };

    ColorMap cm; cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();

    // 每条带一个外半径值；相邻段用 [i]→[i+1] 的斜弦顶 → 脊线连续（bar-line）
    std::vector<float> nv ((size_t) N), rOut ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        nv[(size_t) i] = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
        rOut[(size_t) i] = innerR + nv[(size_t) i] * (maxR - innerR);
    }
    const auto nextIdx = [N] (int i) { return (i + 1) % N; };   // 首尾相接成完整一圈

    if (rp.drawGrid)
    {
        g.setColour (rp.secondary.withAlpha (0.10f));
        for (float f : { 0.5f, 1.0f })
        { const float r = innerR + (maxR - innerR) * f; g.drawEllipse (juce::Rectangle<float> (cx - r, cy - r, r + r, r + r), 1.0f); }
    }

    // 外圈：斜接扇形（实时）
    if (rp.ringOuterOn)
    for (int i = 0; i < N; ++i)
    {
        const int  j  = nextIdx (i);
        const float aL = baseA + (float) i * step, aR = baseA + (float) j * step;
        const float rL = rOut[(size_t) i], rR = rOut[(size_t) j];
        juce::Path quad;
        quad.startNewSubPath (P (aL, innerR));
        quad.lineTo (P (aR, innerR));
        quad.lineTo (P (aR, rR));
        quad.lineTo (P (aL, rL));
        quad.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv[(size_t) i]) : rp.primary);
        g.fillPath (quad);
    }

    // 内圈：峰值斜接扇形
    if (rp.ringInnerOn && rp.barParticles)
    {
        const float span = rp.maxDb - rp.minDb;
        const float hi = innerR * 0.90f, lo = innerR * 0.30f;
        std::vector<float> pk ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            const float pn = (span > 1e-3f) ? juce::jlimit (0.0f, 1.0f, (frame.peakDb[(size_t) i] - rp.minDb) / span) : 0.0f;
            pk[(size_t) i] = hi - pn * (hi - lo);   // 峰值越大越往圆心
        }
        for (int i = 0; i < N; ++i)
        {
            const int j = nextIdx (i);
            const float aL = baseA + (float) i * step, aR = baseA + (float) j * step;
            juce::Path quad;
            quad.startNewSubPath (P (aL, hi));
            quad.lineTo (P (aR, hi));
            quad.lineTo (P (aR, pk[(size_t) j]));
            quad.lineTo (P (aL, pk[(size_t) i]));
            quad.closeSubPath();
            g.setColour (cm.peakColor().withAlpha (0.9f));
            g.fillPath (quad);
        }
    }
}
