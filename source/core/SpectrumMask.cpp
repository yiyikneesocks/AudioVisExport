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
    return composeWithPlan (base, image, cfg, resolvedStroke, nullptr, 0.0);
}

juce::Image SpectrumMask::composeWithPlan (const juce::Image& base,
                                           const juce::Image& image,
                                           const MaskImageLayer& cfg,
                                           juce::Colour resolvedStroke,
                                           PreviewPaletteCache* cache,
                                           double nowSec)
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

    // ---- 4. 描边（v0.5.5 #5：四边独立开关/厚度 + 实时平均色计划）----
    if (cfg.strokeEnabled)
    {
        const int rT = cfg.outTop    ? juce::jlimit (1, 32, (int) std::lround (cfg.outWTop   )) : 0;
        const int rB = cfg.outBottom ? juce::jlimit (1, 32, (int) std::lround (cfg.outWBottom )) : 0;
        const int rL = cfg.outLeft   ? juce::jlimit (1, 32, (int) std::lround (cfg.outWLeft   )) : 0;
        const int rR = cfg.outRight  ? juce::jlimit (1, 32, (int) std::lround (cfg.outWRight  )) : 0;

        if (rT | rB | rL | rR)
        {
            // 颜色计划：在**描边之前**的 out 上现算（避免白描边污染平均）。
            //   GUI（带 cache）→ 节流 + 指数插值；导出 cache=nullptr → 每帧真算＝精确。
            const StrokePlan fresh = makeStrokePlan (out, cfg, resolvedStroke);
            const StrokePlan& plan = cache != nullptr
                                         ? cache->update (nowSec, fresh, cfg)
                                         : fresh;

            // 四条方向各做一次"窗内最小值"。单调队列滑窗 O(W·H)（与 r 无关；旧版 2 次
            //   全窗重扫是 O(W·H·r)，4 个方向若沿用旧写法要再 ×4）。rim = m − 方向窗最小值。
            static thread_local std::vector<uint8> bufT, bufB, bufL, bufR;
            const size_t n = (size_t) W * H;
            if (bufT.size() < n)
            {
                bufT.resize (n); bufB.resize (n); bufL.resize (n); bufR.resize (n);
            }
            if (bufT.data() == nullptr || bufB.data() == nullptr
                || bufL.data() == nullptr || bufR.data() == nullptr)
                return {};   // 极端低内存：JUCE/STL 给不出缓冲 → 放弃描边（图本体已画好，宁缺勿崩）

            if (rT != 0 || rB != 0)   // 纵向：按列滑窗
            {
                std::vector<int> dq ((size_t) H + 2);
                for (int x = 0; x < W; ++x)
                {
                    int hd = 0, tl = 0;                       // 顶缘窗口 [y-rT, y]
                    for (int y = 0; y < H; ++y)
                    {
                        const uint8 v = mA[(size_t) y * W + x];
                        while (tl > hd && mA[(size_t) dq[tl - 1] * W + x] >= v) --tl;
                        dq[tl++] = y;
                        while (dq[hd] < y - rT) ++hd;
                        bufT[(size_t) y * W + x] = mA[(size_t) dq[hd] * W + x];
                    }
                    hd = tl = 0;                              // 底缘窗口 [y, y+rB]
                    for (int y = H - 1; y >= 0; --y)
                    {
                        const uint8 v = mA[(size_t) y * W + x];
                        while (tl > hd && mA[(size_t) dq[tl - 1] * W + x] >= v) --tl;
                        dq[tl++] = y;
                        while (dq[hd] > y + rB) ++hd;
                        bufB[(size_t) y * W + x] = mA[(size_t) dq[hd] * W + x];
                    }
                }
            }
            if (rL != 0 || rR != 0)   // 横向：按行滑窗
            {
                std::vector<int> dq ((size_t) W + 2);
                for (int y = 0; y < H; ++y)
                {
                    const uint8* row = mA.data() + (size_t) y * W;
                    int hd = 0, tl = 0;                       // 左缘窗口 [x-rL, x]
                    for (int x = 0; x < W; ++x)
                    {
                        while (tl > hd && row[dq[tl - 1]] >= row[x]) --tl;
                        dq[tl++] = x;
                        while (dq[hd] < x - rL) ++hd;
                        bufL[(size_t) y * W + x] = row[dq[hd]];
                    }
                    hd = tl = 0;                              // 右缘窗口 [x, x+rR]
                    for (int x = W - 1; x >= 0; --x)
                    {
                        while (tl > hd && row[dq[tl - 1]] >= row[x]) --tl;
                        dq[tl++] = x;
                        while (dq[hd] > x + rR) ++hd;
                        bufR[(size_t) y * W + x] = row[dq[hd]];
                    }
                }
            }

            const bool perCol = plan.perColumn
                             && plan.colColour.size() >= (size_t) W;
            juce::Image::BitmapData bd (out, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < H; ++y)
            {
                auto* line = reinterpret_cast<juce::PixelARGB*> (bd.getLinePointer (y));
                const uint8* m = mA.data()   + (size_t) y * W;
                const uint8* t = bufT.data() + (size_t) y * W;
                const uint8* b = bufB.data() + (size_t) y * W;
                const uint8* l = bufL.data() + (size_t) y * W;
                const uint8* rr = bufR.data() + (size_t) y * W;
                for (int x = 0; x < W; ++x)
                {
                    const int mv = m[x];
                    if (mv == 0) continue;
                    // 启用的缘各算 rim，交汇处取 max（角部自然拼合）。
                    // ⚠️ 未启用的方向 buf 未写，必须用 rX!=0 守卫，绝不能让 mv−0 混进来
                    //    （那会把整个形状涂成描边）。
                    int rim = 0;
                    if (rT != 0) rim = juce::jmax (rim, mv - (int) t[x]);
                    if (rB != 0) rim = juce::jmax (rim, mv - (int) b[x]);
                    if (rL != 0) rim = juce::jmax (rim, mv - (int) l[x]);
                    if (rR != 0) rim = juce::jmax (rim, mv - (int) rr[x]);
                    if (rim <= 0) continue;
                    if (rim > 255) rim = 255;
                    const juce::Colour col = perCol ? plan.colColour[(size_t) x] : plan.uniform;
                    const int inv = 255 - rim;
                    const int srcR = col.getRed()   * rim / 255;
                    const int srcG = col.getGreen() * rim / 255;
                    const int srcB = col.getBlue()  * rim / 255;
                    juce::PixelARGB d = line[x];
                    const int a  = rim + d.getAlpha() * inv / 255;
                    const int cr = srcR + d.getRed()   * inv / 255;
                    const int cg = srcG + d.getGreen() * inv / 255;
                    const int cb = srcB + d.getBlue()  * inv / 255;
                    d.setARGB (static_cast<uint8> (a),
                               static_cast<uint8> (juce::jmin (255, cr)),
                               static_cast<uint8> (juce::jmin (255, cg)),
                               static_cast<uint8> (juce::jmin (255, cb)));
                    line[x] = d;
                }
            }
        }
    }

    return out;
}

