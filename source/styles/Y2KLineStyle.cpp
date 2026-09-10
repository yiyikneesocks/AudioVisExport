// =============================================================================
// Y2KLineStyle.cpp — Y2Kmeter 风格频谱曲线实现（Step 3 完整搬运）
//
// 完全照搬 Y2Kmeter source/ui/modules/SpectrumModule.cpp 的绘制逻辑：
//   · freqToX / dbToY          → SpectrumModule.cpp:92-113
//   · drawCurves（Catmull-Rom）→ SpectrumModule.cpp:469-595
//   · drawGrid                 → SpectrumModule.cpp:419-456
//   · drawAxisLabels           → SpectrumModule.cpp:600-627
//
// 替换：
//   · PinkXP::pinkXXX 颜色 → RenderParams::primary/secondary/peak
//   · PinkXP 字体         → juce::Font 默认
//   · 不依赖 PinkXP / hover ruler / 缓存 Image
//
// opacity 处理：由 VisPipeline::renderFrame 的 g.setOpacity(rp.opacity) 全局控制，
//   本样式内颜色只带自身 alpha（与 Y2K 一致），不再乘 rp.opacity，避免双重叠加。
// =============================================================================
#include "Y2KLineStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

// -----------------------------------------------------------------------------
// 频率 → X 像素坐标（对数轴，照搬 SpectrumModule.cpp:92-98）
// -----------------------------------------------------------------------------
float Y2KLineStyle::freqToX_ (float freqHz, const juce::Rectangle<int>& canvas,
                               float minHz, float maxHz)
{
    const float f = juce::jlimit (minHz, maxHz, freqHz);
    const float t = (std::log10 (f) - std::log10 (minHz))
                  / (std::log10 (maxHz) - std::log10 (minHz));
    return (float) canvas.getX() + t * (float) canvas.getWidth();
}

// -----------------------------------------------------------------------------
// dBFS → Y 像素坐标（越大越响越靠上，照搬 SpectrumModule.cpp:108-113）
// -----------------------------------------------------------------------------
float Y2KLineStyle::dbToY_ (float db, const juce::Rectangle<int>& canvas,
                             float minDb, float maxDb)
{
    const float d = juce::jlimit (minDb, maxDb, db);
    const float t = (d - minDb) / (maxDb - minDb);   // 0..1，越大越响
    return (float) canvas.getBottom() - t * (float) canvas.getHeight();
}

// -----------------------------------------------------------------------------
// Catmull-Rom (tension=0.5) → 三次贝塞尔 平滑路径
//   照搬 SpectrumModule.cpp:501-560，含：
//   · 端点重复（P[-1]=P[0], P[n]=P[n-1]）自然收束
//   · 沉底直线段（p1、p2 都贴底 → 直接 lineTo，消除小弧）
//   · 贴底端切线归零（避免从底抬起/跌入时的鼓包）
//   · 控制点 y 限幅防过冲
// -----------------------------------------------------------------------------
void Y2KLineStyle::buildSmoothPath_ (juce::Path& path,
                                      const std::vector<juce::Point<float>>& P,
                                      bool closeToBottom, float yBot, float yTop) const
{
    const int n = (int) P.size();
    if (n < 2) return;

    // "沉底"判定阈值（像素）：y 到画布底距离 ≤ 此值视为贴底
    constexpr float floorPixelTol = 0.75f;
    auto atFloor = [yBot, floorPixelTol] (const juce::Point<float>& p) noexcept
    {
        return (yBot - p.y) <= floorPixelTol;
    };

    if (closeToBottom) {
        path.startNewSubPath (P[0].x, yBot);  // fill 版：从底部开始
        path.lineTo (P[0]);                    // 先拉一条竖线到第一个点
    } else {
        path.startNewSubPath (P[0]);
    }

    for (int i = 0; i < n - 1; ++i)
    {
        const auto& p0 = (i == 0)     ? P[0]     : P[i - 1];
        const auto& p1 = P[i];
        const auto& p2 = P[i + 1];
        const auto& p3 = (i + 2 >= n) ? P[n - 1] : P[i + 2];

        // 沉底直线段：p1、p2 都贴底 → 直接 lineTo 到 (p2.x, yBot)
        if (atFloor (p1) && atFloor (p2))
        {
            path.lineTo (p2.x, yBot);
            continue;
        }

        // Catmull-Rom (tension=0.5) → Bezier 控制点
        const float c1x = p1.x + (p2.x - p0.x) / 6.0f;
        const float c2x = p2.x - (p3.x - p1.x) / 6.0f;
        float c1y       = p1.y + (p2.y - p0.y) / 6.0f;
        float c2y       = p2.y - (p3.y - p1.y) / 6.0f;

        // 贴底端切线归零
        if (atFloor (p1)) c1y = p1.y;
        if (atFloor (p2)) c2y = p2.y;

        // 控制点 y 限幅防过冲
        c1y = juce::jlimit (yTop, yBot, c1y);
        c2y = juce::jlimit (yTop, yBot, c2y);

        path.cubicTo (c1x, c1y, c2x, c2y, p2.x, p2.y);
    }

    if (closeToBottom)
    {
        path.lineTo (P[n - 1].x, yBot);
        path.closeSubPath();
    }
}

