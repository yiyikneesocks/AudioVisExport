// =============================================================================
// VisTransform.h — 频谱元素自由变换（预览与导出同一套代码）
//
// 设计意图：
//   · 频谱作为一个"图层元素"：在**输出分辨率坐标系**内可自由拖动 / 等比缩放 /
//     非等比拉伸 / 绕中心旋转。
//   · GUI 画布（SpectrumCanvas）直接拖拽修改本结构；导出管线（VisPipeline）
//     用同一个 buildVisAffine() 做合成——预览即所得。
//   · set==false 表示"未变换 = 填满整个输出画布"，即旧版行为；用户首次拖拽时
//     才置为 set=true。
//   · 后续引入图片 / 视频图层时，可复用本文件的 affine 与命中测试函数。
// =============================================================================
#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>

// 元素变换参数（坐标空间 = 输出分辨率像素 0..width, 0..height）
struct VisTransform
{
    bool  set = false;          // false = 填满画布（旧行为，仅频谱用）
    float centerX = 0.0f;       // 枢轴 X（元素自身坐标系：缩放/旋转的不动点）
    float centerY = 0.0f;       // 枢轴 Y
    float scaleX  = 1.0f;       // X 轴缩放
    float scaleY  = 1.0f;       // Y 轴缩放
    float rotationDeg = 0.0f;   // 绕枢轴旋转（度，逆时针为正）
    float posX    = 0.0f;       // 附加平移 X（输出像素；v0.5.1 图片图层用，频谱保持 0）
    float posY    = 0.0f;       // 附加平移 Y
};

// 构建"输出坐标 → 元素变换后坐标"的 AffineTransform：
//   先平移到枢轴为原点 → 旋转 → 缩放 → 平移回枢轴 + 附加平移
// posX/posY == 0 时与 v0.5.0 之前行为完全一致（频谱 JSON 兼容）。
inline juce::AffineTransform buildVisAffine (const VisTransform& t)
{
    auto m = juce::AffineTransform::translation (-t.centerX, -t.centerY);
    m = m.rotated (juce::degreesToRadians (t.rotationDeg));
    m = m.scaled (t.scaleX, t.scaleY);
    return m.translated (t.centerX + t.posX, t.centerY + t.posY);
}

// 图片图层初始变换：等比 contain 适配输出画布并居中。
// 枢轴 = 元素自身中心；平移 = 把元素中心摆到画布中心。
inline VisTransform makeContainTransform (float elemW, float elemH,
                                          float outW, float outH)
{
    VisTransform t;
    t.set      = true;
    t.scaleX   = t.scaleY = juce::jmin (outW / juce::jmax (1.0f, elemW),
                                        outH / juce::jmax (1.0f, elemH));
    t.centerX  = elemW * 0.5f;
    t.centerY  = elemH * 0.5f;
    t.posX     = outW * 0.5f - t.scaleX * t.centerX;
    t.posY     = outH * 0.5f - t.scaleY * t.centerY;
    t.rotationDeg = 0.0f;
    return t;
}

// JUCE 8 的 AffineTransform::transformPoint 只有 (x&, y&) 两参重载，没有单 Point 重载，
// 统一走这个辅助（GUI 与导出共用）。
inline juce::Point<float> visTransformPoint (const juce::AffineTransform& m,
                                             juce::Point<float> p)
{
    float x = p.getX();
    float y = p.getY();
    m.transformPoint (x, y);
    return juce::Point<float> (x, y);
}

// 元素外接四角（变换后，输出坐标）：TL / TR / BR / BL
// 用于画布命中测试 / 手柄绘制；w、h = 元素基础尺寸（输出分辨率）。
inline std::array<juce::Point<float>, 4> visCorners (const VisTransform& t,
                                                     float w, float h)
{
    const auto m = buildVisAffine (t);
    const float px[4] = { 0.0f, w,  w,   0.0f };
    const float py[4] = { 0.0f, 0.0f, h, h    };
    std::array<juce::Point<float>, 4> c;
    for (int i = 0; i < 4; ++i)
        c[i] = visTransformPoint (m, juce::Point<float> (px[i], py[i]));
    return c;
}

