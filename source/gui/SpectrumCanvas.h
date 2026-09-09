// =============================================================================
// SpectrumCanvas.h — 实时频谱渲染画布 + 频谱元素自由变换
//
// 职责：
//   · 每帧把 BandFrame 用 SpectrumStyle 渲染到**输出分辨率**的 ARGB 基础层，
//     再按 VisTransform（拖拽移动 / 等比缩放 / 非等比拉伸 / 旋转）合成到画布，
//     —— 与导出管线 VisPipeline::renderFrame 完全同源，保证"预览即所得"
//   · 画布显示尺寸 ≠ 输出分辨率时按比例 contain（letterbox）居中显示
//   · 画布内直接拖拽操作：拖动移动 / 8 角手柄缩放拉伸 / 顶部圆柄旋转 /
//     双击复位变换
//   · 接受文件拖放 / 点击空白选择音频文件（回调给 MainComponent）
//
// 后续图片图层：可复用 VisTransform.h 的 affine / 命中测试，加一个 List 即可。
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/SpectrumParams.h"
#include "../core/VisTransform.h"
#include "../core/SpectrumStyle.h"
#include "../core/BandFrame.h"
#include <array>
#include <map>
#include <vector>

class SpectrumCanvas : public juce::Component,
                       private juce::FileDragAndDropTarget
{
public:
    explicit SpectrumCanvas (SpectrumParams& paramsRef);

    // ---- 每帧由 MainComponent 更新 ----
    BandFrame frame;
    SpectrumStyle::RenderParams rp;
    SpectrumStyle* style = nullptr;
    bool hasAudio = false;
    bool showCheckerboard = true;

    // ---- 回调 ----
    std::function<void (const juce::File&)> onFileDropped;
    std::function<void (const juce::File&)> onNonAudioDropped;
    std::function<void (const juce::File&)> onImageDropped;   // 拖入图片 → 创建/选中图片图层
    std::function<void ()> onEmptyClicked;
    std::function<void ()> onDeleteRequested;  // v0.5.2: 键盘 Delete/Backspace 删除选中图层

    // ---- 图层选择 API（ParamPanel / MainComponent 访问）----
    int  selectedImageIndex() const noexcept { return selectedImage; }  // -1 = 频谱元素
    void selectSpectrum()    { selectedImage = -1; repaint(); }
    void selectImage (int idx){ selectedImage = idx; repaint(); }
    int  imageCount() const noexcept { return (int) params.images.size(); }

    // ---- 频谱蒙版图片编辑模式（v0.5.4）----
    //   开启后：在频谱范围内拖动 = 平移蒙版图片（maskImage.offset），不移动频谱；
    //   点击频谱范围外 = 自动退出编辑模式。关闭时：图片随频谱作为整体一起拖动。
    void setEditMaskImage (bool on);          // 定义在 cpp：进入时烘焙默认变换，手柄即刻对齐
    bool editMaskImageMode() const noexcept { return editMaskImage; }

    void paint (juce::Graphics& g) override;

private:
    SpectrumParams& params;

    // ---- 交互状态 ----
    enum class DragMode
    {
        None, Move, Rotate,
        ScaleTL, ScaleTR, ScaleBR, ScaleBL,   // 四角（等比缩放）
        ScaleT,  ScaleB,  ScaleL,  ScaleR,    // 四边（单轴拉伸）
        MoveMask,                             // v0.5.4：编辑模式下平移蒙版图片
        BaselineAxis                          // v0.5.4 #4：拖基线轴
    };
    DragMode dragMode = DragMode::None;
    int selectedImage = -1;             // 当前选中元素：-1 = 频谱，>=0 = params.images 下标
    bool editMaskImage = false;         // v0.5.4：蒙版图片编辑模式
    bool dragBaseline  = false;         // v0.5.4 #4：基线轴拖拽中
    float baselineScreenY = -1e9f;      // #4：轴线的画布 y（paint 时更新，命中测试用）
    float dragStartOffX = 0.0f, dragStartOffY = 0.0f;   // 拖图起始 offset

    // ---- 拖放 HUD / 提权诊断（UIPI）----
    bool dragHovering = false;
    juce::StringArray dragHoverFiles;
    bool processElevated = false;
    mutable std::map<juce::String, juce::Image> imageCache;
    juce::Point<float> dragStartOut;    // 拖拽开始时鼠标（输出坐标）
    VisTransform       startTransform;  // 拖拽开始时变换副本
    std::array<juce::Point<float>, 4> currentCorners() const;  // 元素外框四角（输出坐标）

    // ---- 对边锚定缩放（P2）----
    juce::Point<float> dragAnchorElem;      // 锚点元素坐标（对角/对边中点）
    juce::Point<float> dragHandleElem;      // 被拖点元素坐标（角/边中点）

    // ---- 吸附辅助线（v0.5.3，CAD 风格；仅 snapEnabled 且拖拽 Move 期间实时填充/清空）----
    enum class SnapKind { Center, EdgeMid, Corner };
    struct SnapGuide
    {
        bool vertical = false;              // true = X 吸附（竖线），false = Y 吸附（横线）
        float coord = 0.0f;                 // 吸附到的输出坐标（竖线取 x，横线取 y）
        juce::Point<float> dragPt;          // 被拖元素对齐点（输出坐标，coord 已对齐）
        juce::Point<float> targetPt;        // 目标对齐点（输出坐标）
        SnapKind dragKind = SnapKind::Center;
        SnapKind targetKind = SnapKind::Center;
        juce::String label;                 // 目标描述（"Canvas" / "Spectrum" / "Image N"）
    };
    std::vector<SnapGuide> activeSnapGuides;   // 拖拽期间有效，paintOverlay 读取；mouseUp 清空

    // ---- 交互 ----
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseDrag  (const juce::MouseEvent&) override;
    void mouseUp    (const juce::MouseEvent&) override;
    void mouseMove  (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    DragMode hitHandle (juce::Point<float> out) const;
    DragMode hitHandleForCorners (const std::array<juce::Point<float>, 4>& c,
                                  float elemW, float elemH,
                                  juce::Point<float> out) const;
    void beginTransformIfNeeded();
    void updateHoverCursor (juce::Point<float> out);
    void paintOverlay (juce::Graphics& g);
    // v0.5.4: 频谱画框（padding 内绘制区，输出/base 坐标）——编辑手柄移动吸附的参考框
    juce::Rectangle<float> frameRectOut() const;
    // v0.5.4: 进入"编辑蒙版图片"时，若图片尚未 set 变换，则烘焙一个"铺满画框"的等价 transform
    void ensureMaskTransformInit();
    // v0.5.4: 输出坐标 → base/蒙版图片空间（= 撤销频谱元素变换 P⁻¹；未变换时二者重合）
    juce::Point<float> baseFromOutput (juce::Point<float> out) const;
    void paintSnapGuides (juce::Graphics& g, const juce::AffineTransform& disp);  // v0.5.3 吸附辅助线
    // v0.5.4 #3: 缩放吸附——手柄点对齐画布/其它元素特征点，命中则修正 t 并推辅助线
    void applyScaleSnap (VisTransform& t, const juce::Point<float>& mouseOut);
    // 图片图层（v0.5.1 分组渲染：aboveOnly=false=频谱下方组，true=上方组）
    void paintImages (juce::Graphics& g, const juce::AffineTransform& disp, bool aboveOnly);
    void paintImageLayer (juce::Graphics& g, const juce::AffineTransform& disp,
                          const ImageLayer& layer);
    void paintBannerHud (juce::Graphics& g);                                    // 提权警告 + 拖放 HUD
    VisTransform&       activeTransform();
    const VisTransform& activeTransform() const;
    std::pair<float, float> activeElementSize() const;   // 选中元素基础尺寸（输出坐标）
    juce::Image loadCached (const juce::String& path) const;
    static bool isProcessElevated();                                            // UIPI 诊断

    // ---- 坐标映射（输出分辨率坐标系 ↔ 画布坐标系）----
    juce::AffineTransform displayAffine() const;   // 输出坐标 → 画布坐标（contain 适配）
    juce::Point<float> toOutput (juce::Point<float> canvasPos) const;  // 反变换
    float displayScale() const;
    juce::Rectangle<float> outputDisplayRect() const;   // v0.5.3: "范围内"输出画布的组件矩形

    // ---- 拖放 ----
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray& files, int, int) override;    // HUD 反馈
    void fileDragMove  (const juce::StringArray& files, int, int) override;
    void fileDragExit  (const juce::StringArray& files) override;

    static bool isAudioFile (const juce::String& path);
    static void drawCheckerboard (juce::Graphics& g, int w, int h, int cell = 10);

    JUCE_DECLARE_NON_COPYABLE (SpectrumCanvas)
};