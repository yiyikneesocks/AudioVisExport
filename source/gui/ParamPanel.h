// =============================================================================
// ParamPanel.h — 右侧参数面板
//
// 覆盖主要可调参数（滑块 = 可拖动 + 数字输入框，双方式）：
//   样式 / 频带数 / 频率标度 / 频率范围 / 动态曲线 / 增益 / gamma /
//   attack / release / 峰值保持 / 时间平滑 / 斜率 / 线宽 / 不透明度 /
//   三色选择 / 网格开关 / 棋盘格预览 / 导出宽高 / 编码器 / 导出按钮
// 任何改动 → onParamsChanged（MainComponent 下一帧生效）
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/SpectrumParams.h"
#include <map>
#include <memory>

class ParamPanel : public juce::Component
{
public:
    explicit ParamPanel (SpectrumParams& paramsRef);

    void resized() override;

    int getPreferredHeight() const noexcept { return contentHeight + exportAreaHeight; }

    // 回调（MainComponent 设置）
    std::function<void ()> onParamsChanged;
    std::function<void ()> onExportClicked;
    std::function<void ()> onBrowseOutputDir;

    void setProgressText (const juce::String& s);
    void setOutputDirText (const juce::String& s);
    void setExportEnabled (bool b);

    // 棋盘格预览开关状态（MainComponent 读取）
    bool getCheckerPreview() const noexcept { return checkerToggle.getToggleState(); }

private:
    SpectrumParams& params;

    struct Row { juce::String label; juce::Component* editor; int height; };
    std::vector<Row> rows;
    juce::OwnedArray<juce::Component> widgets;                 // 统一持有所有动态控件
    std::map<juce::Component*, std::unique_ptr<juce::Label>> rowLabels;
    int contentHeight = 0;
    int exportAreaHeight = 118;

    // 控件（固定成员）
    juce::ComboBox styleBox, freqScaleBox, dynCurveBox, fpsBox, encoderBox;
    juce::ToggleButton slopeToggle{ "每八度斜率补偿" };
    juce::ToggleButton gridToggle{ "绘制网格" };
    juce::ToggleButton axisLabelToggle{ "绘制坐标轴标签" };
    juce::ToggleButton checkerToggle{ "预览棋盘格背景" };
    juce::TextButton primaryBtn{ "主色" }, secondaryBtn{ "辅色" }, peakBtn{ "峰值" };
    juce::TextButton browseBtn{ "浏览..." }, exportBtn{ "导出" };
    juce::TextEditor widthEditor, heightEditor;
    juce::Label outputDirLabel, progressLabel;
    juce::Colour swatchPrimary, swatchSecondary, swatchPeak;

    // 布局辅助
    void addHeader (const juce::String& text);
    void addRow (const juce::String& label, juce::Component* editor, int h = 26);
    juce::Slider* addSlider (const juce::String& label,
                             double minValue, double maxValue, double step,
                             double skew,
                             std::function<double ()> read,
                             std::function<void (double)> apply);
    juce::ComboBox* addCombo (const juce::String& label,
                              const std::vector<juce::String>& items,
                              int currentId,
                              std::function<void (int)> apply);
    juce::ToggleButton* addToggle (const juce::String& label, bool current,
                                   std::function<void (bool)> apply);
    void notify();
    void openColourPicker (juce::TextButton& btn, juce::Colour current,
                           std::function<void (juce::Colour)> apply);

    JUCE_DECLARE_NON_COPYABLE (ParamPanel)
};
