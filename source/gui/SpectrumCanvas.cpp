// =============================================================================
// SpectrumCanvas.cpp — 实时频谱渲染画布实现
//
// 渲染管线（与导出一致）：
//   背景(棋盘/暗色) → 频谱基础层(输出分辨率 ARGB) → VisTransform 合成 → 手柄 UI
// 交互：
//   拖动元素主体 = 移动；四角 = 等比缩放；四边 = 单轴拉伸；
//   顶部圆柄 = 绕中心旋转；双击元素 = 复位变换。
// =============================================================================
#include "SpectrumCanvas.h"
#include "WinDragCompat.h"
#include <cmath>
#if JUCE_WINDOWS
 #include <windows.h>      // isProcessElevated：UIPI 诊断用
#endif

namespace
{
    constexpr float kHandleScreenPx  = 7.0f;    // 手柄边长（屏幕像素）
    constexpr float kRotHandleDistPx = 24.0f;   // 旋转圆柄到顶边距离（屏幕像素）
    constexpr float kHitRadiusPx     = 12.0f;   // 命中半径（屏幕像素）
}

SpectrumCanvas::SpectrumCanvas (SpectrumParams& paramsRef) : params (paramsRef)
{
    processElevated = isProcessElevated();   // UIPI 诊断：提权进程收不到资源管理器拖放
}

// =============================================================================
// 渲染
// =============================================================================
void SpectrumCanvas::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2b33));   // 深色画布底

    const int ow = juce::jmax (1, params.width);
    const int oh = juce::jmax (1, params.height);
    const auto disp = displayAffine();
    const float s = displayScale();

    // 显示区（letterbox 居中）
    juce::Rectangle<float> dispRect;
    {
        auto b = getLocalBounds();
        dispRect = juce::Rectangle<float> ((b.getWidth()  - ow * s) * 0.5f,
                                           (b.getHeight() - oh * s) * 0.5f,
                                           ow * s, oh * s);
    }

    // 棋盘格（GUI 专用，仅显示区内；不影响导出）
    if (showCheckerboard)
    {
        g.saveState();
        g.addTransform (juce::AffineTransform::translation (dispRect.getX(), dispRect.getY()));
        drawCheckerboard (g, (int) juce::jmax (1.0f, dispRect.getWidth()),
                             (int) juce::jmax (1.0f, dispRect.getHeight()), 10);
        g.restoreState();
    }

    // 基础层：输出分辨率渲染频谱（与 VisPipeline::renderFrame 完全同源；不带变换）
    juce::Image base (juce::Image::ARGB, ow, oh, true);   // true = 全透明初始
    {
        juce::Graphics gb (base);
        gb.setOpacity (rp.opacity);
        rp.width  = ow;
        rp.height = oh;
        const auto canvasRect = juce::Rectangle<int> (
            (int) rp.paddingLeft, (int) rp.paddingTop,
            ow - (int)(rp.paddingLeft + rp.paddingRight),
            oh - (int)(rp.paddingTop  + rp.paddingBottom));
        if (style != nullptr)
            style->render (gb, canvasRect, frame, rp);
    }

    // 合成频谱元素：输出坐标 × 元素变换 × 画布显示适配
    if (style != nullptr)
    {
        // v0.5.1: 与导出 VisPipeline 同源分层合成：
        //   下方图片组 → 频谱元素 → 上方图片组
        paintImages (g, disp, /*aboveOnly=*/false);

        const auto total = buildVisAffine (params.transform).followedBy (disp);
        g.saveState();
        g.addTransform (total);
        g.drawImageAt (base, 0, 0);
        g.restoreState();

        paintImages (g, disp, /*aboveOnly=*/true);
    }
    else
    {
        // 无样式（极端情况）：图片仍需可见
        paintImages (g, disp, /*aboveOnly=*/false);
        paintImages (g, disp, /*aboveOnly=*/true);
    }

    // 变换手柄 UI（有音频时始终显示）
    if (hasAudio)
        paintOverlay (g);

    if (! hasAudio)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawFittedText ("Drag & drop a WAV or AIFF file to begin\n(or click here to browse)",
                          getLocalBounds(), juce::Justification::centred, 2);
    }

    paintBannerHud (g);
}

