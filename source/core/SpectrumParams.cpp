// =============================================================================
// SpectrumParams.cpp — 参数结构实现（JSON I/O + CLI 覆盖）
// =============================================================================
#include "SpectrumParams.h"

// =============================================================================
// 枚举字符串映射
// =============================================================================
SpectrumParams::WindowFunc SpectrumParams::parseWindowFunc (const juce::String& s, bool* ok)
{
    if (ok) *ok = true;
    auto lower = s.toLowerCase().trim();
    if (lower == "hann" || lower == "hanning")              return Hann;
    if (lower == "hamming")                                 return Hamming;
    if (lower == "blackman")                                return Blackman;
    if (lower == "blackman-harris" || lower == "blackmanharris") return BlackmanHarris;
    if (lower == "rectangular" || lower == "rect" || lower == "none") return Rectangular;
    if (ok) *ok = false;
    return Hann;
}

SpectrumParams::FreqScale SpectrumParams::parseFreqScale (const juce::String& s, bool* ok)
{
    if (ok) *ok = true;
    auto lower = s.toLowerCase().trim();
    if (lower == "log" || lower == "logarithmic") return Log;
    if (lower == "linear" || lower == "lin")      return Linear;
    if (lower == "mel")                           return Mel;
    if (lower == "bark")                          return Bark;
    if (ok) *ok = false;
    return Log;
}

SpectrumParams::DynCurve SpectrumParams::parseDynCurve (const juce::String& s, bool* ok)
{
    if (ok) *ok = true;
    auto lower = s.toLowerCase().trim();
    if (lower == "linear" || lower == "lin") return LinearDyn;
    if (lower == "sqrt" || lower == "square-root") return Sqrt;
    if (lower == "loglog" || lower == "log-log") return LogLog;
    if (lower == "perceptual" || lower == "a-weight") return Perceptual;
    if (ok) *ok = false;
    return LinearDyn;
}

SpectrumParams::Encoder SpectrumParams::parseEncoder (const juce::String& s, bool* ok)
{
    if (ok) *ok = true;
    auto lower = s.toLowerCase().trim();
    if (lower == "png-seq" || lower == "png" || lower == "png-sequence") return PngSeq;
    if (lower == "webm-vp9" || lower == "webm") return WebmVp9;
    if (lower == "mov-qtrle" || lower == "mov" || lower == "qtrle") return MovQtrle;
    if (ok) *ok = false;
    return PngSeq;
}

juce::String SpectrumParams::windowFuncName (WindowFunc v)
{
    switch (v) {
        case Hann:            return "hann";
        case Hamming:         return "hamming";
        case Blackman:        return "blackman";
        case BlackmanHarris:  return "blackman-harris";
        case Rectangular:     return "rectangular";
    }
    return "hann";
}

juce::String SpectrumParams::freqScaleName (FreqScale v)
{
    switch (v) {
        case Log:    return "log";
        case Linear: return "linear";
        case Mel:    return "mel";
        case Bark:   return "bark";
    }
    return "log";
}

juce::String SpectrumParams::dynCurveName (DynCurve v)
{
    switch (v) {
        case LinearDyn:  return "linear";
        case Sqrt:       return "sqrt";
        case LogLog:     return "loglog";
        case Perceptual: return "perceptual";
    }
    return "linear";
}

juce::String SpectrumParams::encoderName (Encoder v)
{
    switch (v) {
        case PngSeq:   return "png-seq";
        case WebmVp9:  return "webm-vp9";
        case MovQtrle: return "mov-qtrle";
    }
    return "png-seq";
}

// =============================================================================
// 颜色字符串解析：支持 #rrggbb / #aarrggbb / #rgb / #argb（不区分大小写）
// =============================================================================
static juce::Colour parseColour (const juce::String& s, bool* ok)
{
    if (ok) *ok = false;
    auto t = s.trim();
    if (! t.startsWithChar ('#')) return juce::Colour();
    auto hex = t.substring (1);
    // 不带 # 的 6/8/3/4 位 hex
    auto isValidHex = [](const juce::String& x) -> bool {
        for (int i = 0; i < x.length(); ++i) {
            const juce::juce_wchar c = x[i];
            const bool isHex = (c >= '0' && c <= '9')
                            || (c >= 'a' && c <= 'f')
                            || (c >= 'A' && c <= 'F');
            if (! isHex) return false;
        }
        return true;
    };
    if (! isValidHex (hex) || hex.length() < 3) return juce::Colour();
    if (hex.length() == 6) {
        if (ok) *ok = true;
        return juce::Colour::fromRGB (
            (int) strtol (hex.substring (0, 2).toRawUTF8(), nullptr, 16),
            (int) strtol (hex.substring (2, 4).toRawUTF8(), nullptr, 16),
            (int) strtol (hex.substring (4, 6).toRawUTF8(), nullptr, 16));
    }
    if (hex.length() == 8) {
        if (ok) *ok = true;
        return juce::Colour::fromRGBA (
            (int) strtol (hex.substring (0, 2).toRawUTF8(), nullptr, 16),
            (int) strtol (hex.substring (2, 4).toRawUTF8(), nullptr, 16),
            (int) strtol (hex.substring (4, 6).toRawUTF8(), nullptr, 16),
            (int) strtol (hex.substring (6, 8).toRawUTF8(), nullptr, 16));
    }
    return juce::Colour();
}

