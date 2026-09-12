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
#include <vector>

class ParamPanel : public juce::Component,
                   public juce::FileDragAndDropTarget,
                   public juce::ListBoxModel          // v0.5.4 #H：可见图层栈
{
public:
    // v0.5.4 #H 图层行的身份：-2 = 蒙版图片，-1 = 频谱，>=0 = params.images 下标
    static constexpr int layerTagMask     = -2;
    static constexpr int layerTagSpectrum = -1;
public:
    explicit ParamPanel (SpectrumParams& paramsRef);

    void resized() override;

    int getPreferredHeight() const noexcept { return 30 + activeTabHeight() + exportAreaHeight; }

    // ---- v0.5.4 #6：选项卡 ----
    enum class Tab { Spectrum = 0, Image, Mask, Export };
    void setActiveTab (Tab t);          // 切页（含 MainComponent 选中联动入口）
    Tab  activeTab() const noexcept { return static_cast<Tab> (currentTab); }
    // #6: 画布选中变化时跟随（仅状态翻转时跳页；手动点过页签后停止跟随直到下次选中变化）
    void syncSelectionTab (bool imageSelected);

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
    // v0.5.2: 频谱层操作 + 吸附开关
    std::function<void ()>     onAddSpectrumClicked;
    std::function<void ()>     onSelectSpectrumClicked;
    // v0.5.4: 频谱蒙版图片
    std::function<void ()>     onChooseMaskImage;      // 弹文件框选蒙版图
    std::function<void (bool)> onToggleMaskEdit;       // 切换"编辑图片位置"模式
    std::function<void ()>     onUseMaskAvgColour;     // v0.5.4 #7：描边色固定为平均色
    // v0.5.4 #6: 选中图片图层的色彩调整（ch: 0=B 1=C 2=S；只影响选中图层）
    std::function<double (int)>    onReadImageAdjust;
    std::function<void (int, double)> onWriteImageAdjust;
    // v0.5.4 #6: 拖图片到 Mask 页签 → 设为蒙版图（区别于画布拖放=图片图层）
    std::function<void (const juce::File&)> onMaskFileDropped;
    // v0.5.4 #H：图层列表点击 → MainComponent 把画布选中切到该元素
    std::function<void (int)> onSelectLayerRow;      // 传 layerTag*（旧的单选路径）
    // v0.5.6 #2：图层列表 Ctrl/Shift 多选 → 回传整个选中 tag 集合 + 被点击 tag + 是否点到蒙版行
    std::function<void (const std::vector<int>& tags, int clickedTag, bool clickedMask)> onSelectLayerRows;
    // v0.5.4 #6: 页签切换后高度变化 → MainComponent 重排 viewport
    std::function<void ()>     onPanelHeightChanged;

    void setProgressText (const juce::String& s);
    void setOutputDirText (const juce::String& s);
    void setExportEnabled (bool b);

    // v0.5.2: 画布选中元素变化时同步图层区控件状态（MainComponent 每 tick 调用）
    void refreshLayerControls (bool imageSelected, double opacityPct,
                               bool spectrumPresent);
    // v0.5.4 #H：图层列表数据刷新（MainComponent 每 tick 在选中变化时调；内部按内容哈希去抖）
    void refreshLayerList (const std::vector<int>& sel, bool maskEditMode);  // v0.5.6：多选集合驱动高亮
    // v0.5.4 #H：图层行数（诊断/测试用；vis_tabs_test 靠它验证列表真的被填过）
    int layerRowCount() const noexcept { return (int) layerRows.size(); }

    // ---- v0.5.4 #G：切页残留自检（安全网与 vis_tabs_test 共用同一判据）----
    //   返回"既不属于任何行、也不是行标签、又没登记为常驻"的直接子组件。
    //   resized() 会把它们隐藏（漏登记的表现 = 控件不出现，而不是残留在别的页）；
    //   vis_tabs_test 断言本列表恒空。
    std::vector<juce::Component*> findUnownedChildren() const;

    // 棋盘格预览开关状态（MainComponent 读取）
    bool getCheckerPreview() const noexcept { return checkerToggle.getToggleState(); }

    // v0.5.4: 频谱蒙版图片控件同步（MainComponent 调用）
    void setMaskEditChecked (bool b);   // 编辑模式被"点范围外"自动退出时同步勾选
    void syncMaskControls();            // 选图后从 params 回填蒙版控件勾选态
    // v0.5.5 新 #3：Ctrl+Z 恢复整份 params 后，把所有滑块/开关/下拉**回填显示值**
    //（滑块的 read lambda 只在构造时求值一次，不主动重设就显示旧值）。
    void syncAllFromParams();

private:
    SpectrumParams& params;

    // ---- v0.5.4 #6/#G：选项卡布局 ----
    // 每个 Row 归属一页；**一行可挂多个控件**（等宽平铺），切页时整行一起显隐。
    // #G 教训：旧 Row 只有一个 editor，同行第二个控件（Use average / Secondary / Peak / BG /
    //   高度输入框）从没登记过 → showTab 永远不隐藏它 → 切页后残留在别的页上。
    struct Row
    {
        juce::String label;
        std::vector<juce::Component*> editors;   // editors[0] = 主控件（标签与特殊布局的锚点）
        int height;
        int tab = 0;
        juce::Component* primary() const noexcept
        { return editors.empty() ? nullptr : editors.front(); }
    };
    std::vector<Row> rows;
    std::vector<int> tabHeights { 0, 0, 0, 0 };      // 与 Tab 枚举序一致
    int currentTab = 0;
    int activeTabHeight() const noexcept
    { return (currentTab >= 0 && currentTab < (int) tabHeights.size()) ? tabHeights[(size_t) currentTab] : 0; }
    juce::TextButton tabSpectrumBtn{ "Spectrum" }, tabImageBtn{ "Layers" },
                     tabMaskBtn{ "Mask" }, tabExportBtn{ "Export" };
    void styleTabButton (juce::TextButton& b);
    void showTab (Tab t, bool pinned);               // 内部：切页 + 高亮 + 回调
    bool lastImageSelected_ = false;                 // #6: 选中状态翻转检测
    bool selectionPinned_ = false;                   // #6: 手动点过页签→暂停跟随

    std::vector<juce::Component*> widgetOrder;                // 仅布局顺序参考
    // v0.5.4 #H：可见图层栈
    // 注意：构造时**不**把 this 传进 ListBox（成员声明顺序上 layerList 先于 layerRows，
    // 且在完整对象构造完成前外泄 this 是隐患）；改在构造函数体里 setModel (this)。
    juce::ListBox layerList { "LayerStack" };
    struct LayerRow { int tag = layerTagSpectrum; juce::String text, colour; bool selected = false; };
    std::vector<LayerRow> layerRows;
    juce::String layerListHash;                       // 内容哈希：没变就不重建（每 tick 调也便宜）

    // juce::ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    // 新1-B：ListView 自己吞掉 Delete 转交 model->deleteKeyPressed()，主组件收不到 →
    //   列表里选中图层按删除"没反应"。在此实现：先选中该行（同步画布），再走同一删除回调。
    void deleteKeyPressed (int lastRowSelected) override;

    juce::OwnedArray<juce::Component> widgets;                // 统一持有所有动态控件
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
    // v0.5.4: 频谱蒙版图片控件
    juce::TextButton maskChooseBtn{ "Choose mask image..." };
    juce::ToggleButton maskOnToggle { "Use spectrum mask" };
    juce::ToggleButton maskStrokeToggle { "Outline (avg color)" };
    // v0.5.4 #7：描边色两按钮——手动选色 / 用图片平均色（固定为已算值）
    juce::TextButton maskColorBtn { "Border colour" }, maskAvgBtn { "Use average" };
    juce::ToggleButton maskEditToggle { "Edit image position" };
    // v0.5.5 #5 + v0.5.6 #1b：描边改为"开关组"（实时/逐柱）；outlineMode 字符串仍是唯一真源
    juce::ToggleButton* outlineRealtimePtr = nullptr;   // (3) 实时变色：关=固定色，开=按可见区实时平均
    juce::ToggleButton* outlinePerBarPtr   = nullptr;   // (4) 逐柱变色：仅实时开时可用，关=整块一色
    // 上/左/右三边，各：开关 + 厚度 + 透明度 + 阴影（底部已取消）
    juce::ToggleButton* outEdgeTogPtr[3]    = { nullptr, nullptr, nullptr };
    juce::Slider*       outEdgeWPtr[3]      = { nullptr, nullptr, nullptr };
    juce::Slider*       outEdgeAlphaPtr[3]  = { nullptr, nullptr, nullptr };
    juce::Slider*       outEdgeShadowPtr[3] = { nullptr, nullptr, nullptr };
    juce::Slider* outlineFpsPtr = nullptr;
    juce::ToggleButton* outlineTemporalPtr = nullptr;
    juce::Slider* maskStrokeWidthSliderPtr = nullptr;
    juce::Slider* maskBrightnessPtr = nullptr;
    juce::Slider* maskContrastPtr = nullptr;
    juce::Slider* maskSaturationPtr = nullptr;
    juce::Slider* barWidthSliderPtr = nullptr;    // v0.5.4 #25 三联动
    juce::Slider* barGapSliderPtr = nullptr;
    juce::Slider* barPitchSliderPtr = nullptr;
    juce::Slider* bandCountSliderPtr = nullptr;
    juce::ToggleButton* peakCapsTogglePtr = nullptr;   // #3: bar 系样式才有效
    juce::ToggleButton* lineOnlyTogglePtr = nullptr;  // #3: line 系样式才有效
    juce::Slider* capPullSliderPtr = nullptr;         // #3: 仅 bar-line
public:
    void refreshStyleDependentControls();   // #3: 依当前样式置灰不适用控件
    // v0.5.6 新1-b：边框控件按"总开关 → 实时/逐柱 → 固定色按钮/性能项"层级互斥置灰，
    //   并把 realtime/perbar 两个开关的勾选态从 outlineMode 反推。所有改描边状态处都要调它。
    void syncOutlineEnablement();
    void setSliderEnabled (juce::Slider* s, bool en);   // 连同行标签一起灰
    void syncBarLayoutSliders();                  // #25: 联动回填另两条滑条
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
    // v0.5.4 #G：一行放多个控件（等宽平铺）。**同页并排的新控件一律走这里**——
    //   整行的显隐/布局由 rows 统一驱动，做不到"漏登记残留"。
    void addRowGroup (const juce::String& label, std::vector<juce::Component*> editors, int h = 26);
    // v0.5.4 #G：登记"常驻部件"（页签行、底部导出条）；未登记且不属于任何行的子组件会被
    //   resized() 末尾的安全网一律隐藏（debug 下 jassert 提醒），失败方向是"看不见"而非"残留"。
    void keepVisibleOnAllTabs (juce::Component* c);
    std::vector<juce::Component*> chrome;
    void setBuildingTab (int t) noexcept { buildingTab = t; }   // #6: 之后 add* 的行归此页
    int  buildingTab = 0;
    // v0.5.5 #3：登记 read getter 以便 syncAllFromParams 统一回填
    std::vector<std::pair<juce::Slider*, std::function<double()>>> sliderGetters;

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

    // #6: 仅 Mask 页 + 图片文件才响应
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override {}
    void fileDragExit (const juce::StringArray&) override {}
    void filesDropped (const juce::StringArray& files, int, int) override;

    JUCE_DECLARE_NON_COPYABLE (ParamPanel)
};
