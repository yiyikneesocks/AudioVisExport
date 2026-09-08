// =============================================================================
// SpectrumCore.cpp — 频谱分析引擎实现
//
// 算法照搬 Y2Kmeter：
//   · AnalyserHub.cpp::pushStereo        → pushInterleavedStereo（双路 FFT FIFO）
//   · AnalyserHub.cpp::getSpectrumMagnitudesBlended → computeBandMagnitudes_
//   · SpectrumModule.cpp::rebuildDisplay → getBandFrame 的 dB+slope+平滑+模糊
//   · SpectrumModule.cpp::onFrame 峰值段  → getBandFrame 的峰值保持
//
// 与 Y2K 差异：
//   · 帧率无关平滑：alpha = 1 - exp(-dt/tau)（Y2K 用固定 0.55/0.12 @30fps）
//   · 频带几何走 FreqMap（支持 log/linear/mel/bark），Y2K 只有 log
//   · 无 SpinLock / Timer / FrameListener，纯函数式
// =============================================================================
#include "SpectrumCore.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// -----------------------------------------------------------------------------
// A-weighting 曲线（ISO 226 近似，perceptual 模式用）
// -----------------------------------------------------------------------------
float SpectrumCore::computeAWeightDb_ (float hz) const noexcept
{
    const float f2 = hz * hz;
    const float f4 = f2 * f2;
    constexpr float k12200_2 = 12200.0f * 12200.0f;
    constexpr float k20_6_2  = 20.6f * 20.6f;
    constexpr float k107_7_2 = 107.7f * 107.7f;
    constexpr float k737_9_2 = 737.9f * 737.9f;
    const float num = k12200_2 * f4;
    const float den = (f2 + k20_6_2) * (f2 + k12200_2) * std::sqrt ((f2 + k107_7_2) * (f2 + k737_9_2));
    if (den < 1e-20f) return 0.0f;
    const float ra = num / den;
    if (ra < 1e-20f) return 0.0f;
    return 2.0f + 20.0f * std::log10 (ra);
}

// -----------------------------------------------------------------------------
// 构造：建 FFT / 窗 / FIFO / scratch / 频带几何
// -----------------------------------------------------------------------------
SpectrumCore::SpectrumCore (const SpectrumParams& params)
    : params_ (params)
    , sampleRate_ (44100.0)
{
    // FFT 引擎
    fft_ = std::make_unique<juce::dsp::FFT> (params_.fftOrder);
    if (params_.enableLowFreqPath)
        fftLo_ = std::make_unique<juce::dsp::FFT> (params_.fftOrderLo);

    // 窗函数（normalise=false，与 Y2K 一致；幅度归一靠 2/fftSize）
    auto makeWindow = [](int size, SpectrumParams::WindowFunc wf) -> std::unique_ptr<juce::dsp::WindowingFunction<float>> {
        using WF = juce::dsp::WindowingFunction<float>;
        auto toWf = [](SpectrumParams::WindowFunc v) -> typename WF::WindowingMethod {
            switch (v) {
                case SpectrumParams::Hann:            return WF::hann;
                case SpectrumParams::Hamming:         return WF::hamming;
                case SpectrumParams::Blackman:        return WF::blackman;
                case SpectrumParams::BlackmanHarris:  return WF::blackmanHarris;
                case SpectrumParams::Rectangular:     return WF::rectangular;
            }
            return WF::hann;
        };
        return std::make_unique<WF> (size, toWf (wf), false);
    };
    window_   = makeWindow (params_.fftSize(),   params_.windowFunc);
    if (fftLo_) windowLo_ = makeWindow (params_.fftSizeLo(), params_.windowFunc);

    // FIFO
    fifoHi_.assign (params_.fftSize(), 0.0f);
    if (fftLo_) {
        fifoLo_.assign (params_.fftSizeLo(), 0.0f);
        hopLo_ = (params_.hopSizeLo > 0) ? params_.hopSizeLo : (params_.fftSizeLo() / 4);  // 75% overlap
    } else {
        hopLo_ = 1;  // 不会被触发
    }

    // FFT scratch（JUCE performFrequencyOnlyForwardTransform 需 2*N 缓冲）
    fftBufHi_.assign (2 * params_.fftSize(), 0.0f);
    if (fftLo_) fftBufLo_.assign (2 * params_.fftSizeLo(), 0.0f);

    // 幅度快照
    magHi_.assign (params_.fftSize() / 2, 0.0f);
    if (fftLo_) magLo_.assign (params_.fftSizeLo() / 2, 0.0f);

    // 频带几何（含 slope / A-weight 预计算）
    rebuildBandGeometry_();

    // 平滑/峰值状态
    smoothedDb_.assign (params_.bandCount, params_.minDb);
    peakDb_.assign (params_.bandCount, params_.minDb);
    peakHoldRemainMs_.assign (params_.bandCount, 0.0f);
    peakVelDbPerSec_.assign (params_.bandCount, 0.0f);   // v0.5.3: 峰值帽下落速度
    bandLinScratch_.assign (params_.bandCount, 0.0f);
    blurScratch_.assign (params_.bandCount, params_.minDb);
}