// =============================================================================
// JSON 序列化（手动构造，避免依赖 juce::var 的复杂 API）
// =============================================================================
namespace {
juce::String escJson (const juce::String& s)
{
    juce::String out;
    for (auto c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}
juce::String colourHex (juce::Colour c)
{
    return juce::String::formatted ("#%08x", c.getARGB());
}
}

juce::String SpectrumParams::toJson() const
{
    juce::String s;
    s << "{\n";
    // fft
    s << "  \"fft\": {\n";
    s << "    \"fftOrder\": " << fftOrder << ",\n";
    s << "    \"fftOrderLo\": " << fftOrderLo << ",\n";
    s << "    \"enableLowFreqPath\": " << (enableLowFreqPath ? "true" : "false") << ",\n";
    s << "    \"hopSize\": " << hopSize << ",\n";
    s << "    \"hopSizeLo\": " << hopSizeLo << ",\n";
    s << "    \"windowFunc\": \"" << windowFuncName (windowFunc) << "\",\n";
    s << "    \"crossoverHz\": " << crossoverHz << "\n";
    s << "  },\n";
    // freq
    s << "  \"freq\": {\n";
    s << "    \"minHz\": " << minHz << ",\n";
    s << "    \"maxHz\": " << maxHz << ",\n";
    s << "    \"freqScale\": \"" << freqScaleName (freqScale) << "\",\n";
    s << "    \"bandCount\": " << bandCount << "\n";
    s << "  },\n";
    // time
    s << "  \"time\": {\n";
    s << "    \"fps\": " << fps << ",\n";
    s << "    \"attackMs\": " << attackMs << ",\n";
    s << "    \"releaseMs\": " << releaseMs << ",\n";
    s << "    \"peakHoldMs\": " << peakHoldMs << ",\n";
    s << "    \"peakDecayDbPerSec\": " << peakDecayDbPerSec << ",\n";
    s << "    \"peakDecayAccelDbPerSec2\": " << peakDecayAccelDbPerSec2 << ",\n";
    s << "    \"temporalSmoothing\": " << temporalSmoothing << "\n";
    s << "  },\n";
    // dynamic
    s << "  \"dynamic\": {\n";
    s << "    \"curve\": \"" << dynCurveName (dynCurve) << "\",\n";
    s << "    \"gain\": " << dynGain << ",\n";
    s << "    \"gamma\": " << dynGamma << ",\n";
    s << "    \"slopeEnabled\": " << (slopeEnabled ? "true" : "false") << ",\n";
    s << "    \"slopeDbPerOct\": " << slopeDbPerOct << ",\n";
    s << "    \"minDb\": " << minDb << ",\n";
    s << "    \"maxDb\": " << maxDb << "\n";
    s << "  },\n";
    // visual
    s << "  \"visual\": {\n";
    s << "    \"style\": \"" << escJson (style) << "\",\n";
    s << "    \"colorMap\": \"" << escJson (colorMap) << "\",\n";
    s << "    \"primaryColor\": \"" << colourHex (primaryColor) << "\",\n";
    s << "    \"secondaryColor\": \"" << colourHex (secondaryColor) << "\",\n";
    s << "    \"peakColor\": \"" << colourHex (peakColor) << "\",\n";
    s << "    \"bgColor\": \"" << colourHex (bgColor) << "\",\n";
    s << "    \"barGapRatio\": " << barGapRatio << ",\n";
    s << "    \"barWidthRatio\": " << barWidthRatio << ",\n";
    s << "    \"barParticles\": " << (barParticles ? "true" : "false") << ",\n";
    s << "    \"lineWidth\": " << lineWidth << ",\n";
    s << "    \"opacity\": " << opacity << ",\n";
    s << "    \"drawGrid\": " << (drawGrid ? "true" : "false") << ",\n";
    s << "    \"drawAxisLabels\": " << (drawAxisLabels ? "true" : "false") << "\n";
    s << "  },\n";
    // 频谱元素变换
    s << "  \"transform\": {\n";
    s << "    \"set\": " << (transform.set ? "true" : "false") << ",\n";
    s << "    \"centerX\": " << transform.centerX << ",\n";
    s << "    \"centerY\": " << transform.centerY << ",\n";
    s << "    \"scaleX\": " << transform.scaleX << ",\n";
    s << "    \"scaleY\": " << transform.scaleY << ",\n";
    s << "    \"rotationDeg\": " << transform.rotationDeg << ",\n";
    s << "    \"posX\": " << transform.posX << ",\n";
    s << "    \"posY\": " << transform.posY << "\n";
    s << "  },\n";
    // 频谱层状态 + 统一 z 序
    s << "  \"spectrumPresent\": " << (spectrumPresent ? "true" : "false") << ",\n";
    s << "  \"spectrumIndex\": " << spectrumIndex << ",\n";
    s << "  \"snapEnabled\": " << (snapEnabled ? "true" : "false") << ",\n";
    // output
    s << "  \"output\": {\n";
    s << "    \"width\": " << width << ",\n";
    s << "    \"height\": " << height << ",\n";
    s << "    \"encoder\": \"" << encoderName (encoder) << "\",\n";
    s << "    \"digits\": " << digits << ",\n";
    s << "    \"baseName\": \"" << escJson (baseName) << "\",\n";
    s << "    \"outputDir\": \"" << escJson (outputDir) << "\",\n";
    s << "    \"outputVideoPath\": \"" << escJson (outputVideoPath) << "\",\n";
    s << "    \"ffmpegPath\": \"" << escJson (ffmpegPath) << "\",\n";
    s << "    \"bgCheckerboardPreview\": " << (bgCheckerboardPreview ? "true" : "false") << "\n";
    s << "  },\n";
    // images：图片图层（v0.4.2，序列化在 output 块后单独输出）
    if (! images.empty())
    {
        s << "  \"images\": [\n";
        for (size_t i = 0; i < images.size(); ++i)
        {
            const auto& im = images[i];
            const bool above = ((int) i >= spectrumIndex);
            s << "    { \"path\": \"" << escJson (im.path) << "\","
              << " \"centerX\": " << im.transform.centerX
              << ", \"centerY\": " << im.transform.centerY
              << ", \"scaleX\": " << im.transform.scaleX
              << ", \"scaleY\": " << im.transform.scaleY
              << ", \"rotationDeg\": " << im.transform.rotationDeg
              << ", \"posX\": " << im.transform.posX
              << ", \"posY\": " << im.transform.posY
              << ", \"opacity\": " << im.opacity
              << ", \"aboveSpectrum\": " << (above ? "true" : "false")
              << ", \"visible\": " << (im.visible ? "true" : "false") << " }"
              << (i + 1 < images.size() ? "," : "") << "\n";
        }
        s << "  ],\n";
    }
    // audio
    s << "  \"audio\": {\n";
    s << "    \"path\": \"" << escJson (audioPath) << "\"\n";
    s << "  },\n";
    // maskImage：频谱蒙版图片（v0.5.4）
    {
        const auto& mk = maskImage;
        const auto& mt = mk.transform;
        s << "  \"maskImage\": {\n";
        s << "    \"enabled\": " << (mk.enabled ? "true" : "false") << ",\n";
        s << "    \"path\": \"" << escJson (mk.path) << "\",\n";
        s << "    \"transform\": {\n";
        s << "      \"set\": " << (mt.set ? "true" : "false") << ",\n";
        s << "      \"centerX\": " << mt.centerX << ",\n";
        s << "      \"centerY\": " << mt.centerY << ",\n";
        s << "      \"scaleX\": " << mt.scaleX << ",\n";
        s << "      \"scaleY\": " << mt.scaleY << ",\n";
        s << "      \"rotationDeg\": " << mt.rotationDeg << ",\n";
        s << "      \"posX\": " << mt.posX << ",\n";
        s << "      \"posY\": " << mt.posY << "\n";
        s << "    },\n";
        s << "    \"strokeEnabled\": " << (mk.strokeEnabled ? "true" : "false") << ",\n";
        s << "    \"strokeWidth\": " << mk.strokeWidth << ",\n";
        s << "    \"strokeAutoColor\": " << (mk.strokeAutoColor ? "true" : "false") << ",\n";
        s << "    \"brightness\": " << mk.brightness << ",\n";
        s << "    \"contrast\": " << mk.contrast << ",\n";
        s << "    \"saturation\": " << mk.saturation << ",\n";
        s << "    \"strokeColor\": \"" << colourHex (mk.strokeColor) << "\"\n";
        s << "  }\n";
    }
    s << "}";
    return s;
}

// =============================================================================
// JSON 解析（用 juce::var / juce::JSON 解析，访问嵌套对象）
// =============================================================================
SpectrumParams SpectrumParams::fromJson (const juce::String& jsonText,
                                         juce::String& errorMessage)
{
    SpectrumParams p;  // 默认值
    juce::var root = juce::JSON::parse (jsonText);
    if (root.isVoid()) {
        errorMessage = "JSON parse failed";
        return p;
    }
    auto getStr = [&](const juce::var& obj, const char* key, const juce::String& def) -> juce::String {
        if (auto* o = obj.getDynamicObject(); o != nullptr)
            if (o->hasProperty (key)) return o->getProperty (key).toString();
        return def;
    };
    auto getInt = [&](const juce::var& obj, const char* key, int def) -> int {
        if (auto* o = obj.getDynamicObject(); o != nullptr)
            if (o->hasProperty (key)) return (int) o->getProperty (key);
        return def;
    };
    auto getFloat = [&](const juce::var& obj, const char* key, float def) -> float {
        if (auto* o = obj.getDynamicObject(); o != nullptr)
            if (o->hasProperty (key)) return (float) o->getProperty (key);
        return def;
    };
    auto getDouble = [&](const juce::var& obj, const char* key, double def) -> double {
        if (auto* o = obj.getDynamicObject(); o != nullptr)
            if (o->hasProperty (key)) return (double) o->getProperty (key);
        return def;
    };
    auto getBool = [&](const juce::var& obj, const char* key, bool def) -> bool {
        if (auto* o = obj.getDynamicObject(); o != nullptr)
            if (o->hasProperty (key)) return (bool) o->getProperty (key);
        return def;
    };

    auto fft = root.getProperty ("fft", juce::var());
    if (auto* o = fft.getDynamicObject()) {
        p.fftOrder          = getInt (fft, "fftOrder", p.fftOrder);
        p.fftOrderLo        = getInt (fft, "fftOrderLo", p.fftOrderLo);
        p.enableLowFreqPath = getBool (fft, "enableLowFreqPath", p.enableLowFreqPath);
        p.hopSize           = getInt (fft, "hopSize", p.hopSize);
        p.hopSizeLo         = getInt (fft, "hopSizeLo", p.hopSizeLo);
        p.crossoverHz       = getFloat (fft, "crossoverHz", p.crossoverHz);
        bool wok = false;
        auto wv = parseWindowFunc (getStr (fft, "windowFunc", ""), &wok);
        if (wok) p.windowFunc = wv;
    }
    auto freq = root.getProperty ("freq", juce::var());
    if (auto* o = freq.getDynamicObject()) {
        p.minHz     = getFloat (freq, "minHz", p.minHz);
        p.maxHz     = getFloat (freq, "maxHz", p.maxHz);
        p.bandCount = getInt (freq, "bandCount", p.bandCount);
        bool fok = false;
        auto fv = parseFreqScale (getStr (freq, "freqScale", ""), &fok);
        if (fok) p.freqScale = fv;
    }
    auto time = root.getProperty ("time", juce::var());
    if (auto* o = time.getDynamicObject()) {
        p.fps                = getDouble (time, "fps", p.fps);
        p.attackMs           = getFloat (time, "attackMs", p.attackMs);
        p.releaseMs          = getFloat (time, "releaseMs", p.releaseMs);
        p.peakHoldMs         = getFloat (time, "peakHoldMs", p.peakHoldMs);
        p.peakDecayDbPerSec  = getFloat (time, "peakDecayDbPerSec", p.peakDecayDbPerSec);
        p.peakDecayAccelDbPerSec2 = getFloat (time, "peakDecayAccelDbPerSec2", p.peakDecayAccelDbPerSec2);
        p.temporalSmoothing  = getFloat (time, "temporalSmoothing", p.temporalSmoothing);
    }
    auto dyn = root.getProperty ("dynamic", juce::var());
    if (auto* o = dyn.getDynamicObject()) {
        p.dynGain        = getFloat (dyn, "gain", p.dynGain);
        p.dynGamma       = getFloat (dyn, "gamma", p.dynGamma);
        p.slopeEnabled   = getBool (dyn, "slopeEnabled", p.slopeEnabled);
        p.slopeDbPerOct  = getFloat (dyn, "slopeDbPerOct", p.slopeDbPerOct);
        p.minDb          = getFloat (dyn, "minDb", p.minDb);
        p.maxDb          = getFloat (dyn, "maxDb", p.maxDb);
        bool dok = false;
        auto dv = parseDynCurve (getStr (dyn, "curve", ""), &dok);
        if (dok) p.dynCurve = dv;
    }
    auto vis = root.getProperty ("visual", juce::var());
    if (auto* o = vis.getDynamicObject()) {
        p.style         = getStr (vis, "style", p.style);
        p.colorMap      = getStr (vis, "colorMap", p.colorMap);
        p.barGapRatio   = getFloat (vis, "barGapRatio", p.barGapRatio);
        p.barWidthRatio = getFloat (vis, "barWidthRatio", p.barWidthRatio);
        p.barParticles  = getBool (vis, "barParticles", p.barParticles);
        p.lineWidth     = getFloat (vis, "lineWidth", p.lineWidth);
        p.opacity       = getFloat (vis, "opacity", p.opacity);
        p.drawGrid      = getBool (vis, "drawGrid", p.drawGrid);
        p.drawAxisLabels= getBool (vis, "drawAxisLabels", p.drawAxisLabels);
        bool cok = false;
        auto pc = parseColour (getStr (vis, "primaryColor", ""), &cok);
        if (cok) p.primaryColor = pc;
        auto sc = parseColour (getStr (vis, "secondaryColor", ""), &cok);
        if (cok) p.secondaryColor = sc;
        auto pk = parseColour (getStr (vis, "peakColor", ""), &cok);
        if (cok) p.peakColor = pk;
        auto bg = parseColour (getStr (vis, "bgColor", ""), &cok);
        if (cok) p.bgColor = bg;
    }
    auto tf = root.getProperty ("transform", juce::var());
    if (auto* o = tf.getDynamicObject()) {
        p.transform.set         = getBool  (tf, "set", p.transform.set);
        p.transform.centerX     = getFloat (tf, "centerX", p.transform.centerX);
        p.transform.centerY     = getFloat (tf, "centerY", p.transform.centerY);
        p.transform.scaleX      = getFloat (tf, "scaleX", p.transform.scaleX);
        p.transform.scaleY      = getFloat (tf, "scaleY", p.transform.scaleY);
        p.transform.rotationDeg = getFloat (tf, "rotationDeg", p.transform.rotationDeg);
        p.transform.posX        = getFloat (tf, "posX", p.transform.posX);
        p.transform.posY        = getFloat (tf, "posY", p.transform.posY);
    }
    // 频谱层状态 + 统一 z 序
    p.spectrumPresent = getBool (root, "spectrumPresent", true);
    p.snapEnabled     = getBool (root, "snapEnabled", true);
    p.spectrumIndex   = getInt  (root, "spectrumIndex", -1);  // -1 = 旧 JSON 无此字段
    auto out = root.getProperty ("output", juce::var());
    if (auto* o = out.getDynamicObject()) {
        p.width         = getInt (out, "width", p.width);
        p.height        = getInt (out, "height", p.height);
        p.digits        = getInt (out, "digits", p.digits);
        p.baseName      = getStr (out, "baseName", p.baseName);
        p.outputDir     = getStr (out, "outputDir", p.outputDir);
        p.outputVideoPath = getStr (out, "outputVideoPath", p.outputVideoPath);
        p.ffmpegPath    = getStr (out, "ffmpegPath", p.ffmpegPath);
        p.bgCheckerboardPreview = getBool (out, "bgCheckerboardPreview", p.bgCheckerboardPreview);
        bool eok = false;
        auto ev = parseEncoder (getStr (out, "encoder", ""), &eok);
        if (eok) p.encoder = ev;
    }
    auto aud = root.getProperty ("audio", juce::var());
    if (auto* o = aud.getDynamicObject()) {
        p.audioPath = getStr (aud, "path", p.audioPath);
    }
    // images：图片图层（v0.4.2）
    if (auto* arr = root.getProperty ("images", juce::var()).getArray()) {
        p.images.clear();
        for (auto& item : *arr)
        {
            ImageLayer L;
            L.path = item.getProperty ("path", juce::var()).toString();
            if (L.path.isEmpty()) continue;
            L.transform.centerX    = (float) (double) item.getProperty ("centerX", juce::var (0.0));
            L.transform.centerY    = (float) (double) item.getProperty ("centerY", juce::var (0.0));
            L.transform.scaleX     = (float) (double) item.getProperty ("scaleX", juce::var (1.0));
            L.transform.scaleY     = (float) (double) item.getProperty ("scaleY", juce::var (1.0));
            L.transform.rotationDeg= (float) (double) item.getProperty ("rotationDeg", juce::var (0.0));
            L.transform.posX       = (float) (double) item.getProperty ("posX", juce::var (0.0));
            L.transform.posY       = (float) (double) item.getProperty ("posY", juce::var (0.0));
            L.transform.set        = true;
            L.opacity              = (float) (double) item.getProperty ("opacity", juce::var (1.0));
            L.visible              = (bool) (bool) item.getProperty ("visible", juce::var (true));
            p.images.push_back (L);
        }
    }
    // 统一 z 序：从旧 aboveSpectrum 标志推导 spectrumIndex（新 JSON 直接读 spectrumIndex）
    if (p.spectrumIndex < 0)
    {
        p.spectrumIndex = 0;
        for (size_t i = 0; i < p.images.size(); ++i)
        {
            if (! p.images[i].aboveSpectrum)
                p.spectrumIndex = (int) i + 1;
        }
    }
    // 从 spectrumIndex 派生每张图片的 aboveSpectrum（保持一致）
    for (size_t i = 0; i < p.images.size(); ++i)
        p.images[i].aboveSpectrum = ((int) i >= p.spectrumIndex);
    // maskImage：频谱蒙版图片（v0.5.4）
    auto mk = root.getProperty ("maskImage", juce::var());
    if (auto* o = mk.getDynamicObject())
    {
        p.maskImage.enabled         = getBool   (mk, "enabled", p.maskImage.enabled);
        p.maskImage.path            = getStr    (mk, "path", p.maskImage.path);
        p.maskImage.strokeEnabled   = getBool   (mk, "strokeEnabled", p.maskImage.strokeEnabled);
        p.maskImage.strokeWidth     = getFloat  (mk, "strokeWidth", p.maskImage.strokeWidth);
        p.maskImage.strokeAutoColor = getBool   (mk, "strokeAutoColor", p.maskImage.strokeAutoColor);
        p.maskImage.brightness      = juce::jlimit (0.0f, 2.0f, getFloat (mk, "brightness", p.maskImage.brightness));
        p.maskImage.contrast        = juce::jlimit (0.0f, 2.0f, getFloat (mk, "contrast", p.maskImage.contrast));
        p.maskImage.saturation      = juce::jlimit (0.0f, 2.0f, getFloat (mk, "saturation", p.maskImage.saturation));
        bool cok = false;
        auto sc = parseColour (getStr (mk, "strokeColor", ""), &cok);
        if (cok) p.maskImage.strokeColor = sc;
        auto mt = mk.getProperty ("transform", juce::var());
        if (auto* to = mt.getDynamicObject())
        {
            p.maskImage.transform.set         = getBool  (mt, "set", p.maskImage.transform.set);
            p.maskImage.transform.centerX     = getFloat (mt, "centerX", p.maskImage.transform.centerX);
            p.maskImage.transform.centerY     = getFloat (mt, "centerY", p.maskImage.transform.centerY);
            p.maskImage.transform.scaleX      = getFloat (mt, "scaleX", p.maskImage.transform.scaleX);
            p.maskImage.transform.scaleY      = getFloat (mt, "scaleY", p.maskImage.transform.scaleY);
            p.maskImage.transform.rotationDeg = getFloat (mt, "rotationDeg", p.maskImage.transform.rotationDeg);
            p.maskImage.transform.posX        = getFloat (mt, "posX", p.maskImage.transform.posX);
            p.maskImage.transform.posY        = getFloat (mt, "posY", p.maskImage.transform.posY);
        }
    }
    errorMessage.clear();
    return p;
}

// =============================================================================
// CLI 覆盖：点路径 key = value
// =============================================================================
bool SpectrumParams::applyOverride (const juce::String& dottedKey,
                                    const juce::String& value,
                                    juce::String& errorMessage)
{
    errorMessage.clear();
    auto key = dottedKey.trim();
    auto val = value.trim();
    auto setErr = [&](const char* msg) -> bool {
        errorMessage = msg;
        return false;
    };
    auto toInt = [&](bool* ok) -> int {
        try { return std::stoi (val.toStdString()); }
        catch (...) { if (ok) *ok = false; return 0; }
    };
    auto toFloat = [&](bool* ok) -> float {
        try { return std::stof (val.toStdString()); }
        catch (...) { if (ok) *ok = false; return 0.0f; }
    };
    auto toDouble = [&](bool* ok) -> double {
        try { return std::stod (val.toStdString()); }
        catch (...) { if (ok) *ok = false; return 0.0; }
    };
    auto toBool = [&](bool* ok) -> bool {
        auto l = val.toLowerCase();
        if (l == "true" || l == "on" || l == "1" || l == "yes") { if (ok) *ok = true; return true; }
        if (l == "false" || l == "off" || l == "0" || l == "no") { if (ok) *ok = true; return false; }
        if (ok) *ok = false;
        return false;
    };

    // fft.*
    if      (key == "fft.fftOrder")          { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); fftOrder=v; return true; }
    if      (key == "fft.fftOrderLo")        { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); fftOrderLo=v; return true; }
    if      (key == "fft.enableLowFreqPath") { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); enableLowFreqPath=v; return true; }
    if      (key == "fft.hopSize")           { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); hopSize=v; return true; }
    if      (key == "fft.hopSizeLo")         { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); hopSizeLo=v; return true; }
    if      (key == "fft.crossoverHz")       { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); crossoverHz=v; return true; }
    if      (key == "fft.windowFunc")        { bool ok; auto v=parseWindowFunc(val, &ok); if(!ok) return setErr("invalid windowFunc"); windowFunc=v; return true; }
    // freq.*
    if      (key == "freq.minHz")            { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); minHz=v; return true; }
    if      (key == "freq.maxHz")            { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maxHz=v; return true; }
    if      (key == "freq.bandCount")        { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); bandCount=v; return true; }
    if      (key == "freq.freqScale")        { bool ok; auto v=parseFreqScale(val, &ok); if(!ok) return setErr("invalid freqScale"); freqScale=v; return true; }
    // time.*
    if      (key == "time.fps")              { bool ok=true; double v=toDouble(&ok); if(!ok) return setErr("invalid double"); fps=v; return true; }
    if      (key == "time.attackMs")         { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); attackMs=v; return true; }
    if      (key == "time.releaseMs")        { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); releaseMs=v; return true; }
    if      (key == "time.peakHoldMs")       { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); peakHoldMs=v; return true; }
    if      (key == "time.peakDecayDbPerSec"){ bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); peakDecayDbPerSec=v; return true; }
    if      (key == "time.peakDecayAccelDbPerSec2"){ bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); peakDecayAccelDbPerSec2=v; return true; }
    if      (key == "time.temporalSmoothing"){ bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); temporalSmoothing=v; return true; }
    // dynamic.*
    if      (key == "dynamic.curve")         { bool ok; auto v=parseDynCurve(val, &ok); if(!ok) return setErr("invalid dynCurve"); dynCurve=v; return true; }
    if      (key == "dynamic.gain")          { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); dynGain=v; return true; }
    if      (key == "dynamic.gamma")         { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); dynGamma=v; return true; }
    if      (key == "dynamic.slopeEnabled")  { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); slopeEnabled=v; return true; }
    if      (key == "dynamic.slopeDbPerOct") { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); slopeDbPerOct=v; return true; }
    if      (key == "dynamic.minDb")         { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); minDb=v; return true; }
    if      (key == "dynamic.maxDb")         { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maxDb=v; return true; }
    // visual.*
    if      (key == "visual.style")          { style = val; return true; }
    if      (key == "visual.colorMap")       { colorMap = val; return true; }
    if      (key == "visual.lineWidth")      { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); lineWidth=v; return true; }
    if      (key == "visual.opacity")        { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); opacity=v; return true; }
    if      (key == "visual.barGapRatio")    { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); barGapRatio=v; return true; }
    if      (key == "visual.barWidthRatio")  { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); barWidthRatio=v; return true; }
    if      (key == "visual.barParticles")   { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); barParticles=v; return true; }
    if      (key == "visual.drawGrid")       { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); drawGrid=v; return true; }
    if      (key == "visual.drawAxisLabels") { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); drawAxisLabels=v; return true; }
    if      (key == "visual.primaryColor")   { bool ok; auto c=parseColour(val, &ok); if(!ok) return setErr("invalid color (#rrggbb / #aarrggbb)"); primaryColor=c; return true; }
    if      (key == "visual.secondaryColor") { bool ok; auto c=parseColour(val, &ok); if(!ok) return setErr("invalid color"); secondaryColor=c; return true; }
    if      (key == "visual.peakColor")      { bool ok; auto c=parseColour(val, &ok); if(!ok) return setErr("invalid color"); peakColor=c; return true; }
    if      (key == "visual.bgColor")        { bool ok; auto c=parseColour(val, &ok); if(!ok) return setErr("invalid color"); bgColor=c; return true; }
    // transform.*
    if      (key == "transform.set")         { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); transform.set=v; return true; }
    if      (key == "transform.centerX")     { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); transform.centerX=v; return true; }
    if      (key == "transform.centerY")     { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); transform.centerY=v; return true; }
    if      (key == "transform.scaleX")      { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); transform.scaleX=v; return true; }
    if      (key == "transform.scaleY")      { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); transform.scaleY=v; return true; }
    if      (key == "transform.rotationDeg") { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); transform.rotationDeg=v; return true; }
    if      (key == "transform.posX")        { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); transform.posX=v; transform.set=true; return true; }
    if      (key == "transform.posY")        { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); transform.posY=v; transform.set=true; return true; }
    // output.*
    if      (key == "output.width")          { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); width=v; return true; }
    if      (key == "output.height")         { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); height=v; return true; }
    if      (key == "output.digits")         { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); digits=v; return true; }
    if      (key == "output.baseName")       { baseName = val; return true; }
    if      (key == "output.outputDir")      { outputDir = val; return true; }
    if      (key == "output.outputVideoPath"){ outputVideoPath = val; return true; }
    if      (key == "output.ffmpegPath")     { ffmpegPath = val; return true; }
    if      (key == "output.bgCheckerboardPreview") { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); bgCheckerboardPreview=v; return true; }
    if      (key == "output.encoder")        { bool ok; auto v=parseEncoder(val, &ok); if(!ok) return setErr("invalid encoder"); encoder=v; return true; }
    // audio.*
    if      (key == "audio.path")            { audioPath = val; return true; }
    // spectrum.*
    if      (key == "spectrum.present")      { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); spectrumPresent=v; return true; }
    if      (key == "spectrum.index")        { bool ok=true; int v=toInt(&ok);  if(!ok) return setErr("invalid int"); spectrumIndex=juce::jmax(0, v); return true; }
    if      (key == "spectrum.snapEnabled")  { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); snapEnabled=v; return true; }
    // mask.*（频谱蒙版图片，v0.5.4）
    if      (key == "mask.enabled")          { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); maskImage.enabled=v; return true; }
    if      (key == "mask.path")             { maskImage.path=val; if (!val.isEmpty()) maskImage.enabled=true; return true; }
    if      (key == "mask.strokeEnabled")    { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); maskImage.strokeEnabled=v; return true; }
    if      (key == "mask.strokeWidth")      { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.strokeWidth=v; return true; }
    if      (key == "mask.strokeAutoColor")  { bool ok=true; bool v=toBool(&ok); if(!ok) return setErr("invalid bool"); maskImage.strokeAutoColor=v; return true; }
    if      (key == "mask.strokeColor")      { bool ok; auto c=parseColour(val, &ok); if(!ok) return setErr("invalid color"); maskImage.strokeColor=c; maskImage.strokeAutoColor=false; return true; }
    // mask.brightness / contrast / saturation（v0.5.4 #4）：0..2，1.0=原图
    if      (key == "mask.brightness")       { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.brightness=juce::jlimit(0.0f,2.0f,v); return true; }
    if      (key == "mask.contrast")         { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.contrast=juce::jlimit(0.0f,2.0f,v); return true; }
    if      (key == "mask.saturation")       { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.saturation=juce::jlimit(0.0f,2.0f,v); return true; }
    // 蒙版图片几何（设值即视为已编辑 → set=true；未设则铺满画框，与电平无关）
    if      (key == "mask.reset")            { maskImage.transform=VisTransform{}; return true; }
    if      (key == "mask.centerX")          { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.transform.centerX=v; maskImage.transform.set=true; return true; }
    if      (key == "mask.centerY")          { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.transform.centerY=v; maskImage.transform.set=true; return true; }
    if      (key == "mask.scaleX")           { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.transform.scaleX=v; maskImage.transform.set=true; return true; }
    if      (key == "mask.scaleY")           { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.transform.scaleY=v; maskImage.transform.set=true; return true; }
    if      (key == "mask.rotationDeg")      { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.transform.rotationDeg=v; maskImage.transform.set=true; return true; }
    if      (key == "mask.posX")             { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.transform.posX=v; maskImage.transform.set=true; return true; }
    if      (key == "mask.posY")             { bool ok=true; float v=toFloat(&ok); if(!ok) return setErr("invalid float"); maskImage.transform.posY=v; maskImage.transform.set=true; return true; }

    errorMessage = "unknown key: " + key;
    return false;
}