// 图片图层绘制（与 VisPipeline::drawImageLayer 同一约定）：
// 基础矩形 = 图片自然尺寸；identity 变换 = 等比 contain 居中。
void SpectrumCanvas::paintImageLayer (juce::Graphics& g,
                                      const juce::AffineTransform& disp,
                                      const ImageLayer& layer)
{
    const juce::Image img = loadCached (layer.path);
    if (! img.isValid())
        return;
    const float ew = (float) img.getWidth();
    const float eh = (float) img.getHeight();
    const VisTransform tf = layer.transform.set
        ? layer.transform
        : makeContainTransform (ew, eh, (float) params.width, (float) params.height);
    const auto total = buildVisAffine (tf).followedBy (disp);
    g.saveState();
    g.addTransform (total);
    g.setOpacity (juce::jlimit (0.0f, 1.0f, layer.opacity));
    g.drawImageAt (img, 0, 0);
    g.restoreState();
}

void SpectrumCanvas::paintImages (juce::Graphics& g, const juce::AffineTransform& disp,
                                  bool aboveOnly)
{
    for (const auto& layer : params.images)
    {
        if (layer.aboveSpectrum != aboveOnly)
            continue;
        paintImageLayer (g, disp, layer);
    }
}

void SpectrumCanvas::paintOverlay (juce::Graphics& g)
{
    const auto disp = displayAffine();

    auto cOut = currentCorners();                    // 输出坐标（已按选中元素的尺寸）
    // 元素中心 = 四角平均（仿射保持中点；自动跟随选中元素，v0.5.1 修复：
    // 原实现固定用频谱变换的画布中心，选中图片时旋转柄方向错误）
    const auto centerOut = juce::Point<float> (
        (cOut[0].getX() + cOut[1].getX() + cOut[2].getX() + cOut[3].getX()) * 0.25f,
        (cOut[0].getY() + cOut[1].getY() + cOut[2].getY() + cOut[3].getY()) * 0.25f);

    // 映射到画布坐标
    juce::Point<float> cp[4];
    for (int i = 0; i < 4; ++i) cp[i] = visTransformPoint (disp, cOut[i]);
    const auto cCenter = visTransformPoint (disp, centerOut);

    const auto mid = [] (juce::Point<float> a, juce::Point<float> b)
    {
        return juce::Point<float> ((a.getX() + b.getX()) * 0.5f,
                                   (a.getY() + b.getY()) * 0.5f);
    };
    const juce::Point<float> topMid    = mid (cp[0], cp[1]);
    const juce::Point<float> rightMid  = mid (cp[1], cp[2]);
    const juce::Point<float> bottomMid = mid (cp[2], cp[3]);
    const juce::Point<float> leftMid   = mid (cp[3], cp[0]);

    // 外框
    juce::Path framePath;
    framePath.startNewSubPath (cp[0]);
    framePath.lineTo (cp[1]);
    framePath.lineTo (cp[2]);
    framePath.lineTo (cp[3]);
    framePath.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.strokePath (framePath, juce::PathStrokeType (1.0f));

    // 旋转手柄（顶边中点正上方）
    juce::Point<float> norm (topMid.getX() - cCenter.getX(),
                             topMid.getY() - cCenter.getY());
    const float nlen = std::sqrt (norm.getX() * norm.getX() + norm.getY() * norm.getY());
    if (nlen > 1e-3f)
        norm = juce::Point<float> (norm.getX() / nlen, norm.getY() / nlen);
    const juce::Point<float> rotHandle = topMid + norm * kRotHandleDistPx;
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawLine (juce::Line<float> (topMid, rotHandle), 1.0f);
    g.setColour (juce::Colours::cyan);
    g.fillEllipse (rotHandle.getX() - 5.0f, rotHandle.getY() - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colours::white);
    g.drawEllipse (rotHandle.getX() - 5.0f, rotHandle.getY() - 5.0f, 10.0f, 10.0f, 1.0f);

    // 8 个手柄
    const float hh = kHandleScreenPx;
    const juce::Point<float> handles[8] =
    {
        cp[0], cp[1], cp[2], cp[3],
        topMid, rightMid, bottomMid, leftMid
    };
    for (auto& hp : handles)
    {
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillRect (hp.getX() - hh * 0.5f, hp.getY() - hh * 0.5f, hh, hh);
        g.setColour (juce::Colours::pink);
        g.drawRect (hp.getX() - hh * 0.5f, hp.getY() - hh * 0.5f, hh, hh, 1.0f);
    }

    // 操作提示
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Drag to move - corners:scale - edges:stretch - top:rotate - double-click:reset",
                juce::Rectangle<int> (0, getHeight() - 18, getWidth(), 18),
                juce::Justification::centred);
}
// =============================================================================
// 坐标映射
// =============================================================================
float SpectrumCanvas::displayScale() const
{
    const int ow = juce::jmax (1, params.width);
    const int oh = juce::jmax (1, params.height);
    auto b = getLocalBounds();
    return juce::jmin ((float) b.getWidth()  / ow,
                       (float) b.getHeight() / oh);
}

