// =============================================================================
// MainComponent.h — GUI 主组件
//
// 架构：
//   · 音频播放：AudioDeviceManager + AudioSourcePlayer + AudioTransportSource
//   · 频谱同步：juce::Timer @ 30fps，读 transport 位置 → 从 PcmSource 按帧推
//     PCM 进 SpectrumCore → getBandFrame → SpectrumCanvas 重绘
//     （预览与导出共用同一引擎/样式，预览即所得）
//   · 参数：ParamPanel 直接改 SpectrumParams → paramsDirty → 下一帧重建 core
//   · 导出：后台线程跑 VisPipeline::run，进度回传 UI（原子变量轮询）
// =============================================================================
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/PcmSource.h"
#include "../core/SpectrumCore.h"
#include "../core/SpectrumStyle.h"
#include "../core/SpectrumParams.h"
#include "../core/VisPipeline.h"
#include "SpectrumCanvas.h"
#include "ParamPanel.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    // ---- 音频 ----
    juce::AudioFormatManager formatManager;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    PcmSource pcm;                 // 频谱用离线读取（与播放分离）
    juce::File audioFile;
    double srcRate = 44100.0;
    bool hasAudio = false;

    // ---- 引擎 ----
    SpectrumParams params;
    std::unique_ptr<SpectrumCore> core;
    std::unique_ptr<SpectrumStyle> style;
    juce::String currentStyleName;
    std::atomic<bool> paramsDirty { false }; // 面板改动 → 下一 tick 重建 core
    int64_t coreFramePos = 0;      // core 已推进到的帧索引
    int64_t pendingSeekFrame = -1; // >=0 时下一 tick 执行完整 seek

    // ---- UI ----
    SpectrumCanvas canvas;
    ParamPanel panel;
    juce::Viewport panelViewport;
    juce::TextButton playBtn { "播放" };
    juce::Slider seekBar;
    juce::Label timeLabel;
    bool userSeeking = false;
    double pausedPos = 0.0;

    // ---- 导出 ----
    std::atomic<bool> exporting { false };
    std::atomic<int>  exportPct { 0 };
    std::atomic<bool> exportDone { false };
    std::mutex exportMsgMtx;
    juce::String exportMsg;
    juce::File exportDir;
    std::thread exportThread;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File lastDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    // ---- 流程 ----
    void timerCallback() override;
    void loadFile (const juce::File& f);
    void rebuildCoreLight();               // 重建 core（8 帧静音 warmup，不回放历史）
    void advanceCoreTo (int64_t target);   // 从 coreFramePos 推进到 target
    int64_t currentTargetFrame() const;
    static SpectrumStyle::RenderParams buildRp (const SpectrumParams& p);
    void chooseAudioFile();
    void chooseExportDir (bool runAfter);
    void startExport();
    void startExportJob();
    static juce::String formatTime (double sec);

    JUCE_DECLARE_NON_COPYABLE (MainComponent)
};
