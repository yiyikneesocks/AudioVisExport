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
                      private juce::Timer,
                      private juce::FileDragAndDropTarget
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;  // v0.5.3: Delete/Backspace + Space fallback

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
    juce::TextButton loadBtn { "Load..." };   // v0.5.0: 随时换曲
    juce::TextButton ejectBtn { "Eject" };    // v0.5.0: 移除当前音频
    juce::Slider seekBar;
    juce::Label timeLabel;
    juce::Label nowPlayingLabel;              // v0.5.0: 当前音频文件名（画布左下角）
    bool userSeeking = false;
    double pausedPos = 0.0;

    // ---- 导出 ----
    std::atomic<bool> exporting { false };
    std::atomic<int>  exportPct { 0 };
    std::atomic<bool> exportDone { false };
    std::mutex exportMsgMtx;
    juce::String exportMsg;
    juce::File exportDir;
    bool exportKindPending = false;  // chooseExportDir(true) 回调后执行视频导出
    std::thread exportThread;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File lastDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    // ---- 流程 ----
    void timerCallback() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;   // 窗口级拖放兜底
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void loadFile (const juce::File& f);
    void ejectAudio();                     // v0.5.0: 移除当前音频（停止/清空/回空态）
    void rebuildCoreLight();               // 重建 core（8 帧静音 warmup，不回放历史）
    void advanceCoreTo (int64_t target);   // 从 coreFramePos 推进到 target
    int64_t currentTargetFrame() const;
    static SpectrumStyle::RenderParams buildRp (const SpectrumParams& p);
    void chooseAudioFile();
    void togglePlayPause();                   // v0.5.3: 播放/暂停切换（Play 按钮 + 空格共用）
    void chooseImageFile();
    void chooseMaskImageFile();          // v0.5.4: 选频谱蒙版图片
    void applyMaskImageFile (const juce::File& f);   // v0.5.4 #7: 单槽位应用（替换确认后）
    void requestMaskImageFile (const juce::File& f); // v0.5.4 #6: 选择器/拖放统一入口
    void loadFileInternal (const juce::File& f);     // v0.5.4 #7: 实际加载（替换确认后）
    void addImageLayer (const juce::File& f);   // 拖入/选择图片 → 新建图片图层并选中
    void moveSelectedLayer (int delta);         // +1 = 上移一层，-1 = 下移一层（统一 z 序）
    void removeSelectedLayer();                 // 统一删除（图片或频谱）
    void addSpectrumLayer();                    // v0.5.2: 恢复被删除的频谱层
    void chooseExportDir (bool runAfter);
    void startExport();
    void startExportVideo();        // 一键视频导出（默认透明 WebM，自动生成输出路径）
    void startExportJob();
    static juce::String formatTime (double sec);

    JUCE_DECLARE_NON_COPYABLE (MainComponent)
};