SpectrumCore::~SpectrumCore() = default;

// -----------------------------------------------------------------------------
// 预计算频带几何 + Slope 偏移（4.5 dB/oct，基准 1kHz）
// -----------------------------------------------------------------------------
void SpectrumCore::rebuildBandGeometry_()
{
    freqMap_.configure (params_.freqScale, params_.minHz, params_.maxHz, params_.bandCount);
    bands_.resize (params_.bandCount);
    for (int i = 0; i < params_.bandCount; ++i) {
        float f0, f1;
        freqMap_.bandEdgesHz (i, f0, f1);
        bands_[i].f0 = f0;
        bands_[i].f1 = f1;
        bands_[i].f  = freqMap_.bandCenterHz (i);
        bands_[i].aWeightDb = computeAWeightDb_ (bands_[i].f);
        const double oct = std::log2 (juce::jmax (20.0, (double) bands_[i].f) / 1000.0);
        bands_[i].slopeDb = (float) (params_.slopeDbPerOct * oct);
    }
}

// -----------------------------------------------------------------------------
// reset
// -----------------------------------------------------------------------------
void SpectrumCore::reset()
{
    std::fill (fifoHi_.begin(), fifoHi_.end(), 0.0f);
    if (fftLo_) std::fill (fifoLo_.begin(), fifoLo_.end(), 0.0f);
    std::fill (magHi_.begin(), magHi_.end(), 0.0f);
    if (fftLo_) std::fill (magLo_.begin(), magLo_.end(), 0.0f);
    std::fill (smoothedDb_.begin(), smoothedDb_.end(), params_.minDb);
    std::fill (peakDb_.begin(), peakDb_.end(), params_.minDb);
    std::fill (peakHoldRemainMs_.begin(), peakHoldRemainMs_.end(), 0.0f);
    std::fill (peakVelDbPerSec_.begin(), peakVelDbPerSec_.end(), 0.0f);   // v0.5.3
    fifoIdxHi_ = 0;
    fifoIdxLo_ = 0;
}

// =============================================================================
// === 双路 FFT ===============================================================
// =============================================================================

// 主路：FIFO 攒满 → 窗 + FFT + 归一化（2/fftSize）→ magHi_
void SpectrumCore::runFftHi_()
{
    const int N = params_.fftSize();
    std::fill (fftBufHi_.begin(), fftBufHi_.end(), 0.0f);
    std::copy (fifoHi_.begin(), fifoHi_.end(), fftBufHi_.begin());
    window_->multiplyWithWindowingTable (fftBufHi_.data(), N);
    fft_->performFrequencyOnlyForwardTransform (fftBufHi_.data());

    const int maxBin = N / 2;
    const float invMax = 2.0f / static_cast<float> (N);
    for (int i = 0; i < maxBin; ++i)
        magHi_[i] = fftBufHi_[i] * invMax;
}

// 低频路：环形 FIFO 展平（oldest→newest）→ 窗 + FFT + 归一化 → magLo_
void SpectrumCore::runFftLo_()
{
    const int N = params_.fftSizeLo();
    const int w = fifoIdxLo_;  // 当前写位置 = 最老样本位置
    const int firstChunk = N - w;
    std::memcpy (fftBufLo_.data(), fifoLo_.data() + w,
                 static_cast<size_t> (firstChunk) * sizeof (float));
    if (w > 0)
        std::memcpy (fftBufLo_.data() + firstChunk, fifoLo_.data(),
                     static_cast<size_t> (w) * sizeof (float));
    std::fill (fftBufLo_.begin() + N, fftBufLo_.end(), 0.0f);  // 后半 scratch

    windowLo_->multiplyWithWindowingTable (fftBufLo_.data(), N);
    fftLo_->performFrequencyOnlyForwardTransform (fftBufLo_.data());

    const int maxBin = N / 2;
    const float invMax = 2.0f / static_cast<float> (N);
    for (int i = 0; i < maxBin; ++i)
        magLo_[i] = fftBufLo_[i] * invMax;
}

