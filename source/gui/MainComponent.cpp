// =============================================================================
// MainComponent.cpp — GUI 主组件实现
// =============================================================================
#include "MainComponent.h"
#include "WinDragCompat.h"
#include <cmath>

// ---------------------------------------------------------------------------
// 构建 / 销毁
// ---------------------------------------------------------------------------
MainComponent::MainComponent() : canvas (params), panel (params)
{
    formatManager.registerBasicFormats();

    // v0.5.3: 让本组件可直接持有键盘焦点（配合 loadFile 末尾 grabKeyboardFocus，
    //   修复"刚拖入音频立即按空格无效，须先点一下窗口才生效"——根因：无焦点时
    //   JUCE 按键只投递到顶层窗口并向上走父链，永不进入作为子组件的本类）。
    setWantsKeyboardFocus (true);

    // 音频设备（0 in / 2 out）
    deviceManager.initialiseWithDefaultDevices (0, 2);
    player.setSource (&transport);
    deviceManager.addAudioCallback (&player);

    // 画布
    canvas.onFileDropped = [this] (const juce::File& f) { loadFile (f); };
    canvas.onNonAudioDropped = [this] (const juce::File& f)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Unsupported file",
                                                "Only WAV / AIFF audio can be used:\n"
                                                    + f.getFullPathName());
    };
    canvas.onImageDropped = [this] (const juce::File& f) { addImageLayer (f); };
    canvas.onEmptyClicked = [this] { chooseAudioFile(); };
    canvas.onDeleteRequested = [this] { removeSelectedLayer(); };
    addAndMakeVisible (canvas);

    // 参数面板
    panel.onParamsChanged = [this] { paramsDirty = true; };
    panel.onBrowseOutputDir = [this] { chooseExportDir (false); };
    panel.onExportClicked = [this] { startExport(); };
    panel.onExportVideoClicked = [this] { startExportVideo(); };

    // v0.5.0: 拖放兼容层诊断状态 → 面板进度文本（远程排障可见）
    avx::winDragCompat::onStatus = [this] (const juce::String& s)
    {
        panel.setProgressText (s);
    };

    // v0.5.0: WM_DROPFILES 直连交付（T4 实测 JUCE 分发链跨协议 relay 不可靠）。
    // 分发逻辑与画布 filesDropped 一致：图片 → 图层；音频 → 加载；其他 → 提示。
    avx::winDragCompat::onNativeFilesDropped = [this] (const juce::StringArray& files)
    {
        if (files.isEmpty())
            return;
        const juce::File f (files[0]);
        const auto ext = f.getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp"
            || ext == ".gif" || ext == ".webp")
            addImageLayer (f);
        else if (ext == ".wav" || ext == ".aif" || ext == ".aiff")
            loadFile (f);
        else
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Unsupported file",
                                                    "Only WAV / AIFF audio or image files can be used:\n"
                                                        + f.getFullPathName());
    };

    // 图层（Layers）区：面板按钮 → 主组件操作 params.images + 画布选中态
    panel.onAddImageClicked = [this] { chooseImageFile(); };
    panel.onLayerUp         = [this] { moveSelectedLayer (1);  };   // 上移一层（z 序靠顶）
    panel.onLayerDown       = [this] { moveSelectedLayer (-1); };   // 下移一层
    panel.onLayerRemove     = [this] { removeSelectedLayer(); };
    panel.onAddSpectrumClicked  = [this] { addSpectrumLayer(); };
    panel.onSelectSpectrumClicked = [this] { canvas.selectSpectrum(); canvas.repaint(); };
    // v0.5.4: 频谱蒙版图片
    panel.onChooseMaskImage = [this] { chooseMaskImageFile(); };
    panel.onToggleMaskEdit  = [this] (bool b) { canvas.setEditMaskImage (b); canvas.repaint(); };
    // v0.5.4 #6: 选中图片图层的色彩调整（只影响选中层）+ 页签高度变化重排
    panel.onReadImageAdjust = [this] (int ch) -> double
    {
        const int sel = canvas.selectedImageIndex();
        if (sel < 0 || sel >= (int) params.images.size())
            return 1.0;
        const auto& L = params.images[(size_t) sel];
        return (double) (ch == 0 ? L.brightness : ch == 1 ? L.contrast : L.saturation);
    };
    panel.onWriteImageAdjust = [this] (int ch, double v)
    {
        const int sel = canvas.selectedImageIndex();
        if (sel < 0 || sel >= (int) params.images.size())
            return;
        auto& L = params.images[(size_t) sel];
        if      (ch == 0) L.brightness = (float) v;
        else if (ch == 1) L.contrast   = (float) v;
        else              L.saturation = (float) v;
        canvas.repaint();
    };
    panel.onPanelHeightChanged = [this]
    {
        panel.setSize (panelViewport.getWidth() - panelViewport.getScrollBarThickness(),
                       panel.getPreferredHeight());
    };
    panel.onReadLayerOpacity = [this]() -> double
    {
        const int sel = canvas.selectedImageIndex();
        if (sel < 0 || sel >= (int) params.images.size())
            return 100.0;
        return (double) params.images[(size_t) sel].opacity * 100.0;
    };
    panel.onWriteLayerOpacity = [this] (double v)
    {
        const int sel = canvas.selectedImageIndex();
        if (sel < 0 || sel >= (int) params.images.size())
            return;
        params.images[(size_t) sel].opacity = (float) (v / 100.0);
        canvas.repaint();
    };
    // v0.5.1: 图层上下（aboveSpectrum）
    // v0.5.3: "Above spectrum" UI 按钮已删（层级改由 Up/Down 按钮与数组顺序控制），
    //   aboveSpectrum 标志与 JSON 兼容逻辑保留

    addAndMakeVisible (panel);
    panelViewport.setViewedComponent (&panel, false);
    panelViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (panelViewport);

    // 传输条
    playBtn.onClick = [this] { togglePlayPause(); };
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

    playBtn.setButtonText ("Play");
    timeLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    timeLabel.setJustificationType (juce::Justification::centredRight);
    timeLabel.setText ("0:00 / 0:00", juce::dontSendNotification);
    addAndMakeVisible (timeLabel);

    // v0.5.0: Load / Eject 按钮（换曲 / 移除音频）+ 当前文件名指示
    loadBtn.onClick = [this] { chooseAudioFile(); };
    addAndMakeVisible (loadBtn);
    ejectBtn.onClick = [this] { ejectAudio(); };
    ejectBtn.setEnabled (false);
    addAndMakeVisible (ejectBtn);
    nowPlayingLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9ca3af));
    nowPlayingLabel.setFont (juce::FontOptions (12.0f));
    nowPlayingLabel.setJustificationType (juce::Justification::centredLeft);
    nowPlayingLabel.setText ("No audio loaded", juce::dontSendNotification);
    addAndMakeVisible (nowPlayingLabel);

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
    playBtn.setBounds (t.removeFromLeft (70));
    loadBtn.setBounds (t.removeFromLeft (70).reduced (0, 2));
    ejectBtn.setBounds (t.removeFromLeft (60).reduced (0, 2));
    timeLabel.setBounds (t.removeFromRight (110));
    nowPlayingLabel.setBounds (t.removeFromLeft (t.getWidth() / 2));
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
    {
        // v0.5.0: 暂停时冻结峰值（hold 倒计时/衰减停止），防止暂停期间
        // getBandFrame 的有状态峰值推进导致峰值帽"抖动下落"
        core->setPeaksFrozen (! transport.isPlaying());
        core->getBandFrame (canvas.frame);
    }
    canvas.rp = buildRp (params);
    canvas.style = style.get();
    canvas.showCheckerboard = panel.getCheckerPreview();
    canvas.repaint();

    // v0.5.4: 画布编辑模式可能被"点范围外"自动退出 → 同步面板勾选态（幂等，无通知环）
    panel.setMaskEditChecked (canvas.editMaskImageMode());

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

        // v0.5.2 / v0.5.3: 同步图层区控件（透明度 / 频谱按钮）
        // v0.5.3: 去掉 layerSelCache 守卫改每 tick 调用——Remove 按钮可用性
        // 须随\"选中图片或频谱在场\"实时变化（此前\"无选中→选中频谱\"都是 -1 不触发刷新，
        //   Remove 在无图片时保持禁用，导致\"没图时删不掉频谱\"）
        const int selNow = canvas.selectedImageIndex();
        {
            const bool imgSel = (selNow >= 0 && selNow < (int) params.images.size());
            panel.refreshLayerControls (imgSel,
                                        imgSel ? params.images[(size_t) selNow].opacity * 100.0
                                               : 100.0,
                                        params.spectrumPresent);
        }
    }

    // 导出进度
    if (exporting.load())
    {
        const int pct = exportPct.load();
        panel.setProgressText ("Exporting " + juce::String (pct) + "% ...");
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
// v0.5.3: 播放/暂停切换（Play 按钮与空格键共用同一逻辑）
void MainComponent::togglePlayPause()
{
    if (! hasAudio)
        return;
    if (transport.isPlaying())
    {
        pausedPos = transport.getCurrentPosition();
        transport.stop();
        playBtn.setButtonText ("Play");
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
        playBtn.setButtonText ("Pause");
    }
}

// v0.5.0: 移除当前音频——停止播放、清空状态、画布回空拖放提示
void MainComponent::ejectAudio()
{
    transport.stop();
    playBtn.setButtonText ("Play");
    transport.setSource (nullptr);
    readerSource.reset();
    pcm.close();

    audioFile = juce::File();
    hasAudio = false;
    canvas.hasAudio = false;
    coreFramePos = 0;
    pausedPos = 0.0;
    pendingSeekFrame = -1;
    seekBar.setValue (0.0, juce::dontSendNotification);
    timeLabel.setText ("0:00 / 0:00", juce::dontSendNotification);
    nowPlayingLabel.setText ("No audio loaded", juce::dontSendNotification);
    nowPlayingLabel.setHelpText ("");

    // 清掉的音频图层数据保留（params 不动），core 重建为静音态
    rebuildCoreLight();
    ejectBtn.setEnabled (false);
    canvas.repaint();
}

void MainComponent::loadFile (const juce::File& f)
{
    // #7：同一时间只允许一首音乐——已有音频再加载时弹提示（架构本就单音频槽，
    // 这里按用户要求把"将被替换"显式说出来；不同文件才提示，重载同一首不打扰）
    if (hasAudio && audioFile != f)
    {
        juce::AlertWindow::showOkCancelBox (
            juce::MessageBoxIconType::InfoIcon,
            "Audio already loaded",
            "An audio file is already loaded:\n  " + audioFile.getFileName()
                + "\n\nThe new file will REPLACE it (one audio at a time).",
            "Replace", "Cancel", this,
            juce::ModalCallbackFunction::create (
                [this, f] (int ok) { if (ok) loadFileInternal (f); }));
        return;
    }
    loadFileInternal (f);
}

// #7：实际加载（loadFile 的确认回调；也供无音频时直呼）
void MainComponent::loadFileInternal (const juce::File& f)
{
    transport.stop();
    playBtn.setButtonText ("Play");
    transport.setSource (nullptr);
    readerSource.reset();

    if (! pcm.load (f.getFullPathName()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Load failed",
                                                "Could not read that audio file. "
                                                "Supported formats: WAV, AIFF.");
        hasAudio = false;
        canvas.hasAudio = false;
        nowPlayingLabel.setText ("No audio loaded", juce::dontSendNotification);
        ejectBtn.setEnabled (false);
        canvas.repaint();
        return;
    }

    audioFile = f;
    hasAudio = true;
    canvas.hasAudio = true;
    srcRate = pcm.getSampleRate();
    nowPlayingLabel.setText ("♪ " + f.getFileName(), juce::dontSendNotification);
    nowPlayingLabel.setHelpText (f.getFullPathName());
    ejectBtn.setEnabled (true);

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

    // v0.5.3: 拖入/载入音频后主动抓键盘焦点 → 空格/ Delete 立即生效，无需先点窗口
    grabKeyboardFocus();
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
    rp.barPitchRatio  = p.barPitchRatio;
    rp.baselineY      = p.baselineY;
    rp.fps            = p.fps;
    rp.peakDecayDbPerSec       = p.peakDecayDbPerSec;
    rp.peakDecayAccelDbPerSec2 = p.peakDecayAccelDbPerSec2;
    rp.barGapRatio    = p.barGapRatio;
    rp.barWidthRatio  = p.barWidthRatio;
    rp.drawGrid       = p.drawGrid;
    rp.drawAxisLabels = p.drawAxisLabels;
    rp.minHz          = p.minHz;
    rp.maxHz          = p.maxHz;
    rp.minDb          = p.minDb;
    rp.maxDb          = p.maxDb;
    rp.barPitchRatio  = p.barPitchRatio;
    rp.baselineY      = p.baselineY;
    rp.fps            = p.fps;
    rp.peakDecayDbPerSec       = p.peakDecayDbPerSec;
    rp.peakDecayAccelDbPerSec2 = p.peakDecayAccelDbPerSec2;
    rp.barGapRatio    = p.barGapRatio;
    rp.barWidthRatio  = p.barWidthRatio;
    rp.barParticles   = p.barParticles;
    rp.colorMap       = p.colorMap;
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
                                  {
                                      // 用户点的是 Export Video → 目录选好后继续视频导出
                                      if (exportKindPending)
                                      {
                                          exportKindPending = false;
                                          startExportVideo();
                                      }
                                      else
                                      {
                                          startExport();
                                      }
                                  }
                              });
}

