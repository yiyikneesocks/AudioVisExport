// =============================================================================
// MainComponent.cpp — GUI 主组件实现
// =============================================================================
#include "MainComponent.h"
#include <cmath>

// ---------------------------------------------------------------------------
// 构建 / 销毁
// ---------------------------------------------------------------------------
MainComponent::MainComponent() : panel (params)
{
    formatManager.registerBasicFormats();

    // 音频设备（0 in / 2 out）
    deviceManager.initialiseWithDefaultDevices (0, 2);
    player.setSource (&transport);
    deviceManager.addAudioCallback (&player);

    // 画布
    canvas.onFileDropped = [this] (const juce::File& f) { loadFile (f); };
    canvas.onEmptyClicked = [this] { chooseAudioFile(); };
    addAndMakeVisible (canvas);

    // 参数面板
    panel.onParamsChanged = [this] { paramsDirty = true; };
    panel.onBrowseOutputDir = [this] { chooseExportDir (false); };
    panel.onExportClicked = [this] { startExport(); };
    addAndMakeVisible (panel);
    panelViewport.setViewedComponent (&panel, false);
    panelViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (panelViewport);

    // 传输条
    playBtn.onClick = [this]
    {
        if (! hasAudio)
            return;
        if (transport.isPlaying())
        {
            pausedPos = transport.getCurrentPosition();
            transport.stop();
            playBtn.setButtonText ("播放");
        }
        else
        {
            if (pausedPos >= transport.getLengthInSeconds() - 0.05)
            {
                pausedPos = 0.0;
                pendingSeekFrame = 0;
            }
            transport.setPosition (pausedPos);
            transport.start();
            playBtn.setButtonText ("暂停");
        }
    };
    addAndMakeVisible (playBtn);

    seekBar.setSliderStyle (juce::Slider::LinearHorizontal);
    seekBar.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    seekBar.setRange (0.0, 1.0, 0.001);
    seekBar.onDragStart = [this] { userSeeking = true; };
    seekBar.onDragEnd = [this]
    {
        userSeeking = false;
        if (! hasAudio)
            return;
        const double pos = seekBar.getValue() * transport.getLengthInSeconds();
        transport.setPosition (pos);
        pausedPos = pos;
        pendingSeekFrame = (int64_t) (pos * params.fps);
    };
    addAndMakeVisible (seekBar);

    timeLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    timeLabel.setJustificationType (juce::Justification::centredRight);
    timeLabel.setText ("0:00 / 0:00", juce::dontSendNotification);
    addAndMakeVisible (timeLabel);

    // 初始引擎
    currentStyleName = params.style;
    style = SpectrumStyle::create (params.style);
    rebuildCoreLight();

    setSize (1280, 800);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    transport.stop();
    transport.setSource (nullptr);
    deviceManager.removeAudioCallback (&player);
    player.setSource (nullptr);
    if (exportThread.joinable())
        exportThread.join();
}

// ---------------------------------------------------------------------------
// 布局
// ---------------------------------------------------------------------------
void MainComponent::resized()
{
    auto b = getLocalBounds();

    auto transportBar = b.removeFromBottom (40);
    panelViewport.setBounds (b.removeFromRight (330));
    canvas.setBounds (b.reduced (4));

    auto t = transportBar.reduced (8, 4);
    playBtn.setBounds (t.removeFromLeft (80));
    timeLabel.setBounds (t.removeFromRight (110));
    seekBar.setBounds (t.reduced (4, 0));

    panel.setSize (panelViewport.getWidth() - panelViewport.getScrollBarThickness(),
                   panel.getPreferredHeight());
}

