// =============================================================================
// SpectrumCore.h — 频谱分析引擎
//
// 设计意图：
//   · 输入：交错立体声 PCM（LRLR...），来自 PcmSource::readInterleavedStereo。
//   · 输出：BandFrame（bandCount 个带的 db / peakDb / normalized）。
//   · 内部：双路 FFT（主路 2048 + 低频路 8192，500Hz 交叉）+ 窗 + 频带映射
//           + 时间平滑（attack/release）+ 峰值保持 + Slope 补偿 + 动态曲线。
//
// 算法复用：直接照搬 Y2Kmeter AnalyserHub.cpp + SpectrumModule.cpp 的核心逻辑，
//           剥离 UI / FrameListener / Timer / SpinLock，改为离线纯函数式调用。
//           帧率无关：平滑用 alpha = 1 - exp(-dt/tau)，由 attackMs/releaseMs 推。
//
// 调用流程（离线导出）：
//   SpectrumCore core(params);
//   core.setSampleRate(sr);                 // 用音频真实采样率
//   for (each frame i):
//       pcm.read(i*spf, spf, buf);            // 读 PCM
//       core.pushInterleavedStereo(buf, spf); // 喂数据（内部攒满即跑 FFT）
//       core.advanceTime(1.0 / fps);          // 记录时间步长（供峰值衰减用）
//       core.getBandFrame(bandFrame);         // 取帧（dB+slope+平滑+峰值+动态曲线）
//       style.render(g, canvas, bandFrame, rp);
// =============================================================================
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <memory>
#include "SpectrumParams.h"
#include "BandFrame.h"
#include "FreqMap.h"

class SpectrumCore
{
public:
    explicit SpectrumCore (const SpectrumParams& params);
    ~SpectrumCore();

    SpectrumCore (const SpectrumCore&) = delete;
    SpectrumCore& operator= (const SpectrumCore&) = delete;

    // === 采样率 ===
    // 必须在第一次 getBandFrame 前设置为音频真实采样率（影响 Hz→bin 映射）。
    // 默认 44100；不影响频带几何（几何由 FreqMap 按 minHz/maxHz 算，与 SR 无关）。
    void setSampleRate (double sr) noexcept { sampleRate_ = sr; }
    double getSampleRate() const noexcept { return sampleRate_; }

    // === 喂数据 ===
    // 交错立体声 LRLR...；内部 mid=(L+R)/2 后送双路 FFT FIFO。
    // numFrames 是"立体声对数"（与 PcmSource::readInterleavedStereo 一致）
    // 内部逐样本：主路 FIFO 攒满即跑 FFT（非重叠）；低频路环形 + 75% overlap。
    void pushInterleavedStereo (const float* interleavedLr, int numFrames);

    // === 时间推进 ===
    // 每输出帧调一次。deltaSec 缓存供 getBandFrame 的峰值衰减 + 平滑时间常数用。
    // 离线导出时 deltaSec = 1.0/fps；实时预览时用 wall-clock delta。
    void advanceTime (double deltaSec);

    // === 峰值冻结（v0.5.0）===
    // GUI 暂停时置 true：getBandFrame 不再推进峰值 hold 倒计时和衰减（防止
    // 暂停期间峰值帽继续下落）；新峰跟随（smoothedDb > peak → 更新）仍生效，
    // 不影响 seek 快进时峰值建立。离线导出不受影响（默认 false）。
    void setPeaksFrozen (bool frozen) noexcept { peaksFrozen_ = frozen; }
    bool arePeaksFrozen() const noexcept { return peaksFrozen_; }

    // === 取帧 ===
    // 输出 bandCount 个带的：dB（平滑后）、peakDb（峰值保持）、normalized（动态曲线后 [0,1]）
    // 非常量：内部更新 smoothedDb_ / peakDb_ / peakHoldRemainMs_。
    void getBandFrame (BandFrame& out);

    // === 生命周期 ===
    void reset();   // 清空 FIFO / 平滑状态 / 峰值
    const SpectrumParams& params() const noexcept { return params_; }

    // === 调试 ===
    // 取最近一次 FFT 的原始幅度（线性，归一化后），主路 magSize = fftSize/2。
    // 仅供 --probe-spectrum 调试用，正常渲染不调用。
    void getRawMagnitudesHi (std::vector<float>& out) const;
    void getRawMagnitudesLo (std::vector<float>& out) const;

private:
    const SpectrumParams params_;       // 参数副本

    // ---- FFT 引擎 ----
    std::unique_ptr<juce::dsp::FFT> fft_;        // fftOrder=11 → 2048
    std::unique_ptr<juce::dsp::FFT> fftLo_;     // fftOrderLo=13 → 8192（可选）
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window_;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> windowLo_;

    // ---- FFT scratch（JUCE performFrequencyOnlyForwardTransform 需 2*N 缓冲）----
    std::vector<float> fftBufHi_;       // size = 2 * fftSize
    std::vector<float> fftBufLo_;       // size = 2 * fftSizeLo

    // ---- FIFO（主路线性攒满即跑；低频路环形 + hop overlap）----
    std::vector<float> fifoHi_;       // size = fftSize
    int fifoIdxHi_ = 0;

    std::vector<float> fifoLo_;       // size = fftSizeLo，环形
    int fifoIdxLo_ = 0;
    int hopLo_ = 0;

    // ---- 最新 FFT 幅度快照（线性，归一化系数 2.0/fftSize）----
    std::vector<float> magHi_;
    std::vector<float> magLo_;

    // ---- 预计算：每带的几何带边界 + 中心频率 + A-weighting + Slope 偏移 ----
    struct BandGeometry {
        float f0, f, f1;       // 边界 + 中心
        float aWeightDb;       // A-weighting 补偿 dB（perceptual 模式用）
        float slopeDb;         // Slope 补偿 dB（4.5 * log2(f/1000)）
    };
    std::vector<BandGeometry> bands_;
    FreqMap freqMap_;
    double sampleRate_ = 44100.0;
    double deltaSec_ = 1.0 / 30.0;   // 最近一次 advanceTime 的时间步长
    bool peaksFrozen_ = false;       // 暂停冻结峰值衰减（GUI 暂停时置 true）

    // ---- 平滑/峰值状态（每带一份）----
    std::vector<float> smoothedDb_;       // attack/release 后的 dB
    std::vector<float> peakDb_;           // 峰值保持 dB
    std::vector<float> peakHoldRemainMs_; // 滞留剩余 ms（已滞留时长）
    mutable std::vector<float> bandLinScratch_;   // 临时：每带线性幅度（band mapping 输出）
    std::vector<float> blurScratch_;              // 临时：列间模糊 [1,2,1]/4

    // ---- 内部方法 ----
    void rebuildBandGeometry_();
    void runFftHi_();              // 主路：FIFO 满 → 窗 + FFT + 归一化 → magHi_
    void runFftLo_();              // 低频路：环形展平 → 窗 + FFT + 归一化 → magLo_
    void computeBandMagnitudes_ (std::vector<float>& outLinear) const;  // Y2K getSpectrumMagnitudesBlended 等价
    void applyDynCurve_ (const std::vector<float>& db, std::vector<float>& outNorm) const;
    float computeAWeightDb_ (float hz) const noexcept;   // ISO A-curve
};