void MainComponent::startExport()
{
    if (exporting.load())
        return;
    if (! hasAudio)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Cannot export",
                                                "Drop an audio file first, then try again.");
        return;
    }
    exportKindPending = false;
    if (! exportDir.getFullPathName().isEmpty() && exportDir.exists())
        startExportJob();
    else
        chooseExportDir (true);
}

// 一键视频导出：默认输出带 alpha 透明通道的 MOV（QTRLE rgba，剪辑软件标准格式），
// 输出文件名自动生成在导出目录：<音频名>_vis.mov
void MainComponent::startExportVideo()
{
    if (exporting.load())
        return;
    if (! hasAudio)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Cannot export video",
                                                "Drop an audio file first, then try again.");
        return;
    }
    // 若用户还没在 Encoder 下拉里选视频格式，则默认走 MOV QTRLE alpha（最可靠的透明格式）
    if (params.encoder == SpectrumParams::PngSeq)
        params.encoder = SpectrumParams::MovQtrle;

    exportKindPending = true;
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

    // 视频模式：GUI 不要求用户手填文件名，自动生成
    //   <输出目录>/<音频名>_vis.<webm|mov>
    if (p.encoder != SpectrumParams::PngSeq && p.outputVideoPath.isEmpty())
    {
        juce::File dir (p.outputDir);
        juce::String base = audioFile.getFileNameWithoutExtension();
        if (base.isEmpty()) base = "visual";
        const juce::String ext = (p.encoder == SpectrumParams::MovQtrle) ? ".mov" : ".webm";
        p.outputVideoPath = dir.getChildFile (base + "_vis" + ext).getFullPathName();
    }

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
        {
            if (p.encoder != SpectrumParams::PngSeq)
                msg = "Done: video -> " + res.getValue ("video_path", p.outputVideoPath);
            else
                msg = "Done: " + res.getValue ("frames_written", "0") + " frames -> "
                    + res.getValue ("png_dir", "");
        }
        else
        {
            msg = "Failed: " + res.getValue ("error", "unknown");
        }

        { std::lock_guard<std::mutex> lk (exportMsgMtx); exportMsg = msg; }
        exportDone = true;
    });
}

