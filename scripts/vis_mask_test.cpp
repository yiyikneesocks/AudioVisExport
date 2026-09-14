// vis_mask_test — SpectrumMask::compose 回归：默认 contain 几何（#3）+ 不随电平漂移（BUG1）
// + 显式变换下渲染 bbox 与 visCorners（GUI 手柄）一致。构建：ninja vis_mask_test && ./build/vis_mask_test
#include <juce_graphics/juce_graphics.h>
#include "source/core/SpectrumMask.h"
#include "source/core/VisTransform.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <tuple>
#include <fstream>

namespace
{
    juce::Image makeCanvas (int W, int H, const juce::Rectangle<int>& fill)
    {
        juce::Image img (juce::Image::ARGB, W, H, true);
        juce::Graphics g (img);
        g.fillRect (fill.toFloat());
        return img;
    }

    struct Box { int x0 = -1, y0 = -1, x1 = -1, y1 = -1; int count = 0; };

    Box opaqueBox (const juce::Image& img)   // alpha>=250 的 bbox + 数量
    {
        Box b;
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < img.getHeight(); ++y)
        {
            const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            for (int x = 0; x < img.getWidth(); ++x)
            {
                if (line[x].getAlpha() >= 250)
                {
                    if (b.x0 < 0) { b.x0 = x; b.y0 = y; b.x1 = x; b.y1 = y; }
                    else { if (x < b.x0) b.x0 = x; if (x > b.x1) b.x1 = x;
                           if (y < b.y0) b.y0 = y; if (y > b.y1) b.y1 = y; }
                    ++b.count;
                }
            }
        }
        return b;
    }

    juce::Image makeBars (int W, int H, const std::vector<juce::Rectangle<int>>& bars)
    {
        juce::Image img (juce::Image::ARGB, W, H, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);
        for (auto& b : bars) g.fillRect (b.toFloat());
        return img;
    }
    juce::Image solidImg (int W, int H, juce::Colour c)
    {
        juce::Image img (juce::Image::ARGB, W, H, true);
        juce::Graphics g (img);
        g.fillAll (c);
        return img;
    }
    // 找描边像素（与轮廓内图片色区分的独特色）：按"与 rim 色距离<80、与底色距离>80"判
    std::vector<int> rimPerRow (const juce::Image& im, juce::PixelARGB rim)
    {
        std::vector<int> rows (im.getHeight(), 0);
        juce::Image::BitmapData bd (im, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < im.getHeight(); ++y)
        {
            const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            for (int x = 0; x < im.getWidth(); ++x)
            {
                const auto p = line[x];
                if (p.getAlpha() < 200) continue;
                const int dr = std::abs ((int) p.getRed()   - (int) rim.getRed());
                const int dg = std::abs ((int) p.getGreen() - (int) rim.getGreen());
                const int db = std::abs ((int) p.getBlue()  - (int) rim.getBlue());
                if (dr + dg + db < 90) ++rows[(size_t) y];
            }
        }
        return rows;
    }
    // 矩形并集内的"描边色像素"计数（列过滤：左右缘纵贯全高，顶/底缘判定必须排除）
    int countRimInRects (const juce::Image& im, juce::PixelARGB rim,
                         const std::vector<juce::Rectangle<int>>& rects)
    {
        int n = 0;
        juce::Image::BitmapData bd (im, juce::Image::BitmapData::readOnly);
        for (auto& r : rects)
            for (int y = r.getY(); y < r.getBottom() && y < im.getHeight(); ++y)
            {
                const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
                for (int x = r.getX(); x < r.getRight() && x < im.getWidth(); ++x)
                {
                    const auto p = line[x];
                    if (p.getAlpha() < 200) continue;
                    const int d = std::abs ((int) p.getRed()   - (int) rim.getRed())
                                + std::abs ((int) p.getGreen() - (int) rim.getGreen())
                                + std::abs ((int) p.getBlue()  - (int) rim.getBlue());
                    if (d < 90) ++n;
                }
            }
        return n;
    }
    int sumRange (const std::vector<int>& v, int a, int b)
    {
        int s = 0;
        for (int i = a; i <= b && i < (int) v.size(); ++i) s += v[(size_t) i];
        return s;
    }

    int failures = 0;
    void check (bool ok, const char* what)
    {
        std::printf ("%s %s\n", ok ? "ok  :" : "FAIL:", what);
        if (! ok) ++failures;
    }
}

