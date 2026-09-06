# v0.5.0 实施计划 — Windows 原生交付 + 拖放修复 + GUI/样式增强

> 状态：计划已确认，待实施
> 前置：v0.4.2 交叉编译链路已打通（Clang 18 + xwin + lld-link-18，`scripts/build_win_cross.sh`）
> 用户决策记录：
> - P4 bar-line 斜面柱顶：**间隙断开**（每柱独立梯形，柱间保留 gap 空隙）
> - P5 峰值帽参数：**复用现有** `peakHoldMs` / `peakDecayDbPerSec`（Time 区已有，做可见性优化）
>
> 批准后动作：本文档正式落盘为 `docs/PLAN_v0.5.0.md`，并在 `ARCHITECTURE.md`
> 版本映射表加 v0.5.0 行 + §1/§7 指向本文档。

---

## P1. 编译 exe 并部署到 Windows 目录 — ✅ 已可用，仅补自动化

**现状**：`bash scripts/build_win_cross.sh --deploy` 已能编译两个 exe 并拷贝到
`C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`（静态 CRT，无 VCRUNTIME 依赖）。

**待办**：
- [ ] 脚本增加 `--no-build` 选项（仅重新部署，跳过编译）
- [ ] 部署前用 `x86_64-w64-mingw32-objdump -p` 校验产物无 CRT DLL 依赖（出现 VCRUNTIME/MSVCP 即报错）
- [ ] ARCHITECTURE.md §5 增补交叉编译章节（工具链构成、一次性环境安装、日常命令、三个已知坑：
  STL mismatch 宏 / SDK 大小写符号链接 / clang resource `-isystem` 首位）

**验收**：Vibe Coding 窗口说"编译 Windows 版"→ 执行脚本 → exe 出现在测试目录。

---

## P2. 彻底解决拖放禁止符（UIPI 提权场景）

**根因链**（代码级调查确认）：
1. 管理员运行时 UIPI 阻断 Explorer（Medium）→ GUI（High）的 **OLE 拖放** COM 调用
2. JUCE `juce_Windowing_windows.cpp:2487` **无条件** `RegisterDragDrop` 且忽略 HRESULT →
   Explorer 见目标有 OLE 目标就**只走 OLE**，被拦即禁止符，不回退
3. JUCE `setMessageFilter()`（`:2598`）已放行 `WM_DROPFILES`/`WM_COPYDATA`/`0x49` ——
   **WM_DROPFILES 是微软官方允许跨 UIPI 的消息**
4. 但 OLE 注册存在时 Explorer 不回退老协议 → 放行形同虚设

**修复方案（代码级）**：
- [ ] 新增 `source/gui/WinDragCompat.h/.cpp`（仅 `JUCE_WINDOWS`，其他平台空实现）：
  - `isProcessElevated()` 自 SpectrumCanvas 迁入（OpenProcessToken + TokenElevation）
  - `installDragCompat(hwnd)`：提权 → `RevokeDragDrop(hwnd)` + `DragAcceptFiles(hwnd, TRUE)`
    → Explorer 回退 WM_DROPFILES（已放行）→ **拖放可用**；
    普通权限 → 不动（OLE 全功能保留）
- [ ] 主窗口 peer 就绪后接线（`getPeer()->getNativeHandle()` 取 HWND）
- [ ] WM_DROPFILES 到达路径：先验证 JUCE 8.0.12 是否原生处理 WM_DROPFILES 转 filesDropped；
    若否，`SetWindowSubclass` 子类化窗口过程自行 `DragQueryFile` → `loadFile` 分发（计划内含）
- [ ] 画布横幅文案更新："Legacy drag-drop protocol enabled (elevated process)"
- [ ] 保留绿色 HUD 作验收依据

**风险**：WM_DROPFILES 无拖放悬停高亮（提权场景可接受降级）；普通权限零影响。
**验收**：管理员 cmd 启动 GUI → 普通 Explorer 拖 wav → 无禁止符、加载成功；普通启动行为不变。

---

## P3. 颜色选择器可见性修复

**现状确认**：Primary/Secondary/Peak **存在且正常**，在 Appearance 区第 6 行
（`ParamPanel.cpp:162`）。面板总高 ≈1270px > 首屏可视 ≈760px，Colors 行 y≈852px **需滚动才能看到**。

