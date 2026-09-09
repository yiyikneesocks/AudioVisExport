// =============================================================================
// ParamPanel.cpp — Right-side parameter panel (English UI, avoids code-page issues)
//
// Layout model:
//   · rows: ordered list of (label, editor, height), laid out top-to-bottom in resized()
//   · Each row = 88px label on left + editor on the right; labels are lazy-created
//   · Special rows: color row (4 colour buttons tiled), width-height row (2 editors)
//   · Export section: hand-laid out below the scrollable rows block
// =============================================================================
#include "ParamPanel.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
    // Colour picker popup (ColourSelector is a ChangeBroadcaster)
    struct ColourPickSelector : juce::ColourSelector,
                                private juce::ChangeListener
    {
        std::function<void (juce::Colour)> cb;
        ColourPickSelector()
            : juce::ColourSelector (juce::ColourSelector::showAlphaChannel
                                      | juce::ColourSelector::showColourAtTop
                                      | juce::ColourSelector::showSliders
                                      | juce::ColourSelector::showColourspace)
        {
            addChangeListener (this);
        }
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            if (cb) cb (getCurrentColour());
        }
    };

    // Integer-only input filter for width/height editors
    struct IntInputFilter : juce::TextEditor::InputFilter
    {
        juce::String filterNewText (juce::TextEditor&, const juce::String& newInput) override
        {
            return newInput.retainCharacters ("0123456789");
        }
    };

    constexpr int kLabelWidth = 88;
}