// =============================================================================
// === band mapping：双路 mag → bandCount 带 ==================================
//   照搬 AnalyserHub::getSpectrumMagnitudesBlended
//   · 每带几何边界 [f0,f1]（由 FreqMap 给，尊重 freqScale）
//   · 主路 / 低频路各取带宽内 max 幅度
//   · 过渡带 [xoverLo, xoverHi]（±半八度）做 cos²/sin² 等功率交叉淡化
//   · 能量域线性混合后开方
// =============================================================================
void SpectrumCore::computeBandMagnitudes_ (std::vector<float>& outLinear) const
{
    if (static_cast<int> (outLinear.size()) != params_.bandCount)
        outLinear.assign (params_.bandCount, 0.0f);

    const int N = params_.bandCount;
    if (N < 2) { std::fill (outLinear.begin(), outLinear.end(), 0.0f); return; }

    const double sr = (sampleRate_ > 0.0) ? sampleRate_ : 44100.0;
    const double nyquist = sr * 0.5;

    const int magHiN = static_cast<int> (magHi_.size());
    const int magLoN = static_cast<int> (magLo_.size());
    const double hzToBinHi = (magHiN > 1) ? static_cast<double> (magHiN - 1) / nyquist : 0.0;
    const double hzToBinLo = (magLoN > 1) ? static_cast<double> (magLoN - 1) / nyquist : 0.0;

    const double xover   = params_.crossoverHz;
    const double xoverLo = xover / std::sqrt (2.0);
    const double xoverHi = xover * std::sqrt (2.0);
    const double loHardMax = xover * 2.0;

    auto peakInRange = [] (const float* buf, int bufN, int binLo, int binHi) noexcept -> float {
        if (bufN <= 0) return 0.0f;
        binLo = juce::jlimit (0, bufN - 1, binLo);
        binHi = juce::jlimit (0, bufN - 1, binHi);
        if (binHi < binLo) std::swap (binLo, binHi);
        float m = 0.0f;
        for (int b = binLo; b <= binHi; ++b) {
            const float v = std::abs (buf[b]);
            if (std::isfinite (v) && v > m) m = v;
        }
        return m;
    };

    for (int i = 0; i < N; ++i)
    {
        const double f  = bands_[i].f;
        const double f0 = bands_[i].f0;
        const double f1 = bands_[i].f1;

        // ---- 主路取值 ----
        const int binHiLo = static_cast<int> (std::floor (f0 * hzToBinHi));
        const int binHiUp = static_cast<int> (std::ceil  (f1 * hzToBinHi));
        const float magHi = peakInRange (magHi_.data(), magHiN, binHiLo, binHiUp);

        // ---- 低频路取值（仅有效频段内）----
        float magLo = 0.0f;
        if (fftLo_ && magLoN > 0 && f < loHardMax) {
            const int binLoLo = static_cast<int> (std::floor (f0 * hzToBinLo));
            const int binLoUp = static_cast<int> (std::ceil  (f1 * hzToBinLo));
            magLo = peakInRange (magLo_.data(), magLoN, binLoLo, binLoUp);
        }

        // ---- 交叉淡化（w=0 纯低频路，w=1 纯主路）----
        float w;
        if (! fftLo_)            w = 1.0f;                 // 无低频路 → 纯主路
        else if (f <= xoverLo)   w = 0.0f;
        else if (f >= xoverHi)   w = 1.0f;
        else {
            const double u = (std::log (f) - std::log (xoverLo))
                           / (std::log (xoverHi) - std::log (xoverLo));
            w = static_cast<float> (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * u));
        }
        const float eHi  = magHi * magHi;
        const float eLo  = magLo * magLo;
        const float eMix = w * eHi + (1.0f - w) * eLo;
        outLinear[i] = std::sqrt (juce::jmax (0.0f, eMix));
    }
}

