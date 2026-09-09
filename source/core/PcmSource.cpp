// =============================================================================
// PcmSource.cpp — 离线音频文件读取实现（wav/aiff → 立体声交错 PCM）
//
// 关键点：
//   · AudioFormatManager 注册 basic formats（Wav/Aiff/Flac/Vorbis；Win 下含 mp3/wma
//     等解码器的格式库，避免外部依赖或 JUCE 商业授权触发）。
//   · 用 AudioFormatReader::read(AudioBuffer<float>*) 重载：该方法会自动把
//     读取器的定点/浮点样本格式归一化为 AudioBuffer 的 float 格式
//     （详见 juce_AudioFormatReader.h read() 的第二个重载注释）。
//   · 从 AudioBuffer 取各通道 float* getReadPointer(ch)，然后转写为
//     LRLR... 交错格式供 projectM addPcmFloat 使用。
//   · 单声道 → 立体声复制（L=R），≥3 通道 → 取前 2 通道
// =============================================================================
#include "PcmSource.h"

#include <algorithm>   // std::min
#include <mutex>       // std::once_flag / std::call_once

namespace
{
// 注册所有支持的格式到一个进程共享的 AudioFormatManager 单例。
// 注册是有成本的，用 static 懒加载 + 单次注册，避免每个 PcmSource 实例重复注册。
juce::AudioFormatManager& sharedFormatManager()
{
    static juce::AudioFormatManager manager;
    static std::once_flag flag;
    std::call_once (flag, []
    {
        // v0.5.4 #8：全平台 basic formats（wav/aiff + flac/vorbis 按编译开关；
        //   Windows 额外 WindowsMediaAudioFormat → mp3/wma 可读）
        manager.registerBasicFormats();
    });
    return manager;
}
}

PcmSource::~PcmSource() { close(); }

bool PcmSource::load (const juce::String& path)
{
    close();

    juce::File file (path);
    if (! file.existsAsFile())
        return false;

    // createReaderFor 会按注册顺序尝试；第一个匹配成功的 reader 归调用方所有。
    auto* rawReader = sharedFormatManager().createReaderFor (file);
    if (rawReader == nullptr)
        return false;

    reader_.reset (rawReader);

    sampleRate_     = rawReader->sampleRate;
    totalSamples_   = static_cast<int64_t> (rawReader->lengthInSamples);
    channels_       = rawReader->numChannels;

    return sampleRate_ > 0.0 && totalSamples_ > 0 && channels_ > 0;
}

void PcmSource::close()
{
    reader_.reset();
    sampleRate_   = 0.0;
    totalSamples_ = 0;
    channels_     = 0;
}

bool PcmSource::isOpen() const noexcept { return reader_ != nullptr; }

double PcmSource::getDurationSeconds() const noexcept
{
    return sampleRate_ > 0.0 ? static_cast<double> (totalSamples_) / sampleRate_ : 0.0;
}

int PcmSource::readInterleavedStereo (int64_t start, int numSamples,
                                      float* outInterleaved) const
{
    if (reader_ == nullptr || outInterleaved == nullptr || numSamples <= 0 || start < 0)
        return 0;

    if (start >= totalSamples_)
        return 0;  // 已超出文件末尾

    // 裁剪到文件边界
    const int readCount = static_cast<int> (
        std::min (static_cast<int64_t> (numSamples), totalSamples_ - start));

    // 用 AudioBuffer<float> 持有按通道分开的 float 样本。
    // reader_->read(AudioBuffer<float>*, ...) 会自动把 WAV/AIFF 的
    // 定点样本（16/24-bit int）归一化为 [-1.0, 1.0] 的 float。
    // 分配 channels_ 个通道（与源文件一致），避免无谓的通道复制。
    juce::AudioBuffer<float> tempBuffer (channels_, readCount);
    tempBuffer.clear();

    const bool ok = reader_->read (&tempBuffer,
                                   /*startSampleInDestBuffer=*/0,
                                   /*numSamples=*/readCount,
                                   /*readerStartSample=*/start,
                                   /*useReaderLeftChan=*/true,
                                   /*useReaderRightChan=*/true);
    if (! ok)
    {
        // 读失败：填静音并返回 0（上层会把 0 当作 EOF）
        for (int i = 0; i < readCount * 2; ++i)
            outInterleaved[i] = 0.0f;
        return 0;
    }

    const float* leftCh  = channels_ >= 1 ? tempBuffer.getReadPointer (0) : nullptr;
    const float* rightCh = channels_ >= 2 ? tempBuffer.getReadPointer (1) : nullptr;

    // 按 sample 顺序交替写入 outInterleaved：[L0, R0, L1, R1, ...]
    if (channels_ == 1 && leftCh != nullptr)
    {
        // 单声道：L=R 复制（最常见的输入，单独分支优化）
        for (int i = 0; i < readCount; ++i)
        {
            const float s = leftCh[i];
            outInterleaved[i * 2    ] = s;
            outInterleaved[i * 2 + 1] = s;
        }
    }
    else if (channels_ >= 2 && leftCh != nullptr && rightCh != nullptr)
    {
        // 立体声及以上：取前 2 个通道
        for (int i = 0; i < readCount; ++i)
        {
            outInterleaved[i * 2    ] = leftCh[i];
            outInterleaved[i * 2 + 1] = rightCh[i];
        }
    }
    else
    {
        // 其它异常情况：填静音
        for (int i = 0; i < readCount * 2; ++i)
            outInterleaved[i] = 0.0f;
    }

    return readCount;
}
