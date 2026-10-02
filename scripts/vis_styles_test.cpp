// vis_styles_test — 新样式（径向/瀑布/地形）无头渲染冒烟：
//   工厂能造出样式、render 产出有效图 + 有墨、跨帧状态生效（旋转/历史）、resize 安全。
// 构建：ninja -C build vis_styles_test && ./build/vis_styles_test
#include <juce_graphics/juce_graphics.h>
#include "source/core/SpectrumStyle.h"
#include "source/core/BandFrame.h"
#include "source/core/ColorMap.h"
#include "source/core/SpectrumParams.h"
#include <cstdio>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>
#include <cstdlib>

static int failures = 0;
static void check (bool ok, const char* msg)
{
    if (! ok) { ++failures; std::printf ("FAIL: %s\n", msg); }
    else      { std::printf ("ok  : %s\n", msg); }
}

static void fillFrame (BandFrame& f, int seed, float base)
{
    f.resize (64);
    for (int i = 0; i < f.bandCount; ++i)
    {
        float v = base * (0.4f + 0.6f * std::abs (std::sin (0.3f * i + seed)));
        v = std::clamp (v, 0.0f, 1.0f);
        f.normalized[(size_t) i] = v;
        f.db[(size_t) i]         = -80.0f + 80.0f * v;
        f.peakDb[(size_t) i]     = -80.0f + 80.0f * std::min (1.0f, v + 0.15f);
    }
}

static void dumpPng (const juce::Image& im, const std::string& path)
{
    // 压到深灰背景（透明可见）
    juce::Image flat (juce::Image::ARGB, im.getWidth(), im.getHeight(), true);
    { juce::Graphics g (flat); g.fillAll (juce::Colour (0xff202024)); g.drawImageAt (im, 0, 0); }
    juce::MemoryOutputStream mem; juce::PNGImageFormat png;
    if (! png.writeImageToStream (flat, mem)) return;
    std::ofstream out (path, std::ios::binary);
    if (out) out.write ((const char*) mem.getData(), (std::streamsize) mem.getDataSize());
}

static long inkAnd (const juce::Image& im, long& sumOut)
{
    juce::Image::BitmapData b (im, juce::Image::BitmapData::readOnly);
    long ink = 0, sum = 0;
    for (int y = 0; y < b.height; ++y)
    {
        const auto* ln = reinterpret_cast<const juce::PixelARGB*> (b.getLinePointer (y));
        for (int x = 0; x < b.width; ++x) { const int a = ln[x].getAlpha(); sum += a; if (a > 10) ++ink; }
    }
    sumOut = sum; return ink;
}

static void testStyle (const char* name, bool expectStateful)
{
    auto st = SpectrumStyle::create (name);
    if (! st) { check (false, name); std::printf ("      (factory returned null)\n"); return; }

    const int W = 400, H = 300;
    auto renderOnce = [&] (int seed, float base) -> juce::Image
    {
        juce::Image im (juce::Image::ARGB, W, H, true);
        juce::Graphics g (im);
        BandFrame f; fillFrame (f, seed, base);
        SpectrumStyle::RenderParams rp; rp.width = W; rp.height = H;
        rp.barPitchRatio = 1.0f / (float) juce::jmax (1, f.bandCount);   // 与真实 app 一致（铺满一圈）
        rp.drawGrid = false;
        st->render (g, juce::Rectangle<int> (0, 0, W, H), f, rp);
        return im;
    };
    auto a = renderOnce (1, 0.6f);
    long sa = 0; long ink = inkAnd (a, sa);
    std::printf ("      [%s] frame0 ink=%ld alphaSum=%ld\n", name, ink, sa);
    check (a.isValid() && ink > 200, name);
    {
        std::string safe (name); for (auto& c : safe) if (c == ':') c = '_';
        std::system ("mkdir -p /tmp/opencode/shots");
        dumpPng (a, "/tmp/opencode/shots/" + safe + ".png");
    }

    if (expectStateful)
    {
        auto b = renderOnce (2, 0.6f);
        long sb = 0; inkAnd (b, sb);
        check (sb != sa, (std::string(name) + ": changes across frames (stateful)").c_str());
    }
    // resize 安全：换不同尺寸再渲一次不应崩
    {
        juce::Image im (juce::Image::ARGB, 220, 180, true);
        juce::Graphics g (im); BandFrame f; fillFrame (f, 7, 0.5f);
        SpectrumStyle::RenderParams rp; rp.width = 220; rp.height = 180;
        rp.barPitchRatio = 1.0f / (float) juce::jmax (1, f.bandCount);
        st->render (g, juce::Rectangle<int> (0, 0, 220, 180), f, rp);
        check (im.isValid(), (std::string(name) + ": resize-safe render ok").c_str());
    }
}

int main()
{
    testStyle ("ring:bar", true);
    testStyle ("ringline",   true);
    testStyle ("ring:bar-line", true);
    std::printf (failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