// ---------------------------------------------------------------------------
// 计时回调：核心同步 + UI 更新 + 导出进度
// ---------------------------------------------------------------------------
void MainComponent::timerCallback()
{
    // 面板改动过参数：样式变化则重建 style；重建 core 让参数立即生效
    // （不回放历史，曲线在后续帧自然恢复；重建 = 轻 FFT 分配，30fps 下无感）
    if (paramsDirty.exchange (false))
    {
        if (params.style != currentStyleName)
        {
            currentStyleName = params.style;
            style = SpectrumStyle::create (params.style);
        }
        rebuildCoreLight();
        coreFramePos = currentTargetFrame();
    }

    // 显式 seek（拖动进度条结束）
    if (pendingSeekFrame >= 0)
    {
        const int64_t target = pendingSeekFrame;
        pendingSeekFrame = -1;
        rebuildCoreLight();
        coreFramePos = 0;
        advanceCoreTo (target);
    }

    // 正常推进
    const int64_t target = currentTargetFrame();
    if (target < coreFramePos)
    {
        rebuildCoreLight();
        coreFramePos = 0;
        advanceCoreTo (target);
    }
    else if (target > coreFramePos)
    {
        advanceCoreTo (target);
    }

    // 取帧 + 渲染
    if (core != nullptr)
        core->getBandFrame (canvas.frame);
    canvas.rp = buildRp (params);
    canvas.style = style.get();
    canvas.showCheckerboard = panel.getCheckerPreview();
    canvas.repaint();

    // 传输 UI
    if (hasAudio)
    {
        const double dur = transport.getLengthInSeconds();
        if (dur > 0.0)
        {
            if (! userSeeking)
                seekBar.setValue (transport.getCurrentPosition() / dur, juce::dontSendNotification);
            const double shown = transport.isPlaying() ? transport.getCurrentPosition() : pausedPos;
            timeLabel.setText (formatTime (shown) + " / " + formatTime (dur),
                               juce::dontSendNotification);
        }
    }

    // 导出进度
    if (exporting.load())
    {
        const int pct = exportPct.load();
        panel.setProgressText ("导出中 " + juce::String (pct) + "% ...");
    }
    if (exportDone.exchange (false))
    {
        if (exportThread.joinable())
            exportThread.join();
        exporting = false;
        juce::String msg;
        { std::lock_guard<std::mutex> lk (exportMsgMtx); msg = exportMsg; }
        panel.setProgressText (msg);
        panel.setExportEnabled (true);
    }
}

// ---------------------------------------------------------------------------
// 音频加载与引擎同步
// ---------------------------------------------------------------------------
void MainComponent::loadFile (const juce::File& f)
{
    transport.stop();
    playBtn.setButtonText ("播放");
    transport.setSource (nullptr);
    readerSource.reset();

    if (! pcm.load (f.getFullPathName()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "加载失败",
                                                "无法读取该音频文件（当前支持 WAV / AIFF）");
        hasAudio = false;
        canvas.hasAudio = false;
        canvas.repaint();
        return;
    }

    audioFile = f;
    hasAudio = true;
    canvas.hasAudio = true;
    srcRate = pcm.getSampleRate();

    if (auto* r = formatManager.createReaderFor (f))
    {
        readerSource = std::make_unique<juce::AudioFormatReaderSource> (r, true);
        transport.setSource (readerSource.get(), 0, nullptr, r->sampleRate);
    }

    pausedPos = 0.0;
    pendingSeekFrame = 0;
    coreFramePos = 0;
    paramsDirty = true; // 新采样率 → 重建 core
    seekBar.setRange (0.0, 1.0, 0.001);
    seekBar.setValue (0.0, juce::dontSendNotification);
    canvas.repaint();
}

void MainComponent::chooseAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("选择音频文件", lastDir, "*.wav;*.aif;*.aiff");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                  {
                                      lastDir = f.getParentDirectory();
                                      loadFile (f);
                                  }
                              });
}

int64_t MainComponent::currentTargetFrame() const
{
    if (! hasAudio)
        return 0;
    const double pos = transport.isPlaying() ? transport.getCurrentPosition() : pausedPos;
    return (int64_t) (pos * params.fps);
}