// -----------------------------------------------------------------------------
// 动态曲线：dB → normalized [0,1]
//   d = (db - minDb) * gain;  n = clamp(d/range, 0, 1);
//   y = curve(n);  y' = pow(y, gamma);  normalized = clamp(y', 0, 1)
// -----------------------------------------------------------------------------
void SpectrumCore::applyDynCurve_ (const std::vector<float>& db, std::vector<float>& outNorm) const
{
    if (static_cast<int> (outNorm.size()) != params_.bandCount)
        outNorm.assign (params_.bandCount, 0.0f);

    const float range = params_.maxDb - params_.minDb;
    if (range < 1e-6f) {
        std::fill (outNorm.begin(), outNorm.end(), 0.0f);
        return;
    }

    for (int i = 0; i < params_.bandCount; ++i)
    {
        // 1) 偏移 + 增益
        float d = (db[i] - params_.minDb) * params_.dynGain;
        // 2) 归一化
        float n = std::clamp (d / range, 0.0f, 1.0f);
        // 3) 非线性曲线
        float y;
        switch (params_.dynCurve) {
            case SpectrumParams::LinearDyn:  y = n; break;
            case SpectrumParams::Sqrt:       y = std::sqrt (n); break;
            case SpectrumParams::LogLog:     y = std::log2 (1.0f + n); break;  // log2(1+n) ∈ [0,1]
            case SpectrumParams::Perceptual: {
                // A-weighting 预补偿后归一化（视觉上更贴人耳）
                const float dbEff = db[i] + bands_[i].aWeightDb;
                const float dEff  = (dbEff - params_.minDb) * params_.dynGain;
                y = std::clamp (dEff / range, 0.0f, 1.0f);
                break;
            }
        }
        // 4) Gamma 后处理
        const float y2 = std::pow (std::max (y, 0.0f), params_.dynGamma);
        outNorm[i] = std::clamp (y2, 0.0f, 1.0f);
    }
}

// -----------------------------------------------------------------------------
// 喂数据：交错立体声 → mid 混合 → 双路 FFT FIFO
//   逐样本：主路线性 FIFO 攒满即跑（非重叠）；低频路环形 + hop 触发
// -----------------------------------------------------------------------------
void SpectrumCore::pushInterleavedStereo (const float* interleavedLr, int numFrames)
{
    for (int n = 0; n < numFrames; ++n)
    {
        const float L   = interleavedLr[n * 2 + 0];
        const float R   = interleavedLr[n * 2 + 1];
        const float mid = 0.5f * (L + R);
        const float s   = juce::jlimit (-1.0f, 1.0f, mid);

        // ---- 主路（非重叠，攒满即跑）----
        fifoHi_[fifoIdxHi_] = s;
        ++fifoIdxHi_;
        if (fifoIdxHi_ >= params_.fftSize()) {
            runFftHi_();
            fifoIdxHi_ = 0;
        }

        // ---- 低频路（环形 + 75% overlap）----
        if (fftLo_) {
            fifoLo_[fifoIdxLo_] = s;
            fifoIdxLo_ = (fifoIdxLo_ + 1) % params_.fftSizeLo();
            if ((fifoIdxLo_ % hopLo_) == 0)
                runFftLo_();
        }
    }
}

// -----------------------------------------------------------------------------
// 时间推进：缓存 deltaSec（供 getBandFrame 的平滑时间常数 + 峰值衰减用）
// -----------------------------------------------------------------------------
void SpectrumCore::advanceTime (double deltaSec)
{
    deltaSec_ = (deltaSec > 0.0) ? deltaSec : (1.0 / 30.0);
}

