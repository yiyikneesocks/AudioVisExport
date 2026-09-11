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
//   先平移到枢轴为原点 → 缩放（元素本地轴）→ 旋转 → 平移回枢轴 + 附加平移
// v0.5.3: 合成序从 S·R 改为 R·S——缩放沿**元素本地轴**而非画布轴，
//   旋转后非等比拉伸不再产生平行四边形（θ=0 时两者等价，频谱无影响）。
inline juce::AffineTransform buildVisAffine (const VisTransform& t)
{
    auto m = juce::AffineTransform::translation (-t.centerX, -t.centerY);
    m = m.scaled (t.scaleX, t.scaleY);
    m = m.rotated (juce::degreesToRadians (t.rotationDeg));
    return m.translated (t.centerX + t.posX, t.centerY + t.posY);
}

// 图片图层初始变换：等比 contain 适配输出画布并居中。
// 枢轴 = 元素自身中心；平移 = 把元素中心摆到画布中心。
// v0.5.4 修正：buildVisAffine 语义为 p → M(p−c)+c+pos（枢轴自抵消，pos 即中心位移），
//   旧公式 pos=out/2−s·c 只在 s=1 时居中，图片缩放适配（s≠1）时会整体偏移 (1−s)·c。
inline VisTransform makeContainTransform (float elemW, float elemH,
                                           float outW, float outH)
{
    VisTransform t;
    t.set      = true;
    // v0.5.4 #H 护栏：elemW/elemH <= 0 说明图片**根本没解码成功**（ImageCache 返回空图 → 尺寸为 0）。
    //   旧写法 jmax(1.0f, elemW) 会把 0 当 1 → scale 静默变成 outH（如 720 倍）；再叠加画布侧
    //   "图片无效时退化成用画布尺寸"的回退，手柄框被放大上千倍 → 用户所见"边框范围远超画布"。
    //   现在非正尺寸一律退回 1:1 居中（绝不可能炸框），并由调用方负责报告加载失败。
    if (! (elemW > 0.0f) || ! (elemH > 0.0f) || ! (outW > 0.0f) || ! (outH > 0.0f))
    {
        t.scaleX = t.scaleY = 1.0f;
        t.centerX  = juce::jmax (0.0f, elemW) * 0.5f;
        t.centerY  = juce::jmax (0.0f, elemH) * 0.5f;
        t.posX     = outW * 0.5f - t.centerX;
        t.posY     = outH * 0.5f - t.centerY;
        t.rotationDeg = 0.0f;
        return t;
    }
    t.scaleX   = t.scaleY = juce::jmin (outW / elemW, outH / elemH);
    t.centerX  = elemW * 0.5f;
    t.centerY  = elemH * 0.5f;
    t.posX     = outW * 0.5f - t.centerX;
    t.posY     = outH * 0.5f - t.centerY;
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
                                       juce::Point<float> outPoint, juce::Point<float> grabOut,
                                       VisScaleAxis axis)
{
    const auto startAffine = buildVisAffine (startT);
    const auto anchorOut = visTransformPoint (startAffine, anchorElem);
    juce::ignoreUnused (draggedElem);   // 抓取基线改用 grabOut（鼠标按下时的实际输出位置）

    const float rad = juce::degreesToRadians (startT.rotationDeg);
    const float cosA = std::cos (rad);
    const float sinA = std::sin (rad);

    // 缩放系数：角拖（Both）= 锚点到鼠标距离比（均匀缩放，任意旋转下均正确）；
    // 边拖（OnlyX/OnlyY）= 位移在**元素本地轴**方向上的投影比——旋转后画布位移
    //   混合了两个本地轴分量，直接用距离比会把另一轴的分量也算进本轴缩放。
    // 抓取基线用 grabOut（鼠标按下时的实际输出位置）：无跳变。
    float f = 1.0f;
    if (axis == VisScaleAxis::Both)
    {
        const float dGrab0 = grabOut.getDistanceFrom (anchorOut);
        const float dGrab1 = outPoint.getDistanceFrom (anchorOut);
        f = (dGrab0 > 0.001f) ? (dGrab1 / dGrab0) : 1.0f;
    }
    else
    {
        // 本地轴方向（画布坐标）：X 轴 = R·(1,0) = (cosA, sinA)；Y 轴 = R·(0,1) = (−sinA, cosA)
        const float dirX = (axis == VisScaleAxis::OnlyX) ? cosA : -sinA;
        const float dirY = (axis == VisScaleAxis::OnlyX) ? sinA :  cosA;
        const float projGrab  = (grabOut.getX()  - anchorOut.getX()) * dirX
                              + (grabOut.getY()  - anchorOut.getY()) * dirY;
        const float projMouse = (outPoint.getX() - anchorOut.getX()) * dirX
                              + (outPoint.getY() - anchorOut.getY()) * dirY;
        f = (std::abs (projGrab) > 0.001f) ? (projMouse / projGrab) : 1.0f;
    }

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

    // 锚定补偿：buildVisAffine 的合成顺序 = R·S·(p−c) + c + pos
    const float cx = startT.centerX;
    const float cy = startT.centerY;
    const float ax = anchorElem.getX() - cx;
    const float ay = anchorElem.getY() - cy;
    // 锚点新位置（不含 pos）：c + R·S₁·(a−c)——先沿本地轴缩放，再旋转
    //   （v0.5.3: 与 buildVisAffine 的 R·S 合成序一致；旧 S·R 公式在旋转后
    //    非等比缩放下会把锚点算错位置。）
    const float anchorNewX = cx + cosA * r.scaleX * ax - sinA * r.scaleY * ay;
    const float anchorNewY = cy + sinA * r.scaleX * ax + cosA * r.scaleY * ay;

    // v0.5.3 真修复：锚点补偿 = anchorOut − (c + R·S₁·(a−c))，**不含 startT.pos**。
    //   （旧代码写成 startT.pos + (anchorOut − anchorNew)，把 startT.pos 多算一次，
    //    导致 pos≠0 时"钉死的对角"瞬移 |startT.pos|；v0.5.2 起即存在，被 pos=0 测试掩盖。）
    r.posX = anchorOut.getX() - anchorNewX;
    r.posY = anchorOut.getY() - anchorNewY;
    return r;
}