juce::AffineTransform SpectrumCanvas::displayAffine() const
{
    const int ow = juce::jmax (1, params.width);
    const int oh = juce::jmax (1, params.height);
    const float s = displayScale();
    auto b = getLocalBounds();
    return juce::AffineTransform::scale (s).translated ((b.getWidth()  - ow * s) * 0.5f,
                                                        (b.getHeight() - oh * s) * 0.5f);
}

juce::Point<float> SpectrumCanvas::toOutput (juce::Point<float> p) const
{
    return visTransformPoint (displayAffine().inverted(), p);
}

// 当前选中元素的基础尺寸（输出坐标）：频谱 = 输出画布；图片 = 图片自然尺寸
std::pair<float, float> SpectrumCanvas::activeElementSize() const
{
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
    {
        const juce::Image img = loadCached (params.images[(size_t) selectedImage].path);
        if (img.isValid())
            return { (float) img.getWidth(), (float) img.getHeight() };
    }
    return { (float) juce::jmax (1, params.width), (float) juce::jmax (1, params.height) };
}

std::array<juce::Point<float>, 4> SpectrumCanvas::currentCorners() const
{
    const auto [w, h] = activeElementSize();
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
        return visCorners (params.images[(size_t) selectedImage].transform, w, h);
    return visCorners (params.transform, w, h);
}

// =============================================================================
// 交互
// =============================================================================
void SpectrumCanvas::beginTransformIfNeeded()
{
    if (params.transform.set)
        return;
    params.transform.set         = true;
    params.transform.centerX     = params.width  * 0.5f;
    params.transform.centerY     = params.height * 0.5f;
    params.transform.scaleX      = 1.0f;
    params.transform.scaleY      = 1.0f;
    params.transform.rotationDeg = 0.0f;
}
SpectrumCanvas::DragMode SpectrumCanvas::hitHandle (juce::Point<float> out) const
{
    const float s = displayScale();
    const float hitR = (s > 1e-3f) ? (kHitRadiusPx / s) : (kHitRadiusPx * 20.0f);

    const auto c = currentCorners();
    // 仿射保持中点 → 元素中心 = 四角平均（自动跟随选中元素）
    const auto centerOut = juce::Point<float> (
        (c[0].getX() + c[1].getX() + c[2].getX() + c[3].getX()) * 0.25f,
        (c[0].getY() + c[1].getY() + c[2].getY() + c[3].getY()) * 0.25f);
    const auto mid = [] (juce::Point<float> a, juce::Point<float> b)
    {
        return juce::Point<float> ((a.getX() + b.getX()) * 0.5f,
                                   (a.getY() + b.getY()) * 0.5f);
    };

    // 旋转手柄（顶边上方的线段）
    {
        const auto topMid = mid (c[0], c[1]);
        juce::Point<float> norm (topMid.getX() - centerOut.getX(),
                                 topMid.getY() - centerOut.getY());
        const float nlen2 = norm.getX() * norm.getX() + norm.getY() * norm.getY();
        if (nlen2 > 1e-6f)
        {
            const float nlen = std::sqrt (nlen2);
            norm = juce::Point<float> (norm.getX() / nlen, norm.getY() / nlen);
            const auto outer = topMid + norm * (kRotHandleDistPx / s);
            if (visDistanceToSegment (topMid, outer, out) <= hitR)
                return DragMode::Rotate;
        }
    }

    // 四角
    const juce::Point<float> corners[4] = { c[0], c[1], c[2], c[3] };
    const DragMode cornerModes[4] = { DragMode::ScaleTL, DragMode::ScaleTR,
                                      DragMode::ScaleBR, DragMode::ScaleBL };
    for (int i = 0; i < 4; ++i)
        if (corners[i].getDistanceFrom (out) <= hitR)
            return cornerModes[i];

    // 四边中点
    const juce::Point<float> edges[4]   = { mid (c[0], c[1]), mid (c[1], c[2]),
                                            mid (c[2], c[3]), mid (c[3], c[0]) };
    const DragMode       edgeModes[4]   = { DragMode::ScaleT, DragMode::ScaleR,
                                            DragMode::ScaleB, DragMode::ScaleL };
    for (int i = 0; i < 4; ++i)
        if (edges[i].getDistanceFrom (out) <= hitR)
            return edgeModes[i];

    return DragMode::None;
}
void SpectrumCanvas::mouseDown (const juce::MouseEvent& e)
{
    if (! hasAudio)
    {
        if (onEmptyClicked)
            onEmptyClicked();
        return;
    }

    const auto out = toOutput (e.position);

    auto h = hitHandle (out);          // 针对当前选中元素的手柄

    // 未命中手柄 → 元素拾取：图片顶层优先，其次频谱主体
    if (h == DragMode::None)
    {
        const int img = hitImage (out);
        if (img >= 0)
        {
            if (selectedImage != img)
            {
                selectedImage = img;
                activeTransform();     // 惰性初始化该图层变换
            }
            h = hitHandle (out);       // 对新选中元素重新命中手柄
        }
        else if (visContains (currentCorners(), out))
        {
            selectedImage = -1;        // 频谱
            h = hitHandle (out);
        }
    }

    const bool bodyHit = (selectedImage >= 0)
                       ? (hitImage (out) == selectedImage)
                       : visContains (currentCorners(), out);

    if (h == DragMode::None && ! bodyHit)
    {
        dragMode = DragMode::None;   // 点在外框外 → 不动作
        repaint();
        return;
    }

    if (selectedImage < 0)
        beginTransformIfNeeded();
    dragStartOut   = out;
    startTransform = activeTransform();
    dragMode = (h != DragMode::None) ? h : DragMode::Move;
    repaint();
}

void SpectrumCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::None)
        return;

    const auto out = toOutput (e.position);
    auto& t = activeTransform();
    const auto [ew, eh] = activeElementSize();   // 选中元素基础尺寸（输出坐标）
    // 变换前枢轴的输出坐标（缩放/旋转的不动点，对图片含 pos 偏移）
    const auto startCenterOut = visTransformPoint (buildVisAffine (startTransform),
                                                   juce::Point<float> (ew * 0.5f, eh * 0.5f));

    switch (dragMode)
    {
    case DragMode::Move:
        // v0.5.1: 移动走附加平移（图片/频谱统一；频谱 pos 初始 0，行为不变）
        t.posX = startTransform.posX + (out.getX() - dragStartOut.getX());
        t.posY = startTransform.posY + (out.getY() - dragStartOut.getY());
        break;

    case DragMode::Rotate:
    {
        const float a0 = std::atan2 (dragStartOut.getY() - startCenterOut.getY(),
                                     dragStartOut.getX() - startCenterOut.getX());
        const float a1 = std::atan2 (out.getY() - startCenterOut.getY(),
                                     out.getX() - startCenterOut.getX());
        t.rotationDeg = startTransform.rotationDeg + juce::radiansToDegrees (a1 - a0);
        break;
    }

    case DragMode::ScaleTL: case DragMode::ScaleTR:
    case DragMode::ScaleBR: case DragMode::ScaleBL:
    {
        // 等比缩放：以枢轴为不动点，按拖动距离比例伸缩
        const float d0 = dragStartOut.getDistanceFrom (startCenterOut);
        const float d1 = out.getDistanceFrom (startCenterOut);
        const float f = (d0 > 1e-3f) ? (d1 / d0) : 1.0f;
        t.scaleX = juce::jlimit (0.05f, 50.0f, startTransform.scaleX * f);
        t.scaleY = juce::jlimit (0.05f, 50.0f, startTransform.scaleY * f);
        break;
    }

    case DragMode::ScaleT: case DragMode::ScaleB:
    case DragMode::ScaleL: case DragMode::ScaleR:
    {
        // 单轴拉伸：被拖动的边跟随鼠标，沿对应旋转轴投影求新缩放
        const auto m  = buildVisAffine (startTransform);
        const auto cc = startCenterOut;

        juce::Point<float> axis;
        const bool vertical = (dragMode == DragMode::ScaleT || dragMode == DragMode::ScaleB);
        if (vertical)
        {
            const auto top = visTransformPoint (m, juce::Point<float> (ew * 0.5f, 0.0f));
            axis = top - cc;
        }
        else
        {
            const auto right = visTransformPoint (m, juce::Point<float> (ew, eh * 0.5f));
            axis = right - cc;
        }
        const float alen = std::sqrt (axis.getX() * axis.getX() + axis.getY() * axis.getY());
        if (alen > 1e-4f)
            axis = juce::Point<float> (axis.getX() / alen, axis.getY() / alen);

        const float proj = (out.getX() - cc.getX()) * axis.getX()
                         + (out.getY() - cc.getY()) * axis.getY();
        const float half = (vertical ? eh : ew) * 0.5f;
        float factor = 0.0f;
        switch (dragMode)
        {
        case DragMode::ScaleT: factor =  proj / half; break;
        case DragMode::ScaleB: factor = -proj / half; break;
        case DragMode::ScaleR: factor =  proj / half; break;
        case DragMode::ScaleL: factor = -proj / half; break;
        default: break;
        }
        const float clamped = juce::jlimit (0.05f, 50.0f, factor);
        if (vertical) t.scaleY = clamped; else t.scaleX = clamped;
        break;
    }

    default:
        break;
    }

    t.set = true;
    repaint();
}
void SpectrumCanvas::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::None;
    repaint();
}