ParamPanel::ParamPanel (SpectrumParams& paramsRef) : params (paramsRef)
{
    // ---- v0.5.4 #6：页签行（Spectrum / Image / Mask / Export）----
    for (juce::TextButton* b : { &tabSpectrumBtn, &tabImageBtn, &tabMaskBtn, &tabExportBtn })
    {
        styleTabButton (*b);
        addAndMakeVisible (b);
    }
    tabSpectrumBtn.onClick = [this] { setActiveTab (Tab::Spectrum); };
    tabImageBtn.onClick    = [this] { setActiveTab (Tab::Image); };
    tabMaskBtn.onClick     = [this] { setActiveTab (Tab::Mask); };
    tabExportBtn.onClick   = [this] { setActiveTab (Tab::Export); };

    // ---- Style（Spectrum 页）----
    addHeader ("Style");
    addCombo ("Render style", { "y2k-line", "bar", "bar-line", "bar-mirror", "polyline", "crystal" }, 1,
              [this] (int id)
              {
                  static const char* names[] = { "y2k-line", "bar", "bar-line", "bar-mirror", "polyline", "crystal" };
                  params.style = names[id - 1];
                  refreshStyleDependentControls();   // #3
                  notify();
              })
        ->setTooltip ("bar-mirror is deprecated: use bar / bar-line with Baseline = 50%\n"
                      "for the same mirrored look (plus full axis flexibility).");
    bandCountSliderPtr = addSlider ("Band count", 16, 512, 1, 1.0,
               [this] { return (double) params.bandCount; },
               [this] (double v) { params.setBandCount ((int) v);
                                  syncBarLayoutSliders(); notify(); });
    // v0.5.4 #25：三联动布局（gap = pitch − width，gap 可为负 = 重叠）
    auto* barWidthSlider = addSlider ("Bar width %", 2, 250, 1, 1.0,
               [this] { return (double) params.barWidthRatio * 100.0; },
               [this] (double v) { params.setBarWidth ((float) (v / 100.0));
                                  syncBarLayoutSliders(); notify(); });
    barWidthSlider->setTooltip ("Bar width (x slot). Moving this keeps the PITCH fixed;\n"
                                "the gap follows: gap = pitch - width (negative = overlap).");
    auto* barGapSlider = addSlider ("Bar gap %", -248, 248, 1, 1.0,
               [this] { return (double) params.barGapRatio * 100.0; },
               [this] (double v) { params.setBarGap ((float) (v / 100.0));
                                  syncBarLayoutSliders(); notify(); });
    barGapSlider->setTooltip ("Bar gap = pitch - width. Can be NEGATIVE (bars overlap).\n"
                              "Moving this keeps the pitch fixed and changes width.");
    // #3''：pitch = 目标带宽占画布宽 %；bandCount = floor(100/pitch%)，允许末尾留白
    auto* barPitchSlider = addSlider ("Bar pitch %", 0.2, 50, 0.1, 0.3,
               [this] { return (double) params.barPitchRatio * 100.0; },
               [this] (double v) { params.setBarPitch ((float) (v / 100.0));
                                  syncBarLayoutSliders(); notify(); });
    barPitchSlider->setTooltip ("Pitch = target band width as % of canvas width.\n"
                                "Band count = floor(100 / pitch) — a small remainder may\n"
                                "stay at the right edge. Moving Band count sets pitch = 100/N.");
    peakCapsTogglePtr = addToggle ("Peak caps", params.barParticles,
               [this] (bool v) { params.barParticles = v; notify(); });
    // v0.5.4 #4：基线轴（0=底部；0.5=镜像；画布内可拖 + 吸附）
    auto* capPullSlider = addSlider ("Cap pull", 0, 100, 1, 1.0,
               [this] { return (double) params.capPull * 100.0; },
               [this] (double v) { params.capPull = (float) (v / 100.0); notify(); });
    capPullSliderPtr = capPullSlider;
    capPullSlider->setTooltip ("Peak-cap mutual pull strength (0..100). 0 = no pulling:\n"
                               "caps fall per-band with the legacy long-slope behaviour.");
    auto* baselineSlider = addSlider ("Baseline %", 0, 100, 1, 1.0,
               [this] { return (double) params.baselineY * 100.0; },
               [this] (double v) { params.baselineY = (float) (v / 100.0); notify(); });
    baselineSlider->setTooltip ("Baseline axis: bars grow from this line, split above/below\n"
                                "proportionally (50% = mirror look). Also draggable on the canvas\n"
                                "with snapping (50% hints \"mirror\").");
    // v0.5.4 #6：line 系只画线
    lineOnlyTogglePtr = addToggle ("Line only (no fill)", params.lineOnly,
               [this] (bool v) { params.lineOnly = v; notify(); });
    lineOnlyTogglePtr->setTooltip ("Line styles (y2k / polyline / crystal): draw the curve only,\n"
                      "skip the inner tint / glass-body fill.");
    barWidthSliderPtr = barWidthSlider;
    barGapSliderPtr = barGapSlider;
    barPitchSliderPtr = barPitchSlider;

    // Peak caps (v0.5.0): "fall delay" + "fall speed" live next
    // to the toggle so the three peak-cap controls stay together (was: Time section).
    auto* peakHoldSlider = addSlider ("Peak hold ms", 0, 10000, 50, 0.5,
               [this] { return (double) params.peakHoldMs; },
               [this] (double v) { params.peakHoldMs = (float) v; notify(); });
    peakHoldSlider->setTooltip ("Peak-cap fall delay: how long a peak cap stays\n"
                                "in place before it starts falling (ms).");
    auto* peakDecaySlider = addSlider ("Peak decay dB/s", 1, 60, 1, 1.0,
               [this] { return (double) params.peakDecayDbPerSec; },
               [this] (double v) { params.peakDecayDbPerSec = (float) v; notify(); });
    peakDecaySlider->setTooltip ("Peak-cap fall speed after the hold time elapses\n"
                                 "(dB per second). Higher = faster drop.");
// v0.5.3: 峰值帽下落加速度（二阶下落：accel>0=越落越快，=0=匀速=旧行为）
    auto* peakAccelSlider = addSlider ("Decay accel dB/s²", 0,  200, 10, 1.0,
               [this] { return (double) params.peakDecayAccelDbPerSec2; },
               [this] (double v) { params.peakDecayAccelDbPerSec2 = (float) v; notify(); });
    peakAccelSlider->setTooltip ("Peak-cap fall acceleration after the hold time elapses\n"
                                 "(dB/s².  0=drifting at constant speed (legacy); higher = accelerates falling).");
    addButton ("Reset element transform", [this]
    {
        params.transform = VisTransform {};
        notify();
    });
    addCombo ("Freq scale", { "log", "linear", "mel", "bark" }, 1,
              [this] (int id)
              {
                  switch (id)
                  {
                      case 2:  params.freqScale = SpectrumParams::Linear; break;
                      case 3:  params.freqScale = SpectrumParams::Mel;    break;
                      case 4:  params.freqScale = SpectrumParams::Bark;   break;
                      default: params.freqScale = SpectrumParams::Log;    break;
                  }
                  notify();
              });
    addSlider ("Min Hz", 20, 2000, 1, 0.5,
               [this] { return (double) params.minHz; },
               [this] (double v) { params.minHz = (float) v;
                                   if (params.maxHz < params.minHz * 2)
                                       params.maxHz = params.minHz * 2;
                                   notify(); });
    addSlider ("Max Hz", 1000, 20000, 10, 0.5,
               [this] { return (double) params.maxHz; },
               [this] (double v) { params.maxHz = (float) v; notify(); });

    // ---- Time response ----
    addHeader ("Time");
    addCombo ("FPS", { "24", "30", "60" }, 2,
              [this] (int id)
              {
                  static const double fps[] = { 24.0, 30.0, 60.0 };
                  params.fps = fps[id - 1];
                  notify();
              });
    addSlider ("Attack ms", 5, 500, 1, 0.5,
               [this] { return (double) params.attackMs; },
               [this] (double v) { params.attackMs = (float) v; notify(); });
    addSlider ("Release ms", 20, 2000, 1, 0.5,
               [this] { return (double) params.releaseMs; },
               [this] (double v) { params.releaseMs = (float) v; notify(); });
    addSlider ("Temporal smooth 0..1", 0, 1, 0.01, 1.0,
               [this] { return (double) params.temporalSmoothing; },
               [this] (double v) { params.temporalSmoothing = (float) v; notify(); });

    // ---- Dynamics ----
    addHeader ("Dynamics");
    addCombo ("Y-axis curve", { "linear", "sqrt", "loglog", "perceptual" }, 1,
              [this] (int id)
              {
                  switch (id)
                  {
                      case 2:  params.dynCurve = SpectrumParams::Sqrt;       break;
                      case 3:  params.dynCurve = SpectrumParams::LogLog;     break;
                      case 4:  params.dynCurve = SpectrumParams::Perceptual; break;
                      default: params.dynCurve = SpectrumParams::LinearDyn;  break;
                  }
                  notify();
              });
    addSlider ("Gain", 0.2, 3.0, 0.01, 1.0,
               [this] { return (double) params.dynGain; },
               [this] (double v) { params.dynGain = (float) v; notify(); });
    addSlider ("Gamma", 0.3, 3.0, 0.01, 1.0,
               [this] { return (double) params.dynGamma; },
               [this] (double v) { params.dynGamma = (float) v; notify(); });
    addToggle ("Slope comp (dB/oct)", params.slopeEnabled,
               [this] (bool b) { params.slopeEnabled = b; notify(); });
    addSlider ("Slope dB/oct", 0, 12, 0.1, 1.0,
               [this] { return (double) params.slopeDbPerOct; },
               [this] (double v) { params.slopeDbPerOct = (float) v; notify(); });

    // ---- Appearance ----
    addHeader ("Appearance");

    // ---- Color row (4 buttons tiled, moved to top of Appearance in v0.5.0:
    //      it used to sit at the bottom of the section and required scrolling) ----
    addRow ("Colors", &primaryBtn);

    swatchPrimary   = params.primaryColor;
    swatchSecondary = params.secondaryColor;
    swatchPeak      = params.peakColor;
    swatchBg        = params.bgColor;

    auto setupColourBtn = [this] (juce::TextButton& btn, juce::Colour c)
    {
        btn.setColour (juce::TextButton::buttonColourId, c);
        btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible (btn);
    };
    setupColourBtn (primaryBtn, swatchPrimary);
    setupColourBtn (secondaryBtn, swatchSecondary);
    setupColourBtn (peakBtn, swatchPeak);
    // BG swatch shows transparency as a checkerboard-ish grey so it's not invisible
    bgBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    bgBtn.setColour (juce::TextButton::buttonColourId,
                     swatchBg.withAlpha ((juce::uint8) juce::jmax (60, (int) swatchBg.getAlpha())));
    addAndMakeVisible (bgBtn);

    primaryBtn.onClick = [this]
    {
        openColourPicker (primaryBtn, swatchPrimary, [this] (juce::Colour c)
        {
            params.primaryColor = c;
            swatchPrimary = c;
            notify();
        });
    };
    secondaryBtn.onClick = [this]
    {
        openColourPicker (secondaryBtn, swatchSecondary, [this] (juce::Colour c)
        {
            params.secondaryColor = c;
            swatchSecondary = c;
            notify();
        });
    };
    peakBtn.onClick = [this]
    {
        openColourPicker (peakBtn, swatchPeak, [this] (juce::Colour c)
        {
            params.peakColor = c;
            swatchPeak = c;
            notify();
        });
    };
    bgBtn.onClick = [this]
    {
        openColourPicker (bgBtn, swatchBg, [this] (juce::Colour c)
        {
            params.bgColor = c;
            swatchBg = c;
            bgBtn.setColour (juce::TextButton::buttonColourId,
                             c.withAlpha ((juce::uint8) juce::jmax (60, (int) c.getAlpha())));
            notify();
        });
    };
    bgBtn.setTooltip ("Output background colour (default fully transparent).\n"
                      "Set alpha > 0 for an opaque background in exports.");

    addSlider ("Line width", 0.5, 5.0, 0.1, 1.0,
               [this] { return (double) params.lineWidth; },
               [this] (double v) { params.lineWidth = (float) v; notify(); });
    addSlider ("Opacity", 0.05, 1.0, 0.01, 1.0,
               [this] { return (double) params.opacity; },
               [this] (double v) { params.opacity = (float) v; notify(); });
    addToggle ("Draw grid", params.drawGrid,
               [this] (bool b) { params.drawGrid = b; notify(); });
    addToggle ("Axis labels", params.drawAxisLabels,
               [this] (bool b) { params.drawAxisLabels = b; notify(); });
    addToggle ("Checkerboard BG", true,
               [this] (bool) { notify(); });

    // ---- Layers（Image 页，v0.5.4 #6）----
    setBuildingTab (1);
    addHeader ("Layers");
    addRow ("Add image", &addImageBtn);
    addRow ("Move up",   &layerUpBtn);
    addRow ("Move down", &layerDownBtn);
    addRow ("Remove",    &layerRemoveBtn);
    addRow ("", &addSpectrumBtn);
    addRow ("", &selectSpectrumBtn);
    addAndMakeVisible (addImageBtn);
    addAndMakeVisible (layerUpBtn);
    addAndMakeVisible (layerDownBtn);
    addAndMakeVisible (layerRemoveBtn);
    addAndMakeVisible (addSpectrumBtn);
    addAndMakeVisible (selectSpectrumBtn);
    addImageBtn.onClick       = [this] { if (onAddImageClicked) onAddImageClicked(); };
    layerUpBtn.onClick        = [this] { if (onLayerUp)     onLayerUp(); };
    layerDownBtn.onClick      = [this] { if (onLayerDown)   onLayerDown(); };
    layerRemoveBtn.onClick    = [this] { if (onLayerRemove) onLayerRemove(); };
    addSpectrumBtn.onClick    = [this] { if (onAddSpectrumClicked) onAddSpectrumClicked(); };
    selectSpectrumBtn.onClick = [this] { if (onSelectSpectrumClicked) onSelectSpectrumClicked(); };
    addSpectrumBtn.setTooltip ("Restore deleted spectrum layer");
    selectSpectrumBtn.setTooltip ("Select spectrum layer (useful when covered by images)");
    auto* opacitySlider = addSlider ("Img opacity %", 0, 100, 1, 1.0,
               [this] { return onReadLayerOpacity ? onReadLayerOpacity() : 100.0; },
               [this] (double v) { if (onWriteLayerOpacity) onWriteLayerOpacity (v); });
    layerOpacitySliderPtr = opacitySlider;

    // v0.5.4 #6：选中图片图层的色彩调整（只影响该图层）
    const char* adjNames[] = { "Brightness", "Contrast", "Saturation" };
    for (int ch = 0; ch < 3; ++ch)
    {
        auto* sl = addSlider (adjNames[ch], 0.0, 2.0, 0.01, 1.0,
                   [this, ch] { return onReadImageAdjust ? onReadImageAdjust (ch) : 1.0; },
                   [this, ch] (double v) { if (onWriteImageAdjust) onWriteImageAdjust (ch, v); });
        sl->setTooltip (juce::String ("Adjusts the SELECTED image layer only (1.00 = original).\n")
                          + "No layer selected = no effect.");
    }

    // v0.5.2: 吸附开关
    addRow ("", &snapToggle);
    addAndMakeVisible (snapToggle);
    snapToggle.setToggleState (params.snapEnabled, juce::dontSendNotification);
    snapToggle.onClick = [this]
    {
        params.snapEnabled = snapToggle.getToggleState();
        notify();
    };

    // ---- Spectrum Mask（Mask 页，v0.5.4 #6）----
    setBuildingTab (2);
    addHeader ("Spectrum Mask");
    addRow ("", &maskOnToggle);
    addRow ("", &maskChooseBtn);
    addRow ("", &maskStrokeToggle);
    addRow ("", &maskColorBtn);          // #7: 描边色按钮行（两按钮平铺）
    addAndMakeVisible (maskOnToggle);
    addAndMakeVisible (maskChooseBtn);
    addAndMakeVisible (maskStrokeToggle);
    addAndMakeVisible (maskColorBtn);
    addAndMakeVisible (maskAvgBtn);
    maskColorBtn.onClick = [this]
    {
        openColourPicker (maskColorBtn, params.maskImage.strokeColor, [this] (juce::Colour c)
        {
            params.maskImage.strokeColor    = c;
            params.maskImage.strokeAutoColor = false;   // 手动色
            notify();
        });
    };
    maskAvgBtn.onClick = [this] { if (onUseMaskAvgColour) onUseMaskAvgColour(); };
    maskColorBtn.setTooltip ("Outline colour: pick manually.");
    maskAvgBtn.setTooltip ("Outline colour: fix to the mask image's average colour.");
    maskOnToggle.setToggleState    (params.maskImage.enabled, juce::dontSendNotification);
    maskStrokeToggle.setToggleState(params.maskImage.strokeEnabled, juce::dontSendNotification);
    maskOnToggle.onClick = [this]
    {
        params.maskImage.enabled = maskOnToggle.getToggleState();
        if (! params.maskImage.enabled) setMaskEditChecked (false);   // 关蒙版顺带退编辑
        notify();
    };
    maskStrokeToggle.onClick = [this]
    {
        params.maskImage.strokeEnabled = maskStrokeToggle.getToggleState();
        notify();
    };
    maskChooseBtn.onClick = [this] { if (onChooseMaskImage) onChooseMaskImage(); };
    maskChooseBtn.setTooltip ("Pick an image that is shown only inside the spectrum silhouette "
                              "(bars / shape act as the mask). It moves & scales with the spectrum.");
    maskStrokeWidthSliderPtr = addSlider ("Outline width", 0.5, 12.0, 0.5, 1.0,
               [this] { return (double) params.maskImage.strokeWidth; },
               [this] (double v) { params.maskImage.strokeWidth = (float) v; notify(); });
    // v0.5.4 #4：色彩调整（只作用蒙版图片；1.0=原图）
    maskBrightnessPtr = addSlider ("Brightness", 0.0, 2.0, 0.01, 1.0,
               [this] { return (double) params.maskImage.brightness; },
               [this] (double v) { params.maskImage.brightness = (float) v; notify(); });
    maskContrastPtr = addSlider ("Contrast", 0.0, 2.0, 0.01, 1.0,
               [this] { return (double) params.maskImage.contrast; },
               [this] (double v) { params.maskImage.contrast = (float) v; notify(); });
    maskSaturationPtr = addSlider ("Saturation", 0.0, 2.0, 0.01, 1.0,
               [this] { return (double) params.maskImage.saturation; },
               [this] (double v) { params.maskImage.saturation = (float) v; notify(); });
    for (juce::Slider* sl : { maskBrightnessPtr, maskContrastPtr, maskSaturationPtr })
        if (sl != nullptr) sl->setTooltip ("Adjusts the mask image only (1.00 = original). "
                                           "Export uses the same values.");
    addRow ("", &maskEditToggle);
    addAndMakeVisible (maskEditToggle);
    maskEditToggle.setToggleState (false, juce::dontSendNotification);
    maskEditToggle.onClick = [this]
    {
        if (onToggleMaskEdit) onToggleMaskEdit (maskEditToggle.getToggleState());
    };
    maskEditToggle.setTooltip ("When on: the mask image gets its OWN handles — drag body to move, "
                               "corners/edges to stretch (snaps to the spectrum frame). Spectrum scaling "
                               "still drives the whole unit. Click outside the spectrum frame to stop editing.");

    // ---- Export（Export 页，v0.5.4 #6）----
    setBuildingTab (3);
    addHeader ("Export");
    widthEditor.setInputFilter (new IntInputFilter(), true);
    heightEditor.setInputFilter (new IntInputFilter(), true);
    widthEditor.setText (juce::String (params.width), juce::dontSendNotification);
    heightEditor.setText (juce::String (params.height), juce::dontSendNotification);
    widthEditor.onTextChange = [this]
    {
        auto v = widthEditor.getText().getIntValue();
        if (v >= 64) { params.width = v; notify(); }
    };
    heightEditor.onTextChange = [this]
    {
        auto v = heightEditor.getText().getIntValue();
        if (v >= 64) { params.height = v; notify(); }
    };
    addRow ("W x H", &widthEditor);

    addCombo ("Encoder", { "png-seq", "mov-qtrle (alpha)", "webm-vp9 (no alpha)" }, 1,
              [this] (int id)
              {
                  switch (id)
                  {
                      case 2:  params.encoder = SpectrumParams::MovQtrle; break;
                      case 3:  params.encoder = SpectrumParams::WebmVp9;  break;
                      default: params.encoder = SpectrumParams::PngSeq;   break;
                  }
                  notify();
              });

    browseBtn.onClick = [this] { if (onBrowseOutputDir) onBrowseOutputDir(); };
    exportBtn.onClick = [this] { if (onExportClicked) onExportClicked(); };
    exportVideoBtn.onClick = [this] { if (onExportVideoClicked) onExportVideoClicked(); };
    addAndMakeVisible (browseBtn);
    addAndMakeVisible (exportBtn);
    addAndMakeVisible (exportVideoBtn);

    // 一键视频导出按钮（醒目色 + tooltip 说明行为）
    exportVideoBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffbe185d));
    exportVideoBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffbe185d));
    exportVideoBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    exportVideoBtn.setTooltip ("Render current audio + settings to a transparent MOV "
                               "(QTRLE rgba) video file.\nBest alpha format for "
                               "Premiere/Vegas/Resolve. Requires ffmpeg.exe on PATH.");

    outputDirLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    outputDirLabel.setFont (juce::FontOptions (11.0f));
    outputDirLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (outputDirLabel);

    progressLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    progressLabel.setFont (juce::FontOptions (12.0f));
    progressLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (progressLabel);
}