// ---------------------------------------------------------------------------
// 窗口级拖放兜底：画布以外的区域（按钮 / 参数面板上方）拖文件也能接住
// ---------------------------------------------------------------------------
bool MainComponent::isInterestedInFileDrag (const juce::StringArray&)
{
    return true;   // 接受任意文件；非音频在 filesDropped 中提示
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);

    auto isAudio = [] (const juce::String& p)
    {
        auto ext = juce::File (p).getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff";
    };

    for (auto& f : files)
        if (isAudio (f))
        {
            loadFile (juce::File (f));
            return;
        }

    if (! files.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Unsupported file",
                                                "Only WAV / AIFF audio can be used:\n"
                                                    + juce::File (files[0]).getFullPathName());
}

// ---------------------------------------------------------------------------
// 图片图层（拖入图片 / 面板按钮操作；z 序 = params.images 顺序，频谱固定最底）
// ---------------------------------------------------------------------------
void MainComponent::addImageLayer (const juce::File& f)
{
    ImageLayer layer;
    layer.path    = f.getFullPathName();
    layer.opacity = 1.0f;
    layer.transform = makeContainTransform ((float) juce::ImageCache::getFromFile (f).getWidth(),
                                            (float) juce::ImageCache::getFromFile (f).getHeight(),
                                            (float) params.width, (float) params.height);
    // v0.5.2: 插在频谱下方（spectrumIndex 位置）
    const int N = (int) params.images.size();
    const int k = juce::jlimit (0, N, params.spectrumIndex);
    params.images.insert (params.images.begin() + k, layer);
    ++params.spectrumIndex;
    for (size_t i = 0; i < params.images.size(); ++i)
        params.images[i].aboveSpectrum = ((int) i >= params.spectrumIndex);
    canvas.selectImage (k);
    canvas.repaint();
}

