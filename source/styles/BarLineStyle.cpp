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
#include "../core/ColorMap.h"
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

    // v0.5.4 #2：取消 reduced(2) 缩进（与 BarStyle 同基准）
    auto inner = canvas;
    if (inner.getWidth() <= 2 || inner.getHeight() <= 2) return;

    // 布局：与 BarStyle 完全一致（v0.5.4 #25 三联动：pitch / gap / width）
    // #3''（用户新模型）：pitch = 目标带宽比（相对画布宽）→ 柱系按 pitch 铺、floor 留白；
    //   渲染不再用 innerW/N，而是 slotW = pitch × innerW（bandCount 由 pitch 推得，两量自洽）。
    const float slotW = juce::jlimit (0.001f, 1.0f, rp.barPitchRatio) * (float) inner.getWidth();
    const float gap   = juce::jlimit (-2.48f, 2.48f, rp.barGapRatio) * (float) inner.getWidth() / (float) N;
    const float barW  = juce::jlimit (0.02f, 2.5f, rp.barWidthRatio) * (float) inner.getWidth() / (float) N;
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
    // #5（v0.5.4）：末柱外缘至少抬到邻带的 1/2 —— 高频静音（n≈0）时末柱坡度不再扎到 0
    edge[(size_t) N] = juce::jmax (n[(size_t) N - 1], 0.5f * n[(size_t) N - 2]);

    ColorMap cm;
    cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();

    // 画梯形柱（贴底柱跳过；顶点 y 用 canvas 映射与旧版口径一致）
    // v0.5.4 #4：柱以基线轴为零点上下按比例生长（a=0 退化为原底部生长）。
    const float a = juce::jlimit (0.0f, 1.0f, rp.baselineY);
    for (int i = 0; i < N; ++i)
    {
        if (n[(size_t) i] < 0.005f) continue;

        const float xL    = x0 + (float) i * slotW;
        const float xR    = xL + barW;
        const float yEdgeL = normalizedToY_ (baselineTop    (edge[(size_t) i],     a), canvas);
        const float yEdgeR = normalizedToY_ (baselineTop    (edge[(size_t) i + 1], a), canvas);
        const float yBotL  = normalizedToY_ (baselineBottom (edge[(size_t) i],     a), canvas);
        const float yBotR  = normalizedToY_ (baselineBottom (edge[(size_t) i + 1], a), canvas);
        const float yPeak  = juce::jmin (yEdgeL, yEdgeR);   // 顶边最高点
        if (yBotL - yPeak < 1.0f && yBotR - yPeak < 1.0f) continue;

        juce::Path bar;
        bar.startNewSubPath (xL, yEdgeL);
        bar.lineTo          (xR, yEdgeR);
        bar.lineTo          (xR, yBotR);
        bar.lineTo          (xL, yBotL);
        bar.closeSubPath();

        const juce::Colour bottom = useMap ? cm.colourForBand (i, N, n[(size_t) i]).withAlpha (0.85f)
                                           : rp.primary.withAlpha (0.85f);
        const juce::Colour top    = useMap ? cm.colourForBand (i, N, n[(size_t) i]).withAlpha (0.40f)
                                           : rp.secondary.withAlpha (0.35f);
        juce::ColourGradient grad (bottom, 0.0f, (yBotL + yBotR) * 0.5f, top, 0.0f, yPeak, false);
        g.setGradientFill (grad);
        g.fillPath (bar);

        // 顶边斜面描边（宽度随 lineWidth）
        juce::Path topEdge;
        topEdge.startNewSubPath (xL, yEdgeL);
        topEdge.lineTo          (xR, yEdgeR);
        g.setColour (useMap ? cm.colourForBand (i, N, n[(size_t) i]) : rp.primary);
        g.strokePath (topEdge, juce::PathStrokeType (
            juce::jmax (1.0f, rp.lineWidth),
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    }
    g.setColour (juce::Colours::white);

    // 峰值帽 v3（v0.5.4 #2，受 rp.barParticles 开关控制）——"柱顶的惯性延迟版"：
    //   · 每带一个帽顶点状态 capN[i]（归一化），几何与柱顶边完全同构：
    //     帽线段横跨柱宽 [xL,xR]，左端=(cap[i-1]+cap[i])/2、右端=(cap[i]+cap[i+1])/2
    //     → 相邻帽自然共享端点高度（"尾部 = 下一帽的头部"），gap 处不画（与柱一致）；
    //   · 第一原则：帽任何位置不得低于柱顶 → 顶点被 max 链 clamp 在 n[i] 上方，
    //     柱顶上升立即把帽顶上去并改变帽形（顶点值变化 → 斜率自然变形）；
    //   · 下落按帧积分：core 的 peakDb（含 hold/decay/accel 语义）提供下落下界，
    //     显示层再叠加"高处落得快"（+accel×当前高度）；
    //   · 邻域拉拽：向邻带均值靠拢（平直化趋势）→ 下坠可被邻带拽慢/提前/反向上升；
    //     拉拽后重新 clamp ≥ 柱顶（约束最高优先）。
    if (rp.barParticles)
    {
        const size_t nU = (size_t) N;
        if (capN_.size() != nU)
            capN_.assign (nU, 0.0f);   // 首帧/带数变化：从柱顶重新生长

        const float dbSpan = juce::jmax (1.0f, rp.maxDb - rp.minDb);
        const float dt = 1.0f / juce::jmax (1.0f, rp.fps);

        // 1) 状态推进：下落（高处快）+ 两个硬下界（core 峰值语义 / 柱顶）
        for (int i = 0; i < N; ++i)
        {
            const size_t k = (size_t) i;
            const float peakN  = std::clamp ((frame.peakDb[i] - rp.minDb) / dbSpan, 0.0f, 1.0f);
            const float barTop = n[k];
            const float fall   = (rp.peakDecayDbPerSec
                                  + rp.peakDecayAccelDbPerSec2 * capN_[k]) / dbSpan;   // 归一化/秒
            float c = capN_[k] - fall * dt;
            c = juce::jmax (c, peakN);      // core hold/decay 语义（悬停期不落）
            c = juce::jmax (c, barTop);     // 第一原则：不低于柱顶（柱上来=顶上去）
            capN_[k] = std::clamp (c, 0.0f, 1.0f);
        }

        // 2) 邻域拉拽（#2峰帽：capPull=0 → 跳过，斜面可拉得很长=原版峰值帽逻辑）
        if (rp.capPull > 0.001f)
        for (int pass = 0; pass < 2; ++pass)
        {
            std::vector<float> tmp (nU);
            for (int i = 0; i < N; ++i)
            {
                const size_t k = (size_t) i;
                const float l = capN_[k > 0 ? k - 1 : k];
                const float r = capN_[k < nU - 1 ? k + 1 : k];
                const float nb = 0.5f * (l + r);
                tmp[k] = capN_[k] + rp.capPull * (nb - capN_[k]);
            }
            capN_ = tmp;
            for (int i = 0; i < N; ++i)
                capN_[(size_t) i] = juce::jmax (capN_[(size_t) i], n[(size_t) i]);
        }

        // 3) 绘制：与柱顶边完全同构的帽线段——横跨柱宽 [xL,xR]（gap 处断开不连），
        //    端点高 = 帽折线在缘位置 k 的插值（与柱 edge 同式：内点取邻带均值，端带用自身），
        //    再与柱顶 edge[k] 取 max —— 斜面端点也严守"帽不低于柱顶"第一原则；
        //    edge[] 是两柱共享的 → 相邻帽端点值仍相等（"尾部 = 下一帽的头部"）。
        auto capEdgeDraw = [&] (int k) -> float
        {
            float c;
            if (k <= 0)     c = capN_[0];
            else if (k >= N) c = capN_[(size_t) N - 1];
            else            c = 0.5f * (capN_[(size_t) k - 1] + capN_[(size_t) k]);
            return juce::jmax (c, edge[(size_t) juce::jlimit (0, N, k)]);
        };
        juce::Path capPath;
        for (int i = 0; i < N; ++i)
        {
            const float xL = x0 + (float) i * slotW;
            const float xR = xL + barW;
            capPath.startNewSubPath (xL, normalizedToY_ (baselineTop (capEdgeDraw (i),     a), canvas));
            capPath.lineTo          (xR, normalizedToY_ (baselineTop (capEdgeDraw (i + 1), a), canvas));
        }
        g.setColour (rp.peak.withAlpha (0.9f));
        g.strokePath (capPath, juce::PathStrokeType (
            juce::jmax (1.2f, rp.lineWidth * 0.9f),
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));

        // #3(1)（v0.5.4）：基线轴 a>0 时柱有下臂 → 下侧画镜像峰帽（同一 capN 状态，
        //   用 baselineBottom 映射；端点同样做"至少邻带 1/2"的末柱抬升）。
        if (a > 0.001f)
        {
            auto capEdgeBot = [&] (int k) -> float
            {
                float c;
                if (k <= 0)      c = capN_[0];
                else if (k >= N) c = capN_[(size_t) N - 1];
                else             c = 0.5f * (capN_[(size_t) k - 1] + capN_[(size_t) k]);
                if (k >= N) c = juce::jmax (c, 0.5f * capN_[(size_t) N - 2]);
                return juce::jmax (c, edge[(size_t) juce::jlimit (0, N, k)]);
            };
            juce::Path capB;
            for (int i = 0; i < N; ++i)
            {
                const float xL = x0 + (float) i * slotW;
                const float xR = xL + barW;
                capB.startNewSubPath (xL, normalizedToY_ (baselineBottom (capEdgeBot (i),     a), canvas));
                capB.lineTo          (xR, normalizedToY_ (baselineBottom (capEdgeBot (i + 1), a), canvas));
            }
            g.strokePath (capB, juce::PathStrokeType (
                juce::jmax (1.2f, rp.lineWidth * 0.9f),
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }
        g.setColour (juce::Colours::white);
    }
}