void ParamPanel::addHeader (const juce::String& text)
{
    auto* l = new juce::Label (text, text);
    l->setFont (juce::FontOptions (13.0f, juce::Font::bold));
    l->setColour (juce::Label::textColourId, juce::Colour (0xffec4899));
    l->setJustificationType (juce::Justification::centredLeft);
    widgets.add (l);
    addAndMakeVisible (l);
    rows.push_back ({ text, l, 24, buildingTab });
    tabHeights[(size_t) buildingTab] += 24 + 4;
}

void ParamPanel::addRow (const juce::String& label, juce::Component* editor, int h)
{
    rows.push_back ({ label, editor, h, buildingTab });
    tabHeights[(size_t) buildingTab] += h + 4;
}

juce::Slider* ParamPanel::addSlider (const juce::String& label,
                                      double minValue, double maxValue, double step,
                                      double skew,
                                      std::function<double ()> read,
                                      std::function<void (double)> apply)
{
    auto* s = new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    s->setRange (minValue, maxValue, step);
    s->setSkewFactor (skew);
    s->setValue (read(), juce::dontSendNotification);
    s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    s->onValueChange = [apply, s] { apply (s->getValue()); };
    widgets.add (s);
    addAndMakeVisible (s);
    addRow (label, s);
    return s;
}