// -----------------------------------------------------------------------------
// 取帧：band mapping → dB+slope+平滑+模糊 → 峰值保持 → 动态曲线 → BandFrame
//   非常量：更新 smoothedDb_ / peakDb_ / peakHoldRemainMs_
// -----------------------------------------------------------------------------
void SpectrumCore::getBandFrame (BandFrame& out)
{
    if (out.bandCount != params_.bandCount)
        out.resize (params_.bandCount);

    const int N = params_.bandCount;
    const double dt = (deltaSec_ > 0.0) ? deltaSec_ : (1.0 / 30.0);
    const float dtMs = static_cast<float> (dt * 1000.0);

    // 帧率无关 attack/release：alpha = 1 - exp(-dt/tau)
    const float tauAtt   = juce::jmax (1e-3f, params_.attackMs  / 1000.0f);
    const float tauRel   = juce::jmax (1e-3f, params_.releaseMs / 1000.0f);
    const float alphaAtt = 1.0f - std::exp (-static_cast<float> (dt) / tauAtt);
    const float alphaRel = 1.0f - std::exp (-static_cast<float> (dt) / tauRel);

    // 1) 双路合并 → bandCount 线性幅度
    computeBandMagnitudes_ (bandLinScratch_);

    // 2) 线性 → dB + Slope + attack/release 平滑
    for (int i = 0; i < N; ++i) {
        const float mag = std::abs (bandLinScratch_[i]);
        float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-7f, mag));  // 20*log10
        if (params_.slopeEnabled)
            db += bands_[i].slopeDb;
        const float prev  = smoothedDb_[i];
        const float alpha = (db > prev) ? alphaAtt : alphaRel;
        float sm = prev + alpha * (db - prev);
        sm = juce::jlimit (params_.minDb, params_.maxDb + 12.0f, sm);
        smoothedDb_[i] = sm;
    }

    // 3) 列间模糊 [1,2,1]/4（强度由 temporalSmoothing 控制：0=关闭，1=Y2K 全模糊）
    if (N >= 3 && params_.temporalSmoothing > 0.0f) {
        if (static_cast<int> (blurScratch_.size()) != N) blurScratch_.assign (N, params_.minDb);
        const float k = juce::jlimit (0.0f, 1.0f, params_.temporalSmoothing);
        blurScratch_[0]     = smoothedDb_[0];
        blurScratch_[N - 1] = smoothedDb_[N - 1];
        for (int i = 1; i < N - 1; ++i) {
            const float blurred = 0.25f * smoothedDb_[i - 1]
                                + 0.50f * smoothedDb_[i]
                                + 0.25f * smoothedDb_[i + 1];
            blurScratch_[i] = (1.0f - k) * smoothedDb_[i] + k * blurred;
        }
        smoothedDb_.swap (blurScratch_);
    }

    // 4) 峰值保持：新峰则更新+重置 hold；否则 hold 到期后按 dB/s 线性衰减
    //    peaksFrozen_（GUI 暂停）：跳过 hold 计时与衰减，峰值帽原地保持；
    //    新峰跟随分支保留（seek 快进时峰值仍能建立）
    {
        // v0.5.3: 峰值帽二阶下落（初速度 peakDecayDbPerSec + 加速度 peakDecayAccelDbPerSec2）。
        //   peakVelDbPerSec_[i] = 当前下落速度；accel == 0 时退化为原匀速行为（v 恒等于初速度）。
        //   accel > 0 = 越落越快；accel < 0 = 减速下落（可落停）。peaksFrozen 时速度也冻结。
        const float dtSec = static_cast<float> (dt);
        const float accel = params_.peakDecayAccelDbPerSec2;
        for (int i = 0; i < N; ++i) {
            if (smoothedDb_[i] > peakDb_[i]) {
                peakDb_[i] = smoothedDb_[i];
                peakHoldRemainMs_[i] = 0.0f;
                peakVelDbPerSec_[i] = params_.peakDecayDbPerSec;   // 新峰：速度重置为初速度
            } else if (! peaksFrozen_) {
                peakHoldRemainMs_[i] += dtMs;
                if (peakHoldRemainMs_[i] > params_.peakHoldMs) {
                    peakVelDbPerSec_[i] += accel * dtSec;          // v += a·dt
                    peakDb_[i] -= peakVelDbPerSec_[i] * dtSec;     // db -= v·dt
                }
            }
            peakDb_[i] = juce::jmax (params_.minDb, peakDb_[i]);
        }
    }

    // 5) 动态曲线 → normalized [0,1]
    applyDynCurve_ (smoothedDb_, out.normalized);

    // 6) 输出
    for (int i = 0; i < N; ++i) {
        out.db[i] = smoothedDb_[i];
        out.peakDb[i] = peakDb_[i];
    }
}

// -----------------------------------------------------------------------------
// 调试用：取最近一次 FFT 原始幅度
// -----------------------------------------------------------------------------
void SpectrumCore::getRawMagnitudesHi (std::vector<float>& out) const { out = magHi_; }
void SpectrumCore::getRawMagnitudesLo (std::vector<float>& out) const { out = magLo_; }
