// =============================================================================
// main.cpp — AudioVisExport v2 CLI 入口
//
// 子命令：
//   (无参数 / --help)   → 打印帮助
//   --gen-tone <path>   → 生成 5 秒 1kHz -6dBFS 测试 wav
//   --probe-pcm <path>  → 打印音频元数据 + 前 N samples RMS/peak 校验
//   --probe-spectrum <audio> <frameIdx> → dump BandFrame 前 16 带 dB（数值调试）
//   --preview-frame <out.png>           → 单帧预览（dump 第 N 帧为透明 PNG）
//   --export <audio> <out>              → 完整导出（PNG 序列 / WebM / MOV）
//
// 参数系统：--config <json> + --set key=value + 糖 flag（--width / --fps / --style ...）
// =============================================================================
#include <juce_audio_utils/juce_audio_utils.h>
#include "core/PcmSource.h"
#include "core/VisPipeline.h"
#include "cli/CliArgs.h"
#include "core/CrashReporter.h"

#include <cmath>
#include <iostream>

// =============================================================================
// --gen-tone <path>
//   生成 44.1kHz 立体声 5 秒 1kHz 正弦波，峰值 -6dBFS (≈0.5)
// =============================================================================
static int cmdGenTone (const juce::String& outPath)
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kChannels      = 2;
    constexpr double kDuration   = 5.0;
    constexpr double kToneFreq   = 1000.0;
    constexpr float  kAmplitude  = 0.5f;

    const int64_t totalSamples = static_cast<int64_t> (kSampleRate * kDuration);

    juce::File outFile (outPath);
    outFile.deleteFile();
    auto ostream = outFile.createOutputStream();
    if (ostream == nullptr) {
        std::cout << "[FAIL] Cannot open for write: " << outPath << "\n";
        return 1;
    }

    juce::WavAudioFormat wavFmt;
    auto* writer = wavFmt.createWriterFor (ostream.release(),
                                           kSampleRate, kChannels, 16, {}, 0);
    if (writer == nullptr) {
        std::cout << "[FAIL] WavAudioFormat::createWriterFor failed\n";
        return 1;
    }

    constexpr int kBlock = 65536;
    juce::AudioBuffer<float> buf (kChannels, kBlock);
    int64_t written = 0;
    const double twoPiFOverSr = 2.0 * juce::MathConstants<double>::pi * kToneFreq / kSampleRate;

    std::cout << "Generating tone -> " << outFile.getFullPathName() << "\n";
    std::cout << "  sampleRate=" << kSampleRate << " channels=" << kChannels
              << " duration=" << kDuration << "s freq=" << kToneFreq << "Hz amp=" << kAmplitude << "\n";

    while (written < totalSamples) {
        const int n = static_cast<int> (std::min<int64_t> (kBlock, totalSamples - written));
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getWritePointer (1);
        for (int i = 0; i < n; ++i) {
            const float s = kAmplitude * static_cast<float> (std::sin (twoPiFOverSr * (written + i)));
            L[i] = s;
            R[i] = s;
        }
        if (! writer->writeFromAudioSampleBuffer (buf, 0, n)) {
            std::cout << "[FAIL] writeFromAudioSampleBuffer failed at offset " << written << "\n";
            delete writer;
            return 1;
        }
        written += n;
    }

    delete writer;
    std::cout << "[OK] wrote " << written << " samples, file size="
              << outFile.getSize() << " bytes\n";
    return 0;
}

