// =============================================================================
// ParamPanel.h — 右侧参数面板
//
// 覆盖主要可调参数（滑块 = 可拖动 + 数字输入框，双方式）：
//   样式 / 频带数 / 频率标度 / 频率范围 / 动态曲线 / 增益 / gamma /
//   attack / release / 峰值保持 / 时间平滑 / 斜率 / 线宽 / 不透明度 /
//   四色选择（Primary/Secondary/Peak/BG）/ 网格开关 / 棋盘格预览 / 导出宽高 / 编码器 / 导出按钮
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
    std::function<void ()> onExportVideoClicked;
    std::function<void ()> onBrowseOutputDir;

    // ---- 图层（Layers）区回调：MainComponent 接线，操作画布当前选中元素 ----
    std::function<void ()>     onAddImageClicked;    // 弹文件框添加图片图层
    std::function<void ()>     onLayerUp;            // 选中图层上移一层
    std::function<void ()>     onLayerDown;          // 选中图层下移一层
    std::function<void ()>     onLayerRemove;        // 移除选中图层（图片或频谱）
    std::function<double ()>   onReadLayerOpacity;   // 读选中图片透明度（0..100；未选中返回 100）
    std::function<void (double)> onWriteLayerOpacity;
    // v0.5.1: 图层上下（与频谱的层级关系）读写；未选中图片时读写无效果
    std::function<bool ()>     onReadLayerAbove;
    std::function<void (bool)> onWriteLayerAbove;
    // v0.5.2: 频谱层操作 + 吸附开关
    std::function<void ()>     onAddSpectrumClicked;
    std::function<void ()>     onSelectSpectrumClicked;

    void setProgressText (const juce::String& s);
    void setOutputDirText (const juce::String& s);
    void setExportEnabled (bool b);

    // v0.5.2: 画布选中元素变化时同步图层区控件状态（MainComponent 每 tick 调用）
    void refreshLayerControls (bool imageSelected, bool above, double opacityPct,
                               bool spectrumPresent);

    // 棋盘格预览开关状态（MainComponent 读取）
    bool getCheckerPreview() const noexcept { return checkerToggle.getToggleState(); }

private:
    SpectrumParams& params;

    struct Row { juce::String label; juce::Component* editor; int height; };
    std::vector<Row> rows;
    juce::OwnedArray<juce::Component> widgets;                 // 统一持有所有动态控件
    std::map<juce::Component*, std::unique_ptr<juce::Label>> rowLabels;
    int contentHeight = 0;
    int exportAreaHeight = 152;

    // 控件（固定成员）
    juce::ComboBox styleBox, freqScaleBox, dynCurveBox, fpsBox, encoderBox;
    juce::ToggleButton slopeToggle    { "Slope comp (dB/oct)" };
    juce::ToggleButton gridToggle     { "Draw grid" };
    juce::ToggleButton axisLabelToggle{ "Axis labels" };
    juce::ToggleButton checkerToggle  { "Checkerboard BG" };
    juce::TextButton primaryBtn{ "Primary" }, secondaryBtn{ "Secondary" }, peakBtn{ "Peak" }, bgBtn{ "BG" };
    juce::TextButton browseBtn{ "Browse" }, exportBtn{ "Export" }, exportVideoBtn{ "Export Video" };
    juce::TextButton addImageBtn{ "Add image..." }, layerUpBtn{ "Up" },
                     layerDownBtn{ "Down" }, layerRemoveBtn{ "Remove" },
                     addSpectrumBtn{ "Add spectrum" }, selectSpectrumBtn{ "Select spectrum" };
    juce::ToggleButton layerAboveToggle { "Above spectrum" };   // v0.5.1 图层上下
    juce::ToggleButton snapToggle { "Snapping" };               // v0.5.2 吸附开关
    juce::Slider layerOpacitySlider;                            // v0.5.1 需要引用以刷新
    juce::Slider* layerOpacitySliderPtr = nullptr;
    juce::TextEditor widthEditor, heightEditor;
    juce::Label outputDirLabel, progressLabel;
    juce::Colour swatchPrimary, swatchSecondary, swatchPeak, swatchBg;
    juce::TooltipWindow tooltipWindow;   // 让本面板内 setTooltip 生效

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
    juce::TextButton* addButton (const juce::String& text,
                                 std::function<void ()> onClick);
    void notify();
    void openColourPicker (juce::TextButton& btn, juce::Colour current,
                           std::function<void (juce::Colour)> apply);

    JUCE_DECLARE_NON_COPYABLE (ParamPanel)
};