**待办**：
- [ ] Colors 行上移到 Appearance 区**第 2 行**（Line width 之前）
- [ ] Colors 行扩展为 **4 按钮**：Primary / Secondary / Peak / **BG**（`params.bgColor` 的
  JSON/CLI 已通、仅缺 GUI 控件，见 §10.5 预留），复用 `openColourPicker`
- [ ] ParamPanel 构造时 `setViewPosition(0,0)` 防御残留滚动

**验收**：免滚动看到 4 色按钮；BG 色改动影响导出（透明导出将 BG alpha 调回 0）。

---

## P4. bar-line 斜面柱顶重构（间隙断开版）

**现状**：`BarLineStyle::render()` 画矩形柱 + 柱顶 1px 水平描边 + 相邻柱顶**中点连线**
（`BarLineStyle.cpp:95`）。

**目标形态**（用户描述 + 已确认间隙断开）：
- 柱体从矩形变**五边形梯形**：底边水平、左右边垂直、**顶边斜线**
- 柱 i 左边缘高 = 相邻（prev,cur）归一化中点插值、右边缘高 = 相邻（cur,next）中点插值；
  首柱左缘 / 末柱右缘 = 自身值
- 例：柱高 50、右邻 65 → 界面处柱顶从 50 斜升 65；同高 → 水平
- gap 保留（斜面在各自柱宽内闭合，不跨 gap 填充）
- 顶边斜线即主描边（primary 1px），删中点连线与水平描边
- 柱内垂直渐变填充保留（渐变 y 范围取该柱顶 min/max）

**实现要点**：
- [ ] 重写柱体为 5 顶点 juce::Path；布局参数（barGapRatio/barWidthRatio）沿用
- [ ] 空带（n<0.005）柱体不画但**边缘插值仍参与**邻柱斜面（避免悬空跳变）；连续空带成 V 谷
- [ ] GUI 预览与 CLI 导出同源自动一致

**验收**：相邻带 50/65 音频段 → 柱顶斜面 + 间隙清晰；其他样式零影响。

---

## P5. 峰值帽"下落间隔 + 下落速度"显性化

**现状**：功能已存在——`SpectrumCore::getBandFrame()`（`SpectrumCore.cpp:384`）：
`peakHoldMs`（下落间隔，默认 3500ms）+ `peakDecayDbPerSec`（下落速度，默认 12dB/s），
帧率无关。GUI Time 区已有两滑块，同样被滚动埋没。

**待办（用户已选复用）**：
- [ ] 两滑块上移至 Style 区，紧随 "Peak caps" 开关（峰值帽三件套聚合：开关/间隔/速度）
- [ ] §10.3 参数表标注与峰值帽的对应关系 + 滑块加 tooltip 别名
- [ ] 检查非线性 dynCurve 下峰值帽（dB 线性映射）与柱体（经 gamma）的视觉脱节，
  若明显则在样式层做同曲线映射修正（`BarStyle.cpp` / `BarLineStyle.cpp` peakY 计算）

**验收**：两滑块免滚动可调，实时生效。

---

## 实施顺序与提交切分

| 步骤 | 内容 | 涉及文件 |
|---|---|---|
| S1 | P3 颜色可见性 + BG 按钮 | ParamPanel.cpp/.h |
| S2 | P5 峰值滑块上移 | ParamPanel.cpp |
| S3 | P4 bar-line 斜面柱顶 | BarLineStyle.cpp |
| S4 | P2 拖放修复（WinDragCompat + 窗口接线） | 新文件 + Main.cpp/MainComponent |
| S5 | Linux 回归 + `build_win_cross.sh --deploy` 交叉回归 | — |
| S6 | Windows 实测验收 | 用户侧 |
| S7 | ARCHITECTURE.md bump v0.5.0 + §12 changelog + RELEASE_NOTES.md + 交叉编译章节 | 文档 |

每步 Linux `cmake --build build -j2` 独立验证；S4 后跑完整交叉编译。

## S6 Windows 验收清单
1. `Run_AudioVisGUI.bat`（管理员/普通各一次）→ 拖放均可用
2. Colors 4 按钮免滚动可见、取色器正常、BG 色影响导出
3. bar-line 斜面形态符合预期（50/65 相邻柱）
4. Peak decay / Peak hold 免滚动实时生效
5. CLI `--export` 与 GUI 预览一致