void MainComponent::rebuildCoreLight()
{
    core = std::make_unique<SpectrumCore> (params);
    core->setSampleRate (srcRate);

    // 8 帧静音 warmup（与离线管线一致）
    const int spf = juce::jmax (1, (int) (srcRate / params.fps));
    std::vector<float> zeros ((size_t) spf * 2, 0.0f);
    for (int i = 0; i < 8; ++i)
    {
        core->pushInterleavedStereo (zeros.data(), spf);
        core->advanceTime (1.0 / params.fps);
    }
}

void MainComponent::advanceCoreTo (int64_t target)
{
    const int spf = juce::jmax (1, (int) (srcRate / params.fps));
    std::vector<float> buf ((size_t) spf * 2, 0.0f);

    for (int64_t f = coreFramePos; f < target; ++f)
    {
        int got = hasAudio ? pcm.readInterleavedStereo (f * spf, spf, buf.data()) : 0;
        if (got < spf)
            std::fill (buf.begin() + (size_t) got * 2, buf.end(), 0.0f);
        core->pushInterleavedStereo (buf.data(), spf);
        core->advanceTime (1.0 / params.fps);
    }
    coreFramePos = target;
}

SpectrumStyle::RenderParams MainComponent::buildRp (const SpectrumParams& p)
{
    SpectrumStyle::RenderParams rp;
    rp.width          = p.width;
    rp.height         = p.height;
    rp.paddingLeft    = 32.0f;
    rp.paddingRight   = 6.0f;
    rp.paddingTop     = 4.0f;
    rp.paddingBottom  = 16.0f;
    rp.primary        = p.primaryColor;
    rp.secondary      = p.secondaryColor;
    rp.peak           = p.peakColor;
    rp.bg             = p.bgColor;
    rp.lineWidth      = p.lineWidth;
    rp.opacity        = p.opacity;
    rp.drawGrid       = p.drawGrid;
    rp.drawAxisLabels = p.drawAxisLabels;
    rp.minHz          = p.minHz;
    rp.maxHz          = p.maxHz;
    rp.minDb          = p.minDb;
    rp.maxDb          = p.maxDb;
    return rp;
}

// ---------------------------------------------------------------------------
// 导出
// ---------------------------------------------------------------------------
void MainComponent::chooseExportDir (bool runAfter)
{
    fileChooser = std::make_unique<juce::FileChooser> ("选择导出目录",
                                                       exportDir.getFullPathName().isEmpty() ? lastDir : exportDir);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                              [this, runAfter] (const juce::FileChooser& fc)
                              {
                                  const auto dir = fc.getResult();
                                  if (dir == juce::File())
                                      return;
                                  exportDir = dir;
                                  panel.setOutputDirText (dir.getFullPathName());
                                  if (runAfter)
                                      startExport();
                              });
}

void MainComponent::startExport()
{
    if (exporting.load())
        return;
    if (! hasAudio)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "无法导出", "请先拖入音频文件");
        return;
    }
    if (! exportDir.getFullPathName().isEmpty() && exportDir.exists())
        startExportJob();
    else
        chooseExportDir (true);
}

void MainComponent::startExportJob()
{
    exporting = true;
    exportPct = 0;
    panel.setExportEnabled (false);

    SpectrumParams p = params;
    p.audioPath = audioFile.getFullPathName();
    p.outputDir = exportDir.getFullPathName();

    if (exportThread.joinable())
        exportThread.join();

    exportThread = std::thread ([this, p]()
    {
        VisPipeline vp;
        VisPipeline::Config cfg;
        cfg.params = p;

        auto res = vp.run (cfg, [this] (int done, int total, double)
        {
            exportPct = (int) (100.0 * done / (double) juce::jmax (1, total));
        });

        juce::String msg;
        if (res.getValue ("ok", "false") == "true")
            msg = "完成: " + res.getValue ("frames_written", "0") + " 帧 -> " + res.getValue ("png_dir", "");
        else
            msg = "失败: " + res.getValue ("error", "unknown");

        { std::lock_guard<std::mutex> lk (exportMsgMtx); exportMsg = msg; }
        exportDone = true;
    });
}

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------
juce::String MainComponent::formatTime (double sec)
{
    const int total = (int) std::round (sec);
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
