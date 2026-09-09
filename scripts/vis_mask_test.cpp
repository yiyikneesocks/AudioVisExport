// vis_mask_test — SpectrumMask::compose 回归：默认 contain 几何（#3）+ 不随电平漂移（BUG1）
// + 显式变换下渲染 bbox 与 visCorners（GUI 手柄）一致。构建：ninja vis_mask_test && ./build/vis_mask_test
#include <juce_graphics/juce_graphics.h>
#include "source/core/SpectrumMask.h"
#include "source/core/VisTransform.h"
#include <cstdio>

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

    std::printf (failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
