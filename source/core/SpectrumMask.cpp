// =============================================================================
// SpectrumMask.cpp — 频谱蒙版图片实现（像素级，预乘 alpha 安全）
// =============================================================================
#include "SpectrumMask.h"
#include <map>
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

    // JPEG/BMP 等可能被 load 成 RGB（3 字节/像素）：直接按 PixelARGB（4 字节）读会越界崩溃。
    // 先统一转成 ARGB 再逐像素访问（调用方均有缓存，只转一次）。
    // 大图（>16MP）先缩小：均色为统计量，缩略图与全图等价，且避免大额分配（Windows 崩溃防御）。
    juce::Image srcFull = (img.getFormat() == juce::Image::ARGB)
                                ? img
                                : img.convertedToFormat (juce::Image::ARGB);
    const int64_t px = (int64_t) srcFull.getWidth() * (int64_t) srcFull.getHeight();
    juce::Image src = srcFull;
    if (px > 16 * 1024 * 1024)
    {
        const int w = srcFull.getWidth(), h = srcFull.getHeight();
        const double k = std::sqrt (16.0 * 1024.0 * 1024.0 / (double) px);
        src = srcFull.rescaled (juce::jmax (1, (int) (w * k)),
                                  juce::jmax (1, (int) (h * k)),
                                  juce::Graphics::mediumResamplingQuality);
    }

    const int w = src.getWidth(), h = src.getHeight();
    juce::Image::BitmapData bd (src, juce::Image::BitmapData::readOnly);

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