// =============================================================================
// v0.5.5 #5：描边调色板（实时平均色）+ 预览节流/插值
// =============================================================================
namespace
{
    // 把逐段色铺成逐列数组（compose 描边按 x 查色 O(1)）
    void bakeColumns (SpectrumMask::StrokePlan& plan)
    {
        plan.colColour.clear();
        if (! plan.perColumn) return;
        plan.colColour.resize ((size_t) plan.colWidth, juce::Colours::white);
        for (size_t s = 0; s < plan.segEnd.size(); ++s)
            for (int x = plan.segStart[s]; x <= plan.segEnd[s]; ++x)
                if (x >= 0 && x < plan.colWidth)
                    plan.colColour[(size_t) x] = plan.segColour[s];
    }

    inline uint8 lerp8 (uint8 a, uint8 b, float k) noexcept
    {
        return (uint8) ((float) a + ((float) b - (float) a) * k);
    }
    inline juce::Colour lerpColour (const juce::Colour& a, const juce::Colour& b, float k) noexcept
    {
        return juce::Colour::fromRGB (lerp8 (a.getRed(),   b.getRed(),   k),
                                       lerp8 (a.getGreen(), b.getGreen(), k),
                                       lerp8 (a.getBlue(),  b.getBlue(),  k));
    }
}

