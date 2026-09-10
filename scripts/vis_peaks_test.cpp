// vis_peaks_test — v0.5.4 #3 回归：峰帽/峰线跟随基线轴（双侧）+ Peak caps 对 line 系生效
//   + y2k 下臂描边有色 + crystal 下臂填充不再溢出半屏。
// 构建：ninja vis_peaks_test && ./build/vis_peaks_test
#include <juce_graphics/juce_graphics.h>
#include "source/core/SpectrumStyle.h"
#include "source/core/BandFrame.h"
#include <cstdio>
#include <cmath>
#include <vector>

namespace
{
    int failures = 0;
    void check (bool ok, const char* what)
    {
        std::printf ("%s %s\n", ok ? "ok  :" : "FAIL:", what);
        if (! ok) ++failures;
    }

    constexpr int W = 600, H = 400;
    constexpr int NB = 20;

    BandFrame makeFrame (float level, float peak)   // db/peakDb 同值铺满各带
    {
        BandFrame f; f.resize (NB);
        for (int i = 0; i < NB; ++i)
        {
            f.db[(size_t) i]         = level;
            f.peakDb[(size_t) i]     = peak;
            f.normalized[(size_t) i] = std::clamp ((level + 80.0f) / 80.0f, 0.0f, 1.0f);
        }
        return f;
    }

    SpectrumStyle::RenderParams baseRp()
    {
        SpectrumStyle::RenderParams rp;
        rp.drawGrid = false; rp.drawAxisLabels = false;
        rp.primary = juce::Colour (0xffff0000);   // 纯红
        rp.secondary = juce::Colour (0xff00ff00);   // 纯绿（与 primary / peak 都拉开）
        rp.peak = juce::Colour (0xff0000ff);   // 纯蓝
        rp.barPitchRatio = 0.05f;        // 20 带 × 0.05 = 1.0 画布宽
        return rp;
    }

    juce::Image render (const char* style, const BandFrame& f, const SpectrumStyle::RenderParams& rp)
    {
        juce::Image img (juce::Image::ARGB, W, H, true);
        juce::Graphics g (img);
        if (auto s = SpectrumStyle::create (style))
            s->render (g, { 0, 0, W, H }, f, rp);
        return img;
    }

    bool isBlue (juce::PixelARGB p)   // 峰值色（含 alpha<255 的 0.75/0.8 变体）
    {
        return p.getAlpha() > 30 && p.getBlue() > 120 && p.getRed() < 90 && p.getGreen() < 90;
    }
    bool isRed (juce::PixelARGB p)
    {
        return p.getAlpha() > 120 && p.getRed() > 120 && p.getGreen() < 90 && p.getBlue() < 90;
    }

    // 某色像素的 y 直方图里，最靠上/最靠下的行；返回命中数
    struct Hits { int count = 0; int minY = 1 << 20, maxY = -1; };
    template <typename Pred>
    Hits scan (const juce::Image& img, int yFrom, int yTo, Pred pred)
    {
        Hits h;
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = yFrom; y < yTo; ++y)
        {
            const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            for (int x = 0; x < W; ++x)
                if (pred (line[x]))
                {
                    ++h.count;
                    if (y < h.minY) h.minY = y;
                    if (y > h.maxY) h.maxY = y;
                }
        }
        return h;
    }
    // 两张渲染图在 [yFrom,yTo) 内的显著差异像素数（任一通道差 > 24）
    int diffCount (const juce::Image& A, const juce::Image& B, int yFrom, int yTo)
    {
        int n = 0;
        juce::Image::BitmapData ba (A, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData bb (B, juce::Image::BitmapData::readOnly);
        for (int y = yFrom; y < yTo; ++y)
        {
            const auto* la = reinterpret_cast<const juce::PixelARGB*> (ba.getLinePointer (y));
            const auto* lb = reinterpret_cast<const juce::PixelARGB*> (bb.getLinePointer (y));
            for (int x = 0; x < W; ++x)
                if (std::abs (la[x].getRed() - lb[x].getRed()) > 24 || std::abs (la[x].getGreen() - lb[x].getGreen()) > 24
                 || std::abs (la[x].getBlue()  - lb[x].getBlue())  > 24 || std::abs (la[x].getAlpha() - lb[x].getAlpha()) > 24)
                    ++n;
        }
        return n;
    }

    // 全图某色行集合（y → 该行是否有色）
    template <typename Pred>
    std::vector<int> rowsWith (const juce::Image& img, int yFrom, int yTo, Pred pred, int minPerRow = 1)
    {
        std::vector<int> rows;
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = yFrom; y < yTo; ++y)
        {
            const auto* line = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            int c = 0;
            for (int x = 0; x < W; ++x) if (pred (line[x])) ++c;
            if (c >= minPerRow) rows.push_back (y);
        }
        return rows;
    }
}

