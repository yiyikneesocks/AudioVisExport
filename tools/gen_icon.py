#!/usr/bin/env python3
# =============================================================================
# gen_icon.py — 生成 assets/icon.png（CMake 里 ICON_BIG / ICON_SMALL 的输入）
#
#   为什么要入库：原图标是 v0.5.4 一次会话里用 PIL 现场画出来的，**成品提交了、
#   生成脚本没留**（commit 1c4e01d），结果图标既不能重建也不能改。这个脚本补上那个缺口。
#
#   设计（照现存 icon.png 的观察值复刻）：256x256 RGBA，四角全透明；
#   圆角深色底 (23,23,28)；一组频谱柱，颜色沿频率从粉 #ec4899（= 工程 primary）
#   渐变到蓝 (116,150,216)；每根柱顶一条半透明白高光。
#
#   ⚠️ 诚实说明：原脚本已丢失，本脚本是**设计复刻**，不是逐位复原。实测均值：
#       本脚本  (70, 47, 72, 244)   现存 (76, 62, 90, 229)
#     差异来源＝原图右半蓝色占比更高、边缘更柔（alpha 更低）。所以：
#       - 默认**不覆盖** assets/icon.png（成品已在用）；
#       - 真要换图标再 `--write`，那是一次有意的设计变更。
#
# 用法：
#   python tools/gen_icon.py                 # 只生成到临时文件并与现存的 icon.png 比数值
#   python tools/gen_icon.py --write         # 覆盖 assets/icon.png（改设计时才用）
#   python tools/gen_icon.py --out /tmp/x.png
# 依赖：pillow（用 tools/setup_env.sh 建的 venv；不要装进 conda base）
# =============================================================================
from __future__ import annotations

import argparse
import os
import sys
import tempfile

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("[gen_icon] 需要 pillow：先跑 `bash tools/setup_env.sh`，"
             "或用该 venv 的 python 运行本脚本")

S = 256                          # 边长（CMake 只要 png，尺寸由 JUCE 自行缩放）
BG = (23, 23, 28, 255)           # 深色圆角底
RADIUS = 52                        # 四角透明区较大（现存图标均值偏淡就是这个原因）
PINK = (236, 72, 153)            # #EC4899，与 SpectrumStyle::RenderParams::primary 同色
BLUE = (116, 150, 216)
MARGIN_X, MARGIN_BOT, MARGIN_TOP = 24, 26, 12   # 柱顶几乎顶到圆角区（实测现存柱顶在 y≈8）
GAP = 7                          # 柱间隙


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def bar_heights(n: int, S: int = S) -> list:
    """一帧"典型频谱"的形状：低频高、中频起伏、高频回落（用确定性函数，不用随机数，
    否则每次生成都不一样，等于没有可复现的源）。"""
    hs = []
    for i in range(n):
        t = i / (n - 1)
        base = 0.97 - 0.52 * t                       # 总体下坡
        bump = 0.14 * (0.5 + 0.5 * (1 if i % 3 else -1)) if 0.25 < t < 0.72 else 0.0
        hs.append(max(0.10, min(1.0, base + bump)))
    return hs


def make_icon(size: int = S, ss: int = 2) -> Image.Image:
    """ss>1 = 超采样后缩小：得到与原图同类的柔边（半透明边缘像素）。
    原图标是一次性手画、脚本没入库（见文件头说明），所以这里是"复刻设计"而非逐位复原。"""
    work = size * ss
    img = Image.new("RGBA", (work, work), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # 圆角底（四角透明）
    d.rounded_rectangle([2 * ss, 2 * ss, work - 3 * ss, work - 3 * ss],
                        radius=RADIUS * ss, fill=BG)

    # 柱布局：按 size 等比（256 时 13 根）
    scale = work / 256.0
    n = 13
    usable = work - 2 * MARGIN_X * scale
    bar_w = (usable - GAP * scale * (n - 1)) / n
    x0, ybot = MARGIN_X * scale, work - MARGIN_BOT * scale   # 全用 work 空间，别混 size
    ytop_limit = MARGIN_TOP * scale
    hs = bar_heights(n)

    for i, h in enumerate(hs):
        t = i / (n - 1)
        col = lerp(PINK, BLUE, t)
        x = x0 + i * (bar_w + GAP * scale)
        top = max(ytop_limit, ybot - h * (ybot - ytop_limit))
        # 柱体：底暗顶亮的竖向渐变（逐行画，避免依赖 pillow 的渐变 API）
        steps = max(1, int(ybot - top))
        for yy in range(steps):
            f = yy / steps                                    # 0 = 顶
            c = lerp(col, tuple(round(v * 0.55) for v in col), f)
            d.line([(x, ybot - yy), (x + bar_w, ybot - yy)], fill=c + (255,))
        # 柱顶高光（半透明白）
        d.line([(x, top), (x + bar_w, top)], fill=(255, 255, 255, 230), width=max(1, int(3 * scale)))

    if ss > 1:
        img = img.resize((size, size), Image.LANCZOS)
    return img


def stats(path: str):
    im = Image.open(path).convert("RGBA")
    small = im.resize((1, 1))
    px = im.load()
    return {"size": im.size, "mean": small.getpixel((0, 0)),
            "corner": px[1, 1], "bg_mid_top": px[im.size[0] // 2, int(6 * im.size[0] / 256)]}


def main() -> int:
    ap = argparse.ArgumentParser(description="生成 assets/icon.png")
    ap.add_argument("--out", default=None, help="输出路径（默认临时文件，仅做对照）")
    ap.add_argument("--write", action="store_true", help="直接覆盖 assets/icon.png")
    ap.add_argument("--size", type=int, default=S)
    ap.add_argument("--no-ss", action="store_true", help="关掉超采样（硬边缘版）")
    args = ap.parse_args()

    repo_icon = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "icon.png")
    img = make_icon(args.size, ss=1 if args.no_ss else 2)

    if args.write:
        target = os.path.abspath(repo_icon)
    elif args.out:
        target = os.path.abspath(args.out)
    else:
        fd, target = tempfile.mkstemp(suffix=".png", prefix="avx_icon_")
        os.close(fd)

    img.save(target)
    print("[gen_icon] 写出:", target)
    print("           新:", stats(target))
    if os.path.exists(repo_icon) and os.path.abspath(target) != os.path.abspath(repo_icon):
        print("           现存:", stats(os.path.abspath(repo_icon)))
        print("           （两者量级接近即说明复刻有效；确认满意后用 --write 落地）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
