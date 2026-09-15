// =============================================================================
// RingStyle.cpp — 径向频谱实现
// =============================================================================
#include "RingStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

void RingStyle::render (juce::Graphics& g,
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

    // 旋转：按 fps 把 deg/s 折算成每帧增量（style 实例跨帧持久 → 累计角）
    const float fps = juce::jmax (1.0f, rp.fps);
    spinDeg_ += rp.ringRotationDegPerSec / fps;
    if (spinDeg_ > 360.0f) spinDeg_ -= 360.0f;
    if (spinDeg_ < 0.0f)   spinDeg_ += 360.0f;

    const float twoPi  = 6.2831853f;
    const float step   = twoPi / (float) N;
    const float thick  = step * 0.5f * juce::jlimit (0.05f, 1.0f, rp.ringBarThicknessRatio);
    const float baseA  = -1.5707963f + spinDeg_ * 0.0174533f;   // 顶部起、顺时针

    ColorMap cm;
    cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();

    // 可选：drawGrid → 画两圈淡淡参考圆
    if (rp.drawGrid)
    {
        g.setColour (rp.secondary.withAlpha (0.12f));
        for (float f : { 0.35f, 0.65f, 1.0f })
        {
            const float r = innerR + (maxR - innerR) * f;
            g.drawEllipse (juce::Rectangle<float> (cx - r, cy - r, r + r, r + r), 1.0f);
        }
    }

    // 外层：实时辐条（innerR → innerR + nv*(maxR-innerR)）
    for (int i = 0; i < N; ++i)
    {
        const float nv = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
        const float a  = baseA + (float) i * step;
        const float r1 = innerR + nv * (maxR - innerR);
        const float a0 = a - thick, a1 = a + thick;
        const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };
        juce::Path quad;
        quad.startNewSubPath (P (a0, innerR));
        quad.lineTo (P (a1, innerR));
        quad.lineTo (P (a1, r1));
        quad.lineTo (P (a0, r1));
        quad.closeSubPath();
        g.setColour (useMap ? cm.colourForBand (i, N, nv) : rp.primary);
        g.fillPath (quad);
    }

    // 内层（峰值双层）：从 innerR 内侧向圆心生长的峰值辐条
    if (rp.ringLayers >= 2 && rp.barParticles)
    {
        const float span  = rp.maxDb - rp.minDb;
        const float outer = innerR * 0.90f, inr = innerR * 0.30f;
        for (int i = 0; i < N; ++i)
        {
            const float pn = (span > 1e-3f)
                ? juce::jlimit (0.0f, 1.0f, (frame.peakDb[(size_t) i] - rp.minDb) / span) : 0.0f;
            const float a  = baseA + (float) i * step;
            const float r0 = outer;
            const float r1 = outer - pn * (outer - inr);
            const float a0 = a - thick, a1 = a + thick;
            const auto P = [&] (float ang, float r) { return juce::Point<float> (cx + r * std::cos (ang), cy + r * std::sin (ang)); };
            juce::Path quad;
            quad.startNewSubPath (P (a0, r0));
            quad.lineTo (P (a1, r0));
            quad.lineTo (P (a1, r1));
            quad.lineTo (P (a0, r1));
            quad.closeSubPath();
            g.setColour (cm.peakColor().withAlpha (0.9f));
            g.fillPath (quad);
        }
    }
}