void SpectrumCanvas::mouseMove (const juce::MouseEvent& e)
{
    updateHoverCursor (toOutput (e.position));
}

void SpectrumCanvas::updateHoverCursor (juce::Point<float> out)
{
    const auto h = hitHandle (out);
    const bool body = visContains (currentCorners(), out);

    if (h == DragMode::Rotate)
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else if (h == DragMode::ScaleTL || h == DragMode::ScaleBR)
        setMouseCursor (juce::MouseCursor::TopLeftCornerResizeCursor);
    else if (h == DragMode::ScaleTR || h == DragMode::ScaleBL)
        setMouseCursor (juce::MouseCursor::TopRightCornerResizeCursor);
    else if (h == DragMode::ScaleT  || h == DragMode::ScaleB)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else if (h == DragMode::ScaleL  || h == DragMode::ScaleR)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (body)
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SpectrumCanvas::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! hasAudio)
        return;
    // 双击元素 → 复位变换：频谱回铺满画布（旧行为），图片回等比 contain 居中
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
    {
        const juce::Image img = loadCached (params.images[(size_t) selectedImage].path);
        const float ew = img.isValid() ? (float) img.getWidth()  : (float) params.width;
        const float eh = img.isValid() ? (float) img.getHeight() : (float) params.height;
        params.images[(size_t) selectedImage].transform
            = makeContainTransform (ew, eh, (float) params.width, (float) params.height);
    }
    else
    {
        activeTransform() = VisTransform {};
    }
    dragMode = DragMode::None;
    repaint();
}

// =============================================================================
// 文件拖放
// =============================================================================
bool SpectrumCanvas::isAudioFile (const juce::String& path)
{
    auto ext = juce::File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff";
}

static bool isImageFile (const juce::String& p)
{
    auto ext = juce::File (p).getFileExtension().toLowerCase();
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg"
        || ext == ".bmp" || ext == ".gif" || ext == ".webp";
}

// 接受任意文件拖入（避免非音频文件拖放时全程禁止符）；
// 非音频文件在 filesDropped 里走 onNonAudioDropped 提示。
bool SpectrumCanvas::isInterestedInFileDrag (const juce::StringArray&)
{
    return true;
}

void SpectrumCanvas::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        if (isImageFile (f) && onImageDropped)
        {
            onImageDropped (juce::File (f));
            return;
        }
        if (isAudioFile (f) && onFileDropped)
        {
            onFileDropped (juce::File (f));
            return;
        }
    }

    // 没有任何音频/图片文件 → 回调第一个文件给上层提示
    if (onNonAudioDropped && ! files.isEmpty())
        onNonAudioDropped (juce::File (files[0]));
}

// =============================================================================
// 图层选择 / 双元素（频谱 + 图片）支持
// =============================================================================
const VisTransform& SpectrumCanvas::activeTransform() const
{
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
        return params.images[(size_t) selectedImage].transform;
    return params.transform;
}

VisTransform& SpectrumCanvas::activeTransform()
{
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
    {
        auto& t = params.images[(size_t) selectedImage].transform;
        if (! t.set)
        {
            // 首次交互：等比 contain 居中（与导出 drawImageLayer 的 identity 语义一致）
            const juce::Image img = loadCached (params.images[(size_t) selectedImage].path);
            const float ew = img.isValid() ? (float) img.getWidth()  : (float) params.width;
            const float eh = img.isValid() ? (float) img.getHeight() : (float) params.height;
            t = makeContainTransform (ew, eh, (float) params.width, (float) params.height);
        }
        return t;
    }
    return params.transform;
}

