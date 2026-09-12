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
#include "../core/SpectrumMask.h"
#include <cmath>
#if JUCE_WINDOWS
 #include <windows.h>      // isProcessElevated：UIPI 诊断用
#endif

namespace
{
    constexpr float kHandleScreenPx  = 7.0f;    // 手柄边长（屏幕像素）
    constexpr float kRotHandleDistPx = 24.0f;   // 旋转圆柄到顶边距离（屏幕像素）
    constexpr float kHitRadiusPx     = 12.0f;   // 命中半径（屏幕像素）
    constexpr float kRotHitRadiusPx  =  14.0f;   // v0.5.3: 旋转圆柄圆心命中半径（屏幕像素）

    // v0.5.3: 范围内 / 范围外视觉区分
    const juce::Colour kOutsideBg        (0xff54545c);   // 范围外灰底
    const juce::Colour kInsideBg         (0xff17171c);   // 棋盘关时范围内近黑底
    const juce::Colour kBorderCol        (0xe6ffffff);   // 输出画布边界线（半透白）
    constexpr float    kOutsideImageMul  = 0.35f;        // 范围外元素不透明度乘子（压在灰底上 → 变暗发灰）

    // v0.5.4 #1：取消逐帧平均色缓存——渲染统一读 strokeColor（平均色在加载图片/取色按钮时算一次写入）。
    //   原每帧调用 averageColour 计算矢量 adj 图均色（含透明度通道）曾导致 Windows 越界崩溃，现整体移除。
}

SpectrumCanvas::SpectrumCanvas (SpectrumParams& paramsRef) : params (paramsRef)
{
    // v0.5.3: 确保画布能拿到键盘焦点（修复 Delete/Backspace 偶发无反应：
    // 否则首次按键可能被窗口焦点消化）
    setWantsKeyboardFocus (true);
    processElevated = isProcessElevated();   // UIPI 诊断：提权进程收不到资源管理器拖放
}