juce::ComboBox* ParamPanel::addCombo (const juce::String& label,
                                       const std::vector<juce::String>& items,
                                       int currentId,
                                       std::function<void (int)> apply)
{
    auto* c = new juce::ComboBox();
    for (int i = 0; i < (int) items.size(); ++i)
        c->addItem (items[(size_t) i], i + 1);
    c->setSelectedId (currentId, juce::dontSendNotification);
    c->onChange = [apply, c] { apply (c->getSelectedId()); };
    widgets.add (c);
    addAndMakeVisible (c);
    addRow (label, c);
    return c;
}

juce::ToggleButton* ParamPanel::addToggle (const juce::String& label, bool current,
                                            std::function<void (bool)> apply)
{
    auto* t = new juce::ToggleButton (label);
    t->setToggleState (current, juce::dontSendNotification);
    t->onClick = [apply, t] { apply (t->getToggleState()); };
    widgets.add (t);
    addAndMakeVisible (t);
    rows.push_back ({ "", t, 24, buildingTab });
    tabHeights[(size_t) buildingTab] += 24 + 4;
    return t;
}

juce::TextButton* ParamPanel::addButton (const juce::String& text,
                                         std::function<void ()> onClick)
{
    auto* b = new juce::TextButton (text);
    b->onClick = std::move (onClick);
    widgets.add (b);
    addAndMakeVisible (b);
    rows.push_back ({ juce::String(), b, 26, buildingTab });
    tabHeights[(size_t) buildingTab] += 26 + 4;
    return b;
}

