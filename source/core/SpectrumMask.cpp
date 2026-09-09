// =============================================================================
// SpectrumMask.cpp — 频谱蒙版图片实现（像素级，预乘 alpha 安全）
// =============================================================================
#include "SpectrumMask.h"
#include <vector>
#include <cmath>
#include <cstdint>

using uint8 = std::uint8_t;

namespace
{
    // 轮廓二值化阈值：alpha ≥ 此值视作"实心"（把 bar 内部 0.4~0.85 的渐变 alpha
    // 抬到完全不透明），低于此值按线性过渡保留边缘抗锯齿。
    constexpr uint8 kSolid = 40;

    inline uint8 scaled (uint8 v, int m) noexcept   // m: 0..255 乘子（预乘缩放）
    {
        return static_cast<uint8> ((v * m + 127) / 255);
    }
}

juce::Colour SpectrumMask::averageColour (const juce::Image& img)
{
    if (! img.isValid())
        return juce::Colour (0xff808080);

    const int w = img.getWidth(), h = img.getHeight();
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);

    // 每隔若干像素采样，控制在 ~64k 样本内（大图提速，均色视觉无差）
    const int stepX = juce::jmax (1, w / 256);
    const int stepY = juce::jmax (1, h / 256);

    uint64_t sr = 0, sg = 0, sb = 0, sa = 0;
    for (int y = 0; y < h; y += stepY)
    {
        const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
        for (int x = 0; x < w; x += stepX)
        {
            const juce::PixelARGB p = line[x];
            const int a = p.getAlpha();
            if (a == 0) continue;                 // 透明像素不计权（避免拉暗）
            // 非预乘回推：stored 是预乘 → straight = stored*255/a
            const int ra = juce::jmin (255, p.getRed()   * 255 / a);
            const int ga = juce::jmin (255, p.getGreen() * 255 / a);
            const int ba = juce::jmin (255, p.getBlue()  * 255 / a);
            sr += (uint64_t) ra * a;
            sg += (uint64_t) ga * a;
            sb += (uint64_t) ba * a;
            sa += (uint64_t) a;
        }
    }
    if (sa == 0)
        return juce::Colour (0xff808080);

    const uint8 r = static_cast<uint8> (sr / sa);
    const uint8 g = static_cast<uint8> (sg / sa);
    const uint8 b = static_cast<uint8> (sb / sa);
    return juce::Colour::fromRGB (r, g, b);
}