juce::Image SpectrumMask::adjustedImage (const juce::Image& img,
                                         float brightness, float contrast, float saturation)
{
    if (! img.isValid())
        return {};
    brightness = juce::jlimit (0.0f, 2.0f, brightness);
    contrast   = juce::jlimit (0.0f, 2.0f, contrast);
    saturation = juce::jlimit (0.0f, 2.0f, saturation);
    const bool identity = std::abs (brightness - 1.0f) < 1e-4f
                       && std::abs (contrast   - 1.0f) < 1e-4f
                       && std::abs (saturation - 1.0f) < 1e-4f;
    if (identity)
        return img;   // juce::Image 是 COW 句柄，浅拷贝零开销

    const juce::Image src = (img.getFormat() == juce::Image::ARGB)
                                ? img : img.convertedToFormat (juce::Image::ARGB);
    const int w = src.getWidth(), h = src.getHeight();
    juce::Image out (juce::Image::ARGB, w, h, false);
    juce::Image::BitmapData si (src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData di (out, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
    {
        const auto* row = reinterpret_cast<const juce::PixelARGB*> (si.getLinePointer (y));
        auto* dst       = reinterpret_cast<juce::PixelARGB*> (di.getLinePointer (y));
        for (int x = 0; x < w; ++x)
        {
            const int a = row[x].getAlpha();
            if (a == 0) { dst[x] = juce::PixelARGB (0, 0, 0, 0); continue; }

            // 预乘 → 直通（straight）后在 sRGB 通道上做调整
            float r = (float) row[x].getRed()   * 255.0f / (float) a;
            float g = (float) row[x].getGreen() * 255.0f / (float) a;
            float b = (float) row[x].getBlue()  * 255.0f / (float) a;

            r *= brightness; g *= brightness; b *= brightness;                       // 亮度
            r = (r - 128.0f) * contrast + 128.0f;                                     // 对比度（中灰轴）
            g = (g - 128.0f) * contrast + 128.0f;
            b = (b - 128.0f) * contrast + 128.0f;
            const float luma = 0.299f * r + 0.587f * g + 0.114f * b;                  // 饱和度
            r = luma + (r - luma) * saturation;
            g = luma + (g - luma) * saturation;
            b = luma + (b - luma) * saturation;

            auto cl = [] (float v) { return (int) juce::jlimit (0.0f, 255.0f, v + 0.5f); };
            // 再预乘写回
            dst[x] = juce::PixelARGB (a, cl (r) * a / 255, cl (g) * a / 255, cl (b) * a / 255);
        }
    }
    return out;
}

juce::Image SpectrumMask::adjustedImageCached (const juce::Image& img,
                                               const juce::String& pathKey,
                                               float brightness, float contrast, float saturation)
{
    const juce::String key = juce::String::formatted ("%s|%.4f|%.4f|%.4f",
                                                      pathKey.toRawUTF8(), brightness, contrast, saturation);
    // 有界缓存（8 条）：GUI 绘制线程与导出线程共用；查表加锁，慢计算放锁外（幂等）。
    // 多图层逐帧轮流访问，LRU-1 会持续打爆，故用小 map。
    struct Store
    {
        juce::CriticalSection lock;
        std::map<juce::String, juce::Image> m;
        void put (const juce::String& k, const juce::Image& v)
        {
            const juce::ScopedLock sl (lock);
            m[k] = v;
            while (m.size() > 8)
                m.erase (m.begin());     // map 有序，删最旧（近似 LRU：键含参数，重算代价可接受）
        }
        juce::Image get (const juce::String& k)
        {
            const juce::ScopedLock sl (lock);
            auto it = m.find (k);
            return (it != m.end()) ? it->second : juce::Image();
        }
    };
    static Store store;

    if (juce::Image hit = store.get (key); hit.isValid())
        return hit;

    juce::Image out = adjustedImage (img, brightness, contrast, saturation);
    if (out.isValid())
        store.put (key, out);
    return out;
}

juce::Image SpectrumMask::adjustedImageCached (const juce::Image& img, const MaskImageLayer& cfg)
{
    return adjustedImageCached (img, cfg.path, cfg.brightness, cfg.contrast, cfg.saturation);
}

juce::Image SpectrumMask::compose (const juce::Image& base,
                                   const juce::Image& image,
                                   const MaskImageLayer& cfg,
                                   juce::Colour resolvedStroke)
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

    // ---- 2. 画图片进 out：几何与电平无关 ----
    //   set=false → 与其他图片图层一致：等比 contain 适配输出画布并居中（v0.5.4 #3 修正，
    //               替代旧"拉伸铺满频谱画框"）；
    //   set=true  → 按 buildVisAffine 映射图片本地矩形（可独立缩放/拉伸/旋转/平移）
    juce::Image out (juce::Image::ARGB, W, H, true);   // true = 清空（全透明）
    if (! out.isValid())
        return {};   // 内存不足：JUCE new 返回 NULL（Win），不检查会在稍后写空指针
    {
        juce::Graphics go (out);
        go.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const VisTransform tf = cfg.transform.set
                                    ? cfg.transform
                                    : makeContainTransform ((float) image.getWidth(),
                                                            (float) image.getHeight(),
                                                            (float) W, (float) H);
        go.saveState();
        go.addTransform (buildVisAffine (tf));
        go.drawImageAt (image, 0, 0);
        go.restoreState();
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
        // v0.5.4 #1 修复：scratch 复用，不再每帧分配 2×W×H×int（4K≈70MB/帧、8K≈268MB/帧 →
        //   Windows JUCE new 失败返回 NULL → vector::data()=NULL → 描边循环读 NULL 崩溃）。
        //   thread_local：GUI 消息线程与导出线程各自持有，无数据竞争。
        static thread_local std::vector<int> tmp, er;
        if (tmp.size() < (size_t) W * H)
        {
            tmp.resize ((size_t) W * H);
            er.resize ((size_t) W * H);
        }
        if (tmp.data() == nullptr || er.data() == nullptr)
            return {};

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
                // v0.5.4 #1b 修复：e/m 已是本行行指针，旧代码 e[idx]（idx=y*W+x）=
                //   tmp[2*y*W+x] 二次偏移 → y≥H/2 起越界读堆 → ACCESS_VIOLATION
                //   （Windows 崩溃 dump 0911_0018/0021 实证：compose+0x8f5 "sub esi,[r13+rax]"）。
                //   正确值 = 本行腐蚀结果 e[x]；此前描边环用错行数据，视觉也随之修正。
                int rim = (int) m[x] - e[x];             // 内侧环强度
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