// -----------------------------------------------------------------------------
// 网格：每 20dB 横线（0dB 强调）+ 主 decade(100/1k/10k) + 次(30/50/200/300/500/2k/3k/5k/15k) 纵线
//   照搬 SpectrumModule.cpp:419-456
// -----------------------------------------------------------------------------
void Y2KLineStyle::drawGrid_ (juce::Graphics& g, const juce::Rectangle<int>& canvas,
                               const RenderParams& rp) const
{
    if (! rp.drawGrid) return;
    auto inner = canvas;
    if (inner.isEmpty()) return;

    // 横线（每 20dB；0dB 强调）
    for (int db = (int) rp.minDb; db <= (int) rp.maxDb; db += 20)
    {
        const int y = (int) std::round (dbToY_ ((float) db, canvas, rp.minDb, rp.maxDb));
        if (y < inner.getY() || y > inner.getBottom()) continue;
        g.setColour (db == 0 ? rp.secondary.withAlpha (0.70f)
                             : rp.secondary.withAlpha (0.22f));
        g.drawHorizontalLine (y, (float) inner.getX(), (float) inner.getRight());
    }

    // 纵线（次 decade）
    const float minorFreqs[] = { 30.0f, 50.0f, 200.0f, 300.0f, 500.0f,
                                 2000.0f, 3000.0f, 5000.0f, 15000.0f };
    g.setColour (rp.secondary.withAlpha (0.14f));
    for (float f : minorFreqs)
    {
        const int x = (int) std::round (freqToX_ (f, canvas, rp.minHz, rp.maxHz));
        if (x > inner.getX() && x < inner.getRight())
            g.drawVerticalLine (x, (float) inner.getY(), (float) inner.getBottom());
    }
    // 纵线（主 decade）
    const float majorFreqs[] = { 100.0f, 1000.0f, 10000.0f };
    g.setColour (rp.secondary.withAlpha (0.30f));
    for (float f : majorFreqs)
    {
        const int x = (int) std::round (freqToX_ (f, canvas, rp.minHz, rp.maxHz));
        if (x > inner.getX() && x < inner.getRight())
            g.drawVerticalLine (x, (float) inner.getY(), (float) inner.getBottom());
    }
}

