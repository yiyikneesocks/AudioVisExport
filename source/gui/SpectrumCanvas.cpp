// =============================================================================
// SpectrumCanvas.cpp — 实时频谱渲染画布实现
// =============================================================================
#include "SpectrumCanvas.h"
#include <cmath>

void SpectrumCanvas::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const int w = juce::jmax (1, bounds.getWidth());
    const int h = juce::jmax (1, bounds.getHeight());

    // 与导出管线同源：ARGB Image（默认全透明）+ style.render
    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics ig (img);

        if (showCheckerboard)
            drawCheckerboard (ig, w, h);

        rp.width  = w;
        rp.height = h;
        const auto canvas = juce::Rectangle<int> ((int) rp.paddingLeft, (int) rp.paddingTop,
                                                  w - (int) (rp.paddingLeft + rp.paddingRight),
                                                  h - (int) (rp.paddingTop + rp.paddingBottom));
        if (style != nullptr)
            style->render (ig, canvas, frame, rp);
    }
    g.drawImageAt (img, 0, 0);

    if (! hasAudio)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawFittedText ("Drag & drop a WAV or AIFF file to begin\n(or click here to browse)",
                          getLocalBounds(), juce::Justification::centred, 2);
    }
}

bool SpectrumCanvas::isAudioFile (const juce::String& path)
{
    auto ext = juce::File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff";
}

bool SpectrumCanvas::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f))
            return true;
    return false;
}

void SpectrumCanvas::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (isAudioFile (f) && onFileDropped)
        {
            onFileDropped (juce::File (f));
            return;
        }
}

void SpectrumCanvas::mouseDown (const juce::MouseEvent&)
{
    if (! hasAudio && onEmptyClicked)
        onEmptyClicked();
}

void SpectrumCanvas::drawCheckerboard (juce::Graphics& g, int w, int h, int cell)
{
    g.fillAll (juce::Colour (0xff3a3a44));
    g.setColour (juce::Colour (0xff4a4a55));
    for (int y = 0; y < h; y += cell)
        for (int x = ((y / cell) % 2) * cell; x < w; x += cell * 2)
            g.fillRect (x, y, juce::jmin (cell, w - x), juce::jmin (cell, h - y));
}