// v0.5.2: 恢复被删除的频谱层（以默认变换重建，插入记住的 z 位置）
void MainComponent::addSpectrumLayer()
{
    if (params.spectrumPresent)
        return;
    params.spectrumPresent = true;
    params.transform = VisTransform {};   // 默认铺满画布
    canvas.selectSpectrum();
    canvas.repaint();
}

// v0.5.2: 统一 z 序移动（概念栈：images[0..k) → 频谱 → images[k..N)）
void MainComponent::moveSelectedLayer (int delta)
{
    const int sel = canvas.selectedImageIndex();
    const int N = (int) params.images.size();
    const int k = juce::jlimit (0, N, params.spectrumIndex);

    // 概念位置：图片 i → i < k ? i : i+1；频谱 → k
    auto imgPos = [k] (int i) { return i < k ? i : i + 1; };
    int pos = (sel < 0) ? k : imgPos (sel);
    int target = pos + delta;
    if (target < 0 || target > N)
        return;

    if (sel < 0)
    {
        // 频谱移动
        if (target == k)
            return;
        if (target == k + 1 && k < N)
        {
            // 频谱上移：images[k] → images[k]
            params.images[(size_t) k].aboveSpectrum = false;
            ++params.spectrumIndex;
        }
        else if (target == k - 1 && k > 0)
        {
            // 频谱下移：images[k-1] → images[k-1]
            --params.spectrumIndex;
            params.images[(size_t) params.spectrumIndex].aboveSpectrum = true;
        }
    }
    else
    {
        // 图片移动
        if (target == k)
        {
            // 图片跨边界向上（从 below 到 above）：原 images[sel] → images[k]
            // v0.5.3: 去掉 k<N 守卫——频谱最顶（k==N）时图片也能跨越到上方
            if (sel < k)
            {
                ImageLayer img = std::move (params.images[(size_t) sel]);
                params.images.erase (params.images.begin() + sel);
                params.images.insert (params.images.begin() + k - 1, std::move (img));
                --params.spectrumIndex;
                canvas.selectImage (k - 1);
            }
            else if (sel >= k)
            {
                // 图片跨边界向下（从 above 到 below）
                // v0.5.3: 去掉 k>0 守卫——频谱最底（k==0）时图片也能跨越到下方
                ImageLayer img = std::move (params.images[(size_t) sel]);
                params.images.erase (params.images.begin() + sel);
                params.images.insert (params.images.begin() + k, std::move (img));
                ++params.spectrumIndex;
                canvas.selectImage (k);
            }
        }
        else
        {
            // 同组内交换
            int newIdx = (target < k) ? target : target - 1;
            std::swap (params.images[(size_t) sel], params.images[(size_t) newIdx]);
            canvas.selectImage (newIdx);
        }
    }

    // 同步 aboveSpectrum 标记
    for (size_t i = 0; i < params.images.size(); ++i)
        params.images[i].aboveSpectrum = ((int) i >= params.spectrumIndex);

    canvas.repaint();
}