void ParamPanel::notify()
{
    if (onParamsChanged)
        onParamsChanged();
}

void ParamPanel::openColourPicker (juce::TextButton& btn, juce::Colour current,
                                    std::function<void (juce::Colour)> apply)
{
    auto selector = std::make_unique<ColourPickSelector>();
    selector->setSize (300, 340);
    selector->setCurrentColour (current);
    selector->cb = [&btn, apply] (juce::Colour c)
    {
        btn.setColour (juce::TextButton::buttonColourId, c);
        apply (c);
    };
    juce::CallOutBox::launchAsynchronously (std::move (selector),
                                            btn.getScreenBounds(), nullptr);
}

void ParamPanel::setMaskEditChecked (bool b)
{
    if (maskEditToggle.getToggleState() != b)
        maskEditToggle.setToggleState (b, juce::dontSendNotification);
}

// v0.5.4 #25：三联动滑条回填（任一动，另两条显示同步；dontSendNotification 防递归）
void ParamPanel::syncBarLayoutSliders()
{
    if (barWidthSliderPtr != nullptr)
        barWidthSliderPtr->setValue (params.barWidthRatio * 100.0, juce::dontSendNotification);
    if (barGapSliderPtr != nullptr)
        barGapSliderPtr->setValue (params.barGapRatio * 100.0, juce::dontSendNotification);
    if (barPitchSliderPtr != nullptr)
        barPitchSliderPtr->setValue (params.barPitchRatio * 100.0, juce::dontSendNotification);
    if (bandCountSliderPtr != nullptr)
        bandCountSliderPtr->setValue ((double) params.bandCount, juce::dontSendNotification);
}