SpectrumMask::StrokePlan SpectrumMask::makeStrokePlan (const juce::Image& out,
                                                       const MaskImageLayer& cfg,
                                                       juce::Colour fallbackUniform) noexcept
{
    StrokePlan plan;
    plan.uniform = fallbackUniform;
    if (cfg.outlineMode == "image" || ! out.isValid())   // image 模式＝v0.5.4 行为，零开销直通
        return plan;

    const int W = out.getWidth(), H = out.getHeight();
    if (W <= 0 || H <= 0)
        return plan;

    // 列可见性 + 预乘通道和。"sum(预乘)/sum(alpha)" 数学上恒等于
    //   按 alpha 加权的未预乘平均色，省掉整遍解预乘。
    std::vector<uint8>   colAny ((size_t) W, 0);
    std::vector<int64_t> sumA ((size_t) W, 0), sumR ((size_t) W, 0),
                         sumG ((size_t) W, 0), sumB ((size_t) W, 0);
    {
        juce::Image::BitmapData bd (out, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < H; ++y)
        {
            const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            for (int x = 0; x < W; ++x)
            {
                const int a = line[x].getAlpha();
                if (a > 0)
                {
                    colAny[(size_t) x] = 1;
                    sumA[(size_t) x] += a;
                    sumR[(size_t) x] += line[x].getRed();
                    sumG[(size_t) x] += line[x].getGreen();
                    sumB[(size_t) x] += line[x].getBlue();
                }
            }
        }
    }

    // 连续可见列 = 一个"可视段"。bar 之间有全零列 → 每柱一段（perBar 语义）；
    // 折线一整个 blob → 单段。gap=0 柱粘连时自动并段 → 颜色随之并，仍符合"可视范围平均"。
    for (int x = 0; x < W; )
    {
        if (! colAny[(size_t) x]) { ++x; continue; }
        const int x0 = x;
        int64_t a = 0, r = 0, g = 0, b = 0;
        while (x < W && colAny[(size_t) x])
        {
            a += sumA[(size_t) x]; r += sumR[(size_t) x];
            g += sumG[(size_t) x]; b += sumB[(size_t) x];
            ++x;
        }
        const juce::Colour c = (a > 0)
            ? juce::Colour::fromRGB (
                (uint8) juce::jlimit (0, 255, (int) (r * 255 / a)),
                (uint8) juce::jlimit (0, 255, (int) (g * 255 / a)),
                (uint8) juce::jlimit (0, 255, (int) (b * 255 / a)))
            : fallbackUniform;
        plan.segStart.push_back (x0);
        plan.segEnd.push_back   (x - 1);
        plan.segColour.push_back (c);
    }

    if (plan.segColour.empty())
        return plan;                                   // 无电平 → 保持 fallback

    if (plan.segColour.size() == 1 || cfg.outlineMode != "perbar")
    {
        int64_t A = 0, R = 0, G = 0, B = 0;
        for (size_t s = 0; s < plan.segColour.size(); ++s)   // 按段宽加权 = 全可视区平均
        {
            const int64_t w = plan.segEnd[s] - plan.segStart[s] + 1;
            A += w;
            R += (int64_t) plan.segColour[s].getRed()   * w;
            G += (int64_t) plan.segColour[s].getGreen() * w;
            B += (int64_t) plan.segColour[s].getBlue()  * w;
        }
        plan.uniform = (A > 0) ? juce::Colour::fromRGB ((uint8)(R/A), (uint8)(G/A), (uint8)(B/A))
                               : fallbackUniform;
        return plan;                                   // perColumn=false
    }

    plan.perColumn = true;
    plan.colWidth  = W;
    bakeColumns (plan);
    return plan;
}

const SpectrumMask::StrokePlan&
SpectrumMask::PreviewPaletteCache::update (double nowSec,
                                           const StrokePlan& fresh,
                                           const MaskImageLayer& cfg)
{
    const bool throttled = cfg.outlinePreviewFps > 0.01f;
    const bool due = ! throttled || (nowSec - lastCompute) >= 1.0 / (double) cfg.outlinePreviewFps;

    if (due || ! primed)
    {
        if (! primed)
        {
            target = shown = fresh;
            prevSeg = fresh.segColour;
            lastCompute = lastLerp = nowSec;
            primed = true;
            return shown;
        }
        const bool sameBands = (fresh.segStart == target.segStart
                                && fresh.segEnd == target.segEnd);
        target = fresh;
        lastCompute = nowSec;
        if (! sameBands)          // 柱数/形状变了：索引不可跨帧混合，直接吸附
        {
            shown = target;
            prevSeg = target.segColour;
            bakeColumns (shown);
            return shown;
        }
    }

    // 指数逼近：每秒收敛率 = previewFps（关节流时按 30 计）。
    const double rate = throttled ? cfg.outlinePreviewFps : 30.0;
    const double dt = nowSec - lastLerp;
    lastLerp = nowSec;
    if (! cfg.outlineTemporal || dt <= 0.0)
    {
        shown = target;
        prevSeg = target.segColour;
        bakeColumns (shown);
        return shown;
    }
    const float k = (float) juce::jmin (1.0, 1.0 - std::exp (-dt * rate));

    if (target.perColumn)
    {
        shown.perColumn = true;
        shown.colWidth  = target.colWidth;
        shown.segStart  = target.segStart;
        shown.segEnd    = target.segEnd;
        shown.segColour.resize (target.segColour.size());
        for (size_t s = 0; s < shown.segColour.size(); ++s)
        {
            const juce::Colour from = (s < prevSeg.size()) ? prevSeg[s] : target.segColour[s];
            shown.segColour[s] = lerpColour (from, target.segColour[s], k);
        }
        prevSeg = shown.segColour;
        bakeColumns (shown);
    }
    else
    {
        shown.perColumn = false;
        shown.uniform = lerpColour (shown.uniform, target.uniform, k);
        prevSeg.clear();
    }
    return shown;
}
