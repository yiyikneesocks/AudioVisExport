// =============================================================================
// BarLineStyle.cpp — bar-line：斜面柱顶（v0.5.0 重构，布局与 bar 一致）
//
// 柱体 = 五边形梯形：
//   · 底边：[x, yBot] → [x+barW, yBot]（水平）
//   · 左右边：垂直
//   · 顶边：(x, yEdgeL) → (x+barW, yEdgeR)（斜线或水平）
// 边缘高度插值：柱 i 左边缘 = (n[i-1] + n[i]) / 2，右边缘 = (n[i] + n[i+1]) / 2
//   → 相邻柱的衔接缘共享同一高度，整条顶部自然连续；同高 → 水平顶。
// 空带（n < 0.005）：柱体不画，但边缘插值仍参与邻柱计算（无悬空跳变）。
// =============================================================================
#include "BarLineStyle.h"
#include <cmath>
#include <algorithm>
#include <vector>

float BarLineStyle::normalizedToY_ (float n, const juce::Rectangle<int>& canvas)
{
    const float t = std::clamp (n, 0.0f, 1.0f);
    return (float) canvas.getBottom() - t * (float) canvas.getHeight();
}

void BarLineStyle::render (juce::Graphics& g,
                           const juce::Rectangle<int>& canvas,
                           const BandFrame& frame,
                           const RenderParams& rp)
{
    const int N = frame.bandCount;
    if (N <= 0) return;

    auto inner = canvas.reduced (2);
    if (inner.getWidth() <= 2 || inner.getHeight() <= 2) return;

    // 布局：与 BarStyle 完全一致（slot / gap / barW / 居中）
    const float slotW = (float) inner.getWidth() / (float) N;
    const float gap   = slotW * juce::jlimit (0.0f, 1.0f, rp.barGapRatio);
    const float barW  = (slotW - gap) * juce::jlimit (0.05f, 2.0f, rp.barWidthRatio);
    const float x0    = (float) inner.getX() + (slotW - barW) * 0.5f;
    const float yBot  = (float) inner.getBottom();

    // 归一化值表（clamp 后）
    std::vector<float> n ((size_t) N);
    for (int i = 0; i < N; ++i)
        n[(size_t) i] = std::clamp (frame.normalized[i], 0.0f, 1.0f);

    // 边缘高度表：edge[i] = 柱 i 左缘高度, edge[i+1] = 柱 i 右缘高度
    // edge[k] = (n[k-1] + n[k]) / 2；两端点用自身值（不悬空）
    std::vector<float> edge ((size_t) N + 1);
    edge[0] = n[0];
    for (int k = 1; k < N; ++k)
        edge[(size_t) k] = 0.5f * (n[(size_t) k - 1] + n[(size_t) k]);
    edge[(size_t) N] = n[(size_t) N - 1];

    // 渐变模板（底=primary@0.85, 顶=secondary@0.35，逐柱重设范围）
    juce::ColourGradient grad (rp.primary.withAlpha (0.85f),
                               0.0f, yBot,
                               rp.secondary.withAlpha (0.35f),
                               0.0f, 0.0f,
                               false);

    // 画梯形柱（贴底柱跳过；顶点 y 用 canvas 映射与旧版口径一致）
    for (int i = 0; i < N; ++i)
    {
        if (n[(size_t) i] < 0.005f) continue;

        const float xL    = x0 + (float) i * slotW;
        const float xR    = xL + barW;
        const float yEdgeL = normalizedToY_ (edge[(size_t) i],     canvas);
        const float yEdgeR = normalizedToY_ (edge[(size_t) i + 1], canvas);
        const float yPeak  = juce::jmin (yEdgeL, yEdgeR);   // 顶边最高点
        if (yBot - yPeak < 1.0f) continue;

        juce::Path bar;
        bar.startNewSubPath (xL, yEdgeL);
        bar.lineTo          (xR, yEdgeR);
        bar.lineTo          (xR, yBot);
        bar.lineTo          (xL, yBot);
        bar.closeSubPath();

        grad.point1 = juce::Point<float> (0.0f, yBot);
        grad.point2 = juce::Point<float> (0.0f, yPeak);
        g.setGradientFill (grad);
        g.fillPath (bar);

        // 顶边斜面描边（primary，宽度随 lineWidth）
        juce::Path topEdge;
        topEdge.startNewSubPath (xL, yEdgeL);
        topEdge.lineTo          (xR, yEdgeR);
        g.setColour (rp.primary);
        g.strokePath (topEdge, juce::PathStrokeType (
            juce::jmax (1.0f, rp.lineWidth),
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    }
    g.setColour (juce::Colours::white);

    // 峰值帽（可选，斜面状态机 v2）：
    //   · 仅峰值刷新（peakDb 上升 = 顶到新峰）时捕获形状 = 当时柱顶斜率
    //   · 持平/下落 → 冻结形状，位置随 y(peakDb) 移动（保持期悬停可见、下落期刚性下落）
    //   · x 范围伸出柱两侧各 gap/4，端点 y 沿捕获斜率线性外推
    if (rp.barParticles)
    {
        const size_t nU = (size_t) N;
        if (lastPeakDb_.size() != nU)
        {
            lastPeakDb_.assign (nU, -1.0e9f);   // 首帧/带数变化：视为峰值刷新 → 捕获
            capOffL_.assign (nU, 0.0f);
            capOffR_.assign (nU, 0.0f);
        }

        g.setColour (rp.peak.withAlpha (0.85f));
        for (int i = 0; i < N; ++i)
        {
            const size_t k = (size_t) i;
            const float pn = std::clamp ((frame.peakDb[i] - rp.minDb)
                                           / (rp.maxDb - rp.minDb), 0.0f, 1.0f);
            const float yCentre = normalizedToY_ (pn, canvas);
            const float yEdgeL  = normalizedToY_ (edge[k],     canvas);
            const float yEdgeR  = normalizedToY_ (edge[k + 1], canvas);

            // 峰值刷新检测：peakDb 上升 = 新峰顶到 → 重新捕获帽形状（含当时斜率）
            if (frame.peakDb[i] > lastPeakDb_[k] + 1.0e-4f)
            {
                capOffL_[k] = yEdgeL - yCentre;
                capOffR_[k] = yEdgeR - yCentre;
            }
            lastPeakDb_[k] = frame.peakDb[i];

            if (pn < 0.01f) continue;

            // x 伸出柱两侧 gap/4（旧版可见性），y 沿斜率外推
            const float xL    = x0 + (float) i * slotW;
            const float xR    = xL + barW;
            const float slope = (barW > 0.5f) ? (capOffR_[k] - capOffL_[k]) / barW : 0.0f;
            const float capLx = xL - gap * 0.25f;
            const float capRx = xR + gap * 0.25f;
            const float capLy = yCentre + capOffL_[k] - slope * (gap * 0.25f);
            const float capRy = yCentre + capOffR_[k] + slope * (gap * 0.25f);

            juce::Path cap;
            cap.startNewSubPath (capLx, capLy);
            cap.lineTo          (capRx, capRy);
            g.strokePath (cap, juce::PathStrokeType (1.2f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }
    }
}
