// =============================================================================
// SpectrumCanvas.h — 实时频谱渲染画布
//
// 职责：
//   · 每帧把 BandFrame 用 SpectrumStyle 画到一张 ARGB Image 上（与导出管线
//     VisPipeline::renderFrame 完全同源，保证"预览即所得"）
//   · 默认叠加棋盘格背景，肉眼可见透明区域
//   · 接受文件拖放 / 点击空白选择音频文件（回调给 MainComponent）
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/SpectrumStyle.h"
#include "../core/BandFrame.h"

class SpectrumCanvas : public juce::Component,
                       private juce::FileDragAndDropTarget
{
public:
    SpectrumCanvas() = default;

    // 每帧由 MainComponent 更新
    BandFrame frame;
    SpectrumStyle::RenderParams rp;
    SpectrumStyle* style = nullptr;
    bool hasAudio = false;
    bool showCheckerboard = true;

    // 回调
    std::function<void (const juce::File&)> onFileDropped;
    std::function<void ()> onEmptyClicked;

    void paint (juce::Graphics& g) override;

private:
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void mouseDown (const juce::MouseEvent&) override;

    static bool isAudioFile (const juce::String& path);
    static void drawCheckerboard (juce::Graphics& g, int w, int h, int cell = 10);

    JUCE_DECLARE_NON_COPYABLE (SpectrumCanvas)
};