// =============================================================================
// --probe-pcm <path>
//   打印 wav/aiff 元数据 + 前 N samples 的 RMS/peak 校验
// =============================================================================
static int cmdProbePcm (const juce::String& path)
{
    std::cout << "=== Probe PCM: " << path << " ===\n";

    PcmSource pcm;
    if (! pcm.load (path)) {
        std::cout << "[FAIL] PcmSource::load() returned false\n";
        std::cout << "  Possible reasons: file missing, corrupt, or not wav/aiff\n";
        return 1;
    }

    std::cout << "[OK] opened\n";
    std::cout << "  sampleRate  = " << pcm.getSampleRate() << " Hz\n";
    std::cout << "  channels    = " << pcm.getNumChannels() << " (file's native)\n";
    std::cout << "  numSamples  = " << pcm.getNumSamples() << "\n";
    std::cout << "  duration    = " << pcm.getDurationSeconds() << " s\n";

    constexpr int kProbeSamples = 8192;
    std::vector<float> interleaved (static_cast<size_t> (kProbeSamples) * 2, 0.0f);
    const int got = pcm.readInterleavedStereo (0, kProbeSamples, interleaved.data());
    std::cout << "\n--- first block read start=0 request=" << kProbeSamples
              << " got=" << got << " stereo pairs ---\n";
    if (got <= 0) {
        std::cout << "[FAIL] readInterleavedStereo returned 0\n";
        return 1;
    }

    double sumSq = 0.0;
    float peak = 0.0f;
    const int totalFloats = got * 2;
    for (int i = 0; i < totalFloats; ++i) {
        const float s = interleaved[i];
        const float a = std::abs (s);
        if (a > peak) peak = a;
        sumSq += static_cast<double> (s) * static_cast<double> (s);
    }
    const double rms = std::sqrt (sumSq / std::max (1, totalFloats));

    std::cout << "  peak sample = " << peak << " (≈ "
              << (peak > 0.0f ? 20.0 * std::log10 (peak) : -999.0) << " dBFS)\n";
    std::cout << "  RMS         = " << rms << " (≈ "
              << (rms  > 0.0  ? 20.0 * std::log10 (rms)  : -999.0) << " dBFS)\n";
    std::cout << "  (for reference: 1kHz -6dBFS sine → peak≈0.5, RMS≈0.3535)\n";

    std::cout << "\n=== Probe finished ===\n";
    return 0;
}

