// =============================================================================
// PolylineStyle.cpp — 折线形频谱图（完整实现）
//
// 与 Y2KLineStyle 的核心区别：用直线段 (lineTo) 连接采样点，无 Catmull-Rom 平滑。
// 视觉效果：棱角分明，呈"折线/锯齿"状，适合电子/复古风格。
//
// 三层：
//   1) 半透明填充（从曲线到底部）
//   2) 主折线描边（双层：外粗半透明 + 内细不透明）
//   3) 峰值折线虚线
// 可选：采样点圆点（drawDots，默认关，用 --set visual.drawDots=true 开启）
// =============================================================================
#include "PolylineStyle.h"
#include "../core/ColorMap.h"
#include <cmath>
#include <algorithm>

float PolylineStyle::freqToX_ (float freqHz, const juce::Rectangle<int>& canvas,
                                float minHz, float maxHz)
{
    const float f = juce::jlimit (minHz, maxHz, freqHz);
    const float t = (std::log10 (f) - std::log10 (minHz))
                  / (std::log10 (maxHz) - std::log10 (minHz));
    return (float) canvas.getX() + t * (float) canvas.getWidth();
}

float PolylineStyle::dbToY_ (float db, const juce::Rectangle<int>& canvas,
                              float minDb, float maxDb)
{
    const float d = juce::jlimit (minDb, maxDb, db);
    const float t = (d - minDb) / (maxDb - minDb);
    return (float) canvas.getBottom() - t * (float) canvas.getHeight();
}

