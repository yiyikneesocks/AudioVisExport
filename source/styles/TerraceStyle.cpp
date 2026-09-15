// =============================================================================
// TerraceStyle.cpp — 地形剖面实现
// =============================================================================
#include "TerraceStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

void TerraceStyle::render (juce::Graphics& g,
                           const juce::Rectangle<int>& canvas,
                           const BandFrame& frame,
                           const RenderParams& rp)
{
    const int N = frame.bandCount;
    if (N <= 1 || canvas.getWidth() < 8 || canvas.getHeight() < 8) return;

    const int layers = juce::jlimit (4, 64, rp.terraceLayers);
    const int maxHist = 64;

    // bandCount 变化 → 历史缓冲失效重置（resize 安全）
    if (! history_.empty() && (int) history_.front().size() != N)
        history_.clear();

    std::vector<float> cur ((size_t) N);
    for (int i = 0; i < N; ++i) cur[(size_t) i] = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) i]);
    history_.insert (history_.begin(), std::move (cur));        // front = 最新
    if ((int) history_.size() > maxHist) history_.resize ((size_t) maxHist);

    const int L = juce::jmin (layers, (int) history_.size());
    const float depth = juce::jmax (1.0f, rp.terraceDepthStep);
    const float amp   = juce::jmax (6.0f, depth * 4.0f);
    const float decay = 1.0f - juce::jlimit (0.0f, 0.95f, rp.terraceFade);   // fade 越大后层越淡

    auto inner = canvas;
    const float x0   = (float) inner.getX();
    const float xLen = (float) inner.getWidth();
    const float invN = 1.0f / (float) juce::jmax (1, N - 1);
    const float bottom = (float) inner.getBottom() - 2.0f;

    ColorMap cm;
    cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();

    // 后（旧、上、淡）→ 前（新、下、浓）：前层填充遮住后层，形成层峦
    for (int k = L - 1; k >= 0; --k)
    {
        const auto& prof = history_[(size_t) k];
        const float base = juce::jmax ((float) inner.getY() + 2.0f, bottom - (float) k * depth);
        const float aK   = std::pow (decay, (float) k);
        if (aK < 0.02f) continue;

        juce::Path area;
        area.startNewSubPath (x0, base);
        for (int i = 0; i < N; ++i)
        {
            const float x = x0 + (float) i * invN * xLen;
            const float y = base - juce::jlimit (0.0f, 1.0f, prof[(size_t) i]) * amp;
            area.lineTo (x, y);
        }
        area.lineTo (x0 + xLen, base);
        area.closeSubPath();

        if (useMap)
        {
            g.setGradientFill (cm.horizontalGradient (x0, x0 + xLen, base, 0.55f * aK));
            g.fillPath (area);
        }
        else
        {
            g.setColour (rp.primary.withAlpha (0.30f * aK));
            g.fillPath (area);
        }
        // 前缘脊线
        juce::Path ridge;
        ridge.startNewSubPath (x0, base - prof[0] * amp);
        for (int i = 1; i < N; ++i)
            ridge.lineTo (x0 + (float) i * invN * xLen, base - juce::jlimit (0.0f, 1.0f, prof[(size_t) i]) * amp);
        g.setColour (useMap ? rp.secondary.withAlpha (aK) : rp.secondary.withAlpha (0.9f * aK));
        g.strokePath (ridge, juce::PathStrokeType (juce::jmax (1.0f, rp.lineWidth)));
    }
}