// v0.5.3: keyboard fallback - Delete/Backspace still removes the selected layer
// when the canvas does not have keyboard focus (canvas handles its own case
// first and returns true; TextEditor-style controls self-consume, no conflict).
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        removeSelectedLayer();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)   // v0.5.3: 空格播放/暂停（方案 A：画布/滑块/无焦点时生效；
    {                                     // 文本框内空格照常输入；某非播放按钮持焦时被其消费）
        togglePlayPause();
        return true;
    }
    return false;
}

void MainComponent::removeSelectedLayer()
{
    const int sel = canvas.selectedImageIndex();
    if (sel >= 0)
    {
        // 删除图片
        params.images.erase (params.images.begin() + sel);
        // 重算 spectrumIndex（图片数减少，钳制）
        params.spectrumIndex = juce::jlimit (0, (int) params.images.size(), params.spectrumIndex);
        for (size_t i = 0; i < params.images.size(); ++i)
            params.images[i].aboveSpectrum = ((int) i >= params.spectrumIndex);
        canvas.selectImage (params.images.empty() ? -1
                            : juce::jmin (sel, (int) params.images.size() - 1));
    }
    else if (params.spectrumPresent)
    {
        // 删除频谱（真删除，音乐不受影响）
        params.spectrumPresent = false;
        canvas.selectImage (params.images.empty() ? -1 : (int) params.images.size() - 1);
    }
    canvas.repaint();
}