void PolylineStyle::render (juce::Graphics& g,
                            const juce::Rectangle<int>& canvas,
                            const BandFrame& frame,
                            const RenderParams& rp)
{
    // 网格
    if (rp.drawGrid)
    {
        auto inner = canvas.reduced (2);
        if (! inner.isEmpty())
        {
            for (int db = (int) rp.minDb; db <= (int) rp.maxDb; db += 20)
            {
                int y = (int) std::round (dbToY_ ((float) db, canvas, rp.minDb, rp.maxDb));
                if (y < inner.getY() || y > inner.getBottom()) continue;
                g.setColour (db == 0 ? rp.secondary.withAlpha (0.70f)
                                     : rp.secondary.withAlpha (0.22f));
                g.drawHorizontalLine (y, (float) inner.getX(), (float) inner.getRight());
            }
            const float minorFreqs[] = { 30.0f, 50.0f, 200.0f, 300.0f, 500.0f,
                                         2000.0f, 3000.0f, 5000.0f, 15000.0f };
            g.setColour (rp.secondary.withAlpha (0.14f));
            for (float f : minorFreqs)
            {
                int x = (int) std::round (freqToX_ (f, canvas, rp.minHz, rp.maxHz));
                if (x > inner.getX() && x < inner.getRight())
                    g.drawVerticalLine (x, (float) inner.getY(), (float) inner.getBottom());
            }
            const float majorFreqs[] = { 100.0f, 1000.0f, 10000.0f };
            g.setColour (rp.secondary.withAlpha (0.30f));
            for (float f : majorFreqs)
            {
                int x = (int) std::round (freqToX_ (f, canvas, rp.minHz, rp.maxHz));
                if (x > inner.getX() && x < inner.getRight())
                    g.drawVerticalLine (x, (float) inner.getY(), (float) inner.getBottom());
            }
        }
    }

    const int N = frame.bandCount;
    if (N <= 1) return;

    auto inner = canvas.reduced (2);
    if (inner.getWidth() <= 2 || inner.getHeight() <= 2) return;

    // 采样点 → 像素坐标（x 等距，y 由 dbToY）
    std::vector<juce::Point<float>> pts ((size_t) N);
    const float x0   = (float) inner.getX();
    const float xLen = (float) inner.getWidth();
    const float invN = 1.0f / (float) juce::jmax (1, N - 1);
    for (int i = 0; i < N; ++i)
    {
        const float x = x0 + (float) i * invN * xLen;
        const float y = dbToY_ (frame.db[i], canvas, rp.minDb, rp.maxDb);
        pts[(size_t) i] = { x, y };
    }

    // v0.5.4 #4/#3(2)：基线轴 a —— a>0 = 双折线（上臂/下臂镜像扩展，与柱同构）；a=0 原版
    const float a    = juce::jlimit (0.0f, 1.0f, rp.baselineY);
    const float yBot = (float) inner.getBottom() - a * (float) canvas.getHeight();
    std::vector<float> nv ((size_t) N);
    for (int i = 0; i < N; ++i)
        nv[(size_t) i] = juce::jlimit (0.0f, 1.0f, (frame.db[i] - rp.minDb) / (rp.maxDb - rp.minDb));

    ColorMap cm;
    cm.configure (rp.colorMap, rp.primary, rp.secondary, rp.peak);
    const bool useMap = ! cm.isSolid();
    const float xRight = x0 + xLen;

    // 上下臂折线点（a>0 时）
    std::vector<juce::Point<float>> up ((size_t) N), dn ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const float x = pts[(size_t) i].getX();
        up[(size_t) i] = { x, (float) inner.getBottom() - baselineTop    (nv[(size_t) i], a) * (float) canvas.getHeight() };
        dn[(size_t) i] = { x, (float) inner.getBottom() - baselineBottom (nv[(size_t) i], a) * (float) canvas.getHeight() };
    }

    if (a > 0.001f)
    {
        // 双填充（各自闭合到轴线）
        if (! rp.lineOnly)
        for (int which = 0; which < 2; ++which)
        {
            const auto& P = which == 0 ? up : dn;
            juce::Path fp;
            fp.startNewSubPath (P[0].x, yBot);
            fp.lineTo (P[0]);
            for (int i = 1; i < N; ++i) fp.lineTo (P[(size_t) i]);
            fp.lineTo (P[(size_t) N - 1].x, yBot);
            fp.closeSubPath();
            if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 0.25f));
            else        g.setColour (rp.secondary.withAlpha (0.25f));
            g.fillPath (fp);
        }
        // 双折线描边
        for (int which = 0; which < 2; ++which)
        {
            const auto& P = which == 0 ? up : dn;
            juce::Path lp;
            lp.startNewSubPath (P[0]);
            for (int i = 1; i < N; ++i) lp.lineTo (P[(size_t) i]);
            if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 1.0f));
            else        g.setColour (rp.primary);
            g.strokePath (lp, juce::PathStrokeType (rp.lineWidth));
        }
        g.setColour (juce::Colours::white);
    }
    else
    {
        // 1) 半透明填充（#6 lineOnly → 跳过）
        if (rp.lineOnly) { }
        else
        {
        juce::Path fillPath;
        fillPath.startNewSubPath (pts[0].x, yBot);
        fillPath.lineTo (pts[0]);
        for (int i = 1; i < N; ++i)
            fillPath.lineTo (pts[i]);
        fillPath.lineTo (pts[N - 1].x, yBot);
        fillPath.closeSubPath();
        if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 0.25f));
        else        g.setColour (rp.secondary.withAlpha (0.25f));
        g.fillPath (fillPath);
        }   // end !lineOnly

        // 2) 主折线双层描边（外粗半透明 + 内细不透明）
        juce::Path linePath;
        linePath.startNewSubPath (pts[0]);
        for (int i = 1; i < N; ++i)
            linePath.lineTo (pts[i]);

        if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 0.35f));
        else        g.setColour (rp.primary.withAlpha (0.35f));
        g.strokePath (linePath, juce::PathStrokeType (3.0f));
        if (useMap) g.setGradientFill (cm.horizontalGradient (x0, xRight, yBot, 1.0f));
        else        g.setColour (rp.primary);
        g.strokePath (linePath, juce::PathStrokeType (rp.lineWidth));
        g.setColour (juce::Colours::white);   // 清渐变
    }

    // 3) 峰值折线虚线
    // #5：基线轴模式下峰线跟随上臂映射
    auto peakY = [&] (float db) -> float
    {
        const float y0 = dbToY_ (db, canvas, rp.minDb, rp.maxDb);
        if (a <= 0.001f) return y0;
        const float pn = std::clamp ((db - rp.minDb) / (rp.maxDb - rp.minDb), 0.0f, 1.0f);
        return (float) inner.getBottom() - baselineTop (pn, a) * (float) canvas.getHeight();
    };
    juce::Path peakPath;
    peakPath.startNewSubPath (x0, peakY (frame.peakDb[0]));
    for (int i = 1; i < N; ++i)
    {
        const float x = x0 + (float) i * invN * xLen;
        peakPath.lineTo (x, peakY (frame.peakDb[i]));
    }
    juce::Path dashedPeak;
    const float dashes[] = { 3.0f, 3.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashedPeak, peakPath, dashes, 2);
    g.setColour (rp.peak.withAlpha (0.75f));
    g.fillPath (dashedPeak);
}
