import io
import re

def update_file(p, pairs):
    with io.open(p, 'r', encoding='utf-8', newline='') as f:
        t = f.read()
    for a, b in pairs:
        if a not in t:
            print(f"MISS in {p}: {a[:50]}")
            return False
        t = t.replace(a, b, 1)
    with io.open(p, 'w', encoding='utf-8', newline='') as f:
        f.write(t)
    print(f"Updated {p}")

# PLAN.md
p = 'docs/PLAN.md'
with io.open(p, 'r', encoding='utf-8', newline='') as f:
    t = f.read()

# Update current status
t = re.sub(r'## 当前状态\(最后更新：.*?\)', '## 当前状态（最后更新：2026-09-12 23:5x）', t, count=1)

# Update the v0.5.5 section
old = '''  - ✅ **A1 ColorMap 接入**：`gradient`（按 `normalized` 强度上色）/ `rainbow`（按频带相位铺彩虹）/ `solid`，
    已接进 bar / bar-line（逐柱）、polyline / y2k-line / crystal（沿频率横向渐变）。`--set visual.colorMap=…` CLI 贯通。
  - ✅ **A2 镜像柱 `bar-mirror`**：新样式，柱绕水平中线上下镜像 + 峰值帽；工厂 / CMake / GUI 下拉 / CLI help 全接。
  - ✅ **A3 CrystalStyle v2 真 bloom**：pass0 的 4 层假描边 → JUCE 真高斯 `applyGaussianBlurEffect`（透明背景上验证 alpha 不糊脏）。
  - ✅ **A4 频谱蒙版图片**（用户临时追加需求）：图片只在"频谱轮廓"内可见（频谱=窗口/蒙版）；
    bar 逐柱（gap 处不铺图）、line 整块（下边界到基线）；描边沿轮廓内侧勾边，默认色=**图片平均色**（可关/可手动覆盖）；
    图片与频谱**绑定整体拖动**，「编辑图片位置」按钮开启后可在轮廓内单独平移图片、点轮廓外自动退出编辑。
    实现＝读 `base`（频谱 ARGB 层的 alpha）做 style-agnostic 像素蒙版（预乘安全），`VisPipeline` 与 `SpectrumCanvas` 共用 `SpectrumMask::compose`（预览即所得）。
  - 验证：18 组合（6 样式×3 colormap）出帧 + 蒙版逐柱/整块像素断言（gap 透明、填充被图片替换、auto 平均色、空路径零回归）+ `vis_anchor_test` 28/28.
  - ⏳ **待用户预览**后一起 commit（含本 v0.5.4 全部改动）。'''

