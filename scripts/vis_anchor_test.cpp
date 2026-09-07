// 对边锚定缩放独立断言测试
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include "../source/core/VisTransform.h"
#include <cstdio>
#include <cmath>

static int fails = 0;
#define CHECK(cond, msg) do { if (! (cond)) { printf("FAIL: %s\n", msg); ++fails; } \
                              else printf("ok: %s\n", msg); } while (0)

int main()
{
    // 情形 1：未旋转图片 100x50，contain 于 200x100 → scale 2, pos 补偿
    VisTransform t = makeContainTransform (100, 50, 200, 100);
    CHECK (std::abs (t.scaleX - 2.0f) < 1e-5f, "contain scale=2");
    CHECK (std::abs (t.posX) < 1e-5f && std::abs (t.posY) < 1e-5f, "contain pos=0 (200x100 exactly)");

    // 情形 2：拖左下角 → 右下角锚定。起始右下角输出 (200,100)
    // 用 4:3 画布 contain: elem 100x50 → out 200x100
    VisTransform s {};
    s.set = true; s.centerX = 50; s.centerY = 25; s.scaleX = s.scaleY = 2.0f;
    // anchor = 右下角元素坐标 (100,50)；dragged = 左下角 (0,50)
    juce::Point<float> anchor (100, 50), dragged (0, 50);
    auto aff = buildVisAffine (s);
    auto anchorOut = visTransformPoint (aff, anchor);
    CHECK (std::abs (anchorOut.getX() - 150.0f) < 1e-4f && std::abs (anchorOut.getY() - 75.0f) < 1e-4f,
           "anchor starts at (100,100) output");

    // 鼠标拖到距锚点 1.5 倍处（沿 x 反方向）
    auto startDragged = visTransformPoint (aff, dragged);
    auto dir = (startDragged - anchorOut) / startDragged.getDistanceFrom (anchorOut);
    auto mouse = anchorOut + dir * (startDragged.getDistanceFrom (anchorOut) * 1.5f);

    auto r = applyAnchorScaled (s, anchor, dragged, mouse, VisScaleAxis::Both);

    // 锚点必须不动
    auto rAff = buildVisAffine (r);
    auto anchorNew = visTransformPoint (rAff, anchor);
    CHECK (std::abs (anchorNew.getX() - anchorOut.getX()) < 1e-3f
        && std::abs (anchorNew.getY() - anchorOut.getY()) < 1e-3f,
           "corner drag: anchor fixed");

    // 距离比 = 1.5 → scale = 3
    CHECK (std::abs (r.scaleX - 3.0f) < 1e-3f && std::abs (r.scaleY - 3.0f) < 1e-3f,
           "scale factor = 1.5x2 = 3");

    // 被拖点跟着鼠标
    auto draggedNew = visTransformPoint (rAff, dragged);
    CHECK (std::abs (draggedNew.getDistanceFrom (mouse)) < 1e-3f,
           "dragged point follows mouse");

    // 情形 3：旋转 30 度后锚点仍固定
    VisTransform s3 = s;
    s3.rotationDeg = 30.0f;
    auto aff3 = buildVisAffine (s3);
    auto a3 = visTransformPoint (aff3, anchor);
    auto d3 = visTransformPoint (aff3, dragged);
    auto dir3 = (d3 - a3) / d3.getDistanceFrom (a3);
    auto mouse3 = a3 + dir3 * (d3.getDistanceFrom (a3) * 1.25f);
    auto r3 = applyAnchorScaled (s3, anchor, dragged, mouse3, VisScaleAxis::Both);
    auto a3n = visTransformPoint (buildVisAffine (r3), anchor);
    CHECK (a3n.getDistanceFrom (a3) < 1e-3f, "rotated 30deg: anchor fixed");
    CHECK (std::abs (r3.scaleX - 2.5f) < 1e-3f && std::abs (r3.scaleY - 2.5f) < 1e-3f,
           "rotated: scale 2.5");

    // 情形 4：单轴拉伸（拖下边 → 上边中点锚定，仅 Y 变化）
    VisTransform s4 {}; s4.set = true; s4.centerX = 50; s4.centerY = 25;
    s4.scaleX = s4.scaleY = 2.0f;
    juce::Point<float> anchor4 (50, 0);   // 上边中点（元素坐标）
    juce::Point<float> dragged4 (50, 50); // 下边中点
    auto aff4 = buildVisAffine (s4);
    auto a4 = visTransformPoint (aff4, anchor4);
    auto d4 = visTransformPoint (aff4, dragged4);
    auto mouse4 = a4 + (d4 - a4) * 1.5f;  // 鼠标沿 y 拉到 1.5 倍
    auto r4 = applyAnchorScaled (s4, anchor4, dragged4, mouse4, VisScaleAxis::OnlyY);
    auto a4n = visTransformPoint (buildVisAffine (r4), anchor4);
    CHECK (a4n.getDistanceFrom (a4) < 1e-3f, "edge drag: opposite edge midpoint fixed");
    CHECK (std::abs (r4.scaleY - 3.0f) < 1e-3f, "scaleY = 3");
    CHECK (std::abs (r4.scaleX - 2.0f) < 1e-5f, "scaleX unchanged (single axis)");

    printf(fails == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", fails);
    return fails == 0 ? 0 : 1;
}
