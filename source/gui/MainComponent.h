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
    bool keyPressed (const juce::KeyPress&) override;  // v0.5.3: Delete/Backspace + Space fallback；v0.5.5 #3: 方向键/Ctrl+Z/Ctrl+A

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
    // v0.5.4 #H：解码优先的取图助手。图片解不开 → 明确弹错并返回空图，调用方**不得改动任何状态**。
    //   （旧路径先写 maskImage.path / 先 insert 图层，再发现解不开 → 留下"已设置但看不见"的
    //    死状态，而且下次拖入只会弹"already set"，用户看到的就是"拖放没反应"。）
    juce::Image loadValidatedImage (const juce::File& f, const juce::String& what);
    // v0.5.4 #H：拖放文件的**唯一**路由（OLE 兜底链与 WM_DROPFILES 直连链共用，避免两边规则漂移）。
    //   pos = 落点屏幕坐标；落在面板视口内且停在 Mask 页 → 设蒙版图，否则图片进图层、音频进播放器。
    void routeDroppedFile (const juce::File& f, const juce::Point<int>& pos);
    void moveSelectedLayer (int delta);         // +1 = 上移一层，-1 = 下移一层（统一 z 序）
    void removeSelectedLayer();                 // 统一删除（图片或频谱）
    // ---- v0.5.5 新 #3：快捷键小功能 ----
    void doSeekStep (double deltaSec);          // ←/→ 快进快退（含长按加速，配合 timerCallback）
    void pushUndoSnapshot ();                   // 结构性操作前压入 params 快照
    void undoOnce ();                           // Ctrl+Z：弹快照恢复
    std::vector<juce::String> undoStack;        // params.toJson() 历史（上限 30）
    int    seekHeldDir = 0;                      // -1 左 / +1 右 / 0 未按住
    double seekLastMs = -1e9;                    // 上一次方向键事件时刻（新按 vs OS 自动重发判定）
    int    seekRepeatCount = 0;                  // 本次长按已自动 seek 的次数（步长加速用）
    void addSpectrumLayer();                    // v0.5.2: 恢复被删除的频谱层
    void chooseExportDir (bool runAfter);
    void startExport();
    void startExportVideo();        // 一键视频导出（默认透明 WebM，自动生成输出路径）
    void startExportJob();
    static juce::String formatTime (double sec);

    JUCE_DECLARE_NON_COPYABLE (MainComponent)
};
