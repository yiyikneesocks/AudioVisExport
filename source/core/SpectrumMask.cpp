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

bool SpectrumMask::isBarStyle (const juce::String& style) noexcept
{
    const juce::String s = style.toLowerCase();
    return s == "bar" || s == "bars" || s == "bar-line" || s == "barline" || s == "perbar";
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
                                   juce::Colour resolvedStroke,
                                   bool sideEdgesAllowed)
{
    return composeWithPlan (base, image, cfg, resolvedStroke, nullptr, 0.0, sideEdgesAllowed);
}

juce::Image SpectrumMask::composeWithPlan (const juce::Image& base,
                                           const juce::Image& image,
                                           const MaskImageLayer& cfg,
                                           juce::Colour resolvedStroke,
                                           PreviewPaletteCache* cache,
                                           double nowSec,
                                           bool sideEdgesAllowed)
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

    // ---- 4. 描边（v0.5.6 新1c：上/左/右三边各 开关+厚度+透明度+阴影；底部取消）----
    //   sideEdgesAllowed=false（line 系）→ 左右侧边强制关，整条只走上边框。
    if (cfg.strokeEnabled)
    {
        const int rT = cfg.outTop   ? juce::jlimit (1, 32, (int) std::lround (cfg.outWTop )) : 0;
        const int rL = (cfg.outLeft  && sideEdgesAllowed) ? juce::jlimit (1, 32, (int) std::lround (cfg.outWLeft )) : 0;
        const int rR = (cfg.outRight && sideEdgesAllowed) ? juce::jlimit (1, 32, (int) std::lround (cfg.outWRight)) : 0;

        // 阴影 = 在实边外再叠一条更宽的低透明度带；半径 = 厚度 + 阴影外扩。
        // v0.5.6 暂时禁用边框阴影（用户 2026-09-12 要求，保留原式勿删）：阴影外扩=0，
        // 于是 shT/shL/shR == 各自 width，edgeRim 内 `shadow > width` 分支自然不触发。
        const int shT = 0, shL = 0, shR = 0;
        // const int shT = rT ? rT + juce::jlimit (0, 32, (int) std::lround (cfg.outShadowTop )) : 0;
        // const int shL = rL ? rL + juce::jlimit (0, 32, (int) std::lround (cfg.outShadowLeft )) : 0;
        // const int shR = rR ? rR + juce::jlimit (0, 32, (int) std::lround (cfg.outShadowRight)) : 0;
        const int capR = juce::jmax (1, juce::jmax (juce::jmax (rT, rL), juce::jmax (rR, juce::jmax (shT, juce::jmax (shL, shR)))));

        if (rT | rL | rR)
        {
            const StrokePlan fresh = makeStrokePlan (out, cfg, resolvedStroke);
            const StrokePlan& plan = cache != nullptr
                                         ? cache->update (nowSec, fresh, cfg)
                                         : fresh;

            // 每个方向一次"窗内最小值"（单调队列滑窗，O(W·H)，与半径无关）。
            //   为做斜面归属：另算一个 capR 的"上方覆盖"bufCap——只有当某像素正上方 capR 内仍实心时
            //   才算真正的**竖直侧边**；斜面/上边界（上方会变透明）一律归给上边框，修 1c(3)。
            static thread_local std::vector<uint8> bufT, bufL, bufR, bufCap;
            const size_t n = (size_t) W * H;
            if (bufT.size() < n) { bufT.resize (n); bufL.resize (n); bufR.resize (n); bufCap.resize (n); }
            if (bufT.data() == nullptr || bufL.data() == nullptr
                || bufR.data() == nullptr || bufCap.data() == nullptr)
                return {};   // 极端低内存：宁缺勿崩

            // 法向等宽描边：当前沿是曲线，"厚度"应垂直于切线测量。旧实现是竖直方向固定 rT
            //   （斜面的法向厚度只有 rT*cosθ，越斜越细）。改为：逐列按顶面局部斜率 θ 把竖直窗
            //   半径放大 rT -> rT/cosθ = rT*sqrt(1+slope^2)，使**法向**厚度恒为 rT。
            std::vector<int> rTcol ((size_t) W, 0);
            if (rT)
            {
                static thread_local std::vector<int> ytop;
                if ((int) ytop.size() < W) ytop.resize ((size_t) W);
                for (int x = 0; x < W; ++x)
                {
                    int yt = -1;
                    for (int y = 0; y < H; ++y)
                        if (mA[(size_t) y * W + x] >= 128) { yt = y; break; }
                    ytop[(size_t) x] = yt;
                }
                const int k = 3;                      // 中心差分窗，抑制逐列噪声
                for (int x = 0; x < W; ++x)
                {
                    const int xa = juce::jmax (0, x - k), xb = juce::jmin (W - 1, x + k);
                    const int ya = ytop[(size_t) xa], yb = ytop[(size_t) xb];
                    float scale = 1.0f;
                    if (ya >= 0 && yb >= 0 && xb > xa)
                    {
                        const float s = (float) (yb - ya) / (float) (xb - xa);   // 斜率 dy/dx
                        scale = std::sqrt (1.0f + s * s);                         // = 1/cosθ
                        if (scale > 4.0f) scale = 4.0f;                          // 限制最大 4x（θ≤约75°）
                    }
                    rTcol[(size_t) x] = juce::jlimit (rT, 64, (int) std::lround ((float) rT * scale));
                }
            }

            std::vector<int> dqV ((size_t) H + 2);
            for (int x = 0; x < W; ++x)
            {
                const int rw = rTcol[x];                             // 顶缘窗 [y-rw, y]，rw 随斜率=法向等宽
                int hd = 0, tl = 0;
                for (int y = 0; y < H; ++y)
                {
                    const uint8 v = mA[(size_t) y * W + x];
                    while (tl > hd && mA[(size_t) dqV[tl - 1] * W + x] >= v) --tl;
                    dqV[tl++] = y;
                    while (dqV[hd] < y - rw) ++hd;
                    bufT[(size_t) y * W + x] = (rw && v) ? mA[(size_t) dqV[hd] * W + x] : v;
                }
                hd = tl = 0;                                         // "上方 capR 覆盖" 判定（斜面归属用）
                for (int y = 0; y < H; ++y)
                {
                    const uint8 v = mA[(size_t) y * W + x];
                    while (tl > hd && mA[(size_t) dqV[tl - 1] * W + x] >= v) --tl;
                    dqV[tl++] = y;
                    while (dqV[hd] < y - capR) ++hd;
                    bufCap[(size_t) y * W + x] = v ? mA[(size_t) dqV[hd] * W + x] : 0;
                }
            }
            std::vector<int> dqH ((size_t) W + 2);
            for (int y = 0; y < H; ++y)
            {
                const uint8* row = mA.data() + (size_t) y * W;
                int hd = 0, tl = 0;                                  // 左缘窗 [x-rL, x]
                for (int x = 0; x < W; ++x)
                {
                    while (tl > hd && row[dqH[tl - 1]] >= row[x]) --tl;
                    dqH[tl++] = x;
                    while (dqH[hd] < x - rL) ++hd;
                    bufL[(size_t) y * W + x] = (rL && row[x]) ? row[dqH[hd]] : row[x];
                }
                hd = tl = 0;                                         // 右缘窗 [x, x+rR]
                for (int x = W - 1; x >= 0; --x)
                {
                    while (tl > hd && row[dqH[tl - 1]] >= row[x]) --tl;
                    dqH[tl++] = x;
                    while (dqH[hd] > x + rR) ++hd;
                    bufR[(size_t) y * W + x] = (rR && row[x]) ? row[dqH[hd]] : row[x];
                }
            }

            const bool perCol = plan.perColumn && plan.colColour.size() >= (size_t) W;
            // 每边的 (rim, 透明度) 合成：实边用自身 alpha，阴影用更宽窗、更低 alpha 且压在实边下。
            auto edgeRim = [] (int mv, int minv, int width, int shadow, float alpha)
            {
                int a = 0;
                if (shadow > width)                                  // 阴影带（外圈）：低透明度
                {
                    const int srim = (mv - minv) * 60 / 255;        // ~0.24 强度
                    a = juce::jmax (a, (int) (srim * alpha));
                }
                const int rim = mv - minv;
                if (rim > 0) a = juce::jmax (a, (int) (rim * alpha));
                return juce::jlimit (0, 255, a);
            };
            juce::Image::BitmapData bd (out, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < H; ++y)
            {
                auto* line = reinterpret_cast<juce::PixelARGB*> (bd.getLinePointer (y));
                const uint8* m  = mA.data()    + (size_t) y * W;
                const uint8* t  = bufT.data()  + (size_t) y * W;
                const uint8* l  = bufL.data()  + (size_t) y * W;
                const uint8* rr = bufR.data()  + (size_t) y * W;
                const uint8* cp = bufCap.data()+ (size_t) y * W;
                for (int x = 0; x < W; ++x)
                {
                    const int mv = m[x];
                    if (mv == 0) continue;
                    int rim = 0;
                    if (rT != 0) rim = juce::jmax (rim, edgeRim (mv, t[x], rT, shT, cfg.outAlphaTop));
                    // 侧边仅当"上方 capR 内仍实心"（= 真正竖直边）；斜面/上边界交给上边框（修 1c(3)）
                    const bool vertSide = (cp[x] >= 250);
                    if (rL != 0 && vertSide) rim = juce::jmax (rim, edgeRim (mv, l[x],  rL, shL, cfg.outAlphaLeft));
                    if (rR != 0 && vertSide) rim = juce::jmax (rim, edgeRim (mv, rr[x], rR, shR, cfg.outAlphaRight));
                    if (rim <= 0) continue;
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
    // ⚠️ 大小写归一：GUI 下拉历史上存 "perBar"/"perFrame"（驼峰），CLI/fromJson 存小写，
    //   两边不一致曾导致 perBar 永不自匹配 → 全柱走 uniform（用户报"所有 bar 颜色都一样"）。
    //   统一按小写比较，两种写法今后都对，不依赖各处 token 拼写。
    const juce::String mode = cfg.outlineMode.toLowerCase();
    if (mode == "image" || ! out.isValid())   // image 模式＝v0.5.4 行为，零开销直通
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

    if (plan.segColour.size() == 1 || mode != "perbar")
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
