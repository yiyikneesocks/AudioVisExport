// =============================================================================
// PcmSource.h — 离线音频文件读取（wav / aiff → 立体声交错 PCM）
//
// 设计意图：
//   · 这是 source/core/ 引擎层的一员，不依赖 UI / 系统时钟。
//   · 离线导出需要按 sample 索引随机访问音频，故用 AudioFormatReader 而非
//     PositionableAudioSource 的流式回调模型。
//   · 单声道文件会在 readInterleavedStereo 自动复制 L=R，让上层（projectM）
//     永远拿到立体声数据，简化调用方逻辑。
//
// 后续大工程衔接：剪辑软件的"音频元素"可直接复用本类读取任意 wav/aiff。
// =============================================================================
#pragma once

// JUCE module headers（juce_audio_utils 传递性带出 audio_basics/formats/devices/core 等）
#include <juce_audio_utils/juce_audio_utils.h>

class PcmSource
{
public:
    PcmSource() = default;
    ~PcmSource();

    // 打开音频文件。当前支持 wav / aiff（JUCE juce_audio_formats 原生支持）。
    // 失败返回 false（reader 创建失败 / 文件损坏 / 格式不支持）。
    bool load (const juce::String& path);
    void close();
    bool isOpen() const noexcept;

    double getSampleRate() const noexcept { return sampleRate_; }
    int64_t getNumSamples() const noexcept { return totalSamples_; }
    int getNumChannels() const noexcept { return channels_; }
    double getDurationSeconds() const noexcept;

    // 取 [start, start+numSamples) 的立体声交错 PCM（LRLR...）。
    // 单声道输入会自动复制 L=R，输出始终是 channels()==2 的格式。
    // outInterleaved 长度必须 ≥ numSamples * 2 * sizeof(float)。
    // 返回实际读到的"立体声对数"（≤ numSamples，到文件尾时不足）。
    int readInterleavedStereo (int64_t start, int numSamples,
                               float* outInterleaved) const;

private:
    std::unique_ptr<juce::AudioFormatReader> reader_;
    double sampleRate_ = 0.0;
    int64_t totalSamples_ = 0;
    int channels_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PcmSource)
};