void MainComponent::chooseImageFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("选择图片",
                                                       lastDir,
                                                       "*.png;*.jpg;*.jpeg");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File())
                                      addImageLayer (f);
                              });
}

// v0.5.4: 选择频谱蒙版图片（图片只在频谱轮廓内可见，随频谱整体变换）
void MainComponent::chooseMaskImageFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("选择蒙版图片",
                                                       lastDir,
                                                       "*.png;*.jpg;*.jpeg;*.gif;*.bmp");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f == juce::File())
                                      return;
                                  // #7：一个频谱只允许一张蒙版图——已有则弹提示确认替换
                                  if (! params.maskImage.path.isEmpty())
                                  {
                                      juce::AlertWindow::showOkCancelBox (
                                          juce::MessageBoxIconType::InfoIcon,
                                          "Mask image already set",
                                          "This spectrum already has a mask image:\n  "
                                              + juce::File (params.maskImage.path).getFileName()
                                              + "\n\nThe new image will REPLACE it (one mask image per spectrum).",
                                          "Replace", "Cancel", this,
                                          juce::ModalCallbackFunction::create (
                                              [this, f] (int ok) { if (ok) applyMaskImageFile (f); }));
                                      return;
                                  }
                                  applyMaskImageFile (f);
                              });
}

// #7：蒙版图片单槽位的实际应用（chooseMaskImageFile 的确认回调）
void MainComponent::applyMaskImageFile (const juce::File& f)
{
    params.maskImage.path    = f.getFullPathName();
    params.maskImage.enabled = true;
    params.maskImage.transform = VisTransform {};   // set=false = 等比 contain 居中（#3）
    lastDir = f.getParentDirectory();
    panel.syncMaskControls();   // 反映"启用"勾选
    canvas.setEditMaskImage (false);
    canvas.repaint();
}

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------
juce::String MainComponent::formatTime (double sec)
{
    const int total = (int) std::round (sec);
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
