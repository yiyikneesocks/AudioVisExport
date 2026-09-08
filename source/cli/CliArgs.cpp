// =============================================================================
// CliArgs.cpp — CLI 参数解析实现
// =============================================================================
#include "CliArgs.h"
#include <iostream>

// -----------------------------------------------------------------------------
// 帮助文本
// -----------------------------------------------------------------------------
juce::String CliArgs::helpText()
{
    // ⚠️ 必须用 CharPointer_UTF8 包装：juce::String(const char*) 按 ASCII 处理
    // （>127 的 UTF-8 多字节会被逐字节重编码成双重编码乱码，Release 下 jassert 被禁用无提示）
    return juce::String (juce::CharPointer_UTF8 (R"(
AudioVisExport v0.5.3 — 透明背景频谱图视频生成器

用法:
  AudioVisExport --export <audio.wav> <out_dir|out_video> [OPTIONS]
  AudioVisExport --preview-frame <out.png> [OPTIONS]
  AudioVisExport --probe-spectrum <audio.wav> <frameIdx> [OPTIONS]
  AudioVisExport --gen-tone <out.wav>
  AudioVisExport --probe-pcm <in.wav>
  AudioVisExport --help

模式:
  --export <audio> <out>          导出完整频谱图视频（PNG 序列 / WebM / MOV）
  --preview-frame <out.png>       单帧预览（dump 第 N 帧为 PNG，配合 --frame-index）
  --probe-spectrum <audio> <idx>  数值调试（dump 第 N 帧 BandFrame 前 16 带 dB）
  --gen-tone <out.wav>            生成 5 秒 1kHz -6dBFS 测试 wav
  --probe-pcm <in.wav>            打印音频元数据

参数:
  --config <path>                 从 JSON 加载参数模板
  --set key=value                 任意点路径覆盖（可多次，例 --set fft.fftOrder=12）

常用糖（等价于 --set）:
  --audio <path>                  audio.path
  --width N / --height N          output.width / output.height
  --fps N                         time.fps
  --style <y2k-line|bar|polyline> visual.style
  --color-map <solid|gradient|rainbow>  visual.colorMap
  --primary-color #rrggbb         visual.primaryColor
  --secondary-color #rrggbb       visual.secondaryColor
  --peak-color #rrggbb            visual.peakColor
  --bg-color #aarrggbb            visual.bgColor（默认 #00000000 全透明）
  --band-count N                  freq.bandCount
  --fft-order N                   fft.fftOrder
  --fft-order-lo N                fft.fftOrderLo
  --window <hann|hamming|blackman|blackman-harris|rectangular>  fft.windowFunc
  --freq-scale <log|linear|mel|bark>  freq.freqScale
  --min-hz F / --max-hz F         freq.minHz / freq.maxHz
  --attack-ms F / --release-ms F  time.attackMs / time.releaseMs
  --peak-hold-ms F                time.peakHoldMs
  --peak-decay-db-per-sec F       time.peakDecayDbPerSec
  --dyn-curve <linear|sqrt|loglog|perceptual>  dynamic.curve
  --dyn-gain F / --dyn-gamma F    dynamic.gain / dynamic.gamma
  --slope <on|off>                dynamic.slopeEnabled
  --line-width F / --opacity F    visual.lineWidth / visual.opacity
  --draw-grid <on|off>            visual.drawGrid
  --draw-axis-labels <on|off>     visual.drawAxisLabels
  --encoder <png-seq|webm-vp9|mov-qtrle>  output.encoder
  --output-dir <dir>              output.outputDir
  --base-name <str>               output.baseName
  --digits N                      output.digits
  --ffmpeg <path>                 output.ffmpegPath
  --frame-index N                 配合 --preview-frame 选择第 N 帧

示例:
  AudioVisExport --gen-tone test_tone.wav
  AudioVisExport --preview-frame out.png --audio test_tone.wav --frame-index 100
  AudioVisExport --export test_tone.wav out_frames --style y2k-line --bg-color #00000000
  AudioVisExport --export song.wav out.webm --encoder webm-vp9 --fps 30
)"));
}

// -----------------------------------------------------------------------------
// 工具：把糖 flag 转成点路径 key
// -----------------------------------------------------------------------------
static juce::String sugarToKey (const juce::String& flag)
{
    auto f = flag.toLowerCase().trim();
    if (f == "--audio")                  return "audio.path";
    if (f == "--width")                  return "output.width";
    if (f == "--height")                 return "output.height";
    if (f == "--fps")                    return "time.fps";
    if (f == "--style")                  return "visual.style";
    if (f == "--color-map" || f == "--colormap") return "visual.colorMap";
    if (f == "--primary-color" || f == "--primarycolor")    return "visual.primaryColor";
    if (f == "--secondary-color" || f == "--secondarycolor")return "visual.secondaryColor";
    if (f == "--peak-color" || f == "--peakcolor")          return "visual.peakColor";
    if (f == "--bg-color" || f == "--bgcolor")              return "visual.bgColor";
    if (f == "--band-count" || f == "--bandcount")          return "freq.bandCount";
    if (f == "--fft-order" || f == "--fftorder")            return "fft.fftOrder";
    if (f == "--fft-order-lo" || f == "--fftorderlo")       return "fft.fftOrderLo";
    if (f == "--window")                 return "fft.windowFunc";
    if (f == "--freq-scale" || f == "--freqscale")         return "freq.freqScale";
    if (f == "--min-hz" || f == "--minhz")                 return "freq.minHz";
    if (f == "--max-hz" || f == "--maxhz")                 return "freq.maxHz";
    if (f == "--attack-ms" || f == "--attackms")           return "time.attackMs";
    if (f == "--release-ms" || f == "--releasems")         return "time.releaseMs";
    if (f == "--peak-hold-ms" || f == "--peakholems" || f == "--peakheldms") return "time.peakHoldMs";
    if (f == "--peak-decay-db-per-sec")  return "time.peakDecayDbPerSec";
    if (f == "--dyn-curve" || f == "--dyncurve")           return "dynamic.curve";
    if (f == "--dyn-gain" || f == "--dyngain")             return "dynamic.gain";
    if (f == "--dyn-gamma" || f == "--dyncurve")           return "dynamic.gamma";
    if (f == "--slope")                  return "dynamic.slopeEnabled";
    if (f == "--line-width" || f == "--linewidth")         return "visual.lineWidth";
    if (f == "--opacity")                return "visual.opacity";
    if (f == "--draw-grid" || f == "--drawgrid")           return "visual.drawGrid";
    if (f == "--draw-axis-labels" || f == "--drawaxislabels") return "visual.drawAxisLabels";
    if (f == "--encoder")                return "output.encoder";
    if (f == "--output-dir" || f == "--outputdir")         return "output.outputDir";
    if (f == "--base-name" || f == "--basename")           return "output.baseName";
    if (f == "--digits")                 return "output.digits";
    if (f == "--ffmpeg")                 return "output.ffmpegPath";
    return {};
}

// -----------------------------------------------------------------------------
// parse：扫描 argv 填充本结构
// -----------------------------------------------------------------------------
bool CliArgs::parse (int argc, char** argv)
{
    errorMessage.clear();

    // argv[0] = exe 路径，从 1 开始
    auto takeValue = [&](int& i) -> juce::String {
        if (i + 1 >= argc) {
            errorMessage = "missing value for " + juce::String (argv[i]);
            return {};
        }
        return juce::String::fromUTF8 (argv[++i]);
    };

    for (int i = 1; i < argc; ++i) {
        juce::String arg = juce::String::fromUTF8 (argv[i]);
        auto lower = arg.toLowerCase().trim();

        if (lower == "--help" || lower == "-h") {
            mode = Mode::Help;
            return true;
        }
        if (lower == "--gen-tone") {
            mode = Mode::GenTone;
            genToneOut = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            continue;
        }
        if (lower == "--probe-pcm") {
            mode = Mode::ProbePcm;
            probePcmIn = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            continue;
        }
        if (lower == "--probe-spectrum") {
            mode = Mode::ProbeSpectrum;
            probeSpectrumAudio = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            auto idxStr = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            try { probeSpectrumFrameIdx = std::stoi (idxStr.toStdString()); }
            catch (...) { errorMessage = "invalid frame index: " + idxStr; return false; }
            continue;
        }
        if (lower == "--preview-frame") {
            mode = Mode::PreviewFrame;
            previewFrameOut = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            continue;
        }
        if (lower == "--export") {
            mode = Mode::Export;
            // --export <audio> <out>
            params.audioPath = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            // out 参数：根据 encoder 决定是目录还是文件路径
            // 这里先存到 outputDir，applyToParams 后再判断
            juce::String outVal = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            // 若看起来像文件路径（有扩展名），存到 outputVideoPath
            if (outVal.containsAnyOf (".") && ! outVal.endsWith (".") && ! outVal.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-\\")) {
                params.outputVideoPath = outVal;
            } else {
                params.outputDir = outVal;
            }
            continue;
        }
        if (lower == "--config") {
            configPath = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            continue;
        }
        if (lower == "--set") {
            auto kv = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            setOverrides.add (kv);
            continue;
        }
        if (lower == "--frame-index") {
            auto idxStr = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            try { frameIndex = std::stoi (idxStr.toStdString()); }
            catch (...) { errorMessage = "invalid frame index: " + idxStr; return false; }
            continue;
        }
        // 糖 flag
        auto key = sugarToKey (arg);
        if (key.isNotEmpty()) {
            auto val = takeValue (i);
            if (errorMessage.isNotEmpty()) return false;
            sugarOverrides.add (key + "=" + val);
            continue;
        }

        errorMessage = "unknown argument: " + arg;
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// applyToParams：--config → --set → 糖
// -----------------------------------------------------------------------------
bool CliArgs::applyToParams()
{
    errorMessage.clear();

    // 1) --config JSON
    if (configPath.isNotEmpty()) {
        juce::File cfgFile (configPath);
        if (! cfgFile.existsAsFile()) {
            errorMessage = "config file not found: " + configPath;
            return false;
        }
        auto jsonText = cfgFile.loadFileAsString();
        juce::String jsonErr;
        params = SpectrumParams::fromJson (jsonText, jsonErr);
        if (jsonErr.isNotEmpty()) {
            errorMessage = "config JSON parse: " + jsonErr;
            return false;
        }
    }

    // 2) --set key=value
    for (const auto& kv : setOverrides) {
        auto eq = kv.indexOfChar ('=');
        if (eq < 0) {
            errorMessage = "invalid --set (missing =): " + kv;
            return false;
        }
        auto key = kv.substring (0, eq).trim();
        auto val = kv.substring (eq + 1).trim();
        juce::String err;
        if (! params.applyOverride (key, val, err)) {
            errorMessage = "--set " + key + " failed: " + err;
            return false;
        }
    }

    // 3) 糖 flag（已转成 key=value）
    for (const auto& kv : sugarOverrides) {
        auto eq = kv.indexOfChar ('=');
        if (eq < 0) continue;
        auto key = kv.substring (0, eq).trim();
        auto val = kv.substring (eq + 1).trim();
        juce::String err;
        if (! params.applyOverride (key, val, err)) {
            errorMessage = "sugar " + key + " failed: " + err;
            return false;
        }
    }

    return true;
}
