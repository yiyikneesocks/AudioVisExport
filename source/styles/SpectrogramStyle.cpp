// =============================================================================
// SpectrogramStyle.cpp — 频谱瀑布图实现（滚动累积画布）
// =============================================================================
#include "SpectrogramStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

void SpectrogramStyle::render (juce::Graphics& g,
                               const juce::Rectangle<int>& canvas,
                               const BandFrame& frame,
                               const RenderParams& rp)
{
    const int N = frame.bandCount;
    const int cw = canvas.getWidth(), ch = canvas.getHeight();
    if (N <= 1 || cw < 4 || ch < 4) return;

    // 尺寸/带数变化 → 重置滚动画布（resize 安全）
    if (! acc_.isValid() || acc_.getWidth() != cw || acc_.getHeight() != ch || N != lastBandCount_)
    {
        acc_ = juce::Image (juce::Image::ARGB, cw, ch, true);
        lastBandCount_ = N;
    }

    const float fps = juce::jmax (1.0f, rp.fps);
    const int dpx = juce::jlimit (1, cw, (int) std::lround (rp.spectrogramScrollSpeed / fps));
    // 拖尾：每帧把旧内容按 opac 衰减（persistence=1 → 几乎保留；0 → 快速淡出）
    const float opac = juce::jlimit (0.5f, 0.999f, 0.99f - 0.30f * (1.0f - juce::jlimit (0.0f, 1.0f, rp.spectrogramPersistence)));

    // 1) 左移：拷贝 → 清空 → 以 opac 画回（右移的空白留在最右待写新列）
    {
        juce::Image src (juce::Image::ARGB, cw, ch, false);
        { juce::Graphics gs (src); gs.drawImageAt (acc_, 0, 0); }
        { juce::Graphics ga (acc_); ga.setOpacity (1.0f); ga.fillAll (juce::Colours::transparentBlack);
          ga.setOpacity (opac); ga.drawImageAt (src, -dpx, 0); }
    }

    // 2) 最右侧 dpx 宽写入"当前列"（按 cellSize 方块化；低频在下）
    const int cell = juce::jlimit (1, ch, (int) std::lround (rp.spectrogramCellSize));
    ColorMap cm; cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    {
        juce::Graphics ga (acc_);
        ga.setOpacity (1.0f);
        const int x0 = cw - dpx;
        for (int j = 0; (j * cell) < ch; ++j)
        {
            const float ycFromBottom = (float) j * cell + cell * 0.5f;
            const float norm = juce::jlimit (0.0f, 1.0f, ycFromBottom / (float) ch);
            const int band = juce::jlimit (0, N - 1, (int) std::lround (norm * (float) (N - 1)));
            const float v  = juce::jlimit (0.0f, 1.0f, frame.normalized[(size_t) band]);
            const float a  = 0.12f + 0.88f * v;
            const int yTop = ch - (j + 1) * cell;
            ga.setColour (cm.map (v).withAlpha (a));
            ga.fillRect (juce::Rectangle<int> (x0, yTop, dpx, juce::jmin (cell, ch - yTop)));
        }
    }

    // 3) 合成到目标
    g.drawImageAt (acc_, canvas.getX(), canvas.getY());
}