// =============================================================================
// main
// =============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    CrashReporter::install();   // v0.5.4 #8：Windows 崩溃报告

    CliArgs args;
    if (! args.parse (argc, argv)) {
        std::cerr << "[FAIL] CLI parse: " << args.errorMessage << "\n";
        std::cerr << CliArgs::helpText() << "\n";
        return 1;
    }

    // Help
    if (args.mode == CliArgs::Mode::Help) {
        std::cout << CliArgs::helpText();
        return 0;
    }

    // gen-tone / probe-pcm 不需要参数系统
    if (args.mode == CliArgs::Mode::GenTone) {
        return cmdGenTone (args.genToneOut);
    }
    if (args.mode == CliArgs::Mode::ProbePcm) {
        return cmdProbePcm (args.probePcmIn);
    }

    // 其余模式需要参数系统
    if (! args.applyToParams()) {
        std::cerr << "[FAIL] params: " << args.errorMessage << "\n";
        return 1;
    }

    // 把模式的位置参数音频路径接入 params（--audio 糖 flag 优先：若已设置则不覆盖）
    if (args.mode == CliArgs::Mode::ProbeSpectrum && args.params.audioPath.isEmpty())
        args.params.audioPath = args.probeSpectrumAudio;

    // 打印解析后的参数（Step 1 验证用）
    std::cout << "=== SpectrumParams ===\n";
    const auto& p = args.params;
    std::cout << "  audio:       " << p.audioPath << "\n";
    std::cout << "  style:       " << p.style << "\n";
    std::cout << "  resolution:  " << p.width << "x" << p.height << "\n";
    std::cout << "  fps:         " << p.fps << "\n";
    std::cout << "  encoder:     " << SpectrumParams::encoderName (p.encoder) << "\n";
    std::cout << "  fftSize:     " << p.fftSize() << " (order " << p.fftOrder << ")\n";
    if (p.enableLowFreqPath)
        std::cout << "  fftSizeLo:   " << p.fftSizeLo() << " (order " << p.fftOrderLo << ") xover=" << p.crossoverHz << "Hz\n";
    std::cout << "  bandCount:   " << p.bandCount << " (" << SpectrumParams::freqScaleName (p.freqScale) << " " << p.minHz << "-" << p.maxHz << "Hz)\n";
    std::cout << "  window:      " << SpectrumParams::windowFuncName (p.windowFunc) << "\n";
    std::cout << "  dynCurve:    " << SpectrumParams::dynCurveName (p.dynCurve) << " gain=" << p.dynGain << " gamma=" << p.dynGamma << "\n";
    std::cout << "  colors:      primary=" << juce::String::formatted("#%08x", p.primaryColor.getARGB())
              << " bg=" << juce::String::formatted("#%08x", p.bgColor.getARGB()) << "\n";
    std::cout << "  outputDir:   " << p.outputDir << "\n\n";

    VisPipeline pipe;
    VisPipeline::Config cfg;
    cfg.params = p;

    if (args.mode == CliArgs::Mode::ProbeSpectrum) {
        auto result = pipe.probeSpectrum (cfg, args.probeSpectrumFrameIdx);
        std::cout << "\n--- Result ---\n";
        const auto keys = result.getAllKeys();
        for (int k = 0; k < keys.size(); ++k)
            std::cout << "  " << keys[k] << " = " << result[keys[k]] << "\n";
        return result["ok"] == "true" ? 0 : 1;
    }

    if (args.mode == CliArgs::Mode::PreviewFrame) {
        std::cout << "=== Preview Frame ===\n";
        std::cout << "  frame index: " << args.frameIndex << "\n";
        std::cout << "  output:      " << args.previewFrameOut << "\n";
        if (p.bgCheckerboardPreview)
            std::cout << "  (checkerboard preview enabled)\n";
        std::cout << "\n";
        auto result = pipe.previewFrame (cfg, args.frameIndex, args.previewFrameOut);
        std::cout << "\n--- Result ---\n";
        const auto keys = result.getAllKeys();
        for (int k = 0; k < keys.size(); ++k)
            std::cout << "  " << keys[k] << " = " << result[keys[k]] << "\n";
        if (result["ok"] == "true") {
            std::cout << "\n[OK] Preview frame written\n";
            return 0;
        }
        std::cerr << "\n[FAIL] " << result["error"] << "\n";
        return 1;
    }

    if (args.mode == CliArgs::Mode::Export) {
        std::cout << "=== Export ===\n";
        std::cout << "  audio:       " << p.audioPath << "\n";
        std::cout << "  output:      " << (p.outputVideoPath.isNotEmpty() ? p.outputVideoPath : p.outputDir) << "\n\n";

        std::cout.setf (std::ios::unitbuf);
        auto cb = [&](int done, int total, double elapsedSec) {
            if (total <= 0) return;
            const int pct = static_cast<int> (100.0 * done / total);
            juce::String eta;
            if (done >= 2 && elapsedSec > 0.5) {
                const double remain = (static_cast<double>(total - done) * elapsedSec)
                                    / static_cast<double>(done);
                if (remain < 60)       eta = juce::String (remain, 1) + "s";
                else if (remain < 3600) eta = juce::String (remain / 60.0, 1) + "min";
                else                    eta = juce::String (remain / 3600.0, 1) + "h";
            } else {
                eta = "calculating...";
            }
            printf ("  progress: %3d%%  %d/%d frames  elapsed %.1fs  ETA %s\n",
                    pct, done, total, elapsedSec, eta.toRawUTF8());
        };

        auto result = pipe.run (cfg, cb);
        std::cout << "\n--- Result ---\n";
        const auto keys = result.getAllKeys();
        for (int k = 0; k < keys.size(); ++k)
            std::cout << "  " << keys[k] << " = " << result[keys[k]] << "\n";
        if (result["ok"] == "true") {
            std::cout << "\n[OK] Export complete!\n";
            return 0;
        }
        std::cerr << "\n[FAIL] " << result["error"] << "\n";
        return 1;
    }

    std::cerr << "[FAIL] unknown mode\n";
    return 1;
}