int main()
{
    // 轴位 a 下：y(n) = bottom − n·H（bottom=H）
    auto expectY = [] (float n) { return (int) std::lround (H - n * H); };

    // ================= #3.2 bar 峰帽跟随基线轴 + 双侧 =================
    {
        auto f = makeFrame (-40.0f, -64.0f);          // pn = 0.2
        auto rp = baseRp(); rp.barParticles = true;

        rp.baselineY = 0.0f;
        auto a0 = render ("bar", f, rp);
        // a=0：帽在 baselineTop(0.2,0)=0.2 → y=320；下臂不存在
        auto rowsA0 = rowsWith (a0, 0, H, isBlue, 20);
        check (! rowsA0.empty(), "bar a=0: peak caps present");
        check (rowsA0.size() <= 3 && std::abs (rowsA0.front() - expectY (0.2f)) <= 2,
               "bar a=0: caps at n=0.2 (old bottom-growth position, zero regression)");
        check (scan (a0, expectY (0.2f) + 4, H, isBlue).count == 0,
               "bar a=0: no caps below the axis line");

        rp.baselineY = 0.5f;
        auto a5 = render ("bar", f, rp);
        // a=0.5：上帽 baselineTop=0.5+0.5*0.2=0.6 → y=160；下帽 baselineBottom=0.5*0.8=0.4 → y=240
        auto rowsUp = rowsWith (a5, 0,   200, isBlue, 20);
        auto rowsDn = rowsWith (a5, 200, H,   isBlue, 20);
        check (! rowsUp.empty() && std::abs (rowsUp.front() - expectY (0.6f)) <= 2,
               "bar a=0.5: upper cap moved with the axis (n=0.2 -> 0.6)");
        check (! rowsDn.empty() && std::abs (rowsDn.back() - expectY (0.4f)) <= 2,
               "bar a=0.5: lower cap exists at 0.4 (two-sided caps)");
        // 旧码（帽用原始 pn，不过轴）会同时错在这两处：不该再看到 y=320 的帽
        check (scan (a5, 300, 340, isBlue).count == 0,
               "bar a=0.5: no stale cap at the un-mapped pn row (y~320)");
    }

    // ================= #3.3 Peak caps 开关对三个 line 样式生效 =================
    {
        auto f = makeFrame (-40.0f, -20.0f);          // 峰远高于曲线
        for (const char* st : { "y2k-line", "polyline", "crystal" })
        {
            auto on = baseRp();  on.baselineY = 0.5f; on.barParticles = true;
            auto off = baseRp(); off.baselineY = 0.5f; off.barParticles = false;
            auto A = render (st, f, on);
            auto B = render (st, f, off);
            const int dAll = diffCount (A, B, 0, H);
            const int dUp  = diffCount (A, B, 0, 200);
            const int dDn  = diffCount (A, B, 200, H);
            char buf[128];
            std::snprintf (buf, sizeof buf, "%s: Peak caps toggle changes pixels (on-off diff=%d)", st, dAll);
            check (dAll > 500, buf);
            std::snprintf (buf, sizeof buf, "%s: peak line on BOTH sides of the axis (up=%d down=%d)", st, dUp, dDn);
            check (dUp > 100 && dDn > 100, buf);   // #3.3 + #3.4
        }
    }

    // ================= #3.4 y2k 下臂描边必须显式着色（旧码继承 0.25f 填充色→近乎不可见） =
    {
        auto f = makeFrame (-40.0f, -40.0f);
        auto rp = baseRp(); rp.baselineY = 0.5f; rp.barParticles = false;
        auto img = render ("y2k-line", f, rp);
        // 下臂曲线（primary 纯红，轴以下）：baselineBottom(n=0.5,0.5)=0.25 → y=300 附近
        const auto rows = rowsWith (img, 250, H, isRed, 30);
        check (! rows.empty(), "y2k-line: lower arm has a solid primary stroke below the axis");
        if (! rows.empty())
            check (std::abs (rows[rows.size() / 2] - expectY (0.25f)) <= 8,
                   "y2k-line: lower-arm stroke sits at the mirrored curve height");
    }

    // ================= #3.4 crystal 下臂填充不得铺满下半屏 =================
    {
        auto f = makeFrame (-72.0f, -72.0f);          // n=0.1 → 正确下臂只占轴下方 ~10%H
        auto rp = baseRp(); rp.baselineY = 0.5f; rp.barParticles = false;
        auto img = render ("crystal", f, rp);
        // 轴在 y=200，下半屏 = 200..H；正确实现填充高度 ≈ (0.5−0.45)·H = 20px，
        //   反推漏减 a 的旧码 dn=bottom → 填满 200 行。阈值取 25% 半屏面积（100 行）留足余量。
        const int low = scan (img, 200, H, [] (juce::PixelARGB p) { return p.getAlpha() > 40; }).count;
        const int area = W * (H - 200) / 4;           // 半屏 25% 面积
        std::printf ("      [crystal] opaque px in lower half = %d (25%% bound = %d)\n", low, area);
        check (low > 0,    "crystal: lower arm is drawn at all");
        check (low < area, "crystal: lower arm fill no longer floods the whole lower half");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