void ParamPanel::syncMaskControls()
{
    maskOnToggle.setToggleState    (params.maskImage.enabled, juce::dontSendNotification);
    maskStrokeToggle.setToggleState(params.maskImage.strokeEnabled, juce::dontSendNotification);
    const auto sc = params.maskImage.strokeColor;
    for (juce::TextButton* b : { &maskColorBtn, &maskAvgBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, sc);
        b->setColour (juce::TextButton::textColourOffId,
                      sc.getBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white);
    }
    if (maskStrokeWidthSliderPtr != nullptr)
        maskStrokeWidthSliderPtr->setValue (params.maskImage.strokeWidth, juce::dontSendNotification);
    if (maskBrightnessPtr != nullptr)
        maskBrightnessPtr->setValue (params.maskImage.brightness, juce::dontSendNotification);
    if (maskContrastPtr != nullptr)
        maskContrastPtr->setValue (params.maskImage.contrast, juce::dontSendNotification);
    if (maskSaturationPtr != nullptr)
        maskSaturationPtr->setValue (params.maskImage.saturation, juce::dontSendNotification);
}

void ParamPanel::setProgressText (const juce::String& s)
{
    progressLabel.setText (s, juce::dontSendNotification);
}

void ParamPanel::setOutputDirText (const juce::String& s)
{
    outputDirLabel.setText (s, juce::dontSendNotification);
    outputDirLabel.setHelpText (s);
}