// 点是否落在凸四边形内（含旋转后的频谱外框）
inline bool visContains (const std::array<juce::Point<float>, 4>& c,
                         juce::Point<float> p)
{
    auto cross2 = [] (juce::Point<float> a, juce::Point<float> b, juce::Point<float> q)
    {
        return (b.getX() - a.getX()) * (q.getY() - a.getY())
             - (b.getY() - a.getY()) * (q.getX() - a.getX());
    };

    const float s0 = cross2 (c[0], c[1], p);
    for (int i = 1; i < 4; ++i)
    {
        const float s = cross2 (c[i], c[(i + 1) % 4], p);
        if ((s < 0.0f) != (s0 < 0.0f))
            return false;
    }
    return true;
}

// 点到线段距离（输出像素）
inline float visDistanceToSegment (juce::Point<float> a, juce::Point<float> b,
                                   juce::Point<float> p)
{
    const float abx = b.getX() - a.getX();
    const float aby = b.getY() - a.getY();
    const float len2 = abx * abx + aby * aby;
    float t = 0.0f;
    if (len2 > 1e-6f)
        t = juce::jlimit (0.0f, 1.0f,
                          ((p.getX() - a.getX()) * abx + (p.getY() - a.getY()) * aby) / len2);
    const float dx = p.getX() - (a.getX() + abx * t);
    const float dy = p.getY() - (a.getY() + aby * t);
    return std::sqrt (dx * dx + dy * dy);
}

// 对边锚定缩放轴模式：角柄 = 双轴等比；上下边 = 仅 Y；左右边 = 仅 X
enum class VisScaleAxis { Both, OnlyX, OnlyY };

// 对边锚定缩放：拖角/边时锚点（对角/对边中点）在输出坐标中固定不动。
// startT = 拖拽开始时的变换；anchorElem / draggedElem = 锚点与被拖点的**元素坐标**；
// outPoint = 当前鼠标输出坐标。枢轴 = startT.centerX/centerY（无需元素尺寸）。
// 返回：缩放后的 VisTransform（pos 已补偿使锚点输出位置恒定）。
inline VisTransform applyAnchorScaled (const VisTransform& startT,
                                       juce::Point<float> anchorElem,
                                       juce::Point<float> draggedElem,
                                       juce::Point<float> outPoint,
                                       VisScaleAxis axis)
{
    const auto startAffine = buildVisAffine (startT);
    const auto anchorOut = visTransformPoint (startAffine, anchorElem);
    const auto startDraggedOut = visTransformPoint (startAffine, draggedElem);

    // 缩放系数 = 锚点到鼠标距离 / 锚点到被拖点起始距离
    const float d0 = startDraggedOut.getDistanceFrom (anchorOut);
    const float d1 = outPoint.getDistanceFrom (anchorOut);
    const float f = (d0 > 1e-3f) ? (d1 / d0) : 1.0f;

    VisTransform r = startT;
    if (axis == VisScaleAxis::Both)
    {
        r.scaleX = juce::jlimit (0.05f, 50.0f, startT.scaleX * f);
        r.scaleY = juce::jlimit (0.05f, 50.0f, startT.scaleY * f);
    }
    else if (axis == VisScaleAxis::OnlyX)
        r.scaleX = juce::jlimit (0.05f, 50.0f, startT.scaleX * f);
    else
        r.scaleY = juce::jlimit (0.05f, 50.0f, startT.scaleY * f);

    // 锚定补偿：buildVisAffine 的合成顺序 = S·R·(p−c) + c + pos
    const float cx = startT.centerX;
    const float cy = startT.centerY;
    const float ax = anchorElem.getX() - cx;
    const float ay = anchorElem.getY() - cy;
    const float rad = juce::degreesToRadians (startT.rotationDeg);
    const float cosA = std::cos (rad);
    const float sinA = std::sin (rad);
    const float rx = ax * cosA - ay * sinA;
    const float ry = ax * sinA + ay * cosA;
    const float anchorNewX = cx + r.scaleX * rx;
    const float anchorNewY = cy + r.scaleY * ry;

    r.posX = startT.posX + (anchorOut.getX() - anchorNewX);
    r.posY = startT.posY + (anchorOut.getY() - anchorNewY);
    return r;
}