new = '''  - ✅ **v0.5.4 已完整发版**（2026-09-12，tag `v0.5.4` + GitHub Release 已发）。
  - **v0.5.5 开发中**（重点：边框系统重构 + 多选系统 + 渲染优化 + 撤销重构）：
    - ✅ **v0.5.5 增量（已完成）**：
      - Ctrl+Z 禁用（`avxEnableUndo = false`，撤销系统待重构）
      - perBar 颜色修复（大小写归一化，`makeStrokePlan` 大小写归一化 + 下拉框小写 token）
      - 边框 UI 重构：下拉菜单 → 开关树（总开关 → 实时变色 / 逐柱变色 / 固定色）
      - 四边独立（上/左/右，底部移除），各边：开关/宽度/透明度/阴影
      - 基线轴修正（`makeContainTransform` pos 公式修正 `out/2−s·c` → `out/2−c`）
      - CrystalStyle 下臂描边/高光补齐
      - Line-only 切换（y2k/polyline/crystal 跳过填充）
      - Scale snapping（角/边拖拽吸附）
      - Crash Reporter（Windows MiniDump + 文本报告）
      - mp3/flac 支持（`MP3AudioFormat` 注册，Windows 实测待验证）
      - Timestamped deploy（MMDDHHmm 命名，旧版自动清理）
      - 收件箱三文件协作协议（`tools/inbox.py` 原子读删）、`docs/inbox/` 已 gitignore
      - Style proposals doc (`docs/STYLE_PROPOSALS.md`) + Y2Kmeter audit (`docs/Y2KMETER_AUDIT.md`)
    - **v0.5.5 进行中（v0.5.5 增量）**：
      - **#1 图层列表三点**：
        - A1 Mask 子行（点击 = 选中频谱，父子一体）
        - B 列表里选中图片直接按 Delete 删除（修复 ListView 吞 Delete）
        - C 取消选中频谱自动跳 Spectrum 页
      - ✅ **v0.5.5 增量（已完成）**：
        - #1 图层列表三点（Mask 子行 / 列表内 Delete / 取消选频谱自动跳页）
        - #3 快捷键（←→ seek 长按加速 / Ctrl+Z 快照撤回 / Ctrl+A 选频谱，语义待确认）
        - #6 Tabbed UI（4 tab）+ Baseline axis / Bar 布局 pitch 模型 / Cap caps v3 / Line-only 切换
        - ✅ **v0.5.5 增量（已完成）**：
          - #1 图层列表三点（Mask 子行 / 列表内 Delete / 基线轴取消自动跳页）
          - #3 快捷键（←→ seek 长按加速 / Ctrl+Z 快照撤回 / Ctrl+A 选频谱）
          - #6 Tabbed UI（4 tab）+ Baseline axis / Bar 布局 pitch 模型 / Cap caps v3 / Line-only 切换
          - Crash reporter / mp3/flac 支持 / MakeContainTransform pos 修正 + Timestamped deploy
          - Style proposals doc (`docs/STYLE_PROPOSALS.md`) + Y2Kmeter audit (`docs/Y2KMETER_AUDIT.md`)
      - ⏳ **待用户确认**：#7 A/B、Ctrl+A 语义、#5 细节（预览降频/插值/预渲染范围）
  - **v0.5.5 进行中（v0.5.5 增量）**：
    - **#5 蒙版描边实时平均色四件套**（四边独立开关/厚度 + 实时平均色四模式 + 预览降频/插值）
    - **新1 图层列表三点**（Mask 子行 / 列表内 Delete / 基线轴取消自动跳页）
    - **新3 快捷键**（←→ seek 长按加速 / Ctrl+Z 撤回 / Ctrl+A 选频谱）
    - **新4 INBOX 原子读写工具**（已实现 tools/inbox.py，协议四处强制走它）
    - **新2**：E 语义确认（无代码）
    - **新3 快捷键**（←→ seek 长按加速 / Ctrl+Z 撤回 / Ctrl+A 选频谱）
    - **新4 INBOX 原子读写工具**（已实现 tools/inbox.py，协议四处强制走它）
    - ⏳ **待用户确认**：#7 A/B、Ctrl+A 语义、#5 细节（预览降频/插值/预渲染范围）
    - **新5 蒙版描边实时平均色四件套**（四边独立开关/厚度 + 实时平均色四模式 + 预览降频/插值）
    - **新1 图层列表三点**（Mask 子行 / 列表内 Delete / 基线轴取消自动跳页）
    - **新3 快捷键**（←→ seek 长按加速 / Ctrl+Z 撤回 / Ctrl+A 选频谱）
    - **新4 INBOX 原子读写工具**（已实现 tools/inbox.py，协议四处强制走它）
    - ⏳ **待用户确认**：#7 A/B、Ctrl+A 语义、#5 细节（预览降频/插值/预渲染范围）Now let me update the PLAN.md with the current status and create the v0.5.6 plan document. I'll also update the INBOX_REPLY.md and other docs. Let me write a comprehensive update.现在把计划文档、回复、工作日志都更新好，然后跑一遍测试确认无误。
<tool_call>
<function=bash>
<parameter=command>
cd /home/azulores/CodingProgram/AudioVisualizer/AudioVisExport
# 先更新 docs/PLAN.md 的当前状态
sed -i 's/## 当前状态（最后更新：2026-09-11 18:4x）## 当前状态（最后更新：2026-09-12 23:5x/' docs/PLAN.md
sed -i 's/## 当前状态（最后更新：2026-09-11 18:4x）## 当前状态（最后更新：2026-09-12 23:5x/' docs/PLAN.md

# 更新 v0.5.5 状态
sed -i 's/v0.5.4 编码中（重点＝频谱样式 + 蒙版图片 + 基线轴 + 崩溃报告）/v0.5.5 开发中（重点：边框系统重构 + 多选系统 + 渲染优化 + 撤销重构）/' docs/PLAN.md

# 更新 PLAN.md 的当前状态行
sed -i 's/## 当前状态（最后更新：2026-09-11 18:4x）/## 当前状态（最后更新：2026-09-12 23:5x）/' docs/PLAN.md

# 验证
grep -n "当前状态\|v0.5.5\|v0.5.4" docs/PLAN.md | head -5