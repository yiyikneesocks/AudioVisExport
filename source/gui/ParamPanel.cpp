// =============================================================================
// ParamPanel.cpp — Right-side parameter panel (English UI, avoids code-page issues)
//
// Layout model:
//   · rows: ordered list of (label, editor, height), laid out top-to-bottom in resized()
//   · Each row = 88px label on left + editor on the right; labels are lazy-created
//   · Special rows: color row (3 colour buttons tiled), width-height row (2 editors)
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
    // ---- Style ----
    addHeader ("Style");
    addCombo ("Render style", { "y2k-line", "bar", "polyline", "crystal" }, 1,
              [this] (int id)
              {
                  static const char* names[] = { "y2k-line", "bar", "polyline", "crystal" };
                  params.style = names[id - 1];
                  notify();
              });
    addSlider ("Band count", 16, 512, 1, 1.0,
               [this] { return (double) params.bandCount; },
               [this] (double v) { params.bandCount = (int) v; notify(); });
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
    addSlider ("Peak hold ms", 0, 10000, 50, 0.5,
               [this] { return (double) params.peakHoldMs; },
               [this] (double v) { params.peakHoldMs = (float) v; notify(); });
    addSlider ("Peak decay dB/s", 1, 60, 1, 1.0,
               [this] { return (double) params.peakDecayDbPerSec; },
               [this] (double v) { params.peakDecayDbPerSec = (float) v; notify(); });
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

    // ---- Color row (3 buttons tiled) ----
    addRow ("Colors", &primaryBtn);

    swatchPrimary   = params.primaryColor;
    swatchSecondary = params.secondaryColor;
    swatchPeak      = params.peakColor;

    auto setupColourBtn = [this] (juce::TextButton& btn, juce::Colour c)
    {
        btn.setColour (juce::TextButton::buttonColourId, c);
        btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible (btn);
    };
    setupColourBtn (primaryBtn, swatchPrimary);
    setupColourBtn (secondaryBtn, swatchSecondary);
    setupColourBtn (peakBtn, swatchPeak);

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

    // ---- Export ----
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

    addCombo ("Encoder", { "png-seq", "webm-vp9 (alpha)", "mov-qtrle (alpha)" }, 1,
              [this] (int id)
              {
                  switch (id)
                  {
                      case 2:  params.encoder = SpectrumParams::WebmVp9;  break;
                      case 3:  params.encoder = SpectrumParams::MovQtrle; break;
                      default: params.encoder = SpectrumParams::PngSeq;   break;
                  }
                  notify();
              });

    browseBtn.onClick = [this] { if (onBrowseOutputDir) onBrowseOutputDir(); };
    exportBtn.onClick = [this] { if (onExportClicked) onExportClicked(); };
    addAndMakeVisible (browseBtn);
    addAndMakeVisible (exportBtn);

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
    rows.push_back ({ text, l, 24 });
    contentHeight += 24 + 4;
}

void ParamPanel::addRow (const juce::String& label, juce::Component* editor, int h)
{
    rows.push_back ({ label, editor, h });
    contentHeight += h + 4;
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
    rows.push_back ({ "", t, 24 });
    contentHeight += 24 + 4;
    return t;
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
}

void ParamPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 4);

    for (auto& r : rows)
    {
        auto rowBounds = area.removeFromTop (r.height);
        area.removeFromTop (4);

        // Special: color row — 3 tiled buttons
        if (r.editor == &primaryBtn)
        {
            auto w = rowBounds.getWidth() / 3;
            primaryBtn.setBounds (rowBounds.removeFromLeft (w).reduced (2));
            secondaryBtn.setBounds (rowBounds.removeFromLeft (w).reduced (2));
            peakBtn.setBounds (rowBounds.reduced (2));
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

    // Export section
    auto exp = area.removeFromTop (exportAreaHeight);
    auto line1 = exp.removeFromTop (30);
    browseBtn.setBounds (line1.removeFromLeft (90).reduced (2));
    exportBtn.setBounds (line1.reduced (2));
    outputDirLabel.setBounds (exp.removeFromTop (26));
    progressLabel.setBounds (exp.removeFromTop (24));
}