int main()
{
    constexpr int W = 200, H = 100;

    juce::Image photo (juce::Image::ARGB, 100, 200, true);   // 竖图（宽高比 1:2）
    { juce::Graphics g (photo); g.fillAll (juce::Colours::red); }

    // ---- 1) 默认 contain：100x200 图进 200x100 画布 → s=min(2,0.5)=0.5 → 50x100 居中 (75..124) ----
    {
        MaskImageLayer cfg; cfg.enabled = true;
        juce::Image base = makeCanvas (W, H, { 0, 0, W, H });
        Box b = opaqueBox (SpectrumMask::compose (base, photo, cfg, {}));
        check (b.x0 == 75 && b.x1 == 124 && b.y0 == 0 && b.y1 == 99,
               "default set=false = contain into OUTPUT CANVAS (not frame-stretch)");
        std::printf ("      box=(%d,%d)-(%d,%d) n=%d\n", b.x0, b.y0, b.x1, b.y1, b.count);
    }

    // ---- 2) BUG1 回归：轮廓宽窄不同，图片几何必须一致（只有露出量不同） ----
    {
        MaskImageLayer cfg; cfg.enabled = true;
        juce::Image baseLow  = makeCanvas (W, H, { 0, 0, 30, H });    // "低电平"：左侧窄条
        juce::Image baseWide = makeCanvas (W, H, { 0, 0, 150, H });   // "高电平"：宽条
        Box b1 = opaqueBox (SpectrumMask::compose (baseLow,  photo, cfg, {}));
        Box b2 = opaqueBox (SpectrumMask::compose (baseWide, photo, cfg, {}));
        check (b1.count < b2.count, "wide contour reveals more pixels");
        juce::Image m1 = SpectrumMask::compose (baseLow, photo, cfg, {});
        juce::Image m2 = SpectrumMask::compose (baseWide, photo, cfg, {});
        juce::Image::BitmapData d1 (m1, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData d2 (m2, juce::Image::BitmapData::readOnly);
        bool same = true;
        for (int y = 0; y < H && same; ++y)
        {
            const auto* r1 = reinterpret_cast<const juce::PixelARGB*> (d1.getLinePointer (y));
            const auto* r2 = reinterpret_cast<const juce::PixelARGB*> (d2.getLinePointer (y));
            for (int x = 0; x < 30 && same; ++x)   // 共同可见列 0..29：像素应完全一致
                same = r1[x].getRed() == r2[x].getRed() && r1[x].getGreen() == r2[x].getGreen() && r1[x].getBlue() == r2[x].getBlue() && r1[x].getAlpha() == r2[x].getAlpha();
        }
        check (same, "shared columns identical pixel structure (geometry not drifting)");
    }

    // ---- 3) set=true：compose 渲染结果与 visCorners（GUI 手柄同一套数学）预测一致 ----
    {
        MaskImageLayer cfg; cfg.enabled = true;
        VisTransform t; t.set = true; t.scaleX = 0.3f; t.scaleY = 0.35f;
        t.rotationDeg = 20.0f; t.centerX = 50; t.centerY = 100; t.posX = 50; t.posY = -50;
        cfg.transform = t;
        juce::Image base = makeCanvas (W, H, { 0, 0, W, H });
        auto cs = visCorners (t, 100.0f, 200.0f);
        float ex0 = cs[0].getX(), ex1 = cs[0].getX(), ey0 = cs[0].getY(), ey1 = cs[0].getY();
        for (auto& c : cs) { ex0 = juce::jmin (ex0, c.getX()); ex1 = juce::jmax (ex1, c.getX());
                             ey0 = juce::jmin (ey0, c.getY()); ey1 = juce::jmax (ey1, c.getY()); }
        Box b = opaqueBox (SpectrumMask::compose (base, photo, cfg, {}));
        bool near = std::abs (b.x0 - (int) std::floor (ex0)) <= 3
                 && std::abs (b.x1 - (int) std::ceil (ex1))  <= 3
                 && std::abs (b.y0 - (int) std::floor (ey0)) <= 3
                 && std::abs (b.y1 - (int) std::ceil (ey1))  <= 3;
        check (near, "set=true: render bbox == visCorners prediction (handles align)");
        std::printf ("      render=(%d,%d)-(%d,%d) expect=(%d,%d)-(%d,%d)\n",
                     b.x0, b.y0, b.x1, b.y1, (int) ex0, (int) ey0, (int) ex1, (int) ey1);
    }

    // ---- 4) #4 色彩调整：去饱和=灰、亮度减半、对比度 0=中灰；identity 返回原图 ----
    {
        juce::Image photo (juce::Image::ARGB, 4, 4, true);
        { juce::Graphics g (photo); g.fillAll (juce::Colour::fromRGB (200, 100, 50)); }

        const auto id = SpectrumMask::adjustedImage (photo, 1, 1, 1);
        check (id == photo, "adjustedImage identity returns same image");

        juce::Image dImg = SpectrumMask::adjustedImage (photo, 1, 1, 0);
        juce::Image::BitmapData bd1 (dImg, juce::Image::BitmapData::readOnly);
        const juce::PixelARGB desat = *reinterpret_cast<const juce::PixelARGB*> (bd1.getLinePointer (0));
        const int luma = juce::roundToInt (0.299f * 200 + 0.587f * 100 + 0.114f * 50);
        check (std::abs ((int) desat.getRed() - luma) <= 2 && desat.getRed() == desat.getGreen()
               && desat.getGreen() == desat.getBlue(), "saturation=0 → pure grey (luma)");

        juce::Image bImg = SpectrumMask::adjustedImage (photo, 0.5f, 1, 1);
        juce::Image::BitmapData bd2 (bImg, juce::Image::BitmapData::readOnly);
        const juce::PixelARGB dark = *reinterpret_cast<const juce::PixelARGB*> (bd2.getLinePointer (0));
        check (std::abs ((int) dark.getRed() - 100) <= 2 && std::abs ((int) dark.getBlue() - 25) <= 2,
               "brightness=0.5 halves channels");

        juce::Image cImg = SpectrumMask::adjustedImage (photo, 1, 0, 1);
        juce::Image::BitmapData bd3 (cImg, juce::Image::BitmapData::readOnly);
        const juce::PixelARGB flat = *reinterpret_cast<const juce::PixelARGB*> (bd3.getLinePointer (0));
        check (flat.getRed() == 128 && flat.getGreen() == 128 && flat.getBlue() == 128,
               "contrast=0 → flat mid grey");
    }

    // ---- 5) 半透明像素保持预乘一致性（调整后 R'<=A' 恒成立）----
    {
        juce::Image half (juce::Image::ARGB, 2, 2, true);
        {
            juce::Graphics g (half);
            g.setColour (juce::Colour (0x80ff8040u));           // alpha=128 的橙色
            g.fillRect (0, 0, 2, 2);
        }
        juce::Image adj = SpectrumMask::adjustedImage (half, 2.0f, 2.0f, 2.0f);
        juce::Image::BitmapData bd (adj, juce::Image::BitmapData::readOnly);
        const juce::PixelARGB p = *reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (0));
        check (p.getAlpha() == 128 && p.getRed() <= p.getAlpha() && p.getGreen() <= p.getAlpha()
               && p.getBlue() <= p.getAlpha(), "premultiplied invariants hold after adjust (no unpremult leak)");
    }

    // ---- 6) #1b 描边环回归（compose stroke 路径）----
    //   旧 bug：行指针 e 后再用绝对下标 e[idx]（idx=y*W+x）= tmp[2*y*W+x]，
    //   y≥H/2 越界读堆（Windows 上 ACCESS_VIOLATION，dump=compose+0x8f5）。
    //   大画布让越界量达数 MB → 任何平台都暴露；四边都必须有勾边像素。
    {
        constexpr int SW = 1200, SH = 800;
        juce::Image base = makeCanvas (SW, SH, { 100, 50, SW - 200, SH - 100 });
        juce::Image pic (juce::Image::ARGB, 64, 64, true);
        { juce::Graphics g (pic); g.fillAll (juce::Colour::fromRGB (128, 128, 128)); }

        MaskImageLayer offCfg; offCfg.enabled = true;
        MaskImageLayer onCfg;  onCfg.enabled = true; onCfg.strokeEnabled = true; onCfg.strokeWidth = 5.0f;

        juce::Image outA = SpectrumMask::compose (base, pic, offCfg, juce::Colours::white);
        juce::Image outB = SpectrumMask::compose (base, pic, onCfg,  juce::Colours::white);
        check (outB.isValid(), "stroke compose returns valid image (no crash / no OOB)");

        int top = 0, bottom = 0, left = 0, right = 0, tot = 0;
        bool interiorDiff = false, outsideDiff = false;
        if (outA.isValid() && outB.isValid())
        {
            juce::Image::BitmapData da (outA, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData db (outB, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < SH; ++y)
            {
                const auto* ra = reinterpret_cast<const juce::PixelARGB*> (da.getLinePointer (y));
                const auto* rb = reinterpret_cast<const juce::PixelARGB*> (db.getLinePointer (y));
                for (int x = 0; x < SW; ++x)
                {
                    if (ra[x].getAlpha() == rb[x].getAlpha() && ra[x].getRed() == rb[x].getRed()
                     && ra[x].getGreen() == rb[x].getGreen() && ra[x].getBlue() == rb[x].getBlue()) continue;
                    ++tot;
                    if (y >= 50 && y <= 54 && x >= 300 && x <= 800)  ++top;
                    if (y >= 745 && y <= 749 && x >= 300 && x <= 800) ++bottom;
                    if (x >= 100 && x <= 104 && y >= 250 && y <= 550) ++left;
                    if (x >= 1095 && x <= 1099 && y >= 250 && y <= 550) ++right;
                    if (x == 600 && y == 400) interiorDiff = true;     // 深内部不应被描边
                    if (x == 5   && y == 5)   outsideDiff  = true;     // 轮廓外必须保持透明
                }
            }
        }
        std::printf ("      ring diffs: top=%d bottom=%d left=%d right=%d total=%d\n",
                     top, bottom, left, right, tot);
        // #1b 越界回归：左/右带扫到 y∈[250,550]（含 H/2 以下）→ 若仍有 2yW+x 越界会崩/乱；
        //   底部边已按 #1c1 取消 → 底缘 diff 应为 0。
        check (tot > 100 && top > 0 && left > 0 && right > 0 && bottom == 0,
               "rim on top/left/right (crosses lower half → anti-#1b), bottom edge removed (#1c1)");
        check (! interiorDiff, "rim does not touch deep interior");
        check (! outsideDiff,  "rim does not leak outside contour");
        check (tot < (SW + SH) * 40, "rim is a thin band, not whole-shape repaint");
    }



    // ============ 用例 7：上/左/右三边独立开关 + 无底部 + 斜面归属 + line 禁侧边（v0.5.6 #1c）============
    {
        const int CW = 200, CH = 40;
        auto base = makeBars (CW, CH, { {10, 4, 60, 32}, {80, 4, 60, 32} });
        auto img  = solidImg (CW, CH, juce::Colours::red);
        const juce::Colour rimC = juce::Colours::white;
        MaskImageLayer cfg; cfg.enabled = true;
        cfg.strokeEnabled = true; cfg.outlineMode = "image";
        cfg.outWTop = cfg.outWLeft = cfg.outWRight = 5.0f;

        // 柱体中段列专测顶缘（排除左右缘±8）；底缘区现在应恒空
        const std::vector<juce::Rectangle<int>> topR { {18, 4, 44, 5}, {88, 4, 44, 5} };
        const std::vector<juce::Rectangle<int>> botR { {18, 31, 44, 5}, {88, 31, 44, 5} };
        const std::vector<juce::Rectangle<int>> lefR { {10, 14, 5, 12}, {80, 14, 5, 12} };
        const std::vector<juce::Rectangle<int>> rigR { {65, 14, 5, 12}, {135, 14, 5, 12} };
        auto rims = [&] (const MaskImageLayer& c, bool sides = true)
        {
            auto im = SpectrumMask::compose (base, img, c, rimC, sides);
            return std::make_tuple (countRimInRects (im, rimC.getPixelARGB(), topR),
                                    countRimInRects (im, rimC.getPixelARGB(), botR),
                                    countRimInRects (im, rimC.getPixelARGB(), lefR),
                                    countRimInRects (im, rimC.getPixelARGB(), rigR));
        };
        int tA, bA, lA, rA;
        std::tie (tA, bA, lA, rA) = rims (cfg);
        check (tA > 80 && lA > 80 && rA > 80, "three edges on: top/left/right rims present");
        check (bA == 0, "bottom edge no longer drawn (removed per #1c1)");

        cfg.outTop = false; cfg.outWTop = 0.0f;
        int t2, b2, l2, r2;
        std::tie (t2, b2, l2, r2) = rims (cfg);
        check (t2 == 0 && l2 > 80 && r2 > 80, "outTop=false kills ONLY the top edge");
        cfg.outTop = true; cfg.outWTop = 5.0f;

        cfg.outLeft = false; cfg.outWLeft = 0.0f;
        std::tie (t2, b2, l2, r2) = rims (cfg);
        check (t2 > 80 && l2 == 0 && r2 > 80, "outLeft=false kills ONLY the left edge");
        cfg.outLeft = true; cfg.outWLeft = 5.0f;

        cfg.outRight = false; cfg.outWRight = 0.0f;
        std::tie (t2, b2, l2, r2) = rims (cfg);
        check (t2 > 80 && l2 > 80 && r2 == 0, "outRight=false kills ONLY the right edge");
        cfg.outRight = true; cfg.outWRight = 5.0f;

        // 新1c2：line 系（sideEdgesAllowed=false）→ 左右侧边强制无
        std::tie (t2, b2, l2, r2) = rims (cfg, /*sides=*/false);
        check (t2 > 80 && l2 == 0 && r2 == 0, "sideEdgesAllowed=false: top kept, sides suppressed (line styles)");

        // 新1c4：透明度 0 → 该边描边不可见；阴影 0/大 只影响外圈
        cfg.outAlphaLeft = 0.0f;
        auto imNoAlpha = SpectrumMask::compose (base, img, cfg, rimC);
        check (countRimInRects (imNoAlpha, rimC.getPixelARGB(), lefR) == 0,
               "edge opacity 0 → that edge invisible");
        cfg.outAlphaLeft = 1.0f;
    }

    // ============ 用例 7b：斜面归属（#1c3）——关顶只开侧，斜面上不该有侧边 ============
    {
        // 造一个斜顶多边形（模拟 bar-line 顶缘），高差明显
        const int CW = 120, CH = 80;
        juce::Image base (juce::Image::ARGB, CW, CH, true);
        { juce::Graphics g (base); g.setColour (juce::Colours::white);
          juce::Path tri; tri.startNewSubPath (10, 70); tri.lineTo (110, 10); tri.lineTo (110, 70); tri.closeSubPath();
          g.fillPath (tri); }   // 右边竖直、顶是斜面
        auto img = solidImg (CW, CH, juce::Colours::red);
        MaskImageLayer cfg; cfg.enabled = true; cfg.strokeEnabled = true; cfg.outlineMode = "image";
        cfg.outTop = false; cfg.outWTop = 0.0f;           // 关键：顶关
        cfg.outLeft = false; cfg.outRight = true; cfg.outWRight = 6.0f;  // 只开右
        auto im = SpectrumMask::compose (base, img, cfg, juce::Colours::white, /*sides=*/true);
        // 沿斜面取一小条（斜面上半段），关掉顶边后这里不该有白描边
        const std::vector<juce::Rectangle<int>> slantBand { {40, 32, 26, 12} };
        const int onSlant = countRimInRects (im, juce::Colours::white.getPixelARGB(), slantBand);
        check (onSlant == 0, "slanted top edge not drawn when only side edge on (fixes #1c3 leak)");
    }

    // ============ 用例 8：perBar 每柱独立可视区平均色（#5a）============
    {
        const int CW = 200, CH = 60;
        auto base = makeBars (CW, CH, { {10, 5, 30, 50}, {90, 5, 30, 50}, {150, 5, 30, 50} });
        juce::Image img (juce::Image::ARGB, CW, CH, true);
        { juce::Graphics g (img);
          g.fillAll (juce::Colours::blue);
          g.setColour (juce::Colours::green); g.fillRect (66, 0, 68, CH);
          g.setColour (juce::Colours::red);   g.fillRect (134, 0, 66, CH); }
        MaskImageLayer cfg; cfg.enabled = true;
        cfg.strokeEnabled = true; cfg.strokeWidth = 3.0f; cfg.outlineMode = "perbar";
        MaskImageLayer cfgNoS = cfg; cfgNoS.strokeEnabled = false;   // 无描边 out（= 描边前的中间态）
        auto out = SpectrumMask::compose (base, img, cfgNoS, juce::Colours::white);
        {
            juce::Image::BitmapData bdd (out, juce::Image::BitmapData::readOnly);
            for (int x : {20, 100, 160}) {
                const auto* ln = reinterpret_cast<const juce::PixelARGB*>(bdd.getLinePointer(30));
                auto q = ln[x];
                std::printf("      [dbg-out] x=%d y=30 a=%d r=%d g=%d b=%d\n", x, q.getAlpha(), q.getRed(), q.getGreen(), q.getBlue());
            }
        }
        auto plan = SpectrumMask::makeStrokePlan (out, cfg, juce::Colours::white);
        check (plan.perColumn, "perBar: per-column palette");
        check (plan.segColour.size() == 3, "perBar: 3 bars -> 3 segments");
        for (size_t s = 0; s < plan.segColour.size(); ++s)
            std::printf ("      [dbg] seg%u x[%d..%d] = %08x\n", (unsigned) s,
                         plan.segStart[s], plan.segEnd[s], plan.segColour[s].getARGB());
        if (plan.segColour.size() == 3)
        {
            check (plan.segColour[0].getBlue() > 150 && plan.segColour[0].getRed() < 80,
                   "perBar bar0 (over blue band) avg is blue");
            check (plan.segColour[1].getGreen() > 100
                  && plan.segColour[1].getGreen() > plan.segColour[1].getRed()
                  && plan.segColour[1].getGreen() > plan.segColour[1].getBlue(),
               "perBar bar1 (straddles blue|green) avg leans green (channel-wise)");
            check (plan.segColour[2].getRed() > 150, "perBar bar2 (over red band) avg is red");
        }
    }

    // ============ 用例 8b：GUI 下拉曾存驼峰 "perBar" 而核心比较小写 → 回归锁死 ============
    {
        const int CW = 200, CH = 60;
        auto base = makeBars (CW, CH, { {10, 5, 30, 50}, {90, 5, 30, 50}, {150, 5, 30, 50} });
        juce::Image img (juce::Image::ARGB, CW, CH, true);
        { juce::Graphics g (img);
          g.fillAll (juce::Colours::blue);
          g.setColour (juce::Colours::green); g.fillRect (66, 0, 68, CH);
          g.setColour (juce::Colours::red);   g.fillRect (134, 0, 66, CH); }
        MaskImageLayer cfg; cfg.enabled = true;
        cfg.strokeEnabled = true; cfg.strokeWidth = 3.0f;
        cfg.outlineMode = "perBar";                       // 故意用驼峰（GUI 历史写法）
        MaskImageLayer cfgNoS = cfg; cfgNoS.strokeEnabled = false;
        auto out = SpectrumMask::compose (base, img, cfgNoS, juce::Colours::white);
        auto plan = SpectrumMask::makeStrokePlan (out, cfg, juce::Colours::white);
        check (plan.perColumn, "camelCase \"perBar\" still yields per-column palette (case canonicalized)");
        check (plan.segColour.size() == 3
               && plan.segColour[0].getBlue() > 150 && plan.segColour[2].getRed() > 150,
               "camelCase perBar gives 3 distinct bar colours (regression of the GUI-all-uniform bug)");
    }

    // ============ 用例 9：uniform（#5d）与 image（#5f）模式 ============
    {
        const int CW = 200, CH = 60;
        auto base = makeBars (CW, CH, { {10, 5, 30, 50}, {90, 5, 30, 50} });
        auto img  = solidImg (CW, CH, juce::Colours::red);
        MaskImageLayer cfg; cfg.enabled = true;
        cfg.strokeEnabled = true; cfg.strokeWidth = 3.0f;
        cfg.outlineMode = "uniform";
        MaskImageLayer cfgNoS = cfg; cfgNoS.strokeEnabled = false;
        auto out = SpectrumMask::compose (base, img, cfgNoS, juce::Colours::blue);
        auto plan = SpectrumMask::makeStrokePlan (out, cfg, juce::Colours::blue);
        check (! plan.perColumn, "uniform: one colour for all bars");
        check (plan.uniform.getRed() > 150 && plan.uniform.getBlue() < 60,
               "uniform: = all-bars visible-region average (red)");
        cfg.outlineMode = "image";
        auto planImg = SpectrumMask::makeStrokePlan (out, cfg, juce::Colours::blue);
        check (planImg.uniform == juce::Colours::blue,
               "image mode keeps legacy resolvedStroke semantics (#5f)");
    }

    // ============ 用例 10：预览节流 + 指数插值（#5e）============
    {
        const int CW = 100, CH = 40;
        auto base = makeBars (CW, CH, { {5, 5, 40, 30} });
        auto imgA = solidImg (CW, CH, juce::Colours::red);
        auto imgB = solidImg (CW, CH, juce::Colours::blue);
        MaskImageLayer cfg; cfg.enabled = true;
        cfg.strokeEnabled = true; cfg.strokeWidth = 3.0f;
        cfg.outlineMode = "uniform"; cfg.outlinePreviewFps = 4.0f; cfg.outlineTemporal = true;
        MaskImageLayer cfgNoS = cfg; cfgNoS.strokeEnabled = false;
        auto outA = SpectrumMask::compose (base, imgA, cfgNoS, juce::Colours::white);
        auto outB = SpectrumMask::compose (base, imgB, cfgNoS, juce::Colours::white);
        const auto freshA = SpectrumMask::makeStrokePlan (outA, cfg, juce::Colours::white);
        const auto freshB = SpectrumMask::makeStrokePlan (outB, cfg, juce::Colours::white);
        check (freshA.uniform.getRed() > 150 && freshB.uniform.getBlue() > 150,
               "makeStrokePlan reads clipped pre-stroke out (no white contamination)");

        SpectrumMask::PreviewPaletteCache cache;
        const auto& p0 = cache.update (0.0,  freshA, cfg);
        check (p0.uniform.getRed() > 150, "cache primes immediately at t=0 (no garbage fade)");
        const auto& p1 = cache.update (0.1,  freshB, cfg);
        check (p1.uniform.getRed() > 150,
               "throttle: within 1/fps window stale value is KEPT (no recompute)");
        const auto& p2 = cache.update (0.30, freshB, cfg);
        check (p2.uniform.getBlue() > p2.uniform.getRed(),
               "after window elapses: lerp heads toward blue");
        for (double s = 0.4; s < 3.0; s += 0.05) cache.update (s, freshB, cfg);
        const auto& pEnd = cache.update (3.1, freshB, cfg);
        check (pEnd.uniform.getBlue() > 240 && pEnd.uniform.getRed() < 8,
               "temporal smoothing converges within ~3s");
        cfg.outlineTemporal = false;
        SpectrumMask::PreviewPaletteCache c2;
        c2.update (0.0, freshA, cfg);
        const auto& q = c2.update (0.5, freshB, cfg);
        check (q.uniform.getBlue() > 240, "outlineTemporal=false: snaps straight to target");
    }

    // ============ 用例 11：法向等宽描边（顶边厚度垂直于切线测量）============
    {
        const int CW = 140, CH = 120;
        const float rT = 8.0f;
        auto base = juce::Image (juce::Image::ARGB, CW, CH, true);
        { juce::Graphics g (base);
          g.setColour (juce::Colours::black);
          juce::Path poly;                              // 平段 x[10..60] 在 y=30，45° 斜坡到 x[60..120] y=90
          poly.startNewSubPath (10, 30); poly.lineTo (60, 30); poly.lineTo (120, 90);
          poly.lineTo (120, CH - 1);    poly.lineTo (10, CH - 1); poly.closeSubPath();
          g.fillPath (poly); }
        auto img  = solidImg (CW, CH, juce::Colours::black);   // 形状内部=黑，描边用白，好区分
        MaskImageLayer cfg; cfg.enabled = true;
        cfg.strokeEnabled = true; cfg.outlineMode = "image";
        cfg.outTop = true;  cfg.outWTop = rT;
        cfg.outLeft = false; cfg.outRight = false;             // 只测顶边
        auto im = SpectrumMask::compose (base, img, cfg, juce::Colours::white, /*sides=*/false);
        // 量某列顶边白色带的**竖直**连续长度（从表面往下）
        auto vrun = [&] (int x) {
            juce::Image::BitmapData b (im, juce::Image::BitmapData::readOnly);
            int best = 0, cur = 0;
            for (int y = 0; y < CH; ++y)
            {
                const auto p = reinterpret_cast<const juce::PixelARGB*> (b.getLinePointer (y))[x];
                const bool white = p.getRed() > 30 && p.getGreen() > 30 && p.getBlue() > 30 && p.getAlpha() > 30;
                cur = white ? cur + 1 : 0;
                best = juce::jmax (best, cur);
            }
            return best;
        };
        const int flat = vrun (30);     // 平段：竖直带 ≈ rT
        const int ramp = vrun (90);     // 45° 斜面：竖直带 ≈ rT*sqrt2（法向等宽的体现）
        std::printf ("      [normal-width] rT=%.0f  flatV=%d  ramp45V=%d (expect flat~8, ramp~11)\n", rT, flat, ramp);
        check (flat >= (int) rT - 1 && flat <= (int) rT + 3, "flat top: vertical band ~ rT (centered stroke)");
        check (ramp > flat + 1, "45deg top: vertical band WIDER than rT -> perpendicular width held at rT (centered normal)");
        check (ramp >= (int)(rT * 1.25f) && ramp <= (int)(rT * 1.8f), "45deg band ~ rT*sqrt2 (within tol)");
    }

    // ============ 诊断用例 12：把描边渲染成 PNG 供肉眼检查锯齿（不判定，仅落盘）============
    {
        auto dump = [] (const juce::Image& im, const char* path)
        {
            juce::MemoryOutputStream mem;
            juce::PNGImageFormat png;
            if (! png.writeImageToStream (im, mem)) { std::printf ("      [dump-encfail] %s\n", path); return; }
            std::ofstream out (path, std::ios::binary);
            if (! out) { std::printf ("      [dump-openfail] %s\n", path); return; }
            out.write ((const char*) mem.getData(), (std::streamsize) mem.getDataSize());
            std::printf ("      [dump] %s (%lld bytes)\n", path, (long long) mem.getDataSize());
        };
        const int CW = 300, CH = 160; const int rT = 6;
        // 12a 锯齿台阶顶面（模拟频谱逐 bin 台阶）
        {
            auto base = juce::Image (juce::Image::ARGB, CW, CH, true);
            { juce::Graphics g (base); g.setColour (juce::Colours::black);
              for (int x = 0; x < CW; ++x) { int yt = 60 + (x / 10 % 2) * 10 + (x / 47 % 3) * 4; g.fillRect (x, yt, 1, CH - yt); } }
            auto img = solidImg (CW, CH, juce::Colours::black);
            MaskImageLayer cfg; cfg.enabled = true; cfg.strokeEnabled = true; cfg.outlineMode = "image";
            cfg.outTop = true; cfg.outWTop = (float) rT; cfg.outAlphaTop = 1.0f;
            cfg.outLeft = false; cfg.outRight = false;
            auto out = SpectrumMask::compose (base, img, cfg, juce::Colours::red, false);
            dump (out, "/tmp/opencode/beta/stroke_stair.png");
        }
        // 12b 正弦曲线顶面（平滑，检验等宽）
        {
            auto base = juce::Image (juce::Image::ARGB, CW, CH, true);
            { juce::Graphics g (base); g.setColour (juce::Colours::black);
              for (int x = 0; x < CW; ++x) { int yt = (int) (70 + 28.0 * std::sin (x * 0.05)); g.fillRect (x, yt, 1, CH - yt); } }
            auto img = solidImg (CW, CH, juce::Colours::black);
            MaskImageLayer cfg; cfg.enabled = true; cfg.strokeEnabled = true; cfg.outlineMode = "image";
            cfg.outTop = true; cfg.outWTop = (float) rT; cfg.outAlphaTop = 1.0f;
            cfg.outLeft = false; cfg.outRight = false;
            auto out = SpectrumMask::compose (base, img, cfg, juce::Colours::red, false);
            dump (out, "/tmp/opencode/beta/stroke_sine.png");

            // 简版度量（case 11 已严格证明 45°=√2·rT 的法向等宽；这里只作曲线整体健康度粗筛）：
            //   ① 沿正弦顶缘"描边像素"总数 ≈ 每列都有描边覆盖；② 存在明显半透明过渡像素=JUCE AA 生效。
            auto strokeCols = [] (const juce::Image& im, int W2, int H2)
            {
                juce::Image::BitmapData b (im, juce::Image::BitmapData::readOnly);
                int colsWithStroke = 0, aaPixels = 0, strokePixels = 0;
                for (int x = 0; x < W2; ++x)
                {
                    bool any = false;
                    for (int y = 0; y < H2; ++y)
                    {
                        const auto px = reinterpret_cast<const juce::PixelARGB*>(b.getLinePointer (y))[x];
                        const bool red = px.getRed() > 60 && px.getGreen() < 90 && px.getBlue() < 90 && px.getAlpha() > 8;
                        if (! red) continue;
                        any = true; ++strokePixels;
                        if (px.getAlpha() < 250 && px.getRed() < 240) ++aaPixels;
                    }
                    if (any) ++colsWithStroke;
                }
                return std::make_tuple (colsWithStroke, strokePixels, aaPixels);
            };
            const auto [colsN, pxN, aaN] = strokeCols (out, CW, CH);
            std::printf ("      [sine coverage] colsWithStroke=%d/%d strokePx=%d aaPx=%d\n", colsN, CW, pxN, aaN);
            check (colsN >= CW * 3 / 4, "sine: contour stroke covers nearly every column (no gaps / no bridge)");
            check (aaN > 20, "sine: JUCE strokePath produced anti-aliased edge pixels (not hard jaggies)");
        }
    }

    // ============ 用例 13：边框必须锚定柱体、忽略浮动的 peak cap（回归"盖到 peak caps 上"）============
    {
        const int CW = 60, CH = 140, rT = 6;
        // 造一列结构：cap 细条在 y=20..21；gap y=22..79；柱体 y=80..139。多列同构。
        auto base = juce::Image (juce::Image::ARGB, CW, CH, true);
        { juce::Graphics g (base); g.setColour (juce::Colours::black);
          g.fillRect (0, 80, CW, CH - 80);        // 柱体（顶在 y=80）
          g.fillRect (0, 20, CW, 2);             // peak cap 细条（顶在 y=20，与柱体隔 gap）
        }
        auto img = solidImg (CW, CH, juce::Colours::black);
        MaskImageLayer cfg; cfg.enabled = true; cfg.strokeEnabled = true; cfg.outlineMode = "image";
        cfg.outTop = true; cfg.outWTop = (float) rT; cfg.outAlphaTop = 1.0f;
        cfg.outLeft = false; cfg.outRight = false;
        auto out = SpectrumMask::compose (base, img, cfg, juce::Colours::red, false);
        juce::Image::BitmapData ob (out, juce::Image::BitmapData::readOnly);
        auto isStroke = [&] (int x, int y) {
            const auto px = reinterpret_cast<const juce::PixelARGB*>(ob.getLinePointer (y))[x];
            return px.getRed() > 120 && px.getGreen() < 90 && px.getBlue() < 90 && px.getAlpha() > 40;
        };
        int capRegion = 0, aboveCap = 0, bodyTopRegion = 0;
        for (int x = 0; x < CW; ++x)
        {
            for (int y = 0; y <= 21; ++y) if (isStroke (x, y)) ++aboveCap;        // cap 顶部及其以上不该有描边
            for (int y = 22; y < 80 - rT; ++y) if (isStroke (x, y)) ++capRegion;  // gap 中段也不该有
            if (isStroke (x, 80) || isStroke (x, 80 + 1)) ++bodyTopRegion;         // 柱体顶 y≈80 应有描边
        }
        std::printf ("      [cap-anchor] aboveCapStroke=%d gapMidStroke=%d bodyTopStroke=%d/%d\n",
                     aboveCap, capRegion, bodyTopRegion, CW);
        check (aboveCap == 0 && capRegion == 0, "border ignores the floating peak cap (no stroke at/above cap or in the gap)");
        check (bodyTopRegion >= CW * 3 / 4, "border anchors to the bar BODY top, not the peak cap");
    }

    // ============ 用例 14：overlayCapsFrom —— 描边只认柱体，peak cap 原样叠回（bar/bar-line 回归）============
    {
        const int CW = 60, CH = 140, rT = 6;
        auto body = juce::Image (juce::Image::ARGB, CW, CH, true);   // 柱体 only
        { juce::Graphics g (body); g.setColour (juce::Colours::black); g.fillRect (10, 90, CW-20, CH-90); }
        auto full = juce::Image (juce::Image::ARGB, CW, CH, true);   // 柱体 + 更宽、悬浮的绿色 peak cap
        { juce::Graphics g (full);
          g.setColour (juce::Colours::black); g.fillRect (10, 90, CW-20, CH-90);
          g.setColour (juce::Colours::green); g.fillRect (4, 20, CW-8, 2); }   // cap 比柱宽，外伸到 x=4..55
        auto img = solidImg (CW, CH, juce::Colours::white);
        MaskImageLayer cfg; cfg.enabled = true; cfg.strokeEnabled = true; cfg.outlineMode = "image";
        cfg.outTop = true;  cfg.outWTop = (float) rT; cfg.outAlphaTop = 1.0f;
        cfg.outLeft = true; cfg.outWLeft = (float) rT; cfg.outAlphaLeft = 1.0f;
        cfg.outRight = true; cfg.outWRight = (float) rT; cfg.outAlphaRight = 1.0f;
        auto masked = SpectrumMask::compose (body, img, cfg, juce::Colours::red, true);
        SpectrumMask::overlayCapsFrom (masked, full, body);
        juce::Image::BitmapData ob (masked, juce::Image::BitmapData::readOnly);
        auto at = [&] (int x, int y) { return reinterpret_cast<const juce::PixelARGB*>(ob.getLinePointer (y))[x]; };
        int redAboveBody = 0, redAtBodyTop = 0, greenCap = 0;
        for (int x = 0; x < CW; ++x)
            for (int y = 0; y < 85; ++y) { auto p = at (x, y); if (p.getRed()>120 && p.getGreen()<90) ++redAboveBody; }
        for (int x = 10; x < CW-10; ++x) { auto p = at (x, 90); if (p.getRed()>120 && p.getGreen()<90) ++redAtBodyTop; }
        for (int x = 4; x < CW-4; ++x) { auto p = at (x, 20); if (p.getGreen()>120 && p.getRed()<90) ++greenCap; }
        std::printf ("      [caps-decouple] redAboveBody=%d redAtBodyTop=%d/%d greenCap=%d/%d\n",
                     redAboveBody, redAtBodyTop, CW-20, greenCap, CW-8);
        check (redAboveBody == 0, "no border near/above the peak cap or its overhang (bar sides too)");
        check (redAtBodyTop >= (CW-20) * 3 / 4, "border sits on the bar top");
        check (greenCap >= (CW-8) * 3 / 4, "peak cap re-composited on top in its own colour");
    }

    // ============ 用例 15：config.json 默认值文件 save→load 往返（round-trip）============
    {
        SpectrumParams src;
        src.width = 813; src.height = 457;
        src.style = "bar-line";
        src.baselineY = 0.33f;
        src.maskImage.outWTop = 7.5f;
        src.maskImage.outlineMode = "perbar";
        juce::String serr;
        const bool saved = src.saveToDefaultConfig (serr);
        if (saved)
        {
            SpectrumParams fresh;                 // 内置默认起步
            juce::String lerr;
            const bool loaded = fresh.loadFromDefaultConfig (lerr);
            check (saved && loaded, "config.json: save + loadFromDefaultConfig both succeed");
            check (fresh.width == 813 && fresh.height == 457, "config.json: int fields round-trip");
            check (fresh.style == "bar-line", "config.json: style string round-trip");
            check (std::abs (fresh.baselineY - 0.33f) < 1e-4f, "config.json: float round-trip");
            check (std::abs (fresh.maskImage.outWTop - 7.5f) < 1e-4f && fresh.maskImage.outlineMode == "perbar",
                   "config.json: nested mask fields round-trip");
            SpectrumParams::defaultConfigFile().deleteFile();   // 清理，别污染 build/
        }
        else
        {
            std::printf ("      [cfg] save failed on this FS (%s) — skipping load checks\n", serr.toRawUTF8());
        }
        // 缺失文件 → load 应静默返回 false 且不改动默认
        SpectrumParams untouched;
        juce::String e2;
        const bool ap = untouched.loadFromDefaultConfig (e2);
        check (! ap, "config.json: missing file -> load returns false, built-in defaults kept");
    }

    // ============ 用例 16：bar 家族向内带——顶/左/右闭合、无圆帽外伸、不越界 ============
    {
        const int CW = 60, CH = 120, rW = 4;
        auto base = juce::Image (juce::Image::ARGB, CW, CH, true);
        { juce::Graphics g (base); g.setColour (juce::Colours::black); g.fillRect (20, 30, 21, 60); } // bar x[20..40] y[30..89]
        auto img = solidImg (CW, CH, juce::Colours::white);
        MaskImageLayer cfg; cfg.enabled = true; cfg.strokeEnabled = true; cfg.outlineMode = "image";
        cfg.outTop = cfg.outLeft = cfg.outRight = true;
        cfg.outWTop = cfg.outWLeft = cfg.outWRight = (float) rW;
        cfg.outAlphaTop = cfg.outAlphaLeft = cfg.outAlphaRight = 1.0f;
        auto out = SpectrumMask::compose (base, img, cfg, juce::Colours::red, /*sides=*/true); // barFamily
        juce::Image::BitmapData ob (out, juce::Image::BitmapData::readOnly);
        auto redAt = [&] (int x, int y) { auto p = reinterpret_cast<const juce::PixelARGB*>(ob.getLinePointer(y))[x];
                                          return p.getRed()>120 && p.getGreen()<90 && p.getBlue()<90 && p.getAlpha()>120; };
        int aboveBar = 0, leftOverhang = 0, rightOverhang = 0;
        for (int x = 0; x < CW; ++x) for (int y = 0; y < 30; ++y) if (redAt (x, y)) ++aboveBar;         // bar 顶以上不该有任何描边（向内）
        for (int y = 0; y < CH; ++y) { if (redAt (16, y)) ++leftOverhang; if (redAt (44, y)) ++rightOverhang; } // 外侧不该有（无外伸）
        const bool topOK = redAt (30, 31), leftOK = redAt (21, 60), cornerOK = redAt (21, 31), interiorNotRed = ! redAt (30, 60);
        std::printf ("      [bar-closure] aboveBar=%d leftOver=%d rightOver=%d top=%d left=%d corner=%d interiorRed=%d\n",
                     aboveBar, leftOverhang, rightOverhang, (int)topOK, (int)leftOK, (int)cornerOK, (int)(!interiorNotRed));
        check (aboveBar == 0 && leftOverhang == 0 && rightOverhang == 0, "bar border is INWARD: nothing above the top edge, no sideways overhang past bar sides");
        check (topOK && leftOK && cornerOK, "bar top+left meet (closed corner, no gap)");
        check (interiorNotRed, "bar interior shows the mask image (not covered by border)");
    }

    // ============ 用例 17：peak cap 两模式（蒙版穿透 vs 边框样式）============
    {
        const int CW = 60, CH = 120, rW = 4;
        auto bodyOnly = juce::Image (juce::Image::ARGB, CW, CH, true);   // 柱体
        { juce::Graphics g (bodyOnly); g.setColour (juce::Colours::black); g.fillRect (15, 70, 31, 30); }
        auto withCap  = juce::Image (juce::Image::ARGB, CW, CH, true);   // 柱体 + 更宽的绿色帽（浮在上方）
        { juce::Graphics g (withCap);
          g.setColour (juce::Colours::black); g.fillRect (15, 70, 31, 30);
          g.setColour (juce::Colours::green); g.fillRect (10, 30, 41, 2); }
        auto img = solidImg (CW, CH, juce::Colours::white);
        MaskImageLayer cfg; cfg.enabled = true; cfg.strokeEnabled = true; cfg.outlineMode = "image";
        cfg.outTop = cfg.outLeft = cfg.outRight = true;
        cfg.outWTop = cfg.outWLeft = cfg.outWRight = (float) rW;
        cfg.outAlphaTop = cfg.outAlphaLeft = cfg.outAlphaRight = 1.0f;
        auto classify = [] (const juce::Image& im, int x, int y, bool& red, bool& white, bool& green)
        {
            juce::Image::BitmapData b (im, juce::Image::BitmapData::readOnly);
            auto p = reinterpret_cast<const juce::PixelARGB*>(b.getLinePointer (y))[x];
            const int R = p.getRed(), G = p.getGreen(), B = p.getBlue();
            red   = (R>120 && G<90 && B<90);
            white = (R>160 && G>160 && B>160);
            green = (G>120 && R<90 && B<90);
        };
        // 17a 穿透模式：base=含帽(clip含帽→帽透出白图)，strokeBase=无帽(帽不描边)
        {
            auto out = SpectrumMask::compose (withCap, img, cfg, juce::Colours::red, true, &bodyOnly, nullptr);
            bool red, white, green; classify (out, 25, 31, red, white, green);   // 帽处
            bool bRed, bWhite, bGreen; classify (out, 30, 71, bRed, bWhite, bGreen); // 柱顶应有红描边
            check (white && ! red && ! green, "passthrough: peak-cap region shows the mask IMAGE (not bordered, not peak colour)");
            check (bRed, "passthrough: bar body still bordered");
        }
        // 17b 边框样式：base=无帽，strokeBase=无帽，capOverlay=含帽 → 帽重涂成边框红
        {
            auto out = SpectrumMask::compose (bodyOnly, img, cfg, juce::Colours::red, true, &bodyOnly, &withCap);
            bool red, white, green; classify (out, 25, 31, red, white, green);
            check (red && ! green, "border-style: peak cap recolored to the bar's border colour (not peak green)");
        }
    }

    std::printf (failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