int SpectrumCanvas::hitImage (juce::Point<float> out) const
{
    const auto [w0, h0] = activeElementSize();
    juce::ignoreUnused (w0, h0);
    for (int i = (int) params.images.size() - 1; i >= 0; --i)   // 顶层优先
    {
        const juce::Image img = loadCached (params.images[(size_t) i].path);
        const float w = img.isValid() ? (float) img.getWidth()  : (float) params.width;
        const float h = img.isValid() ? (float) img.getHeight() : (float) params.height;
        if (visContains (visCorners (params.images[(size_t) i].transform, w, h), out))
            return i;
    }
    return -1;
}

juce::Image SpectrumCanvas::loadCached (const juce::String& path) const
{
    auto it = imageCache.find (path);
    if (it != imageCache.end())
        return it->second;
    juce::Image img = juce::ImageCache::getFromFile (juce::File (path));
    if (img.isValid())
        imageCache[path] = img;
    return img;
}

// 图片图层绘制（与 VisPipeline::drawImageLayer 同一约定）：
// 基础矩形 = 图片自然尺寸；identity 变换 = 等比 contain 居中。
// （v0.5.1：旧版 paintImages 已被 paintImages(g,disp,aboveOnly) 分组渲染取代）

// =============================================================================
// 提权警告横幅 + 拖放 HUD（诊断"禁止符号"是系统 UIPI 还是程序问题）
// =============================================================================
void SpectrumCanvas::paintBannerHud (juce::Graphics& g)
{
    auto b = getLocalBounds();

    if (processElevated)
    {
        auto r = b.removeFromTop (28).reduced (8, 3);
        g.setColour (juce::Colour (0xd0b91c1c));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawFittedText ("Elevated process: legacy drag-drop (WM_DROPFILES) active - "
                          "drag files here normally.",
                          r, juce::Justification::centred, 1);
    }

    if (dragHovering)
    {
        auto r = juce::Rectangle<float> (10.0f, 10.0f, 250.0f, 30.0f);
        g.setColour (juce::Colour (0xe61d6f42));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        auto name = dragHoverFiles.isEmpty() ? juce::String()
                    : juce::File (dragHoverFiles[0]).getFileName();
        g.drawFittedText ("DROP -> " + (name.isNotEmpty()
                            ? name : juce::String ((int) dragHoverFiles.size()) + " file(s)"),
                          r.toNearestInt(), juce::Justification::centred, 1);
    }

    // v0.5.0: 拖放兼容层诊断常显行（右下角，不滚动、不遮挡）。
    // 有音频加载提示时也保持可见（合并进同一行右侧）。
#if JUCE_WINDOWS
    {
        auto status = avx::winDragCompat::getStatusLine();
        if (status.isNotEmpty())
        {
            g.setFont (juce::FontOptions (11.0f));
            g.setColour (juce::Colours::grey);
            g.drawText ("[drag] " + status,
                        getLocalBounds().removeFromBottom (18).reduced (8, 0),
                        juce::Justification::centredRight);
        }
    }
#endif
}

bool SpectrumCanvas::isProcessElevated()
{
#if JUCE_WINDOWS
    BOOL elevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        DWORD retLen = 0;
        TOKEN_ELEVATION elev {};
        if (GetTokenInformation (token, TokenElevation, &elev, sizeof (elev), &retLen))
            elevated = elev.TokenIsElevated;
        CloseHandle (token);
    }
    return elevated != FALSE;
#else
    return false;
#endif
}

// 拖放 HUD 反馈：拖动进入窗口即点亮（证明 OLE 链路已到达程序）
void SpectrumCanvas::fileDragEnter (const juce::StringArray& files, int, int)
{
    dragHovering  = true;
    dragHoverFiles = files;
    repaint();
}

void SpectrumCanvas::fileDragMove (const juce::StringArray&, int, int) {}

void SpectrumCanvas::fileDragExit (const juce::StringArray&)
{
    dragHovering = false;
    dragHoverFiles.clear();
    repaint();
}

void SpectrumCanvas::drawCheckerboard (juce::Graphics& g, int w, int h, int cell)
{
    g.fillAll (juce::Colour (0xff3a3a44));
    g.setColour (juce::Colour (0xff4a4a55));
    for (int y = 0; y < h; y += cell)
        for (int x = ((y / cell) % 2) * cell; x < w; x += cell * 2)
            g.fillRect (x, y, juce::jmin (cell, w - x), juce::jmin (cell, h - y));
}