juce::Image SpectrumMask::compose (const juce::Image& base,
                                   const juce::Image& image,
                                   const MaskImageLayer& cfg,
                                   juce::Colour resolvedStroke,
                                   const juce::Rectangle<float>& frameRect)
{
    if (! base.isValid() || ! image.isValid())
        return {};

    const int W = base.getWidth();
    const int H = base.getHeight();
    if (W <= 0 || H <= 0)
        return {};

    // ---- 1. base 轮廓 alpha 数组 + bbox ----
    std::vector<uint8> mA ((size_t) W * H, 0);
    int minX = W, minY = H, maxX = -1, maxY = -1;
    {
        juce::Image::BitmapData bd (base, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < H; ++y)
        {
            const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            uint8* dst = mA.data() + (size_t) y * W;
            for (int x = 0; x < W; ++x)
            {
                const uint8 a = line[x].getAlpha();
                if (a != 0)
                {
                    // 实心抬到 255，边缘按线性保留抗锯齿
                    dst[x] = (a >= kSolid) ? 255 : static_cast<uint8> (a * 255 / kSolid);
                    if (a >= 8)
                    {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            }
        }
    }
    if (maxX < minX || maxY < minY)
        return {};   // 无有效轮廓

    // ---- 2. 画图片进 out：几何与电平无关（用固定 frameRect 或用户 transform）----
    //   set=false → 铺满 frameRect（fill）；set=true → 按 buildVisAffine 映射图片本地矩形
    juce::Image out (juce::Image::ARGB, W, H, true);   // true = 清空（全透明）
    {
        juce::Graphics go (out);
        go.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (cfg.transform.set)
        {
            go.saveState();
            go.addTransform (buildVisAffine (cfg.transform));
            go.drawImageAt (image, 0, 0);
            go.restoreState();
        }
        else
        {
            juce::Rectangle<float> fr = frameRect;
            if (fr.getWidth() <= 0.0f || fr.getHeight() <= 0.0f)
                fr = juce::Rectangle<float> (0.0f, 0.0f, (float) W, (float) H);
            go.drawImage (image, fr);   // fill = 拉伸铺满画框
        }
    }

    // ---- 3. 用轮廓 alpha 裁剪 out（预乘：整体乘 m/255）----
    {
        juce::Image::BitmapData bd (out, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < H; ++y)
        {
            auto* line = reinterpret_cast<juce::PixelARGB*> (bd.getLinePointer (y));
            const uint8* m = mA.data() + (size_t) y * W;
            for (int x = 0; x < W; ++x)
            {
                const int mv = m[x];
                if (mv == 255) continue;               // 全实心不动
                juce::PixelARGB p = line[x];
                p.setARGB (scaled (p.getAlpha(), mv),
                           scaled (p.getRed(),   mv),
                           scaled (p.getGreen(), mv),
                           scaled (p.getBlue(),  mv));
                line[x] = p;
            }
        }
    }

    // ---- 4. 描边（轮廓内侧勾边：mA 减去腐蚀(r) 的环）----
    if (cfg.strokeEnabled && cfg.strokeWidth > 0.01f)
    {
        const int r = juce::jlimit (1, 32, (int) std::lround (cfg.strokeWidth));
        std::vector<int> tmp ((size_t) W * H);
        std::vector<int> er  ((size_t) W * H);

        // 水平 min（半径 r）
        for (int y = 0; y < H; ++y)
        {
            const uint8* row = mA.data() + (size_t) y * W;
            for (int x = 0; x < W; ++x)
            {
                if (row[x] == 0) { er[(size_t) y * W + x] = 0; continue; }
                int lo = x - r, hi = x + r;
                int mn = 255;
                for (int k = lo; k <= hi; ++k)
                {
                    const int kk = juce::jlimit (0, W - 1, k);
                    const int v = row[kk];
                    if (v < mn) mn = v;
                    if (mn == 0) break;
                }
                er[(size_t) y * W + x] = mn;
            }
        }
        // 垂直 min（半径 r）
        for (int x = 0; x < W; ++x)
        {
            for (int y = 0; y < H; ++y)
            {
                const int v0 = er[(size_t) y * W + x];
                if (v0 == 0) { tmp[(size_t) y * W + x] = 0; continue; }
                int mn = 255;
                for (int k = y - r; k <= y + r; ++k)
                {
                    const int kk = juce::jlimit (0, H - 1, k);
                    const int v = er[(size_t) kk * W + x];
                    if (v < mn) mn = v;
                    if (mn == 0) break;
                }
                tmp[(size_t) y * W + x] = mn;
            }
        }

        const uint8 sR = resolvedStroke.getRed();
        const uint8 sG = resolvedStroke.getGreen();
        const uint8 sB = resolvedStroke.getBlue();

        juce::Image::BitmapData bd (out, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < H; ++y)
        {
            auto* line = reinterpret_cast<juce::PixelARGB*> (bd.getLinePointer (y));
            const uint8* m = mA.data() + (size_t) y * W;
            const int* e = tmp.data() + (size_t) y * W;
            for (int x = 0; x < W; ++x)
            {
                const int idx = (size_t) y * W + x;
                int rim = (int) m[x] - e[idx];          // 内侧环强度
                if (rim <= 0) continue;
                if (rim > 255) rim = 255;
                const int inv = 255 - rim;
                // 描边色（不透明）预乘 src = color * rim/255
                const int srcR = sR * rim / 255;
                const int srcG = sG * rim / 255;
                const int srcB = sB * rim / 255;
                juce::PixelARGB d = line[x];
                // source-over：out = src + dst*(1-srcA)
                const int a = rim + d.getAlpha() * inv / 255;
                const int rr = srcR + d.getRed()   * inv / 255;
                const int gg = srcG + d.getGreen() * inv / 255;
                const int bb = srcB + d.getBlue()  * inv / 255;
                d.setARGB (static_cast<uint8> (a),
                           static_cast<uint8> (juce::jmin (255, rr)),
                           static_cast<uint8> (juce::jmin (255, gg)),
                           static_cast<uint8> (juce::jmin (255, bb)));
                line[x] = d;
            }
        }
    }

    return out;
}