// -----------------------------------------------------------------------------
// 坐标轴标签：左侧 dB + 底部 Hz（照搬 SpectrumModule.cpp:600-627）
// -----------------------------------------------------------------------------
void Y2KLineStyle::drawAxisLabels_ (juce::Graphics& g, const juce::Rectangle<int>& canvas,
                                     const RenderParams& rp) const
{
    if (! rp.drawAxisLabels) return;
    g.setColour (rp.secondary.withAlpha (0.55f));
    g.setFont (10.0f);

    // 左侧 dB 标签
    for (int db = (int) rp.minDb; db <= (int) rp.maxDb; db += 20)
    {
        const int y = (int) std::round (dbToY_ ((float) db, canvas, rp.minDb, rp.maxDb));
        const juce::String s = (db == 0) ? " 0" : juce::String (db);
        g.drawText (s, canvas.getX() - 30, y - 6, 26, 12,
                    juce::Justification::centredRight, false);
    }

    // 底部 Hz 标签
    struct L { float hz; const char* label; };
    const L labels[] = {
        { 20.0f, "20" }, { 100.0f, "100" }, { 1000.0f, "1k" },
        { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };
    for (const auto& l : labels)
    {
        const int x = (int) std::round (freqToX_ (l.hz, canvas, rp.minHz, rp.maxHz));
        if (x < canvas.getX() - 4 || x > canvas.getRight() + 4) continue;
        g.drawText (l.label, x - 18, canvas.getBottom() + 2, 36, 12,
                    juce::Justification::centred, false);
    }
}

// -----------------------------------------------------------------------------
// render 主入口（照搬 SpectrumModule::drawCurves 469-595）
//   1) 网格 + 坐标轴标签
//   2) N 个采样点映射到像素坐标（等距 x；y 由 dbToY 换算）
//   3) 填充区域（closeToBottom，半透明 tint）
//   4) 主曲线双层描边（外粗半透明 + 内细不透明）
//   5) 峰值保持虚线（Catmull-Rom 平滑后 createDashedStroke）
// -----------------------------------------------------------------------------
void Y2KLineStyle::render (juce::Graphics& g,
                            const juce::Rectangle<int>& canvas,
                            const BandFrame& frame,
                            const RenderParams& rp)
{
    // 1) 网格 + 标签
    drawGrid_ (g, canvas, rp);
    drawAxisLabels_ (g, canvas, rp);

    const int N = frame.bandCount;
    if (N <= 1) return;

    auto inner = canvas;
    if (inner.getWidth() <= 2 || inner.getHeight() <= 2) return;

    // 2) 采样点 → 像素坐标（x 走 inner 等距；y 走 canvas 的 dbToY，与 Y2K 一致）
    std::vector<juce::Point<float>> curvePts ((size_t) N);
    const float x0     = (float) inner.getX();
    const float xLen   = (float) inner.getWidth();
    const float invMax = 1.0f / (float) juce::jmax (1, N - 1);
    for (int i = 0; i < N; ++i)
    {
        const float x = x0 + (float) i * invMax * xLen;
        const float y = dbToY_ (frame.db[i], canvas, rp.minDb, rp.maxDb);
        curvePts[(size_t) i] = { x, y };
    }

    const float yTop = (float) inner.getY();
    // v0.5.4 #4/#3(2)：基线轴 a —— 柱/线同构的"轴零点生长"：
    //   显示值 = baselineTop(n) = a + (1−a)·n（上臂），baselineBottom(n) = a·(1−n)（下臂）。
    //   a=0 退化为原版（曲线全量程、填充到画布底）。
    const float a    = juce::jlimit (0.0f, 1.0f, rp.baselineY);
    const float yBot = (float) inner.getBottom()
                     - a * (float) canvas.getHeight();
    ColorMap cm;
    cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap  = ! cm.isSolid();
    const float xRight = x0 + xLen;

    // 归一化值表（曲线语义：db → 0..1）
    std::vector<float> nv ((size_t) N);
    for (int i = 0; i < N; ++i)
        nv[(size_t) i] = juce::jlimit (0.0f, 1.0f, (frame.db[i] - rp.minDb) / (rp.maxDb - rp.minDb));

    // 3) 填充区域：a>0 → **双线**：上臂（a..1 区间内扩）+ 下臂（a..0 镜像扩）
    //    （下臂曲线点 = baselineBottom(nv)；填充 = 上臂曲线→轴、下臂曲线→轴 两大块）
    if (a > 0.001f)
    {
        std::vector<juce::Point<float>> up ((size_t) N), dn ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            const float x = x0 + (float) i * invMax * xLen;
            up[(size_t) i] = { x, (float) inner.getBottom() - baselineTop    (nv[(size_t) i], a) * (float) canvas.getHeight() };
            dn[(size_t) i] = { x, (float) inner.getBottom() - baselineBottom (nv[(size_t) i], a) * (float) canvas.getHeight() };
        }
        // 上臂：平滑（yBot=轴 在点集下方 → closeToBottom 剪枝安全）；#6 lineOnly → 跳过填充
        if (! rp.lineOnly)
        {
            juce::Path fillUp;
            buildSmoothPath_ (fillUp, up, /*closeToBottom*/ true, yBot, yTop);
            if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 0.25f));
            else        g.setColour (rp.secondary.withAlpha (0.25f));
            g.fillPath (fillUp);
        }
        juce::Path curveUp;
        buildSmoothPath_ (curveUp, up, false, yBot, yTop);
        if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 1.0f));
        else        g.setColour (rp.primary);
        g.strokePath (curveUp, juce::PathStrokeType (rp.lineWidth));
        // 下臂：点在轴下方，buildSmoothPath_ 的贴底剪枝会把它们砍平 → 手构折线（与 polyline 下臂同法）
        if (! rp.lineOnly)
        {
            juce::Path fillDn;
            fillDn.startNewSubPath (dn[0].getX(), yBot);
            fillDn.lineTo (dn[0]);
            for (int i = 1; i < N; ++i) fillDn.lineTo (dn[(size_t) i]);
            fillDn.lineTo (dn[(size_t) N - 1].getX(), yBot);
            fillDn.closeSubPath();
            // v0.5.4 #10：下臂填充须重置为与上臂填充同色——此前继承了上臂描边的 1.0f 渐变/颜色，
            //   导致下臂比上臂实得多（拖动基线轴时"基轴两侧颜色不一致"）
            if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 0.25f));
            else        g.setColour (rp.secondary.withAlpha (0.25f));
            g.fillPath (fillDn);
        }
        juce::Path curveDn;
        curveDn.startNewSubPath (dn[0]);
        for (int i = 1; i < N; ++i) curveDn.lineTo (dn[(size_t) i]);
        // v0.5.4 #3.4：下臂描边必须显式设色——此前直接 strokePath，沿用上面 fill 留下的
        //   secondary 0.25f（或 0.25 渐变），描边几乎不可见（用户报"下面那侧没有描边"）。
        if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 1.0f));
        else        g.setColour (rp.primary);
        g.strokePath (curveDn, juce::PathStrokeType (rp.lineWidth));
        g.setColour (juce::Colours::white);
    }
    else
    {
        // 3) 填充区域（从底部到曲线的半透明 tint；#6 lineOnly → 跳过）
        if (! rp.lineOnly)
        {
            juce::Path fillPath;
            buildSmoothPath_ (fillPath, curvePts, /*closeToBottom*/ true, yBot, yTop);
            if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 0.25f));
            else        g.setColour (rp.secondary.withAlpha (0.25f));
            g.fillPath (fillPath);
        }

        // 4) 主曲线双层描边（外粗半透明 + 内细不透明，视觉厚度）
        juce::Path curvePath;
        buildSmoothPath_ (curvePath, curvePts, /*closeToBottom*/ false, yBot, yTop);
        if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 0.35f));
        else        g.setColour (rp.primary.withAlpha (0.35f));
        g.strokePath (curvePath, juce::PathStrokeType (3.0f));
        if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 1.0f));
        else        g.setColour (rp.primary);
        g.strokePath (curvePath, juce::PathStrokeType (rp.lineWidth));
        g.setColour (juce::Colours::white);   // 清渐变
    }

    // 5) 峰值保持虚线（同样 Catmull-Rom 平滑后 createDashedStroke）
    //    v0.5.4 #3.3：line 系峰线改由 "Peak caps" 开关统一控制（此前该开关对本样式置灰、无法关闭）
    if (! rp.barParticles) return;

    std::vector<float> pnv ((size_t) N);
    for (int i = 0; i < N; ++i)
        pnv[(size_t) i] = std::clamp ((frame.peakDb[i] - rp.minDb) / (rp.maxDb - rp.minDb), 0.0f, 1.0f);

    // 上臂峰线：a=0 走原始 dbToY（零回归）；a>0 与曲线同构地过 baselineTop（#5）
    std::vector<juce::Point<float>> peakPts ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const float x = x0 + (float) i * invMax * xLen;
        const float y = (a > 0.001f)
            ? (float) inner.getBottom() - baselineTop (pnv[(size_t) i], a) * (float) canvas.getHeight()
            : dbToY_ (frame.peakDb[i], canvas, rp.minDb, rp.maxDb);
        peakPts[(size_t) i] = { x, y };
    }
    juce::Path peakPath;
    buildSmoothPath_ (peakPath, peakPts, /*closeToBottom*/ false, yBot, yTop);

    juce::Path dashedPeakPath;
    const float dashes[] = { 3.0f, 3.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashedPeakPath, peakPath, dashes, 2);
    g.setColour (rp.peak.withAlpha (0.75f));
    g.fillPath (dashedPeakPath);

    // v0.5.4 #3.4：轴不在底/顶时，下臂也要有一条峰线（此前只有上侧有）。
    //   下臂点在轴下方，buildSmoothPath_ 的贴底剪枝会砍平 → 手构折线（与下臂曲线同法）。
    if (a > 0.001f)
    {
        juce::Path peakDnPath;
        for (int i = 0; i < N; ++i)
        {
            const float x = x0 + (float) i * invMax * xLen;
            const float y = (float) inner.getBottom()
                          - baselineBottom (pnv[(size_t) i], a) * (float) canvas.getHeight();
            if (i == 0) peakDnPath.startNewSubPath (x, y);
            else        peakDnPath.lineTo (x, y);
        }
        juce::Path dashedDn;
        juce::PathStrokeType (1.2f).createDashedStroke (dashedDn, peakDnPath, dashes, 2);
        g.setColour (rp.peak.withAlpha (0.75f));
        g.fillPath (dashedDn);
    }
}
