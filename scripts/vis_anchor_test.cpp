// 对边锚定缩放独立断言测试
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include "../source/core/VisTransform.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>

static int fails = 0;
#define CHECK(cond, msg) do { if (! (cond)) { printf("FAIL: %s\n", msg); ++fails; } \
                              else printf("ok: %s\n", msg); } while (0)

int main()
{
    // 情形 1：未旋转图片 100x50，contain 于 200x100 → scale 2，中心摆到画布中心
    //（v0.5.4 修正：旧公式 pos=out/2−s·c 在 s≠1 时整体偏移 (1−s)·c，并非"恰好铺满 pos=0"）
    VisTransform t = makeContainTransform (100, 50, 200, 100);
    CHECK (std::abs (t.scaleX - 2.0f) < 1e-5f, "contain scale=2");
    CHECK (std::abs (t.posX - 50.0f) < 1e-5f && std::abs (t.posY - 25.0f) < 1e-5f,
           "contain pos = out/2 − center (真正居中)");
    {   // 直接验证四角落位：100x50@2x 应正好铺满 0..200,0..100
        auto c = visCorners (t, 100.0f, 50.0f);
        CHECK (std::abs (c[0].getX()) < 1e-4f && std::abs (c[0].getY()) < 1e-4f
            && std::abs (c[2].getX() - 200.0f) < 1e-4f && std::abs (c[2].getY() - 100.0f) < 1e-4f,
               "contain corners exactly fill output canvas");
    }

    // 情形 2：拖左下角 → 右下角锚定。起始右下角输出 (200,100)
    // 用 4:3 画布 contain: elem 100x50 → out 200x100（居中式：pos=out/2−c=(50,25)）
    VisTransform s {};
    s.set = true; s.centerX = 50; s.centerY = 25; s.scaleX = s.scaleY = 2.0f;
    s.posX = 50; s.posY = 25;
    // anchor = 右下角元素坐标 (100,50)；dragged = 左下角 (0,50)
    juce::Point<float> anchor (100, 50), dragged (0, 50);
    auto aff = buildVisAffine (s);
    auto anchorOut = visTransformPoint (aff, anchor);
    CHECK (std::abs (anchorOut.getX() - 200.0f) < 1e-4f && std::abs (anchorOut.getY() - 100.0f) < 1e-4f,
           "anchor starts at (200,100) output");

    // 鼠标拖到距锚点 1.5 倍处（沿 x 反方向）
    auto startDragged = visTransformPoint (aff, dragged);
    auto dir = (startDragged - anchorOut) / startDragged.getDistanceFrom (anchorOut);
    auto mouse = anchorOut + dir * (startDragged.getDistanceFrom (anchorOut) * 1.5f);

    auto r = applyAnchorScaled (s, anchor, dragged, mouse, startDragged, VisScaleAxis::Both);

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
    auto r3 = applyAnchorScaled (s3, anchor, dragged, mouse3, d3, VisScaleAxis::Both);
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
    auto r4 = applyAnchorScaled (s4, anchor4, dragged4, mouse4, d4, VisScaleAxis::OnlyY);
    auto a4n = visTransformPoint (buildVisAffine (r4), anchor4);
    CHECK (a4n.getDistanceFrom (a4) < 1e-3f, "edge drag: opposite edge midpoint fixed");
    CHECK (std::abs (r4.scaleY - 3.0f) < 1e-3f, "scaleY = 3");
    CHECK (std::abs (r4.scaleX - 2.0f) < 1e-5f, "scaleX unchanged (single axis)");

// 情形 5：旋转 30 度 + 单轴拉伸——锚点（对边中点）纹丝不动（v0.5.3 回归）
    {
        VisTransform s5 {}; s5.set = true; s5.centerX =  50; s5.centerY =  25;
        s5.scaleX = s5.scaleY =  2.0f; s5.rotationDeg =  30.0f;
        juce::Point<float> anchor5 (50,0);
        juce::Point<float> dragged5 (50,50);
        auto aff5 = buildVisAffine (s5);
        auto a5 = visTransformPoint (aff5, anchor5);
        auto d5 = visTransformPoint (aff5, dragged5);
        auto mouse5 = a5 + (d5 - a5) * 1.5f;
        auto r5 = applyAnchorScaled (s5, anchor5, dragged5, mouse5, d5, VisScaleAxis::OnlyY);
        auto a5n = visTransformPoint (buildVisAffine (r5), anchor5);
        CHECK (a5n.getDistanceFrom (a5) < 1e-3f, "rotated 30deg + single-axis: anchor fixed");
        CHECK (std::abs (r5.scaleY -  3.0f) < 1e-3f, "rotated single-axis: scaleY =  3");
        CHECK (std::abs (r5.scaleX -  2.0f) < 1e-5f, "rotated single-axis: scaleX unchanged");

        // 情形 6：起始抓取点偏离被拖点（无旋转）——首帧 f 应 ≈ 1（无闪现）
        VisTransform s6 {}; s6.set = true; s6.centerX =  50; s6.centerY =  25;
        s6.scaleX = s6.scaleY =  2.0f;
        juce::Point<float> anchor6 (100,50);
        juce::Point<float> dragged6 (0,50);
        auto aff6 = buildVisAffine (s6);
        auto a6 = visTransformPoint (aff6, anchor6);
        auto d6 = visTransformPoint (aff6, dragged6);
        auto grab6 = d6 + juce::Point<float> (20,0);
        auto r6 = applyAnchorScaled (s6, anchor6, dragged6, grab6, grab6, VisScaleAxis::Both);
        auto a6n = visTransformPoint (buildVisAffine (r6), anchor6);
        CHECK (a6n.getDistanceFrom (a6) < 1e-3f, "grab baseline: anchor fixed with grab offset");
        CHECK (std::abs (r6.scaleX -  2.0f) < 1e-5f, "grab baseline: scale unchanged at grab (f=1)");
    }

    // 情形 7：pos ≠ 0 时锚点仍必须钉死（v0.5.3 真 bug 回归：旧代码多算 startT.pos 致漂移）
    {
        VisTransform s7 {}; s7.set = true; s7.centerX = 50; s7.centerY = 25;
        s7.scaleX = s7.scaleY = 2.0f; s7.posX = 40.0f; s7.posY = 10.0f;
        juce::Point<float> anchor7 (100, 50), dragged7 (0, 50);
        auto aff7 = buildVisAffine (s7);
        auto a7 = visTransformPoint (aff7, anchor7);
        auto d7 = visTransformPoint (aff7, dragged7);
        auto mouse7 = a7 + (d7 - a7) * 1.5f;
        auto r7 = applyAnchorScaled (s7, anchor7, dragged7, mouse7, d7, VisScaleAxis::Both);
        auto a7n = visTransformPoint (buildVisAffine (r7), anchor7);
        CHECK (a7n.getDistanceFrom (a7) < 1e-3f, "pos!=0: anchor fixed (no drift by startT.pos)");
        CHECK (std::abs (r7.scaleX - 3.0f) < 1e-3f && std::abs (r7.scaleY - 3.0f) < 1e-3f,
               "pos!=0: scale = 3");
        auto d7n = visTransformPoint (buildVisAffine (r7), dragged7);
        CHECK (d7n.getDistanceFrom (mouse7) < 1e-3f, "pos!=0: dragged point follows mouse");

        // 情形 8：f=1（鼠标停在抓取点）→ pos 与 scale 完全不变（无跳变）
        auto r8 = applyAnchorScaled (s7, anchor7, dragged7, d7, d7, VisScaleAxis::Both);
        CHECK (std::abs (r8.posX - s7.posX) < 1e-4f && std::abs (r8.posY - s7.posY) < 1e-4f,
               "f=1: pos unchanged (no jump)");
        CHECK (std::abs (r8.scaleX - s7.scaleX) < 1e-4f && std::abs (r8.scaleY - s7.scaleY) < 1e-4f,
               "f=1: scale unchanged");
    }

    // 情形 9（v0.5.4 B6）：旋转 + 非等比缩放后，四角必须仍是**矩形**（相邻边垂直）。
    //   旧 S·R 合成序在 θ≠0 且 Sx≠Sy 时产生平行四边形。
    {
        auto checkOrtho = [&] (const VisTransform& t, float ew, float eh, const char* tag)
        {
            const auto c = visCorners (t, ew, eh);
            auto edge = [&] (int i, int j)
            {
                return juce::Point<float> (c[j].getX() - c[i].getX(),
                                           c[j].getY() - c[i].getY());
            };
            const auto e01 = edge (0, 1), e12 = edge (1, 2);
            const float dot = e01.getX() * e12.getX() + e01.getY() * e12.getY();
            const float len01 = e01.getDistanceFromOrigin();
            const float len12 = e12.getDistanceFromOrigin();
            // 归一化点积（cos 夹角）应 ≈ 0
            const float cosA = (len01 > 1e-6f && len12 > 1e-6f)
                             ? dot / (len01 * len12) : 999.0f;
            CHECK (std::abs (cosA) < 1e-3f,
                   (std::string (tag) + ": corners stay rectangular (no parallelogram)").c_str());
        };

        VisTransform s9 {}; s9.set = true; s9.centerX = 50; s9.centerY = 25;
        s9.scaleX = 3.0f; s9.scaleY = 1.5f; s9.rotationDeg = 37.0f;   // 非等比 + 非对称角
        checkOrtho (s9, 100, 50, "rotated+nonuniform transform");

        // 拖右边缘中点（OnlyX）拉伸后再验证：结果仍矩形 + 锚点不动
        VisTransform s9b = s9;
        juce::Point<float> anchor9 (0, 25), dragged9 (100, 25);   // 左/右边中点
        auto aff9 = buildVisAffine (s9b);
        auto a9  = visTransformPoint (aff9, anchor9);
        auto d9  = visTransformPoint (aff9, dragged9);
        auto mouse9 = a9 + (d9 - a9) * 1.4f;   // 沿画布方向拖（非本地轴方向，考验投影）
        auto r9 = applyAnchorScaled (s9b, anchor9, dragged9, mouse9, d9, VisScaleAxis::OnlyX);
        auto a9n = visTransformPoint (buildVisAffine (r9), anchor9);
        CHECK (a9n.getDistanceFrom (a9) < 1e-3f, "B6 edge drag rotated: anchor fixed");
        checkOrtho (r9, 100, 50, "B6 edge drag result");

        // 拖角（Both）后同样仍矩形
        VisTransform s9c = s9;
        juce::Point<float> anchorC (100, 50), draggedC (0, 0);
        auto affC = buildVisAffine (s9c);
        auto aC = visTransformPoint (affC, anchorC);
        auto dC = visTransformPoint (affC, draggedC);
        auto rC = applyAnchorScaled (s9c, anchorC, draggedC, aC + (dC - aC) * 1.3f, dC,
                                     VisScaleAxis::Both);
        auto aCn = visTransformPoint (buildVisAffine (rC), anchorC);
        CHECK (aCn.getDistanceFrom (aC) < 1e-3f, "B6 corner drag rotated: anchor fixed");
        checkOrtho (rC, 100, 50, "B6 corner drag result");

        // 拖角跟随鼠标（旋转 + 非等比起始状态下）
        auto dCn = visTransformPoint (buildVisAffine (rC), draggedC);
        CHECK (dCn.getDistanceFrom (aC + (dC - aC) * 1.3f) < 1e-3f,
               "B6 corner drag rotated: dragged point follows mouse");
    }

    // 情形 10（v0.5.4 B6）：θ=0 时 R·S 与旧 S·R 完全等价（频谱回归保障）
    {
        VisTransform s10 {}; s10.set = true; s10.centerX = 60; s10.centerY = 30;
        s10.scaleX = 2.5f; s10.scaleY = 1.2f; s10.posX = -15.0f; s10.posY = 7.0f;
        juce::Point<float> p[] = { {0,0}, {120,60}, {37,44}, {120,0} };
        auto aff = buildVisAffine (s10);
        bool ok = true;
        for (auto& q : p)
        {
            auto out = visTransformPoint (aff, q);
            // 手工算 S·(q−c)+c+pos（θ=0 时 R=I，R·S=S·R）
            const float ex = s10.scaleX * (q.getX() - s10.centerX) + s10.centerX + s10.posX;
            const float ey = s10.scaleY * (q.getY() - s10.centerY) + s10.centerY + s10.posY;
            ok = ok && std::abs (out.getX() - ex) < 1e-3f && std::abs (out.getY() - ey) < 1e-3f;
        }
        CHECK (ok, "theta=0: R·S equals legacy S·R (spectrum unaffected)");
    }
    // 情形 11（v0.5.4 #H）：图片解码失败 → 尺寸为 0 时 contain 变换不得炸框
    //   旧实现 jmax(1.0f, elemW) 把 0 当 1 → scale=720，画布侧再用"画布尺寸"回退画手柄
    //   → 框被放大上千倍跑到画布外（用户所见"提示的边框范围远超画布"）。
    {
        const float OW = 1280.0f, OH = 720.0f;
        for (float w : { 0.0f, -5.0f })
        {
            const auto c = makeContainTransform (w, 0.0f, OW, OH);
            CHECK (std::isfinite (c.scaleX) && std::isfinite (c.scaleY),
                   "contain(<=0 dims): scale stays finite");
            CHECK (std::abs (c.scaleX - 1.0f) < 1e-4f && std::abs (c.scaleY - 1.0f) < 1e-4f,
                   "contain(<=0 dims): scale falls back to 1:1, never a huge magnification");
            auto pts = visCorners (c, OW, OH);   // 画布侧无效图回退用画布尺寸当元素尺寸
            float maxAbs = 0.0f;
            for (auto& q : pts)
            {
                CHECK (std::isfinite (q.getX()) && std::isfinite (q.getY()),
                       "contain(<=0 dims): corners finite");
                maxAbs = std::max ({ maxAbs, std::abs (q.getX()), std::abs (q.getY()) });
            }
            CHECK (maxAbs <= 2.0f * std::max (OW, OH),
                   "contain(<=0 dims): handle frame stays near the canvas (no 1000x blow-up)");
        }
        // 正常尺寸仍是严格 contain（零回归）
        const auto ok = makeContainTransform (4000.0f, 3000.0f, OW, OH);
        CHECK (std::abs (ok.scaleX - 0.24f) < 1e-4f, "contain(4000x3000 into 1280x720): scale = 0.24");
    }

    printf(fails == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", fails);
    return fails == 0 ? 0 : 1;
}