// =============================================================================
// 渲染
// =============================================================================
void SpectrumCanvas::paint (juce::Graphics& g)
{
    // v0.5.3: 范围内 / 范围外 背景区分——范围外统一偏灰，范围内偏黑（或棋盘），
    //   并描一圈边界线，让人一眼看出元素是否越出输出画布（导出仅取范围内，不受影响）。
    const int ow = juce::jmax (1, params.width);
    const int oh = juce::jmax (1, params.height);
    const auto disp = displayAffine();

    g.fillAll (kOutsideBg);                       // 先整体铺"范围外"灰底

    const auto dispRect = outputDisplayRect();    // "范围内"输出画布矩形
    if (showCheckerboard)
    {
        g.saveState();
        g.reduceClipRegion (dispRect.toNearestInt());
        g.addTransform (juce::AffineTransform::translation (dispRect.getX(), dispRect.getY()));
        drawCheckerboard (g, (int) juce::jmax (1.0f, dispRect.getWidth()),
                             (int) juce::jmax (1.0f, dispRect.getHeight()), 10);
        g.restoreState();
    }
    else
    {
        g.setColour (kInsideBg);                  // 棋盘关时范围内铺近黑底
        g.fillRect (dispRect);
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
        // v0.5.2: 统一 z 序分层合成：
        //   下方图片组 → 频谱元素（若存在）→ 上方图片组
        const int N = (int) params.images.size();
        const int k = juce::jlimit (0, N, params.spectrumIndex);

        for (int i = 0; i < k; ++i)
            paintImageLayer (g, disp, params.images[(size_t) i]);

        if (params.spectrumPresent)
        {
            // 频谱蒙版：图片填轮廓（+可选描边），取代裸频谱填充层（与导出同源）
            // #2 防御：mask 路径任何异常（分配失败/未知格式）→ 回退裸频谱，绝不闪退
            juce::Image specLayer = base;
            if (params.maskImage.enabled && ! params.maskImage.path.isEmpty())
            {
                const juce::Image im = loadCached (params.maskImage.path);
                if (im.isValid())
                try
                {
                    // v0.5.4 #4：色彩调整后的图（identity 时零开销返回原图；带缓存）
                    const juce::Image adj = SpectrumMask::adjustedImageCached (im, params.maskImage);
                    // v0.5.4 #1：outlineMode=image 时渲染只读 strokeColor（加载图/Use average 时已算一次）。
                    // v0.5.5 #5：uniform/perBar/perFrame 走 composeWithPlan + 预览缓存（节流+插值），
                    //   导出侧仍用 compose()（cache=nullptr）逐帧真算 → 两路径同源。
                    const juce::Colour stroke = params.maskImage.strokeColor;
                    const double nowSec = (double) juce::Time::getMillisecondCounterHiRes() * 0.001;
                    juce::Image masked = SpectrumMask::composeWithPlan (
                        base, adj, params.maskImage, stroke, &maskPalette, nowSec,
                        SpectrumMask::isBarStyle (params.style));   // line 系禁左右侧边（新1c2）
                    if (masked.isValid()) specLayer = masked;
                }
                catch (...) { }   // #2 防御：异常 → specLayer 保持 base（无蒙版回退）
            }

            const auto total = buildVisAffine (params.transform).followedBy (disp);
            // 范围内：原样
            g.saveState();
            g.reduceClipRegion (dispRect.toNearestInt());
            g.addTransform (total);
            g.drawImageAt (specLayer, 0, 0);
            g.restoreState();
            // 范围外：频谱 base 大部分是透明的，只把"画到的部分"变暗压在灰底上（不整块涂灰，避免矩形灰框伪影）
            g.saveState();
            g.excludeClipRegion (dispRect.toNearestInt());
            g.addTransform (total);
            g.setOpacity (kOutsideImageMul);
            g.drawImageAt (specLayer, 0, 0);
            g.restoreState();
        }

        for (int i = k; i < N; ++i)
            paintImageLayer (g, disp, params.images[(size_t) i]);
    }
    else
    {
        // 无样式（极端情况）：图片仍需可见
        for (const auto& im : params.images)
            paintImageLayer (g, disp, im);
    }

    // v0.5.3: 输出画布边界（"范围内/范围外"分界）。压在图之上、UI 手柄之下。
    {
        const auto dr = outputDisplayRect();
        g.setColour (kBorderCol);
        g.drawLine (juce::Line<float> (dr.getTopLeft(),    dr.getTopRight()),    1.0f);
        g.drawLine (juce::Line<float> (dr.getBottomLeft(), dr.getBottomRight()),  1.0f);
        g.drawLine (juce::Line<float> (dr.getTopLeft(),    dr.getBottomLeft()),   1.0f);
        g.drawLine (juce::Line<float> (dr.getTopRight(),   dr.getBottomRight()),  1.0f);
    }

    // v0.5.4: 蒙版图片编辑模式提示（橙色框 + 文案；范围内拖图，范围外退出）
    if (editMaskImage && params.maskImage.enabled)
    {
        const auto dr = outputDisplayRect();
        const juce::Colour oc (0xFFFF7A00);
        g.setColour (oc);
        const float dd[] = { 6.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (dr.getTopLeft(), dr.getTopRight()), dd, 2, 2.0f);
        g.drawDashedLine (juce::Line<float> (dr.getBottomLeft(), dr.getBottomRight()), dd, 2, 2.0f);
        g.drawDashedLine (juce::Line<float> (dr.getTopLeft(), dr.getBottomLeft()), dd, 2, 2.0f);
        g.drawDashedLine (juce::Line<float> (dr.getTopRight(), dr.getBottomRight()), dd, 2, 2.0f);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText ("Editing mask image — drag inside to move · click outside to stop",
                    dr.reduced (10).removeFromTop (24), juce::Justification::left);
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
// v0.5.3: 范围内正常绘制；范围外**变暗+灰衣**，明确告知"这部分不在导出画布内"。
void SpectrumCanvas::paintImageLayer (juce::Graphics& g,
                                      const juce::AffineTransform& disp,
                                      const ImageLayer& layer)
{
    const juce::Image raw = loadCached (layer.path);
    if (! raw.isValid())
        return;
    // v0.5.4 #6：图层自身色彩调整（identity 零开销；有界缓存共享）
    const juce::Image img = SpectrumMask::adjustedImageCached (raw, layer.path,
                                                               layer.brightness, layer.contrast,
                                                               layer.saturation);
    const float ew = (float) img.getWidth();
    const float eh = (float) img.getHeight();
    const VisTransform tf = layer.transform.set
        ? layer.transform
        : makeContainTransform (ew, eh, (float) params.width, (float) params.height);
    const auto total = buildVisAffine (tf).followedBy (disp);
    const float op = juce::jlimit (0.0f, 1.0f, layer.opacity);
    const auto dispRect = outputDisplayRect();

    // 范围内：正常绘制
    g.saveState();
    g.reduceClipRegion (dispRect.toNearestInt());
    g.addTransform (total);
    g.setOpacity (op);
    g.drawImageAt (img, 0, 0);
    g.restoreState();

    // 范围外：降不透明压在灰底上 → 自然"变暗 + 发灰"（不对 alpha 图整块涂灰，避免透明角出现灰框）
    g.saveState();
    g.excludeClipRegion (dispRect.toNearestInt());
    g.addTransform (total);
    g.setOpacity (op * kOutsideImageMul);
    g.drawImageAt (img, 0, 0);
    g.restoreState();
}

void SpectrumCanvas::paintImages (juce::Graphics& g, const juce::AffineTransform& disp,
                                  bool aboveOnly)
{
    const int N = (int) params.images.size();
    const int k = juce::jlimit (0, N, params.spectrumIndex);
    for (int i = aboveOnly ? k : 0; i < (aboveOnly ? N : k); ++i)
        paintImageLayer (g, disp, params.images[(size_t) i]);
}

void SpectrumCanvas::paintOverlay (juce::Graphics& g)
{
    // 无选中元素或频谱已删除且无图片选中时不画
    if (selectedImage < 0 && ! params.spectrumPresent)
        return;

    const auto disp = displayAffine();

    // v0.5.4 #4：基线轴手柄（仅选中频谱、非蒙版编辑时显示；橙色横线 + 两端把手）
    if (selectedImage < 0 && params.spectrumPresent && ! editMaskImage)
    {
        const float oh = (float) juce::jmax (1, params.height);
        const auto total = buildVisAffine (params.transform).followedBy (disp);
        const juce::Point<float> axL = visTransformPoint (total, { 0.0f, (1.0f - params.baselineY) * oh });   // #4: 0=底部（画布 y 反）
        const juce::Point<float> axR = visTransformPoint (total, { (float) params.width, (1.0f - params.baselineY) * oh });
        baselineScreenY = (axL.getY() + axR.getY()) * 0.5f;   // 命中测试用（近似，轴理论上水平）

        const juce::Colour axisCol (0xFFFF7A00);
        g.setColour (axisCol.withAlpha (0.8f));
        g.drawLine (axL.getX(), axL.getY(), axR.getX(), axR.getY(), 1.6f);
        for (const auto& p : { axL, axR })   // 两端方形把手
        {
            g.setColour (axisCol);
            g.fillRect (p.getX() - 5.0f, p.getY() - 5.0f, 10.0f, 10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRect (p.getX() - 5.0f, p.getY() - 5.0f, 10.0f, 10.0f, 1.0f);
            g.setColour (axisCol);
        }
        g.setColour (axisCol.withAlpha (0.9f));
        // #3：百分比标签与轴线齐平（垂直居中于线）
        g.drawText (juce::String::formatted ("baseline %d%%", (int) std::round (params.baselineY * 100.0f)),
                    (int) axL.getX() + 10, (int) axL.getY() - 8, 130, 16,
                    juce::Justification::centredLeft);
    }

    const auto cOut = currentCorners();                    // 输出坐标（已按选中元素的尺寸）
    // v0.5.4: 编辑蒙版图片时 cOut 是 base 坐标 → 需再套频谱元素变换 P 才到输出，再 disp 到画布
    const auto map = editMaskImage ? buildVisAffine (params.transform).followedBy (disp) : disp;
    // 元素中心 = 四角平均（仿射保持中点；自动跟随选中元素，v0.5.1 修复：
    // 原实现固定用频谱变换的画布中心，选中图片时旋转柄方向错误）
    const auto centerOut = juce::Point<float> (
        (cOut[0].getX() + cOut[1].getX() + cOut[2].getX() + cOut[3].getX()) * 0.25f,
        (cOut[0].getY() + cOut[1].getY() + cOut[2].getY() + cOut[3].getY()) * 0.25f);

    // 映射到画布坐标
    juce::Point<float> cp[4];
    for (int i = 0; i < 4; ++i) cp[i] = visTransformPoint (map, cOut[i]);
    const auto cCenter = visTransformPoint (map, centerOut);

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
    g.fillEllipse (rotHandle.getX() - 7.0f, rotHandle.getY() - 7.0f, 14.0f, 14.0f);
    g.setColour (juce::Colours::white);
    g.drawEllipse (rotHandle.getX() - 7.0f, rotHandle.getY() - 7.0f, 14.0f, 14.0f, 1.0f);

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

    // v0.5.3: 吸附辅助线 + 对齐点标记（画在手柄之上、提示文字之下）
    paintSnapGuides (g, map);

    // 操作提示
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Drag to move - corners:scale - edges:stretch - top:rotate - double-click:reset",
                juce::Rectangle<int> (0, getHeight() - 18, getWidth(), 18),
                juce::Justification::centred);
}

// v0.5.3: CAD 风格吸附辅助线——竖/横虚线 + 两端特征点（中心圆/边中点菱形/角点方框）+ 目标标签
// v0.5.4 #3: 缩放吸附——被拖手柄点对齐画布/其它图片/频谱的特征点（与移动吸附同目标集）。
//   做法：先用未吸附鼠标算出的 t 求手柄输出位置 h0 → 找最近特征点 (dx,dy) →
//   用修正后的鼠标位置重算 t（锚点由 applyAnchorScaled 钉死，吸附不破坏锚定）→ 推辅助线。
void SpectrumCanvas::applyScaleSnap (VisTransform& t, const juce::Point<float>& mouseOut)
{
    activeSnapGuides.clear();
    if (! params.snapEnabled)
        return;

    const float dispS = displayScale();
    const float thresh = (dispS > 1e-3f) ? (8.0f / dispS) : 2.0f;

    const auto h0 = visTransformPoint (buildVisAffine (t), dragHandleElem);
    const float cw = (float) params.width, ch = (float) params.height;

    struct Feat { juce::Point<float> p; SnapKind k; };
    auto featOf = [] (const std::array<juce::Point<float>, 4>& c)
    {
        auto mid = [] (juce::Point<float> a, juce::Point<float> b)
        { return juce::Point<float> ((a.getX() + b.getX()) * 0.5f, (a.getY() + b.getY()) * 0.5f); };
        std::array<Feat, 9> f;
        f[0] = { c[0], SnapKind::Corner }; f[1] = { c[1], SnapKind::Corner };
        f[2] = { c[2], SnapKind::Corner }; f[3] = { c[3], SnapKind::Corner };
        f[4] = { mid (c[0], c[1]), SnapKind::EdgeMid };
        f[5] = { mid (c[1], c[2]), SnapKind::EdgeMid };
        f[6] = { mid (c[2], c[3]), SnapKind::EdgeMid };
        f[7] = { mid (c[3], c[0]), SnapKind::EdgeMid };
        f[8] = { juce::Point<float> (
                     (c[0].getX() + c[1].getX() + c[2].getX() + c[3].getX()) * 0.25f,
                     (c[0].getY() + c[1].getY() + c[2].getY() + c[3].getY()) * 0.25f),
                 SnapKind::Center };
        return f;
    };

    const SnapKind dk = (dragMode >= DragMode::ScaleTL && dragMode <= DragMode::ScaleBR)
                            ? SnapKind::Corner : SnapKind::EdgeMid;

    struct Cand { float dx, dy; juce::Point<float> dragPt, targetPt;
                  SnapKind dragKind, targetKind; juce::String label; float dist; };
    std::vector<Cand> cands;

    auto addTarget = [&] (const std::array<juce::Point<float>, 4>& tc, const juce::String& label)
    {
        for (const auto& tf : featOf (tc))
        {
            const float dx = tf.p.getX() - h0.getX();
            const float dy = tf.p.getY() - h0.getY();
            const float d = std::sqrt (dx * dx + dy * dy);
            if (d < thresh)
                cands.push_back ({ dx, dy, h0, tf.p, dk, tf.k, label, d });
        }
    };

    addTarget (std::array<juce::Point<float>, 4> { { {0, 0}, {cw, 0}, {cw, ch}, {0, ch} } }, "Canvas");

    const bool draggingImage = (selectedImage >= 0);
    const int N = (int) params.images.size();
    for (int i = 0; i < N; ++i)
    {
        if (i == selectedImage) continue;
        const auto& im = params.images[(size_t) i];
        const juce::Image img = loadCached (im.path);
        const float iw = img.isValid() ? (float) img.getWidth()  : cw;
        const float ih = img.isValid() ? (float) img.getHeight() : ch;
        addTarget (visCorners (im.transform, iw, ih), "Image " + juce::String (i + 1));
    }
    if (draggingImage && params.spectrumPresent)
        addTarget (visCorners (params.transform, cw, ch), "Spectrum");

    const Cand* best = nullptr;
    for (const auto& c : cands)
        if (best == nullptr || c.dist < best->dist) best = &c;
    if (best == nullptr)
        return;

    const juce::Point<float> mouseFixed = mouseOut + juce::Point<float> (best->dx, best->dy);
    const VisScaleAxis ax =
        (dragMode == DragMode::ScaleT || dragMode == DragMode::ScaleB) ? VisScaleAxis::OnlyY
      : (dragMode == DragMode::ScaleL || dragMode == DragMode::ScaleR) ? VisScaleAxis::OnlyX
      : VisScaleAxis::Both;
    t = applyAnchorScaled (startTransform, dragAnchorElem, dragHandleElem,
                           mouseFixed, dragStartOut, ax);

    // 辅助线：手柄点吸附后的实际位置 ↔ 目标点
    const juce::Point<float> snappedPt = h0 + juce::Point<float> (best->dx, best->dy);
    if (std::abs (best->dx) > 0.01f)
        activeSnapGuides.push_back ({ true, best->targetPt.getX(), snappedPt, best->targetPt,
                                      dk, best->targetKind, best->label });
    if (std::abs (best->dy) > 0.01f)
        activeSnapGuides.push_back ({ false, best->targetPt.getY(), snappedPt, best->targetPt,
                                      dk, best->targetKind, best->label });
}

void SpectrumCanvas::paintSnapGuides (juce::Graphics& g, const juce::AffineTransform& disp)
{
    if (activeSnapGuides.empty())
        return;

    const juce::Colour lineCol = juce::Colour (0xFFFF7A00);   // 高对比橙（区别于 cyan 旋转柄 / pink 缩放手柄）
    const float dashes[] = { 6.0f, 4.0f };
    const float r = 4.5f;

    auto marker = [&] (juce::Point<float> p, SnapKind k)
    {
        juce::Path path;
        if (k == SnapKind::Corner)
        {
            path.addRectangle (p.getX() - r, p.getY() - r, 2.0f * r, 2.0f * r);
        }
        else if (k == SnapKind::EdgeMid)
        {
            path.startNewSubPath (p.getX(), p.getY() - r);
            path.lineTo (p.getX() + r, p.getY());
            path.lineTo (p.getX(), p.getY() + r);
            path.lineTo (p.getX() - r, p.getY());
            path.closeSubPath();
        }
        else
        {
            path.addEllipse (p.getX() - r, p.getY() - r, 2.0f * r, 2.0f * r);
        }
        g.setColour (lineCol);
        g.fillPath (path);
        g.setColour (juce::Colours::white);
        g.strokePath (path, juce::PathStrokeType (1.0f));
    };

    g.setColour (lineCol.withAlpha (0.9f));
    for (const auto& gd : activeSnapGuides)
    {
        if (gd.vertical)
        {
            const float gx = visTransformPoint (disp, juce::Point<float> (gd.coord, 0.0f)).getX();
            g.drawDashedLine (juce::Line<float> (gx, 0.0f, gx, (float) getHeight()),
                              dashes, 2, 1.0f);
        }
        else
        {
            const float gy = visTransformPoint (disp, juce::Point<float> (0.0f, gd.coord)).getY();
            g.drawDashedLine (juce::Line<float> (0.0f, gy, (float) getWidth(), gy),
                              dashes, 2, 1.0f);
        }
        marker (visTransformPoint (disp, gd.dragPt),   gd.dragKind);
        marker (visTransformPoint (disp, gd.targetPt), gd.targetKind);

        // 目标点旁小标签
        const auto tp = visTransformPoint (disp, gd.targetPt);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (gd.label,
                    juce::Rectangle<float> (tp.getX() + 7.0f, tp.getY() - 8.0f, 90.0f, 16.0f),
                    juce::Justification::left);
    }
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

// 输出分辨率画布（"范围内"）在组件坐标系下的矩形（letterbox 居中）
juce::Rectangle<float> SpectrumCanvas::outputDisplayRect() const
{
    const int ow = juce::jmax (1, params.width);
    const int oh = juce::jmax (1, params.height);
    const float s = displayScale();
    auto b = getLocalBounds();
    return { (b.getWidth()  - ow * s) * 0.5f,
             (b.getHeight() - oh * s) * 0.5f,
             ow * s, oh * s };
}

// v0.5.4: 频谱画框（padding 内绘制区，输出/base 坐标）。与 paint 里渲染 base 用的 canvasRect 同口径。
juce::Rectangle<float> SpectrumCanvas::frameRectOut() const
{
    const float ow = (float) juce::jmax (1, params.width);
    const float oh = (float) juce::jmax (1, params.height);
    return { rp.paddingLeft, rp.paddingTop,
             juce::jmax (1.0f, ow - (rp.paddingLeft + rp.paddingRight)),
             juce::jmax (1.0f, oh - (rp.paddingTop  + rp.paddingBottom)) };
}

// 进入编辑模式时把默认态（与其他图片一致的等比 contain 居中，v0.5.4 #3）烘焙成显式
// VisTransform，好让手柄/四角与渲染完全对齐。
void SpectrumCanvas::ensureMaskTransformInit()
{
    if (params.maskImage.transform.set)
        return;
    const float ow = (float) juce::jmax (1, params.width);
    const float oh = (float) juce::jmax (1, params.height);
    const juce::Image im = loadCached (params.maskImage.path);
    const float iw = im.isValid() ? (float) im.getWidth()  : ow;
    const float ih = im.isValid() ? (float) im.getHeight() : oh;
    params.maskImage.transform = makeContainTransform (iw, ih, ow, oh);
}

// 输出坐标 → base/蒙版图片空间（撤销频谱元素变换；未变换时 identity）
juce::Point<float> SpectrumCanvas::baseFromOutput (juce::Point<float> out) const
{
    if (! params.transform.set)
        return out;
    return visTransformPoint (buildVisAffine (params.transform).inverted(), out);
}

// v0.5.4: 进入编辑模式即烘焙"铺满画框"变换，使手柄/四角在首次点击前就与渲染对齐。
void SpectrumCanvas::setEditMaskImage (bool on)
{
    editMaskImage = on;
    if (on && params.maskImage.enabled && ! params.maskImage.path.isEmpty())
        ensureMaskTransformInit();
    repaint();
}

// 当前选中元素的基础尺寸（输出坐标）：频谱 = 输出画布；图片 = 图片自然尺寸
std::pair<float, float> SpectrumCanvas::activeElementSize() const
{
    if (editMaskImage)                       // v0.5.4：编辑蒙版图片 → 图片自然尺寸
    {
        const juce::Image img = loadCached (params.maskImage.path);
        if (img.isValid())
            return { (float) img.getWidth(), (float) img.getHeight() };
        const auto fr = frameRectOut();
        return { fr.getWidth(), fr.getHeight() };
    }
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
    if (editMaskImage)                        // v0.5.4：编辑蒙版图片
        return visCorners (params.maskImage.transform, w, h);
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

SpectrumCanvas::DragMode SpectrumCanvas::hitHandleForCorners (
    const std::array<juce::Point<float>, 4>& c,
    float elemW, float elemH,
    juce::Point<float> out) const
{
    const float s = displayScale();
    const float hitR = (s > 1e-3f) ? (kHitRadiusPx / s) : (kHitRadiusPx * 20.0f);
    juce::ignoreUnused (elemW, elemH);

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
            // v0.5.3: 旋转柄命中仅限圆柄圆心——修复\"上边中点被旋转抢走\"
            //   （此前是 topMid→圆柄整条线段且先于边柄判定；现上边中点完整归还 ScaleT）
            if (outer.getDistanceFrom (out) <= (kRotHitRadiusPx / s))
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

SpectrumCanvas::DragMode SpectrumCanvas::hitHandle (juce::Point<float> out) const
{
    // v0.5.4: 编辑蒙版图片 → 命中测试针对该图片
    if (editMaskImage)
    {
        const auto [w, h] = activeElementSize();
        const auto c = visCorners (params.maskImage.transform, w, h);
        return hitHandleForCorners (c, w, h, out);
    }

    // 选中的是图片 → 命中测试针对该图片
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
    {
        const auto& layer = params.images[(size_t) selectedImage];
        const juce::Image img = loadCached (layer.path);
        const float w = img.isValid() ? (float) img.getWidth()  : (float) params.width;
        const float h = img.isValid() ? (float) img.getHeight() : (float) params.height;
        const auto c = visCorners (layer.transform, w, h);
        return hitHandleForCorners (c, w, h, out);
    }

    // 选中频谱（或默认）→ 命中测试针对频谱
    if (! params.spectrumPresent)
        return DragMode::None;
    const auto c = currentCorners();
    return hitHandleForCorners (c, (float) params.width, (float) params.height, out);
}

void SpectrumCanvas::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();   // 确保 Delete/Backspace 键能到达本组件

    if (! hasAudio)
    {
        if (onEmptyClicked)
            onEmptyClicked();
        return;
    }

    const auto out = toOutput (e.position);

    // v0.5.4 #4：基线轴命中（优先于手柄/元素拾取；仅选中频谱、非蒙版编辑）
    if (selectedImage < 0 && params.spectrumPresent && ! editMaskImage
        && std::abs (e.position.getY() - baselineScreenY) <= 8.0f)
    {
        dragMode = DragMode::BaselineAxis;
        beginTransformIfNeeded();
        repaint();
        return;
    }

    // v0.5.4: 蒙版图片编辑模式——角/边手柄=独立拉伸图片，body=平移，点输出画框外=退出编辑。
    //   复用与图片/频谱完全相同的手柄 & 对边锚定 & 吸附机制（activeTransform/Size 在编辑态指向蒙版图片）。
    if (editMaskImage)
    {
        if (! params.maskImage.enabled || params.maskImage.path.isEmpty())
        {
            editMaskImage = false;
            dragMode = DragMode::None;
            repaint();
            return;
        }
        ensureMaskTransformInit();
        const auto [iw, ih] = activeElementSize();
        const auto corners = visCorners (params.maskImage.transform, iw, ih);   // base 坐标
        const auto b       = baseFromOutput (out);                              // 鼠标 → base

        const auto hh      = hitHandle (b);
        const bool bodyHit = visContains (corners, b);
        // "频谱范围" = 频谱元素框（输出坐标，默认 = 整画布）；点其外 → 退出编辑
        const auto specFrame = visCorners (params.transform,
                                           (float) juce::jmax (1, params.width),
                                           (float) juce::jmax (1, params.height));
        const bool inSpectrum = visContains (specFrame, out);

        if (hh == DragMode::None && ! bodyHit)
        {
            if (! inSpectrum)
                editMaskImage = false;               // 点频谱框外 → 停止编辑
            dragMode = DragMode::None;
            repaint();
            return;
        }

        dragMode       = (hh != DragMode::None) ? hh : DragMode::Move;
        dragStartOut   = b;
        startTransform = params.maskImage.transform;

        if (dragMode >= DragMode::ScaleTL && dragMode <= DragMode::ScaleR)
        {
            switch (dragMode)
            {
                case DragMode::ScaleTL: dragAnchorElem = { iw, ih };      dragHandleElem = { 0,  0 };  break;
                case DragMode::ScaleTR: dragAnchorElem = { 0,  ih };      dragHandleElem = { iw, 0 };  break;
                case DragMode::ScaleBR: dragAnchorElem = { 0,  0 };       dragHandleElem = { iw, ih }; break;
                case DragMode::ScaleBL: dragAnchorElem = { iw, 0 };       dragHandleElem = { 0,  ih }; break;
                case DragMode::ScaleT:  dragAnchorElem = { iw*0.5f, ih }; dragHandleElem = { iw*0.5f, 0 };  break;
                case DragMode::ScaleB:  dragAnchorElem = { iw*0.5f, 0 };  dragHandleElem = { iw*0.5f, ih }; break;
                case DragMode::ScaleL:  dragAnchorElem = { iw, ih*0.5f }; dragHandleElem = { 0, ih*0.5f }; break;
                case DragMode::ScaleR:  dragAnchorElem = { 0, ih*0.5f };  dragHandleElem = { iw, ih*0.5f }; break;
                default: break;
            }
        }
        repaint();
        return;
    }

    // 1) 先检查当前选中元素的手柄
    auto h = hitHandle (out);

    // 2) 未命中手柄 → 统一 z 序元素拾取（从栈顶向下：上方图片 → 频谱 → 下方图片）
    if (h == DragMode::None)
    {
        const int N = (int) params.images.size();
        const int k = juce::jlimit (0, N, params.spectrumIndex);
        int picked = -2;                       // -2 = 无命中；-1 = 频谱；>=0 = 图片下标

        auto tryImage = [&] (int i) -> bool
        {
            const juce::Image img = loadCached (params.images[(size_t) i].path);
            const float w = img.isValid() ? (float) img.getWidth()  : (float) params.width;
            const float h2 = img.isValid() ? (float) img.getHeight() : (float) params.height;
            return visContains (visCorners (params.images[(size_t) i].transform, w, h2), out);
        };

        for (int i = N - 1; i >= k && picked == -2; --i)      // 上方图片组（顶→底）
            if (tryImage (i)) picked = i;
        if (picked == -2 && params.spectrumPresent
            && visContains (visCorners (params.transform,
                                        (float) params.width, (float) params.height), out))
            picked = -1;                                       // 频谱
        for (int i = k - 1; i >= 0 && picked == -2; --i)      // 下方图片组（顶→底）
            if (tryImage (i)) picked = i;

        if (picked == -2)
        {
            // v0.5.6 #2：点空白——Ctrl 按住则保留当前多选（便于继续框选/操作），否则清空回单选
            if (! e.mods.isCtrlDown() && ! e.mods.isCommandDown())
            {
                selectedSet.clear();
                dragMode = DragMode::None;
                repaint();
            }
            return;
        }

        // 多选切换：Ctrl/Cmd+点击 = 增删该元素（锚点跟到被点元素）；普通点击 = 单选
        const bool ctrl = e.mods.isCtrlDown() || e.mods.isCommandDown();
        if (ctrl)
        {
            const auto it = std::find (selectedSet.begin(), selectedSet.end(), picked);
            if (it != selectedSet.end())
                selectedSet.erase (it);            // 已有 → 取消选中
            else
                selectedSet.push_back (picked);   // 没有 → 加入
            if (selectedSet.empty())
            {
                selectedImage = -1;
                dragMode = DragMode::None;
                repaint();
                return;
            }
            selectedImage = picked;               // 锚点=刚点的，便于面板显示
        }
        else
        {
            selectedSet = { picked };
            selectedImage = picked;
        }
        if (picked >= 0)
            activeTransform();     // 惰性初始化该图层变换
        else
            beginTransformIfNeeded();
        h = hitHandle (out);       // 对新选中元素重新命中手柄
    }

    // 3) bodyHit 检查：当前选中元素是否包含点击点
    bool bodyHit = false;
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
    {
        const juce::Image img = loadCached (params.images[(size_t) selectedImage].path);
        const float w = img.isValid() ? (float) img.getWidth()  : (float) params.width;
        const float h2 = img.isValid() ? (float) img.getHeight() : (float) params.height;
        bodyHit = visContains (visCorners (params.images[(size_t) selectedImage].transform, w, h2), out);
    }
    else if (selectedImage < 0 && params.spectrumPresent)
    {
        bodyHit = visContains (visCorners (params.transform,
                                           (float) params.width, (float) params.height), out);
    }

    if (h == DragMode::None && ! bodyHit)
    {
        dragMode = DragMode::None;
        repaint();
        return;
    }

    if (selectedImage < 0)
        beginTransformIfNeeded();
    dragStartOut   = out;
    startTransform = activeTransform();
    dragMode = (h != DragMode::None) ? h : DragMode::Move;

    // 对边锚定缩放：记录锚点与被拖点（均为元素坐标）
    if (dragMode >= DragMode::ScaleTL && dragMode <= DragMode::ScaleR)
    {
        const auto [ew, eh] = activeElementSize();
        const auto mid = [] (juce::Point<float> a, juce::Point<float> b)
        {
            return juce::Point<float> ((a.getX() + b.getX()) * 0.5f,
                                       (a.getY() + b.getY()) * 0.5f);
        };

        switch (dragMode)
        {
            case DragMode::ScaleTL: dragAnchorElem = { ew, eh }; dragHandleElem = { 0,  0 }; break;
            case DragMode::ScaleTR: dragAnchorElem = { 0, eh };  dragHandleElem = { ew, 0 }; break;
            case DragMode::ScaleBR: dragAnchorElem = { 0,  0 };  dragHandleElem = { ew, eh }; break;
            case DragMode::ScaleBL: dragAnchorElem = { ew,  0 }; dragHandleElem = { 0,  eh }; break;
            case DragMode::ScaleT:  dragAnchorElem = { ew * 0.5f, eh };
                                    dragHandleElem = { ew * 0.5f, 0 }; break;
            case DragMode::ScaleB:  dragAnchorElem = { ew * 0.5f, 0 };
                                    dragHandleElem = { ew * 0.5f, eh }; break;
            case DragMode::ScaleL:  dragAnchorElem = { ew, eh * 0.5f };
                                    dragHandleElem = { 0,  eh * 0.5f }; break;
            case DragMode::ScaleR:  dragAnchorElem = { 0,  eh * 0.5f };
                                    dragHandleElem = { ew, eh * 0.5f }; break;
            default: break;
        }
    }

    repaint();
}

void SpectrumCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode != DragMode::None && ! gestureReported)
    {
        gestureReported = true;
        if (onGestureStart) onGestureStart ();   // v0.5.5 #3：拖拽起点 = 一次 undo 快照边界
    }
    if (dragMode == DragMode::None)
        return;

    const auto out = toOutput (e.position);

    // v0.5.4 #4：拖基线轴——鼠标 y 反算回 base 空间 → baselineY，吸附格点；½ 时提示 mirror
    if (dragMode == DragMode::BaselineAxis)
    {
        const auto b = baseFromOutput (out);
        const float oh = (float) juce::jmax (1, params.height);
        float a = juce::jlimit (0.0f, 1.0f, 1.0f - b.getY() / oh);   // #4: 自底向上

        activeSnapGuides.clear();
        if (params.snapEnabled)
        {
            static constexpr float kStops[] = { 0.0f, 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f };
            const float dispS = displayScale();
            const float thr = (dispS > 1e-3f) ? (8.0f / dispS) : 2.0f;
            for (float st : kStops)
                if (std::abs (a - st) * oh * dispS <= 8.0f)
                { a = st; break; }
        }
        // ½ 吸附提示：mirror（复用吸附辅助线画一条横线 + 文本由 paintOverlay 的百分比标签表达）
        params.baselineY = a;
        if (params.snapEnabled && std::abs (a - 0.5f) < 1e-4f)
            activeSnapGuides.push_back ({ false, b.getY(),
                                          { 0.0f, b.getY() }, { (float) params.width, b.getY() },
                                          SnapKind::EdgeMid, SnapKind::EdgeMid, "mirror" });
        repaint();
        return;
    }

    // v0.5.4: 编辑蒙版图片时，位移/缩放在 base 坐标里算（与 activeTransform=mask 一致）
    if (editMaskImage && dragMode != DragMode::None)
    {
        const auto b  = baseFromOutput (out);
        auto& mt = activeTransform();

        if (dragMode == DragMode::Move)
        {
            const float ow = (float) juce::jmax (1, params.width);
            const float oh = (float) juce::jmax (1, params.height);
            const float s  = displayScale();
            const float thr = (s > 1e-3f) ? (8.0f / s) : 2.0f;

            float px = startTransform.posX + (b.getX() - dragStartOut.getX());
            float py = startTransform.posY + (b.getY() - dragStartOut.getY());

            if (params.snapEnabled)
            {
                const auto [iw, ih] = activeElementSize();
                const auto cc = visCorners (VisTransform { true, startTransform.centerX, startTransform.centerY,
                                              startTransform.scaleX, startTransform.scaleY,
                                              startTransform.rotationDeg, px, py }, iw, ih);
                auto mid = [] (juce::Point<float> a, juce::Point<float> c)
                { return juce::Point<float> ((a.getX()+c.getX())*0.5f,(a.getY()+c.getY())*0.5f); };
                const float fx[3] = { 0.0f, ow*0.5f, ow };          // base 频谱框：左/中/右
                const float fy[3] = { 0.0f, oh*0.5f, oh };
                float bestDx = 0.0f, bestPx = thr + 1.0f;
                float bestDy = 0.0f, bestPy = thr + 1.0f;
                const juce::Point<float> ptsX[6] = { cc[0], cc[3], mid(cc[0],cc[1]), mid(cc[1],cc[2]),
                                                     mid(cc[2],cc[3]), mid(cc[3],cc[0]) };
                for (auto& pt : ptsX)
                    for (float tx : fx)
                    { const float d = std::abs (pt.getX()-tx); if (d<bestPx){bestPx=d; bestDx=tx-pt.getX();} }
                const juce::Point<float> ptsY[6] = { cc[0], cc[1], mid(cc[0],cc[1]), mid(cc[1],cc[2]),
                                                     mid(cc[2],cc[3]), mid(cc[3],cc[0]) };
                for (auto& pt : ptsY)
                    for (float ty : fy)
                    { const float d = std::abs (pt.getY()-ty); if (d<bestPy){bestPy=d; bestDy=ty-pt.getY();} }
                px += bestDx;
                py += bestDy;
            }

            mt.posX = px;
            mt.posY = py;
            repaint();
            return;
        }

        if (dragMode == DragMode::Rotate)
        {
            const auto [iw, ih] = activeElementSize();
            const auto cc = visTransformPoint (buildVisAffine (startTransform),
                                               juce::Point<float> (iw * 0.5f, ih * 0.5f));
            const float a0 = std::atan2 (dragStartOut.getY() - cc.getY(),
                                         dragStartOut.getX() - cc.getX());
            const float a1 = std::atan2 (b.getY() - cc.getY(), b.getX() - cc.getX());
            mt.rotationDeg = startTransform.rotationDeg + juce::radiansToDegrees (a1 - a0);
            repaint();
            return;
        }

        // 角/边拉伸（对边锚定，base 坐标）
        if (dragMode >= DragMode::ScaleTL && dragMode <= DragMode::ScaleR)
        {
            const VisScaleAxis ax =
                (dragMode == DragMode::ScaleT || dragMode == DragMode::ScaleB) ? VisScaleAxis::OnlyY
              : (dragMode == DragMode::ScaleL || dragMode == DragMode::ScaleR) ? VisScaleAxis::OnlyX
              : VisScaleAxis::Both;
            // #3（v0.5.4）：缩放时被拖手柄点吸附到频谱画框特征线（base 空间 0/中/满）。
            //   先按未吸附鼠标算一次 → 取手柄输出位置 → 找最近目标线 → 修正鼠标重算
            //   （锚点仍被 applyAnchorScaled 钉死，缩放吸附不破坏锚定）。
            mt = applyAnchorScaled (startTransform, dragAnchorElem, dragHandleElem, b, dragStartOut, ax);
            activeSnapGuides.clear();
            if (params.snapEnabled)
            {
                const auto h0 = visTransformPoint (buildVisAffine (mt), dragHandleElem);
                const float ow = (float) juce::jmax (1, params.width);
                const float oh = (float) juce::jmax (1, params.height);
                const float s  = displayScale();
                const float thr = (s > 1e-3f) ? (8.0f / s) : 2.0f;

                const float fx[3] = { 0.0f, ow * 0.5f, ow };
                const float fy[3] = { 0.0f, oh * 0.5f, oh };
                float bestPx = thr + 1.0f, bestPy = thr + 1.0f;
                bool hitX = false, hitY = false;
                float snapX = 0.0f, snapY = 0.0f;
                for (float tx : fx)
                { const float d = std::abs (h0.getX() - tx); if (d < bestPx) { bestPx = d; snapX = tx; hitX = true; } }
                for (float ty : fy)
                { const float d = std::abs (h0.getY() - ty); if (d < bestPy) { bestPy = d; snapY = ty; hitY = true; } }

                if (hitX || hitY)
                {
                    juce::Point<float> bFixed = b;
                    if (hitX) bFixed.x += (snapX - h0.getX());
                    if (hitY) bFixed.y += (snapY - h0.getY());
                    mt = applyAnchorScaled (startTransform, dragAnchorElem, dragHandleElem,
                                            bFixed, dragStartOut, ax);
                }
                if (hitX)
                    activeSnapGuides.push_back ({ true, snapX, h0,
                                                  { snapX, h0.getY() }, SnapKind::Corner, SnapKind::EdgeMid,
                                                  "Frame" });
                if (hitY)
                    activeSnapGuides.push_back ({ false, snapY, h0,
                                                  { hitX ? snapX : h0.getX(), snapY }, SnapKind::Corner, SnapKind::EdgeMid,
                                                  "Frame" });
            }
            repaint();
            return;
        }
    }

    auto& t = activeTransform();
    const auto [ew, eh] = activeElementSize();

    switch (dragMode)
    {
    case DragMode::Move:
    {
        activeSnapGuides.clear();   // v0.5.3: 每次拖拽重算辅助线（关吸附 / 未命中 → 空）

        float dx = out.getX() - dragStartOut.getX();
        float dy = out.getY() - dragStartOut.getY();

        // P3: 移动吸附
        if (params.snapEnabled)
        {
            const float dispS = displayScale();
            const float thresh = (dispS > 1e-3f) ? (8.0f / dispS) : 2.0f;
            const float newPosX = startTransform.posX + dx;
            const float newPosY = startTransform.posY + dy;
            const auto corners = visCorners (VisTransform { true, startTransform.centerX, startTransform.centerY,
                                              startTransform.scaleX, startTransform.scaleY,
                                              startTransform.rotationDeg, newPosX, newPosY },
                                              ew, eh);
            // —— 特征点吸附（v0.5.3 N2 增强）——
            //   被拖元素与每个目标各取 9 个特征点（4 角 + 4 边中点 + 中心），逐点按 X/Y 轴对齐比较。
            //   取代旧版"仅被拖中心对齐目标 + 边只对齐画布"：
            //     · 修复两张图片边-边不吸附（旧版被拖侧只有中心参与目标比较）
            //     · 辅助线/标记落在真实角点、边中点而非隐形 AABB（旋转元素 AABB 与视觉不符 → 旧"虚空"感）
            const float cw = (float) params.width, ch = (float) params.height;
            const bool draggingImage = (selectedImage >= 0);   // v0.5.3(B5): 画布特征点仅图片吸附

            struct Feat { juce::Point<float> p; SnapKind k; };
            auto featOf = [] (const std::array<juce::Point<float>, 4>& c)
            {
                auto mid = [] (juce::Point<float> a, juce::Point<float> b)
                { return juce::Point<float> ((a.getX() + b.getX()) * 0.5f, (a.getY() + b.getY()) * 0.5f); };
                std::array<Feat, 9> f;
                f[0] = { c[0], SnapKind::Corner }; f[1] = { c[1], SnapKind::Corner };
                f[2] = { c[2], SnapKind::Corner }; f[3] = { c[3], SnapKind::Corner };
                f[4] = { mid (c[0], c[1]), SnapKind::EdgeMid };   // top
                f[5] = { mid (c[1], c[2]), SnapKind::EdgeMid };   // right
                f[6] = { mid (c[2], c[3]), SnapKind::EdgeMid };   // bottom
                f[7] = { mid (c[3], c[0]), SnapKind::EdgeMid };   // left
                f[8] = { juce::Point<float> (
                             (c[0].getX() + c[1].getX() + c[2].getX() + c[3].getX()) * 0.25f,
                             (c[0].getY() + c[1].getY() + c[2].getY() + c[3].getY()) * 0.25f),
                         SnapKind::Center };
                return f;
            };
            const auto df = featOf (corners);   // 被拖元素 9 特征点（proposed 位置）

            struct Cand { float dragVal, targetVal; juce::Point<float> dragPt, targetPt;
                          SnapKind dragKind, targetKind; juce::String label; };
            std::vector<Cand> xCands, yCands;   // xCands→竖线(X 吸附)  yCands→横线(Y 吸附)

            auto addTarget = [&] (const std::array<juce::Point<float>, 4>& tc, const juce::String& label)
            {
                const auto tf = featOf (tc);
                for (int d = 0; d < 9; ++d)
                    for (int a = 0; a < 9; ++a)
                    {
                        if (std::abs (df[d].p.getX() - tf[a].p.getX()) < thresh)
                            xCands.push_back ({ df[d].p.getX(), tf[a].p.getX(), df[d].p, tf[a].p, df[d].k, tf[a].k, label });
                        if (std::abs (df[d].p.getY() - tf[a].p.getY()) < thresh)
                            yCands.push_back ({ df[d].p.getY(), tf[a].p.getY(), df[d].p, tf[a].p, df[d].k, tf[a].k, label });
                    }
            };

            if (draggingImage)
                addTarget (std::array<juce::Point<float>, 4> { { {0, 0}, {cw, 0}, {cw, ch}, {0, ch} } }, "Canvas");

            const int N = (int) params.images.size();
            for (int i = 0; i < N; ++i)
            {
                if (i == selectedImage) continue;
                const auto& im = params.images[(size_t) i];
                const juce::Image img = loadCached (im.path);
                const float iw = img.isValid() ? (float) img.getWidth()  : cw;
                const float ih = img.isValid() ? (float) img.getHeight() : ch;
                addTarget (visCorners (im.transform, iw, ih), "Image " + juce::String (i + 1));
            }

            // 频谱作为目标——仅拖图片时（N1：不把被拖频谱自身当目标，避免自我吸附抖动）
            if (draggingImage && params.spectrumPresent)
                addTarget (visCorners (params.transform, cw, ch), "Spectrum");

            float bestDx = 0.0f, bestDy = 0.0f;
            float bestDistX = thresh + 1.0f, bestDistY = thresh + 1.0f;
            const Cand* winX = nullptr;
            const Cand* winY = nullptr;
            for (auto& c : xCands)   // 已在 push 时按 thresh 剪枝，这里只取最近
            { const float dd = std::abs (c.dragVal - c.targetVal);
              if (dd < bestDistX) { bestDistX = dd; bestDx = c.targetVal - c.dragVal; winX = &c; } }
            for (auto& c : yCands)
            { const float dd = std::abs (c.dragVal - c.targetVal);
              if (dd < bestDistY) { bestDistY = dd; bestDy = c.targetVal - c.dragVal; winY = &c; } }

            // 命中即生成辅助线；drag 端标记取吸附后真实位置（+bestD/bestDy）
            if (winX != nullptr)
                activeSnapGuides.push_back ({ true, winX->targetVal,
                    { winX->dragPt.getX() + bestDx, winX->dragPt.getY() + bestDy }, winX->targetPt,
                    winX->dragKind, winX->targetKind, winX->label });
            if (winY != nullptr)
                activeSnapGuides.push_back ({ false, winY->targetVal,
                    { winY->dragPt.getX() + bestDx, winY->dragPt.getY() + bestDy }, winY->targetPt,
                    winY->dragKind, winY->targetKind, winY->label });

            dx += bestDx;
            dy += bestDy;
        }

        t.posX = startTransform.posX + dx;
        t.posY = startTransform.posY + dy;
        break;
    }

    case DragMode::Rotate:
    {
        const auto [ew2, eh2] = activeElementSize();
        const auto cc = visTransformPoint (buildVisAffine (startTransform),
                                           juce::Point<float> (ew2 * 0.5f, eh2 * 0.5f));
        const float a0 = std::atan2 (dragStartOut.getY() - cc.getY(),
                                     dragStartOut.getX() - cc.getX());
        const float a1 = std::atan2 (out.getY() - cc.getY(),
                                     out.getX() - cc.getX());
        float rot = startTransform.rotationDeg + juce::radiansToDegrees (a1 - a0);

        // P3: 旋转吸附（接近 90°×n 自动校正）
        if (params.snapEnabled)
        {
            const float snapDeg = 3.0f;
            const float n = std::round (rot / 90.0f);
            const float snapped = n * 90.0f;
            if (std::abs (rot - snapped) <= snapDeg)
                rot = snapped;
        }

        t.rotationDeg = rot;
        break;
    }

    case DragMode::ScaleTL: case DragMode::ScaleTR:
    case DragMode::ScaleBR: case DragMode::ScaleBL:
    {
        // P2: 对边锚定等比缩放——对角固定不动
        t = applyAnchorScaled (startTransform, dragAnchorElem, dragHandleElem, out, dragStartOut,
                               VisScaleAxis::Both);
        applyScaleSnap (t, out);
        break;
    }

    case DragMode::ScaleT: case DragMode::ScaleB:
    case DragMode::ScaleL: case DragMode::ScaleR:
    {
        // P2: 对边锚定单轴拉伸——对边中点固定不动（仅单轴缩放）
        const bool vertical = (dragMode == DragMode::ScaleT || dragMode == DragMode::ScaleB);
        t = applyAnchorScaled (startTransform, dragAnchorElem, dragHandleElem, out, dragStartOut, vertical ? VisScaleAxis::OnlyY : VisScaleAxis::OnlyX);
        applyScaleSnap (t, out);
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
    gestureReported = false;            // v0.5.5 #3：下一次拖拽重新计快照
    activeSnapGuides.clear();   // v0.5.3: 松开鼠标清除吸附辅助线
    repaint();
}

void SpectrumCanvas::mouseMove (const juce::MouseEvent& e)
{
    updateHoverCursor (toOutput (e.position));
}

void SpectrumCanvas::updateHoverCursor (juce::Point<float> out)
{
    const auto h = hitHandle (out);
    const bool body = (selectedImage >= 0 && selectedImage < (int) params.images.size())
        ? [&]
        {
            const auto& layer = params.images[(size_t) selectedImage];
            const juce::Image img = loadCached (layer.path);
            const float w = img.isValid() ? (float) img.getWidth()  : (float) params.width;
            const float h2 = img.isValid() ? (float) img.getHeight() : (float) params.height;
            return visContains (visCorners (layer.transform, w, h2), out);
        }()
        : (params.spectrumPresent && visContains (currentCorners(), out));

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
    // 双击元素 → 复位变换：频谱回铺满画布，图片回等比 contain 居中
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
    {
        const juce::Image img = loadCached (params.images[(size_t) selectedImage].path);
        const float ew = img.isValid() ? (float) img.getWidth()  : (float) params.width;
        const float eh = img.isValid() ? (float) img.getHeight() : (float) params.height;
        params.images[(size_t) selectedImage].transform
            = makeContainTransform (ew, eh, (float) params.width, (float) params.height);
    }
    else if (params.spectrumPresent)
    {
        activeTransform() = VisTransform {};
    }
    dragMode = DragMode::None;
    repaint();
}

bool SpectrumCanvas::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (onDeleteRequested)
            onDeleteRequested();
        return true;
    }
    return false;
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
    if (editMaskImage)                       // v0.5.4
        return params.maskImage.transform;
    if (selectedImage >= 0 && selectedImage < (int) params.images.size())
        return params.images[(size_t) selectedImage].transform;
    return params.transform;
}

VisTransform& SpectrumCanvas::activeTransform()
{
    if (editMaskImage)                        // v0.5.4：编辑蒙版图片
    {
        ensureMaskTransformInit();
        return params.maskImage.transform;
    }
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

// v0.5.6 新 #2：按 tag 取可写 transform（含图片层的惰性 contain 初始化，与 activeTransform 一致）
VisTransform& SpectrumCanvas::transformForTag (int tag)
{
    if (tag >= 0 && tag < (int) params.images.size())
    {
        auto& t = params.images[(size_t) tag].transform;
        if (! t.set)
        {
            const juce::Image img = loadCached (params.images[(size_t) tag].path);
            const float ew = img.isValid() ? (float) img.getWidth()  : (float) params.width;
            const float eh = img.isValid() ? (float) img.getHeight() : (float) params.height;
            t = makeContainTransform (ew, eh, (float) params.width, (float) params.height);
        }
        return t;
    }
    beginTransformIfNeeded();          // 频谱：首次交互铺开
    return params.transform;
}

void SpectrumCanvas::selectAllLayers()
{
    if (editMaskImage)                 // 蒙版图片编辑态不参与多选（那是"子层"单独平移）
        return;
    selectedSet.clear();
    if (params.spectrumPresent)
        selectedSet.push_back (-1);
    for (int i = 0; i < (int) params.images.size(); ++i)
        selectedSet.push_back (i);
    if (selectedSet.empty())
        return;
    selectedImage = selectedSet.back();   // 锚点=最后一个，供面板仍显示某一层属性
    dragMode = DragMode::None;            // 任何进行中的手势作废
    repaint();
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