void ParamPanel::setExportEnabled (bool b)
{
    exportBtn.setEnabled (b);
    exportVideoBtn.setEnabled (b);
}

// v0.5.2: 画布选中元素变化时同步图层区控件（每 tick 由 MainComponent 调用）
void ParamPanel::refreshLayerControls (bool imageSelected, double opacityPct,
                                       bool spectrumPresent)
{
    if (layerOpacitySliderPtr != nullptr)
        layerOpacitySliderPtr->setValue (opacityPct, juce::dontSendNotification);
    // 频谱按钮状态
    addSpectrumBtn.setEnabled (! spectrumPresent);
    selectSpectrumBtn.setEnabled (spectrumPresent);
    // Remove 按钮：有选中元素或频谱在场时可用
    layerRemoveBtn.setEnabled (imageSelected || spectrumPresent);

    // v0.5.4 #6：选中变化 → 跟随跳页（Image↔Spectrum；手动点过页签后仍以新选中为准）
    syncSelectionTab (imageSelected);
}

// =============================================================================
// v0.5.4 #6：选项卡机制
// =============================================================================
void ParamPanel::styleTabButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a33));
    b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffbe185d));
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    b.setClickingTogglesState (false);
}

void ParamPanel::showTab (Tab t, bool pinned)
{
    const int nt = (int) t;
    if (nt == currentTab && ! pinned)
        return;                        // 同页跟随跳过（避免每 tick 重排）
    currentTab = nt;
    selectionPinned_ = pinned;
    // 高亮当前页签
    for (juce::TextButton* b : { &tabSpectrumBtn, &tabImageBtn, &tabMaskBtn, &tabExportBtn })
        b->setColour (juce::TextButton::buttonColourId,
                      (b == &tabSpectrumBtn && nt == 0) || (b == &tabImageBtn && nt == 1)
                   || (b == &tabMaskBtn && nt == 2)   || (b == &tabExportBtn && nt == 3)
                          ? juce::Colour (0xffbe185d) : juce::Colour (0xff2a2a33));
    resized();
    if (onPanelHeightChanged)
        onPanelHeightChanged();
}

void ParamPanel::setActiveTab (Tab t)       { showTab (t, true); }   // 显式切换=钉住

void ParamPanel::syncSelectionTab (bool imageSelected)
{
    if (imageSelected != lastImageSelected_)
    {
        lastImageSelected_ = imageSelected;
        showTab (imageSelected ? Tab::Image : Tab::Spectrum, false);
    }
}

void ParamPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 4);

    // ---- 页签行 ----
    auto tabRow = area.removeFromTop (26);
    const int tw = tabRow.getWidth() / 4;
    tabSpectrumBtn.setBounds (tabRow.removeFromLeft (tw).reduced (2));
    tabImageBtn.setBounds    (tabRow.removeFromLeft (tw).reduced (2));
    tabMaskBtn.setBounds     (tabRow.removeFromLeft (tw).reduced (2));
    tabExportBtn.setBounds   (tabRow.reduced (2));
    area.removeFromTop (4);

    // ---- 只布局当前页的 rows（其余页隐藏，防误触 + 防绘制穿透）----
    for (auto& r : rows)
    {
        const bool on = (r.tab == currentTab);
        if (r.editor != nullptr)
            r.editor->setVisible (on);
        if (! r.label.isEmpty())
        {
            auto it = rowLabels.find (r.editor);
            if (it != rowLabels.end())
                it->second->setVisible (on);
        }
        if (! on) continue;

        auto rowBounds = area.removeFromTop (r.height);
        area.removeFromTop (4);

        // v0.5.4 #7: 描边色两按钮平铺
        if (r.editor == &maskColorBtn)
        {
            auto w = rowBounds.getWidth() / 2;
            maskColorBtn.setBounds (rowBounds.removeFromLeft (w).reduced (2));
            maskAvgBtn.setBounds  (rowBounds.reduced (2));
            continue;
        }
        // Special: color row — 4 tiled buttons (v0.5.0: added BG)
        if (r.editor == &primaryBtn)
        {
            auto w = rowBounds.getWidth() / 4;
            primaryBtn.setBounds   (rowBounds.removeFromLeft (w).reduced (2));
            secondaryBtn.setBounds (rowBounds.removeFromLeft (w).reduced (2));
            peakBtn.setBounds      (rowBounds.removeFromLeft (w).reduced (2));
            bgBtn.setBounds        (rowBounds.reduced (2));
            continue;
        }
        // Special: width/height — 2 editors
        if (r.editor == &widthEditor)
        {
            auto w = rowBounds.getWidth() / 2;
            widthEditor.setBounds (rowBounds.removeFromLeft (w).reduced (2));
            heightEditor.setBounds (rowBounds.reduced (2));
            continue;
        }
        // Full-width button (no side label)
        if (r.label.isEmpty() && dynamic_cast<juce::TextButton*> (r.editor) != nullptr)
        {
            r.editor->setBounds (rowBounds.reduced (2, 2));
            continue;
        }
        // Header label editor (no side label)
        if (auto* asLabel = dynamic_cast<juce::Label*> (r.editor))
        {
            asLabel->setBounds (rowBounds);
            continue;
        }
        // ToggleButton carries its own text
        if (auto* asToggle = dynamic_cast<juce::ToggleButton*> (r.editor))
        {
            asToggle->setBounds (rowBounds);
            continue;
        }
        // Regular row: label + editor
        auto labelBounds = rowBounds.removeFromLeft (kLabelWidth);
        if (r.editor != nullptr)
            r.editor->setBounds (rowBounds);

        if (! r.label.isEmpty())
        {
            juce::Label* rowLabel = nullptr;
            auto it = rowLabels.find (r.editor);
            if (it == rowLabels.end())
            {
                rowLabel = new juce::Label (r.label, r.label);
                rowLabel->setFont (juce::FontOptions (12.0f));
                rowLabel->setColour (juce::Label::textColourId, juce::Colours::white);
                rowLabel->setJustificationType (juce::Justification::centredLeft);
                rowLabels[r.editor] = std::unique_ptr<juce::Label> (rowLabel);
                addAndMakeVisible (rowLabel);
            }
            else
            {
                rowLabel = it->second.get();
            }
            rowLabel->setBounds (labelBounds);
        }
    }

    // Export 固定条（所有页可见：一键导出不隔页）
    auto exp = area.removeFromBottom (exportAreaHeight);
    auto line1 = exp.removeFromTop (30);
    browseBtn.setBounds (line1.removeFromLeft (90).reduced (2));
    exportBtn.setBounds (line1.reduced (2));
    auto videoLine = exp.removeFromTop (34);
    exportVideoBtn.setBounds (videoLine.reduced (2));
    outputDirLabel.setBounds (exp.removeFromTop (26));
    progressLabel.setBounds (exp.removeFromTop (24));
}

// v0.5.4 #3：样式专属控件置灰
void ParamPanel::refreshStyleDependentControls()
{
    const juce::String st = params.style;
    const bool barFam   = (st == "bar" || st == "bar-line" || st == "bar-mirror");
    const bool lineFam  = (st == "y2k-line" || st == "polyline" || st == "crystal");
    if (barWidthSliderPtr != nullptr)  { barWidthSliderPtr->setEnabled (barFam);
                                          if (auto* l = rowLabels[barWidthSliderPtr].get()) l->setEnabled (barFam); }
    if (barGapSliderPtr != nullptr)    { barGapSliderPtr->setEnabled (barFam);
                                          if (auto* l = rowLabels[barGapSliderPtr].get()) l->setEnabled (barFam); }
    if (barPitchSliderPtr != nullptr)  { barPitchSliderPtr->setEnabled (barFam);
                                          if (auto* l = rowLabels[barPitchSliderPtr].get()) l->setEnabled (barFam); }
    if (peakCapsTogglePtr != nullptr)   peakCapsTogglePtr->setEnabled (barFam);
    if (capPullSliderPtr != nullptr)   { capPullSliderPtr->setEnabled (st == "bar-line");
                                          if (auto* l = rowLabels[capPullSliderPtr].get()) l->setEnabled (st == "bar-line"); }
    if (lineOnlyTogglePtr != nullptr)   lineOnlyTogglePtr->setEnabled (lineFam);
}

// v0.5.4 #6：Mask 页拖放蒙版图（区别于画布拖放=普通图片图层）
bool ParamPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (currentTab != (int) Tab::Mask)
        return false;
    for (const auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif"
            || ext == ".bmp" || ext == ".webp")
            return true;
    }
    return false;
}

void ParamPanel::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        const juce::File file (f);
        const auto ext = file.getFileExtension().toLowerCase();
        if ((ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif"
             || ext == ".bmp" || ext == ".webp") && onMaskFileDropped)
        {
            onMaskFileDropped (file);
            return;
        }
    }
}
