# AudioVisExport — 工程架构文档

> **面向后续 AI 协作者**：本文档是工程交接文档，帮助你快速理解项目全貌、设计决策、代码结构和后续目标。读完本文档即可开始协作开发。
> **新协作 AI 接手必读（按顺序）**：
> 1. `docs/PLAN.md`「当前状态」→ 30 秒知道项目推进到哪一步、当前已知问题
> 2. `ARCHITECTURE.md`（本文档）→ 架构/参数/构建/已知限制（§1-§5 概述，§6-§9 决策/FAQ/参数表/控件映射）
> 3. `docs/HISTORY.md` → 已完成内容的详细解析（为什么这样做、怎么验证的、踩过什么坑）
> 4. `docs/inbox/INBOX_REPLY.md` → 最新待办/待用户实测项（如有；`docs/inbox/` 已 gitignore，本地专用）
>
> **何时查什么（速查表）**：
>
> | 你想知道… | 去哪查 |
> |---|---|
> | 现在做到哪了 / 下一步 | `docs/PLAN.md`「当前状态」 |
> | 某功能为什么这样做 | `docs/HISTORY.md` 对应版本条目 |
> | 参数 / 接口 / 构建命令 | 本文档 §8 参数表 / §5 构建 |
> | 用户最新反馈 / 待办 | `docs/inbox/INBOX_REPLY.md` |
> | 某任务的决策细节 / 指纹 | `docs/inbox/INBOX_WORKLOG.md` |
> | 已知 bug / 限制 | 本文档 §4.5 已知限制（L1-L11） |
| 崩溃转储怎么定位 / 辅助工具环境 | 本文档 §5.9 + `tools/dmp_report.py`（python venv，非构建依赖） |
> | 发版公告（用户视角） | `docs/RELEASE_NOTES.md` |
> | 长期方向 / 候选功能 | `docs/ROADMAP.md` |
> 注意：你后续推进任何操作以后都要按下方「文档地图」分工更新对应文档——**任务进展写 `PLAN.md`，发版写 `HISTORY.md` + `RELEASE_NOTES.md` + 本文档版本映射表**。先读相关文档，和 GUI 源码实际内容交叉比对，避免“把推断当事实写入文档”。
>
> **文档版本 ↔ 项目版本映射（严格对齐）**：每次代码发布都要在此处更新版本行号，确保任意 AI 拿到文档后能准确定位到对应 tag / commit。
>
> | 文档 / 项目版本 | 日期 | tag（可）| 里程碑 |
> |---|---|---|---|
> | v0.5.4 | 2026-09-10 | （未发版，代码已推 main）| **ColorMap**（gradient/rainbow/solid 全管线）+ ~~bar-mirror 镜像柱~~（#3.1 又删除：与"bar + 基线轴 50%"重复，保留别名兼容）+ **CrystalStyle v2 真 bloom**（Gaussian blur）+ **频谱蒙版图片**（独立 VisTransform + 平均色描边 + 编辑模式 + vis_mask_test）+ **Tabbed UI**（4 tab）+ **Baseline axis**（baselineY 可拖动+snap）+ **Bar 布局 pitch 模型**（gap=pitch−width + bandCount 联动）+ **Bar-line 峰帽 v3**（连贯分段+capPull+双面 cap）+ **Line-only 切换** + **Scale snapping** + **Crash reporter**（MiniDump+txt，Windows）+ **mp3 软件解码**（`JUCE_USE_MP3AUDIOFORMAT`，5 格式实测通过）+ **peak-caps 全样式跟随基线轴**（#3，`vis_peaks_test`）+ **compose 描边越界读根除**（#1b）+ **makeContainTransform pos 修正** + **收件箱三文件协议** + **style proposals + Y2Kmeter audit docs** + **（09-11 追加）peak-caps 双侧跟随轴 #3 / crystal 两侧亮度 #E / 选项卡一行多控件 + 未登记即隐藏安全网 #G / 拖放三根因 + 可见图层栈 #H / `vis_tabs_test`** |
> | v0.5.3 | 2026-09-09 | `v0.5.3`（已 push + Release）| **锚定缩放修正**（旋转后非等比拉伸不再斜切成平行四边形：合成序 S·R→R·S；对角漂移修复）+ **CAD 吸附辅助线 + 9 特征点对齐**（边对边/角对角）+ **范围内外视觉区分**（超范围内容变暗发灰）+ **空格播放/暂停** + 峰值帽二阶下落 `peakDecayAccelDbPerSec2` + 键盘删除兜底/面板实时刷新 + 移除 Above spectrum 按钮 + 频谱自吸附修复 |
> | v0.5.2 | 2026-09-07 | `v0.5.2`（已 push + Release）| **统一图层模型**（频谱=真图层：可删除/一键恢复/参与 z 序，严格命中序修复"加图后频谱无法拖放"）+ 对边锚定缩放（拖角对角钉死/拖边对边钉死）+ 吸附系统（旋转 90°×n / 移动边缘对齐，可开关）+ Delete/Backspace 删除图层 |
> | v0.5.1 | 2026-09-07 | `v0.5.1`（已 push + Release）| **图片图层系统重构**：图片按自身宽高比等比显示（contain 居中不变形），手柄框贴图片实际边缘，GUI 分组渲染与导出同源（修"图片永远盖住频谱"），Above spec 开关，默认频谱下方 |
> | v0.5.0 | 2026-09-06 | `v0.5.0`（已 push + Release）| **Windows 原生 exe 交叉编译打通**（Clang 18 + xwin + lld-link-18）+ UIPI 拖放彻底修复（自动降权 + WM_DROPFILES 直连交付）+ Colors 4 按钮免滚动（新增 BG 色）+ 峰值帽参数上移 Style 区 + bar-line 斜面柱顶/斜面峰值帽 + 暂停峰值冻结 + Load/Eject 按钮 |
> | v0.4.2 | 2026-09-05 | —（工作区未提交，tag 待补）| 图片图层系统 + bar-line 样式 + 峰值帽开关 + 拖放 UIPI 诊断 + Windows→WSL 迁移（文档/环境）+ CLI UTF-8 修复 + 版本号同步至 v0.4.2 |
> | v0.4.1 | 2026-09-05 | `v0.4.1`（推荐打 tag）| 频谱元素自由变换（拖动/缩放/拉伸/旋转）+ 两段式合成渲染，导出所见即所得 |
> | v0.4.0 | 2026-09-03 | `v0.4.0`（推荐打 tag）| GUI 新增一键 "Export Video" 按钮（默认透明 MOV QTRLE）；PngSequenceEncoder 实现编码器（MOV QTRLE alpha 实测 argb / WebM VP9 实测无 alpha）；视频模式自动补全输出路径；新增 docs/GUI_GUIDE.md 使用说明 |
> | v0.3.2 | 2026-09-02 | `v0.3.2`（推荐打 tag）| ARCHITECTURE.md GUI 章节重写：精确成员字段、类成员同步流程、导出线程原子轮询、面板回调、已知限制 |
> | v0.3.1 | 2026-09-02 | `v0.3.1`（已 push）| GUI 上线 + 全英文 UI + 4 种 style + 双 target |
> | v0.3.0 | 2026-09-02 | `v0.3.0`（已 push）| Engine + 4 styles + CLI + GUI skeleton |
>
> 协作时：若要修改功能，请先新建分支；文档末尾版本号 / 变更日志必须与代码 commit 同步更新。
>
> **验证闸门规则（强制，2026-09-08 起）**：任何功能/文档操作**全部完成后**，先部署测试包并**停下请用户验证**；
> 用户明确回复「全部通过」**之前**，严禁执行 `git push` / 打 tag / 创建 Release。
> 用户确认通过后，**主动提醒用户「尚未推送 GitHub」**，等待用户下达明确推送指令后才执行推送
> （push + tag + Release 一并完成）。变更日志条目与本地 commit 可在闸门前先行完成。
>
> **发布规则（强制，2026-09-05 起；2026-09-08 加入验证闸门）**：每次小版本更新（版本号任一位变化）必须完成——
> ① `docs/HISTORY.md` §2 变更日志写好对应条目；② 单独撰写**更新公告**（写入 `docs/RELEASE_NOTES.md`
> 并作为 GitHub Release 正文）；③ `git commit`（本地）+ 部署测试包；④ **【验证闸门】** 用户确认通过
> 并下达推送指令后：push 到 `origin`（GitHub）+ 打 tag 并 push + 用公告创建 Release。
>
> ⚠️ 开发期间文档同步纪律（强制，2026-09-10 起）：
> - **每次 `git commit`（功能变更）前** → 必须先检查/更新 `docs/PLAN.md`「当前状态」（含最后更新时间戳）,
>    确保文档反映代码最新状态。**绝对禁止**出现「代码已推 20 个 commit 但 PLAN.md 停留在上个版本」的情况。
> - **每完成一个 INBOX 任务** → 必须同步更新 `docs/PLAN.md`「当前状态」+ `docs/inbox/INBOX_WORKLOG.md`（备份+指纹）。
>    不可积压到发版前统一补——v0.5.4 期间连续 20 个 commit 推到 main 但文档停留在 v0.5.3 就是反面案例。
> - **每次部署前** → 确认 `docs/PLAN.md` 已同步。
> - 此纪律适用于所有文档位置：`ARCHITECTURE.md`、`docs/` 文件夹、`docs/inbox/` 文件夹。
> - **违反此纪律 = 流程事故**，必须立即暂停编码、补完文档后才能继续。
>
> **参考优先原则（务必优先执行，非阻断闸门，2026-09-09 起）**：动手实现某个功能 / 算法前，**只要当前
> 环境具备联网能力，就必须优先做一次 GitHub 检索**，找有无同类成熟实现或可借鉴范式，避免重复造轮子 /
> 走弯路。这是**默认动作**、非可选装饰——每一步开工前先搜。检索手段按轻量→重量：
> ① REST API：`curl "https://api.github.com/search/repositories?q=<关键词>&sort=stars"`（匿名 ~60 次/小时，精简查询）；
> ② `webfetch` 读候选仓库的 README / 源码页；③ `git clone --depth 1`（放 `/tmp/opencode`）通读实现。
> 借鉴到的做法 / 踩坑点，在 `docs/PLAN.md`「当前状态」或对应代码注释注明来源仓库 `owner/repo`（兼顾 GPL 合规追溯）。
> **唯一可跳过的情况**：环境无网络——此时在 `PLAN.md` 记一句"本步未联网检索"。检索结论供参考，
> 最终取舍仍以本项目实际情况与用户需求为准，不盲目照搬。
>
> **文档地图（2026-09-08 四拆，按需读取省上下文）**：
> - 本文档 = **架构与设计参考**（改代码前读；§1-§5 概述/结构/模块/构建，§6-§9 决策/FAQ/参数表/控件映射）
> - `docs/HISTORY.md` = **已完成内容与详细解析**（原 §6 里程碑 + §12 变更日志迁入；新 AI 接手必读）
> - `docs/ROADMAP.md` = **长期计划**（原 §7 后续目标迁入）
> - `docs/PLAN.md` = **当前迭代下一步计划**（滚动文件，发版后清空重写）
> - 另有 `docs/RELEASE_NOTES.md`（用户视角发版公告）、`docs/GUI_GUIDE.md`（GUI 使用指南）
> - `docs/PLAN_v0.5.0.md` 为历史计划快照，原样保留
> - `docs/inbox/` = **收件箱三件套**（异步对话，防读写抢占；**本地专用，已 gitignore 不分发，缺失时按 WORKLOG 头部模板重建**）：`INBOX.md`（用户输入·首行「状态 0/1」闸门：1=编辑中 AI 完全只读，0=空闲 AI 仅可精确删「已完成且已备份」的编号行）、`INBOX_WORKLOG.md`（AI 台账/备份+每轮流程）、`INBOX_REPLY.md`（AI 给用户）。完整协议见 `docs/PLAN.md`「文档更新触发点」下的说明。

> **外部目录使用约定（2026-09-10，#3）**：
> - `~/CodingProgram/AudioVisualizer/`（工程上一级）= 授权根（opencode `external_directory` 已放行），隔壁 `Y2Kmeter` 工程也在其中；
> - `assets/testmedia/` = **测试素材放这里**（tone.wav / noise.wav / 测试 png，随 git 复用），不再用 /tmp；
> - `/mnt/c/Users/yiyikneesocks/Desktop/AudioVisExport_test/` = Windows 部署目录（deploy 脚本专用）；
> - `/tmp/opencode/` = 遗留临时目录（仍可用，但新素材一律进 assets/testmedia/）。

---

## 1. 项目概述

AudioVisExport 是一个**音频可视化视频生成器**：导入音频文件，通过自写的频谱分析引擎（双路 FFT + 带映射 + 时间平滑 + 峰值保持），以多种可插拔样式渲染到带 alpha 通道的 ARGB 图像序列，最终输出可导入 Premiere Pro / Vegas / DaVinci Resolve 等剪辑软件的**透明背景**视频素材。

### 已实现的核心功能（"What works now" — 协作 AI 必看）

| 模块 | 已实现功能 | 说明 |
|---|---|---|
| **频谱分析引擎** (SpectrumCore) | 双路 FFT（2048+8192，500Hz crossfade，cos²/sin² 功率交叉），4 种带映射标度，attack/release 指数平滑，峰值保持 + 衰减，4 种动态曲线（linear/sqrt/loglog/perceptual）+ gain + gamma + dB/oct 斜率补偿，列间 [1:2:1] 模糊 | 与 Y2Kmeter AnalyserHub 逐字节一致 |
| **可插拔样式** (SpectrumStyle) | `y2k-line`（Catmull-Rom 平滑曲线）、`bar`（柱状图）、`polyline`（折线）、`crystal`（3-pass 水晶：辉光+玻璃体+高光）| 工厂注册在 `SpectrumStyle.cpp`；新增样式=实现 .h/.cpp + 注册 + CMake |
| **参数系统** (SpectrumParams) | 6 组共 ~50 个参数；JSON I/O；CLI 糖命令 + `--set dotted.key=val` 覆盖；GUI 即改即见 | 参见下方 "完整可调参数表" |
| **离线管线** (VisPipeline) | 完整导出（run）/ 单帧预览（previewFrame，可选棋盘格）/ 数值调试（probeSpectrum）| 引擎 + 样式与 GUI 预览完全同源，预览即所得 |
| **CLI** (AudioVisExport) | `--export` / `--preview-frame` / `--probe-spectrum` / `--probe-pcm` / `--gen-tone` / `--config` | 脚本友好 |
| **GUI** (AudioVisGUI) | 拖放加载（WAV/AIFF）/ 点击选择；实时预览（30fps，与播放位置同步）；播放/暂停/seek；参数面板（滑块+数字输入双方式、下拉、开关、颜色选择、编码选择）；后台导出线程 + 进度条；**一键 "Export Video"（默认透明 MOV QTRLE，自动生成文件名）** | 全英文 UI；默认字体 Segoe UI；无代码页依赖 |
| **编码器** (PngSequenceEncoder) | PNG 序列（默认，单图 ARGB 100% 保真）；**MOV QTRLE rgba 透明通道（实测 pix_fmt=argb，推荐给剪辑软件）**；WebM VP9（**实测无 alpha**，仅供小体积预览） | MOV/WebM 调用系统 ffmpeg（自动搜索 hint / `FFMPEG_PATH` / PATH），需 `ffmpeg` 可见于 PATH |

### 支持的文件格式

| 类别 | 支持格式 | 说明 |
|---|---|---|
| **音频输入**（`PcmSource`）| WAV (PCM), AIFF, FLAC, OGG, MP3, WMA | JUCE `AudioFormatManager::registerBasicFormats()`，单/多声道（多声道自动转立体声）。**注意**：v0.5.4 已调用 `registerBasicFormats()` 注册 FLAC/OGG/MP3/WMA，但用户实测 mp3/flac 仍无法解码——可能 `JUCE_USE_FLAC`/`JUCE_USE_OGGVORBIS` 编译标志未在 CMake 中启用，待排查。|
| **图片输出** | PNG (ARGB) | JUCE `PNGImageFormat`，默认 0x00000000 全透明背景 |
| **视频输出** (ffmpeg) | MP4 (h264 yuv420p — 预览无 alpha) / **WebM (VP9 yuva420p — alpha)** / **MOV (QTRLE — alpha)** | 透明通道视频必须用 WebM/MOV，不能用 MP4/h264 |

### 核心设计原则
- **纯函数式离线管线**：不依赖 UI / 系统时钟 / OpenGL / 实时音频设备，所有渲染离线完成
- **引擎与 UI 解耦**：`source/core/` 是可独立复用的引擎，后续剪辑软件可作为子目录或静态库引入
- **参数全可配**：一份 `SpectrumParams` 结构承载所有参数（FFT / 频率映射 / 时间 / 动态 / 视觉 / 输出），支持 JSON 预设 + CLI 覆盖
- **样式可插拔**：`SpectrumStyle` 抽象基类 + 工厂模式，新增样式只需实现 `render()` 并注册

### 许可证
GPL-3.0（因参考了 Y2Kmeter 代码）

### 目标平台与交付（2026-09-06 v0.5.0 起更新）
- **设计目标平台：Windows**。WSL2 (Ubuntu 22.04) 是开发/构建环境，不是目标平台。
- **交付形态（v0.5.0 起）**：**WSL 内交叉编译原生 Windows .exe**（Clang 18 MSVC ABI + xwin Windows SDK），
  产物为免安装静态 CRT PE32+ 可执行文件，部署到 `C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`。
  工具链与日常命令见 §5「构建」中「Windows 交叉编译」小节；完整实施计划见 `docs/PLAN_v0.5.0.md`。
- **WSLg 兜底**：Linux 构建产物仍可用（`wsl -e` 拉起，WSLg 显示窗口），作为交叉编译不可用时的后备。
- **原生构建后备**：亦可在 Windows 侧用 Visual Studio 按 §5 同一份 CMake 配置构建
  （`cmake -S . -B build && cmake --build build --config Release`）。

---

## 2. 参考来源

### 2.1 Y2Kmeter（主要参考）
- **路径**：`~/CodingProgram/AudioVisualizer/Y2Kmeter/`（2026-09-05 起工程已迁移到 WSL；Windows 时期为 `d:\Study 'n' Work\Program\Y2Kmeter\`）
- **参考内容**：
  - `source/analysis/AnalyserHub.cpp` — 双路 FFT 处理 + 归一化 + 融合逻辑
  - `source/ui/modules/SpectrumModule.cpp` — Catmull-Rom 平滑曲线绘制 + 网格 + 坐标轴
- **照搬**：FFT 归一化因子（`2.0f / fftSize`）、Hann 窗、500Hz 交叉 + cos²/sin² 等功率 crossfade、attack/release 指数平滑、峰值保持算法、Catmull-Rom → Bezier 路径构建
- **重构**：剥离 UI 耦合（PinkXP / FrameListener / Timer / hover ruler）；dB→视觉强度硬编码改为可配 `gain/gamma/curve` 三参数；单层 paint 重构为多 pass 可插拔；RGB PNG 重构为 ARGB PNG（带 alpha）

### 2.2 JUCE 8.0.12
- **用途**：音频文件读取（`AudioFormatManager`）、FFT（`juce::dsp::FFT`）、窗函数（`juce::dsp::WindowingFunction`）、图形渲染（`juce::Graphics` / `juce::Path` / `juce::Image`）、PNG 编码（`juce::PNGImageFormat`）
- **路径**：`third_party/JUCE/`（本地 checkout，通过 `FETCHCONTENT_SOURCE_DIR_JUCE` 指向）

### 2.3 JUCE FFT 归一化确认
JUCE `juce::dsp::FFT` 前向变换是**非归一化的**（`juce_FFT.cpp` FFTFallback::perform：inverse 才乘 `1/size`，forward 无缩放）。`performFrequencyOnlyForwardTransform` 仅取 `std::abs(complex)` 写回幅度，无额外归一化。因此 AudioVisExport 与 Y2Kmeter 都用 `2.0f / fftSize` 归一化——两者逐字节一致。

---

## 3. 工程结构

```
AudioVisExport/
├── CMakeLists.txt              # 构建配置（juce_add_console_app + FetchContent JUCE）
├── ARCHITECTURE.md             # 本文档
├── third_party/
│   └── JUCE/                   # JUCE 8.0.12 本地 checkout（符号链接到 Y2Kmeter 的）
│
├── source/
│   ├── Y2KLogging.h            # 日志宏（复用自 Y2Kmeter）
│   │
│   ├── core/                   # ===== 核心引擎（纯函数式，不依赖 UI）=====
│   │   ├── PcmSource.h/.cpp        # 音频文件读取（WAV/AIFF → 归一化 float 交错立体声）
│   │   ├── PngSequenceEncoder.h/.cpp  # PNG 序列编码器 + ffmpeg mux
│   │   ├── SpectrumParams.h/.cpp   # 参数结构 + JSON 序列化 + CLI 覆盖
│   │   ├── BandFrame.h             # 单帧数据结构（db / peakDb / normalized）
│   │   ├── FreqMap.h/.cpp          # 频率→带映射（Log/Linear/Mel/Bark）
│   │   ├── ColorMap.h/.cpp         # 颜色映射（solid/gradient/rainbow，预留）
│   │   ├── SpectrumCore.h/.cpp     # 频谱分析引擎（双路 FFT + 带映射 + 平滑 + 峰值）
│   │   ├── SpectrumStyle.h/.cpp    # 样式抽象基类 + 工厂
│   │   ├── VisPipeline.h/.cpp      # 高层编排器（音频→分析→渲染→编码→mux）
│   │   └── (VisPipeline.cpp 含 renderFrame/previewFrame/probeSpectrum 三入口)
│   │
│   ├── styles/                # ===== 可插拔样式实现 =====
│   │   ├── Y2KLineStyle.h/.cpp     # Y2Kmeter 风格曲线（Catmull-Rom 平滑 + 填充 + 双层描边 + 虚线峰值）
│   │   ├── BarStyle.h/.cpp         # 传统柱状图（每带一柱 + 渐变填充 + 峰值帽）
│   │   ├── PolylineStyle.h/.cpp    # 折线图（直线段连接 + 填充 + 双层描边 + 虚线峰值）
│   │   └── CrystalStyle.h/.cpp     # 水晶/玻璃效果（3-pass：辉光 + 玻璃体 + 高光线）
│   │
│   └── cli/                   # ===== CLI 入口 =====
│       ├── CliArgs.h/.cpp          # 命令行参数解析（糖 flag + --set key=val + --config）
│       └── main.cpp                # CLI 主入口（--export / --preview-frame / --probe-spectrum / --probe-pcm / --config）
│
├── gui/ (source/gui/)         # ===== GUI 实时预览界面（v0.3 新增）=====
│   ├── Main.cpp                    # JUCEApplication 入口 + DocumentWindow
│   ├── MainComponent.h/.cpp        # 布局 + 音频播放 + 定时器同步 + 导出线程
│   ├── SpectrumCanvas.h/.cpp       # 实时渲染画布（ARGB Image + style.render + 拖放）
│   └── ParamPanel.h/.cpp           # 右侧参数面板（滑块/下拉/颜色/导出）
│
└── build/                     # CMake 构建目录（gitignore）
    ├── AudioVisExport_artefacts/Release/AudioVisExport   # CLI（Linux 无 .exe 后缀）
    └── AudioVisGUI_artefacts/Release/AudioVisGUI          # GUI
```

---

## 4. 核心模块详解

### 4.1 SpectrumCore（频谱分析引擎）
**文件**：`source/core/SpectrumCore.h/.cpp`

**职责**：音频 PCM → 频谱带数据（dB / peakDb / normalized）

**算法流程**（照搬 Y2Kmeter AnalyserHub）：
1. **双路 FFT**：
   - 主路：fftSize=2048（order=11），Hann 窗，无 overlap
   - 低频路：fftSize=8192（order=13），Hann 窗，75% overlap
   - 交叉频率：500Hz，cos²/sin² 等功率域 crossfade
2. **带映射**：根据 FreqScale（Log/Linear/Mel/Bark）将 FFT bin 映射到 N 个带
3. **dB 转换**：`gainToDecibels(linear)` + 可选 slope 补偿（dB/oct）
4. **时间平滑**：attack/release 指数平滑（`alpha = 1 - exp(-dt/tau)`）
5. **列间模糊**：[1,2,1]/4 三点模糊（temporalSmoothing 参数控制）
6. **峰值保持**：peakHoldMs 保持期 + peakDecayDbPerSec 衰减
7. **动态曲线**：LinearDyn/Sqrt/LogLog/Perceptual + gain + gamma → normalized[0,1]

**关键接口**：
```cpp
void pushInterleavedStereo(const float* interleavedLr, int numFrames);
void advanceTime(double deltaSec);
void getBandFrame(BandFrame& out);  // 输出 db[] / peakDb[] / normalized[]
```

### 4.2 SpectrumStyle（样式系统）
**文件**：`source/core/SpectrumStyle.h/.cpp`

**基类接口**：
```cpp
class SpectrumStyle {
    virtual void render(Graphics& g, Rectangle<int> canvas, const BandFrame& frame, const RenderParams& rp) = 0;
    virtual int getNumPasses() const noexcept { return 1; }  // 多 pass 钩子
    virtual void renderPass(Graphics& g, int pass, ...);     // 默认调 render()
    static std::unique_ptr<SpectrumStyle> create(const String& name);  // 工厂
};
```

**RenderParams 结构**（由 VisPipeline 从 SpectrumParams 镜像）：
- 颜色：primary / secondary / peak / bg
- 几何：paddingLeft/Right/Top/Bottom
- 描线：lineWidth / opacity
- 开关：drawGrid / drawAxisLabels
- 范围：minHz/maxHz/minDb/maxDb

**已实现样式**：

| 名称 | CLI `--style` | 特征 |
|------|---------------|------|
| Y2KLineStyle | `y2k-line` | Catmull-Rom 平滑曲线 + 半透明填充 + 双层描边 + 虚线峰值（照搬 Y2Kmeter） |
| BarStyle | `bar` | 每带一根柱 + 垂直渐变填充 + 柱顶描边 + 峰值帽 |
| PolylineStyle | `polyline` | 直线段连接（无平滑）+ 半透明填充 + 双层描边 + 虚线峰值 |
| CrystalStyle | `crystal` | 3-pass：辉光(bloom) + 玻璃体(渐变填充) + 高光线（验证多 pass 架构） |

**新增样式步骤**：
1. 在 `source/styles/` 新建 `MyStyle.h/.cpp`，继承 `SpectrumStyle`
2. 实现 `render()`（和可选的 `getNumPasses()` / `renderPass()`）
3. 在 `SpectrumStyle.cpp` 工厂注册 name → 构造
4. 在 `CMakeLists.txt` 添加源文件

### 4.3 SpectrumParams（参数系统）
**文件**：`source/core/SpectrumParams.h/.cpp`

**参数注入顺序**：默认值 → `--config preset.json` → `--set key=val` / 糖 flag（按出现顺序覆盖）

**参数分组**：
- FFT：fftOrder / fftOrderLo / enableLowFreqPath / crossoverHz / windowFunc / hopSize / hopSizeLo
- 频率映射：minHz / maxHz / freqScale / bandCount
- 时间：fps / attackMs / releaseMs / peakHoldMs / peakDecayDbPerSec / temporalSmoothing
- 动态强度：dynCurve / dynGain / dynGamma / slopeEnabled / slopeDbPerOct / minDb / maxDb
- 视觉：style / colorMap / primaryColor / secondaryColor / peakColor / bgColor / lineWidth / opacity / drawGrid / drawAxisLabels
- 输出：width / height / encoder / digits / baseName / outputDir / outputVideoPath / ffmpegPath

### 4.4 VisPipeline（编排器）
**文件**：`source/core/VisPipeline.h/.cpp`

**三个入口**：
- `run(cfg, cb)` — 完整导出：音频加载 → SpectrumCore → 主循环渲染 → PNG 序列 / WebM / MOV
- `previewFrame(cfg, frameIndex, outPngPath)` — 单帧预览（含可选棋盘格背景）
- `probeSpectrum(cfg, frameIndex)` — 数值调试（dump 带数据 + 原始 FFT bin）

**渲染流程**（`renderFrame()` 函数）：
1. 创建 `juce::Image(ARGB, w, h, true)` — true = 清空全透明
2. 可选：棋盘格背景（预览）或不透明背景色
3. 计算 canvas 矩形（去掉 padding）
4. `core.getBandFrame(bandFrame)` → `style.render(g, canvas, bandFrame, rp)`
5. 返回 Image → PngSequenceEncoder 写帧

### 4.5 GUI 实时预览（AudioVisGUI，v0.3 上线）
**Target**：`AudioVisGUI`（独立 GUI app，CMake `juce_add_gui_app`）
**Source dir**：`source/gui/`（与 CLI 通过 `AVX_ENGINE_SOURCES` 变量**共享全部引擎源**，避免重复编译并确保行为一致）
**链接依赖**：`juce_audio_utils juce_dsp juce_graphics juce_gui_basics juce_gui_extra`（juce_gui_extra 用于 `juce::ColourSelector` + `juce::CallOutBox::launchAsynchronously`）

---

#### 4.5.1 源文件与类映射（类名 / 基类 / 关键成员，按代码实态）

| 文件 | 类 | 基类 | 核心字段 | 说明 |
|---|---|---|---|---|
| `Main.cpp` | `AVXGuiApplication` | `juce::JUCEApplication` | 全局 `LookAndFeel::setDefaultSansSerifTypefaceName("Segoe UI")` | 应用入口；启动时**全局固定字体为 Segoe UI**，避免非 Unicode locale 乱码 |
| `Main.cpp` | `AVXGuiWindow` | `juce::DocumentWindow` | `setUsingNativeTitleBar(true)`；resize limits (960×640..8192×8192)；默认 1280×800 | 原生标题栏 + 可缩放 |
| `MainComponent.h/.cpp` | `MainComponent` | `juce::Component, juce::Timer` | 见下文"成员字段速查表" | 顶层组件：布局、音频、同步、导出 |
| `SpectrumCanvas.h/.cpp` | `SpectrumCanvas` | `juce::Component, juce::FileDragAndDropTarget` | `BandFrame frame; RenderParams rp; SpectrumStyle* style; bool hasAudio; bool showCheckerboard`；回调：`onFileDropped`, `onEmptyClicked` | 左画布：ARGB Image + style.render + 拖放目标 + 棋盘格预览 + 空状态文案 |
| `ParamPanel.h/.cpp` | `ParamPanel` | `juce::Component` | `std::vector<Row> rows; OwnedArray<Component> widgets; std::map<Component*, unique_ptr<Label>> rowLabels; int contentHeight, exportAreaHeight;`；回调：`onParamsChanged, onExportClicked, onBrowseOutputDir`；API：`getCheckerPreview(), setProgressText(t), setOutputDirText(t), setExportEnabled(bool)` | 右侧可滚动参数面板（含 6 个 section + 导出区）|

---

#### 4.5.2 MainComponent 成员字段速查表（协作 AI 读/写参考）

> 字段按职责分组。所有字段名均为代码中真实存在（来自 `MainComponent.h/cpp`）。

| 分组 | 字段 | 类型 | 真实实现行为 |
|---|---|---|---|
| **音频播放** | `formatManager` | `juce::AudioFormatManager` | ctor 中 `registerBasicFormats()`（WAV/AIFF）|
| | `deviceManager` | `juce::AudioDeviceManager` | `initialiseWithDefaultDevices(0, 2)`（0 进 2 出默认设备）|
| | `player` | `juce::AudioSourcePlayer` | `setSource(&transport)` + `deviceManager.addAudioCallback(&player)` |
| | `transport` | `juce::AudioTransportSource` | 播放时钟；play/pause 按钮 + seekBar 写位置；`getCurrentPosition()` 作为同步源 |
| | `readerSource` | `unique_ptr<AudioFormatReaderSource>` | 生命周期：loadFile 时创建 / 下次 loadFile / 析构时 release 给 transport |
| | `pcm` | `PcmSource` | **离线读取通道（与扬声器解耦）**，`readInterleavedStereo(frameIdx*spf, spf, dst)` 给 SpectrumCore。确保暂停/seek 时曲线仍可正确显示，不依赖环形 buffer |
| | `audioFile` / `srcRate` | `juce::File / double` | 记录当前音频路径与采样率；hasAudio=false 时画布显示拖放提示 |
| | `hasAudio` | `bool` | 供 canvas、playBtn、startExport 判断 |
| **引擎** | `params` | `SpectrumParams` | **单源真值（single source of truth）**：GUI 面板改动直接写结构体字段；CLI/JSON 也写入同一字段 |
| | `core` | `unique_ptr<SpectrumCore>` | 当前帧状态容器（双路 FFT + 平滑 + 峰值保持）|
| | `style` | `unique_ptr<SpectrumStyle>` | 工厂产物，命名：`y2k-line / bar / polyline / crystal` |
| | `currentStyleName` | `String` | 与 `params.style` 比较，不一致时重建 style（避免每个 tick 重建）|
| | `paramsDirty` | `atomic<bool>` | ParamPanel 回调置 `true`；timer 中 `exchange(false)` 触发 core/style 重建。**原子变量，跨线程读安全** |
| | `coreFramePos` | `int64_t` | SpectrumCore 已推进到第几帧（= pcm 已读到 `coreFramePos × frames_per_sample` 位置）|
| | `pendingSeekFrame` | `int64_t` | `<0` 表示无 seek；≥0 时下一 tick 执行"重建 core→advanceTo 目标"（处理进度条拖尾 / 点击 播放末尾回到开头）|
| **UI** | `canvas` | `SpectrumCanvas`（成员对象）| |
| | `panel` | `ParamPanel`（成员对象）| |
| | `panelViewport` | `juce::Viewport`（成员对象）| `setViewedComponent(&panel, false)` + 只显示垂直滚动条；窗口高度不够时面板可滚动 |
| | `playBtn` / `seekBar` / `timeLabel` | Button / Slider / Label | 底部 40px 传输条；`userSeeking` 变量避免拖拽时 seekBar 值被 timer 覆盖 |
| | `pausedPos` | `double` | 暂停时保留播放位置；播放末尾按停止时回 0 重新开始 |
| **导出** | `exporting / exportPct / exportDone` | `atomic<bool / int / bool>` | 跨线程：后台 `exportThread` 写，UI timer 轮询。exportDone 用 `exchange(false)` 保证"只处理一次" |
| | `exportMsgMtx` + `exportMsg` | `mutex + juce::String` | 完成字符串回传（"Done: N frames -> dir" / "Failed: ..."）|
| | `exportDir` | `juce::File` | GUI 存储的导出目录（不落盘到 SpectrumParams，仅本次 GUI 会话保存）|
| | `exportThread` | `std::thread` | 跑 VisPipeline::run；析构时 if (joinable()) join() |
| | `fileChooser` / `lastDir` | `unique_ptr<FileChooser> / juce::File` | 默认初始目录 `userMusicDirectory`；每次关闭保存父目录 |

---

#### 4.5.3 同步引擎主循环（`MainComponent::timerCallback @30Hz`，逐行与代码一致）

```cpp
// (1) 参数重建分支：paramsDirty=true → 重建 core/style；coreFramePos 保持与播放位置一致，曲线自然恢复
if (paramsDirty.exchange(false)) {
    if (params.style != currentStyleName) {
        currentStyleName = params.style;
        style = SpectrumStyle::create(params.style);
    }
    rebuildCoreLight();          // 8 帧静音 warmup + 重新分配 FFT
    coreFramePos = currentTargetFrame();
}

// (2) 显式 seek：progress dragEnd / play to end restart
if (pendingSeekFrame >= 0) {
    const int64_t target = pendingSeekFrame;
    pendingSeekFrame = -1;
    rebuildCoreLight();
    coreFramePos = 0;
    advanceCoreTo(target);       // 快进到 target
}

// (3) 正常推进（或倒退）：
const int64_t target = currentTargetFrame();
if (target < coreFramePos)       // 倒退：重置 core + 快进（SpectrumCore 无"读过去 PCM"能力）
    rebuildCoreLight(); coreFramePos = 0;
advanceCoreTo(target);

// (4) 绘制
core->getBandFrame(canvas.frame);
canvas.rp = buildRp(params);
canvas.style = style.get();
canvas.showCheckerboard = panel.getCheckerPreview();
canvas.repaint();

// (5) 传输 UI 同步（拖拽 seek 时不覆盖）
// (6) 导出进度（原子轮询 + exportDone.exchange(false) 一次性消费）
```

**子函数行为**：
- `currentTargetFrame() = floor( transport播放位置 × params.fps )`，暂停时用 `pausedPos` 作为替代值
- `rebuildCoreLight()`：`core.reset(new SpectrumCore(params))` → `core->setSampleRate(srcRate)` → 8 帧 `spf` 静音 `pushInterleavedStereo` warmup（和离线管线 VisPipeline::run 同约定，消除 FFT 启动零值）
- `advanceCoreTo(target)`：`pcm.readInterleavedStereo(frameIdx*spf, spf, dst)`，不足补零 → `core->pushInterleavedStereo` / `advanceTime(1/fps)`；每帧 1 次，循环 N=target-coreFramePos 次

> 设计亮点：参数改动不回溯历史 PCM（"曲线自然恢复"策略），避免重新解码 0..N 分钟整段音频。

---

#### 4.5.4 ParamPanel 回调 / 状态通道

| ParamPanel 侧 | 方向 | MainComponent 侧 |
|---|---|---|
| `onParamsChanged()` | 面板控件 → MC | MC 置 `paramsDirty = true`；**下一个 timer tick**（最长 1/30 秒）生效 |
| `onExportClicked()` | 面板 "Export" 按钮 → MC | `MainComponent::startExport()`：无音频 → `AlertWindow`；无导出目录 → `chooseExportDir(true)` |
| `onBrowseOutputDir()` | 面板 "Browse" 按钮 → MC | `MainComponent::chooseExportDir(false)`：`FileChooser` 以 `openMode + canSelectDirectories` 弹文件夹选择 |
| `panel.getCheckerPreview()` | MC（tick 里） → 画布 | `canvas.showCheckerboard = ...`（**GUI-only，不写入 SpectrumParams**） |
| `panel.setProgressText(s)` / `panel.setOutputDirText(s)` / `panel.setExportEnabled(b)` | MC → 面板 | UI 状态更新（进度条 / 目录文本 / 导出按钮 disable）|

**内部布局**：`rows` 顺序记录每一行，`resized()` 按行 layout。特殊行：
- Color 行：3 按钮平铺（Primary / Secondary / Peak，对应 3 个颜色选择弹层 via `ColourPickSelector : public ColourSelector, private ChangeListener`）
- "W × H" 行：两个 `TextEditor`（左侧宽，右侧高，都通过自定义 `IntInputFilter` 限制只输数字）

---

#### 4.5.5 导出线程流程（后台 std::thread）

```
[UI thread]                                 [exportThread (detached; joined in destructor)]
  │                                            │
  └─ startExportJob()                          │
       exporting = true                        │
       exportPct = 0                           │
       exportEnabled(false)                    │
       exportThread = std::thread([cfg]{       │
         VisPipeline vp;                        │
         vp.run(cfg, [](int done, int total){   │
           exportPct.store(100*done/total);     │
         });                                    │
         (mutex) exportMsg = ok/failed msg;     │
         exportDone = true;                     │
       })                                       │
       │                                        │
  timerCallback 每 tick 轮询：                  │
    exporting ? setProgress("Exporting p%") ;   │
    exportDone.exchange(false):                 │
       join thread; exporting=false;            │
       setProgress(msg); setExportEnabled(true);│
```

---

#### 4.5.6 SpectrumCanvas 渲染管线（与导出同源，预览即所得）

```cpp
// (1) 基础层：输出分辨率 ARGB，与 VisPipeline::renderFrame 完全同源（不含变换）
juce::Image base(ARGB, params.width, params.height, true);
{ juce::Graphics gb(base); gb.setOpacity(rp.opacity);
  style->render(gb, canvas_rect, frame, rp); }

// (2) 合成：显示适配(letterbox) ∘ 频谱元素自由变换，一次性画到画布
total = buildVisAffine(params.transform).followedBy(displayAffine)
g.addTransform(total); g.drawImageAt(base, 0, 0)
// 棋盘格只在画布上直接绘制（不进 base / 不进导出 Image）

// (3) 手柄 UI（hasAudio 时）：外框 + 8 角柄 + 顶部旋转圆柄
//     命中测试：画布坐标 → displayAffine 逆变换 → 输出坐标 → hitHandle/visContains

if (!hasAudio) 画中文案 "Drag & drop a WAV or AIFF file to begin\n(or click here to browse)"
```

拖放判定（v0.4.0 起）：画布接受**任意文件**（`isInterestedInFileDrag` 恒 true，避免非音频拖放时全程禁止符）；音频（`.wav/.aif/.aiff`，小写比较）→ `onFileDropped` → MC `loadFile`；非音频 → `onNonAudioDropped` → MC 弹警告框。画布外区域由 `MainComponent` 自身实现的 `FileDragAndDropTarget` 兜底（逻辑相同）。点击空白区调 `onEmptyClicked` → MC 调 `chooseAudioFile()`。

元素变换交互（v0.4.1 起）：拖动主体 = 移动；四角手柄 = 等比缩放；四边手柄 = 单轴拉伸；顶部圆柄 = 绕中心旋转；双击元素 / 右侧 **Reset element transform** 按钮 = 复位铺满画布。变换写入 `params.transform`（JSON 键 `transform.*`），CLI `--preview-frame` / `--export` 同样生效 → 预览即所得。

---

#### 4.5.7 快速上手指南（基于当前 GUI 状态）

```
启动：./build/AudioVisGUI_artefacts/Release/AudioVisGUI（WSLg 下直接弹出原生窗口）
  初始窗口 1280×800：
    左 = 画布（棋盘格灰 + "Drag & drop..." 提示）
    右 = ParamPanel（Scrollable，Viewport 自动竖滚）
    底部 = [Play] 按钮 + seekBar + "m:ss / m:ss" 时间

第 1 步：加载音频
   (a) 拖一个 .wav/.aiff/.aif 到画布 → 自动播放加载（提示：音频设备出错则 MC ctor 会 initialiseWithDefaultDevices(0,2)）
   (b) 或 点击画布 → FileChooser 选文件
   结果：底部 seekBar 解锁，时间显示 0:00 / 总时长；`paramsDirty = true` 触发重建 core（带正确 srcRate）

第 2 步：播放 + 实时预览
   点 [Play] → transport 启动 → timer 30Hz 逐帧 catch up → 曲线实时跳动
   seekBar 可拖：onDragEnd 写 pendingSeekFrame → core 重建 + 快进到目标帧
   [Play] 切换成 [Pause]；暂停后继续从 pausedPos 开始；播放到末尾则自动回到 0s

第 3 步：即改即见
   Style / Time / Dynamics / Appearance / Export 6 区控件：
     Slider（可拖 + 右侧数字输入）/ ComboBox / Toggle / 颜色按钮（点击开 ColourSelector）
   每次改动 → onParamsChanged → paramsDirty = true → 下一 tick 重建 core/style 生效

第 4 步：导出
   - Export section 底部：W × H（整数）、Encoder 下拉（png-seq / webm-vp9 / mov-qtrle）
   - 点 [Browse] 选输出目录 → 目录文本显示在 Browse/Export 下方（helpText 显示完整路径）
   - 点 [Export] → 后台线程跑 VisPipeline::run
     · progress 显示 "Exporting xx% ..."
     · 期间 Export 按钮 disabled 防止重入
     · 完成后显示 "Done: N frames -> <dir>" 或 "Failed: reason"，Export 按钮恢复可用

第 5 步：查看结果
   - PNG 序列 → 输出目录 frame_XXXXXX.png（ARGB 透明）
   - WebM/MOV → 如果编码器是 webm-vp9 / mov-qtrle，且 PATH 中存在 ffmpeg，则自动 mux 为带 alpha 的视频
```

---

#### 4.5.8 当前实现的已知限制（Important for future AI）

> 这些不是 bug，是 v0.3.x 未实现的功能。后续 AI 改动前必须先确认是否属于此列表，避免误报。

| # | 限制 | 影响 | 建议扩展点 |
|---|---|---|---|
| L1 | ~~音频输入格式 MP3/FLAC/OGG 解码失败~~ | **v0.5.4 已修复**：CMake 开 `JUCE_USE_MP3AUDIOFORMAT=1` + `PcmSource` 注册 `MP3AudioFormat`；用户实测 5 格式（wav/aiff/flac/ogg/mp3）全数通过 | — |
| L2 | **GUI 仅单音频替换，不支持 playlist 或 multi-clip timeline** | 每次 loadFile 会释放旧 readerSource + transport.setSource(nullptr) | 未来 timeline editing（docs/ROADMAP.md「中期」）|
| L3 | **参数改动不回放历史 PCM** → 改参后，曲线需要 `attackMs+releaseMs` 秒才能稳定 | N/A（设计决策：避免重新解码全音频）| 若用户要"立即到达对应视觉稳态"，可在 advanceCoreTo 内部跳过前 N 帧不渲染 |
| L4 | **多 pass 合成目前在单 Graphics 上叠加**，CrystalStyle::renderPass 的 glow 层没有真 blur | v0.5.4 已改用 JUCE `applyGaussianBlurEffect`（真 bloom），此项已修复 |
| L5 | **导出时 GUI 播放引擎不暂停**：后台线程开独立的 `VisPipeline::run` 再次解码同一个音频文件（当前是 OK 的，因为音频只读）| CPU 峰值略高 | 可加 if (exporting) transport.stop(); exportDone exchange 后可选恢复 |
| L6 | **面板 paramsDirty 粒度是"任意字段修改即重建 core/style 全量"**：改颜色/画网格不需要重建 core，只用重绘 | 轻微性能浪费（30fps 下无感，但 60fps + 512 band 时会有影响）| 加细分 dirty flag：`dirtyEngine / dirtyStyle / dirtyRepaintOnly` |
| L7 | **GUI 导出目录不落盘（仅本次会话有效）**：下次启动需重新选 | N/A | 加 `juce::ApplicationProperties` + OptionsPage 记忆最后导出目录 |
| L8 | **颜色选择弹层的 OK/Cancel 不是显式按钮**：JUCE ColourSelector 是实时 change broadcaster，点外部关闭后最后一次选择的颜色立即生效（但如果用户"后悔"，没有 undo）| UX | 加 "Preset colours" combos 与 "Reset to default" 按钮 |
| L9 | **minDb/maxDb/bgColor 没有 GUI 控件**（参见 §8 表中标注"无 GUI，预留"的 3 个字段）| 改 minDb/maxDb 必须用 CLI `--set dynamic.minDb=-96` 或 JSON | 补 2 个 Slider + 1 个 ColourPicker 到 Appearance section 末尾 |
| L10 | **进度条 seekBar 没有播放头刻度样式**，只是标准 LinearHorizontal Slider | UX | 自定义 LookAndFeel method：drawLinearSlider 画一个带圆角的轨迹 + 拖动圆点 |
| L11 | ~~compose() 描边闪退（勾 Outline 必崩）~~ | **v0.5.4 #1b 已根除**：真根因＝`e[idx]` 行指针二次偏移 → `tmp[2yW+x]` 越界读堆（非分配失败、与均色无关）。用户 `crash/*.dmp` 符号化直指 `compose+0x8f5`。修复＝`e[x]` + `vis_mask_test` 用例 6（含负对照） | 见 §7.8 第 7 条避坑 |



### 4.6 CLI
**文件**：`source/cli/main.cpp` + `source/cli/CliArgs.h/.cpp`

**命令**：
```
AudioVisExport --export <audio> <outdir> [options]     # 完整导出 PNG 序列
AudioVisExport --preview-frame <png> [options]          # 单帧预览
AudioVisExport --probe-spectrum <audio> <frameIdx>      # 数值调试
AudioVisExport --probe-pcm <audio>                      # PCM 信息
AudioVisExport --gen-tone <wav> <freq> <dur> <sr>       # 生成测试音
AudioVisExport --config                                 # 打印当前参数 JSON
```

**常用参数**（糖 flag）：
```
--width / --height / --fps / --style
--band-count / --freq-scale / --min-hz / --max-hz
--dyn-curve / --dyn-gain / --dyn-gamma
--attack-ms / --release-ms / --peak-hold-ms
--primary-color / --secondary-color / --peak-color / --bg-color
--line-width / --opacity
--draw-grid {on/off} / --draw-axis-labels {on/off}
--encoder {png-seq/webm-vp9/mov-qtrle}
--set <dotted.key=value>   # 任意参数覆盖
--config <preset.json>     # JSON 预设
```

---

## 5. 构建与运行

### Windows 交叉编译（v0.5.0 主交付路径，日常用这个）

```bash
bash scripts/build_win_cross.sh --deploy
# 产物（静态 CRT PE32+，免 VC_redist）：
#   build_win/AudioVisExport_artefacts/Release/AudioVisExport.exe  (CLI)
#   build_win/AudioVisGUI_artefacts/Release/AudioVisGUI.exe        (GUI)
# --deploy 同步到 C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\
# 附加选项: --clean 全量重建；--no-build 仅部署
```

工具链构成（一次性安装，此后离线构建）：
| 组件 | 用途 | 安装 |
|---|---|---|
| clang-18 + lld-18 | 交叉编译器（MSVC ABI，经 `scripts/clang-cl-wrapper.sh` 加 `--target=x86_64-w64-windows-msvc`）+ lld-link-18 链接器 | `sudo apt-get install clang-18 lld-18`（apt.llvm.org jammy，无国内镜像）|
| xwin (Rust) | 从微软官方拉取 Windows SDK 10.0.22621 + MSVC CRT 14.44 → `~/.xwin-sysroot/` | `cargo install xwin; xwin --accept-license --arch x86_64 --sdk-version 10.0.22621 splat --output ~/.xwin-sysroot` |
| cmake ≥ 3.28 | CMake 3.22 的 Windows-Clang 平台模块不支持跨平台 MSVC 前端，需 4.x | `pip install cmake`（清华镜像快）|
| toolchain 文件 | `cmake/toolchains/clang-cl-msvc.cmake` | 已入库 |

三个已知坑（toolchain 内已固化修复，排查时参考）：
1. **MSVC STL 版本检查**：最新 STL (v143) 要求 Clang 19+，定义 `_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH` 绕过（Clang 18 实测全功能通过）
2. **Linux 大小写敏感**：Windows SDK 头/库文件名大小写与 JUCE `#include` 不完全一致（`Dbghelp.h` vs `dbghelp.h`）；toolchain 把版本化 SDK 目录（原始大小写）排前面 + 脚本补符号链接
3. **SSE intrinsics 必须内联**：MSVC CRT 的 `emmintrin.h` 只声明 extern 函数（依赖 MSVC 编译器硬编码识别），clang 会生成未定义符号；须把 clang resource dir（自带真正内联的 intrinsic 头）用 `-isystem` 排在搜索链首位

### 构建（WSL / Linux 本机，WSLg 预览用）

一次性安装工具链与 JUCE Linux 依赖（Ubuntu 22.04 实测，2026-09-05 WSL 迁移时记录）：
```bash
sudo apt-get update && sudo apt-get install -y \
    cmake ninja-build ffmpeg pkg-config \
    libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxcomposite-dev libxrender-dev \
    libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVX_USE_LOCAL_JUCE=ON
cmake --build build -j2   # ⚠️ 本机 12 核 / 7.7GB：默认 -j$(nproc) 会在 JUCE 大 TU 上 OOM（cc1plus 被 Kill），实测 -j2 安全
# 产物（Linux 无 .exe 后缀）
build/AudioVisExport_artefacts/Release/AudioVisExport   # CLI
build/AudioVisGUI_artefacts/Release/AudioVisGUI         # GUI（WSLg 下直接弹窗）
```
> - **JUCE 来源**：`third_party/JUCE` 是指向 `../../Y2Kmeter/third_party/JUCE` 的符号链接
>   （复用 Y2Kmeter 的 JUCE 8.0.12 本地 checkout，避免 FetchContent 大克隆），需配 `-DAVX_USE_LOCAL_JUCE=ON`；
>   不带该选项则 FetchContent 从 GitHub 克隆（首次配置需联网）。
>   **注意：2026-09-05 实测本机 git 访问 GitHub 出 TLS 中断（`gnutls_handshake() failed`），
>   在线克隆很可能失败——WSL 环境下请始终带 `-DAVX_USE_LOCAL_JUCE=ON`。**
> - Ninja / Makefile 是单配置生成器，没有 `--config` 的多配置概念，构建类型由 configure 时的
>   `-DCMAKE_BUILD_TYPE=Release` 决定（顶层的 powershell/Visual Studio 时期流程已废弃）。
> - GUI 显示走 WSLg（`DISPLAY=:0` / `WAYLAND_DISPLAY=wayland-0`），无需额外 X server；
>   音频播放走 WSLg 的 PulseAudio。

### GUI 使用
```
./build/AudioVisGUI_artefacts/Release/AudioVisGUI   # 终端启动（WSLg 直接弹原生窗口，Ctrl+C 退出）
Step 1. Drag a WAV/AIFF onto the canvas (or click canvas to browse)
Step 2. Click Play. Spectrum follows the music in real time.
Step 3. Drag sliders / type numbers / pick colors in the right panel — changes take effect within one 30 Hz tick.
Step 4. Browse → choose output dir; set W × H and Encoder; click Export.
        Background thread runs VisPipeline::run and progress is polled every tick.
        When finished you will see "Done: N frames -> <dir>" below the Browse/Export row.
```
> Tip: Toggle **"Checkerboard BG"** off in the Appearance section before taking screenshots to judge real transparent (ARGB) pixels without the preview checkers — note this toggle is GUI-only and never affects exported PNG pixels.

### 运行示例
```bash
AVX=./build/AudioVisExport_artefacts/Release/AudioVisExport

# 生成 10s 透明背景柱状图视频
$AVX --export PUPA_10s.wav out_bar --style bar --width 960 --height 540 --fps 30

# 生成水晶效果单帧预览（棋盘格背景验证透明度）
$AVX --preview-frame preview_crystal.png --audio PUPA_10s.wav --frame-index 300 --style crystal --width 960 --height 540 --set output.bgCheckerboardPreview=true

# 数值调试
$AVX --probe-spectrum PUPA_10s.wav 300 --band-count 64
```

### 交付到 Windows 测试（`build_win_cross.sh --deploy` 已自动化；此节为手动流程备查）
```bash
DEST=/mnt/c/Users/yiyikneesocks/Desktop/AudioVisExport_test
mkdir -p "$DEST"
cp build/AudioVisExport_artefacts/Release/AudioVisExport  "$DEST/"
cp build/AudioVisGUI_artefacts/Release/AudioVisGUI        "$DEST/"
# 启动器（Windows 桌面双击 → wsl 拉起 GUI，WSLg 显示窗口）
cat > "$DEST/Run_AudioVisGUI.bat" <<'EOF'
@echo off
wsl -e /mnt/c/Users/yiyikneesocks/Desktop/AudioVisExport_test/AudioVisGUI
if errorlevel 1 pause
EOF
# 测试音频（CLI 现场生成：44.1kHz 立体声 5s 1kHz -6dBFS）
"$DEST/AudioVisExport" --gen-tone "$DEST/test_tone.wav"
```
> 交付后用户在 Windows 桌面双击 `Run_AudioVisGUI.bat` 即测试；CLI 可在 `cmd` 里
> `wsl -e <交付目录内的二进制绝对路径>` 调用。若 WSL 发行版名不是默认值，
> 把 bat 里的 `wsl -e` 换成 `wsl -d <发行版名> -e`。

### 透明背景视频合成
```bash
# PNG 序列 + 音频 → MP4（不透明预览用）
ffmpeg -framerate 30 -i out_bar/frame_%06d.png -i PUPA_10s.wav -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest output.mp4

# PNG 序列 + 音频 → WebM（带 alpha，PR 可叠加）
ffmpeg -framerate 30 -i out_bar/frame_%06d.png -i PUPA_10s.wav -c:v libvpx-vp9 -pix_fmt yuva420p -auto-alt-ref 0 -c:a libvorbis -shortest output.webm
```

### 5.9 主机侧辅助工具（python venv，**不是构建依赖**）

> 2026-09-11 新增。核查结论先说清楚：**工程本身零 python 依赖**——`scripts/*.sh`、`CMakeLists.txt`、
> `cmake/` 里没有任何 python/pip 调用（工具链＝CMake + Ninja + clang-cl + lld-link + xwin）；
> 仓库内也没有 `.py`（`find` 直查为 0；`find -L` 跟符号链接能查到 5 个，全在 `third_party/JUCE` 里，
> 是 JUCE 自己的 CI 脚本，不属于本工程）。`tools/*.py` 只是**人工排障/做素材**时的手边工具。

| 工具 | 干什么 | 依赖 |
|---|---|---|
| `tools/dmp_report.py` | 把 `crash/*.dmp` + 链接期 `/MAP` 翻成「函数名 + 偏移」；v0.5.4 的 compose 描边越界读（#1b）就是这条路定位的 | `minidump` |
| `tools/gen_icon.py` | 重新生成 `assets/icon.png`（CMake `ICON_BIG/ICON_SMALL` 输入）。**原生成脚本曾丢失**（图标只有成品没有源），此脚本是设计复刻，默认不覆盖成品 | `pillow` |
| `tools/setup_env.sh` | 一键建/修那个 venv（幂等；`--print` 只打印激活路径） | — |

**环境放在仓库外面**：默认 `~/CodingProgram/AudioVisualizer/tools-venv`（可用 `AVX_TOOLS_VENV` 覆盖）。
故意不放工程目录内——venv 里成千上万个 `.py` 会污染 `find`/`grep`，让协作 AI 每次搜代码都要分辨哪些是自己的。
`.gitignore` 另有一条防御（万一建在里面）。

**为什么是 venv 而不是再开一个 conda env**（用户 2026-09-11 定的方向）：conda 环境会复制整套解释器+库目录
（几百 MB 起），venv 只是薄壳、装多少算多少；**只有需要非 python 二进制时才值得用 conda**
（例：§7.5 的 `gitenv` 里那个 openssl 版 git，那是系统级二进制，venv 装不了）。

```bash
bash tools/setup_env.sh                                   # 建 venv + 装 requirements.txt
source ~/CodingProgram/AudioVisualizer/tools-venv/bin/activate
tools/.../python tools/dmp_report.py <exe目录>/crash/crash_YYYYmmdd_HHMMSS.dmp \
     --map build_win/AudioVisGUI.map                      # map 必须来自出事那一次构建
```

**踩过的两个坑（脚本已内置修正，别再手搓）**：
1. Ubuntu 的 `python3` 常缺 `ensurepip`（`python3 -m venv` 直接失败）→ `setup_env.sh` 会自动回退到
   conda 的 python 来建 venv（venv 的 site-packages 与 base **隔离**，实测 `base site-packages in path = False`，
   只是 stdlib 仍复用 seeding 解释器，正常）。
2. `minidump` 库解析时会为 PEB 打印整段 traceback（`Memory address ... is not in process memory space`），
   **那是被库自己 catch 掉的日志噪音**，模块表/异常记录照常解析——脚本顶部 `logging.disable` 静音即可，
   不要因此以为 dump 坏了而退回手工解字节流。

**崩溃证据补充**：现存 4 个 dump 用 `dmp_report.py` 全部落在 `SpectrumMask::compose` 内，
且 `ExceptionInformation[0]=0` = **READ** 违例、目标地址是 `0x1ee58b89000` 一类的野生值（**不是 NULL**）——
从硬证据上排除了当初"分配失败返回 NULL"的猜测，指向 `tmp[2·y·W+x]` 越界读（与 #1b 修复一致）。

### ⚠️ 重要环境事实：AI 的 shell 与用户的终端，`cmake` / `python` 不是同一个

`~/.bashrc` 里有 conda 初始化块且 `auto_activate_base: True` → **用户开终端即处于 base**：
`which cmake` = `~/miniconda3/bin/cmake`（pip 装的 **4.4.3**），`which python` = base 的 3.13。
而协作 AI 的工具 shell 是**非交互**的（不读 .bashrc）：`which cmake` = `/usr/bin/cmake`（apt **3.22.1**）。
**别拿自己 shell 里的版本推断用户机器上的版本**（我一开始就据此错判过一回，见下）。

结论：**base 里的 pip `cmake 4.4.3` 保留、不要卸**（2026-09-11 实测）——
- 它恰恰是用户手动敲 `cmake` 时真正生效的那个，卸掉会悄悄改变用户环境；
- 用 `~/miniconda3/bin/cmake`（4.4.3）实测**配置 + 编译本工程全部通过**（configure done；
  `ninja AudioVisExport` 36/36、exit 0），不会踩 CMake 4 移除 `cmake_minimum_required(<3.5)` 兼容的坑
  ——本工程与 JUCE 都声明 `cmake_minimum_required(VERSION 3.22)`。

顺带：`tools/*.py` 缺包时会**自动 execv 到 tools-venv 重跑**（因为用户终端的 `python` 是 base、
已不含 minidump/pillow），所以直接 `python tools/dmp_report.py ...` 就能用。
两个实现坑（都踩过并修）：① 判断"是否已在 venv 内"**不能用 `realpath` 比较**——venv 的 `bin/python`
是指向 seeding 解释器的符号链接，realpath 会把两者判成同一个、导致永不跳转（改为比较 `bin` 目录）；
② `os.execv` 前必须 `sys.stdout.flush()`，否则跳转提示随缓冲丢失。

---

## 6. 关键设计决策记录

| 决策 | 原因 | 替代方案 |
|------|------|----------|
| 弃用 projectM | projectM 4.1.x 无 FBO 支持，离线渲染需 blit workaround；且 Milkdrop 预设是"全屏可视化"非"频谱图" | 自写 SpectrumCore（完全可控） |
| ARGB PNG 序列 | 保留 alpha 通道，导入 PR 透明区正确透出下层 | RGB + 绿幕（精度差） |
| 双路 FFT | Y2Kmeter 验证过的低频增强方案（8192 路 500Hz 以下分辨率） | 单路 8192（计算量大 4×） |
| Catmull-Rom 平滑 | Y2Kmeter 原版方案，视觉效果好 | B-spline / Bezier（需调参） |
| `2.0f / fftSize` 归一化 | 与 Y2Kmeter 逐字节一致 | `1.0f / N` + window sum（理论更精确但视觉无差别） |
| 多 pass 在单 Graphics 叠加 | 简单，无需多 Image 合成 | 真 多 Image + blend mode（后续扩展） |
| drawGrid/drawAxisLabels 默认 false | 可视化视频不需要坐标轴 | 默认 true（Y2Kmeter 原值） |

---

## 7. 常见问题

**Q: 为什么绝对 dB 值偏低（1kHz -6dBFS 音显示 -35dB）？**
A: 不是 bug。FFT 归一化与 Y2Kmeter 逐字节一致（`2.0f / fftSize`）。偏低是 bin 间泄漏 + band 取值所致。用户明确"可视化工具不需严格纵轴精度"。

**Q: 导出的 PNG 导入 PR 后透明区显示黑色？**
A: PR 需要带 alpha 的视频格式。实测最可靠：**MOV QTRLE（rgba / argb）** 无损保留 alpha；
   WebM VP9 的 `yuva420p` 在主流 ffmpeg（含 gyan.dev 2026 发布版）中实际不编码 alpha，会静默降级为 `yuv420p`。建议透明素材用 MOV QTRLE 或 PNG 序列。

**Q: 构建报错找不到 JUCE？**
A: 需指定 `-DAVX_USE_LOCAL_JUCE=ON`，或确保 `third_party/JUCE/` 存在。沙箱环境无法从 GitHub clone。

**Q: ffmpeg 子进程卡死？**
A: 不能先 wait exit 再 drain stdout/stderr（会死锁）。PngSequenceEncoder::finalizeAndMux 用 200ms 轮询 + 增量读取。

**Q: git push 报 `gnutls_handshake() failed` / TLS 中断？**
A: 本机（WSL2 Ubuntu 22.04）固定问题：系统 git 用 gnutls 后端被网络环境干扰。**必须**用
   conda 环境 `gitenv` 的 openssl 版 git + HTTP/1.1，失败重试 1~5 次：
   ```bash
   PATH=/home/azulores/miniconda3/envs/gitenv/bin:$PATH git -c http.version=HTTP/1.1 push origin main
   ```
   凭据已在 `~/.git-credentials`（0600）；token 轮换 / 凭据维护见 `docs/HISTORY.md` 页首「发版流程」。
   协作 AI 严禁让用户把 token 明文贴进对话。

---

## 7.5 Git 推送速查（2026-09-07 实测固化）

```bash
# 推 main（失败就重跑，1~5 次内成功）
PATH=/home/azulores/miniconda3/envs/gitenv/bin:$PATH git -c http.version=HTTP/1.1 push origin main

# 推 tag
PATH=/home/azulores/miniconda3/envs/gitenv/bin:$PATH git -c http.version=HTTP/1.1 push origin vX.Y.Z
```

| 组件 | 说明 |
|---|---|
| `gitenv` conda 环境 | git 2.55 openssl 后端（`~/miniconda3/envs/gitenv/`），绕开系统 gnutls TLS 中断 |
| `http.version=HTTP/1.1` | HTTP/2 在该网络下也会被重置，HTTP/1.1 稳定 |
| `~/.git-credentials` | PAT 凭据（0600，credential.helper=store）；remote URL 干净无 token |
| PAT 权限 | fine-grained，仅 AudioVisExport 仓库 Contents 读写 |
| 验证命令 | `... git -c http.version=HTTP/1.1 ls-remote --heads origin main`（只读，测凭据/网络） |

**发 Release**：push tag 后用 GitHub API POST `/repos/yiyikneesocks/AudioVisExport/releases`
（正文取 `docs/RELEASE_NOTES.md` 对应版本节），或网页 Releases → Draft a new release。

---

## 7.8 协作 / 编辑工具链避坑（AI 高频踩，务必先读）

> 本节由历次会话实际踩坑沉淀。改动前先扫一眼，能省掉大量返工与"假 diff"。

**1. 行尾混用（最坑，已两次造成整文件伪 diff）**
- 本仓库**没有** `.gitattributes`，`core.autocrlf` 未设置 → 各文件行尾**按提交时原样混存**：
  - CRLF（截至 2026-09-11 实测仅存）：`SpectrumStyle.cpp` …
  - LF：`SpectrumCanvas.cpp` / `MainComponent.cpp` / `BarStyle.cpp` / `Y2KLineStyle.cpp` / `CrystalStyle.cpp` /
    `PolylineStyle.cpp` / `BarLineStyle.cpp` / `ParamPanel.cpp` / `ColorMap.cpp` …
  - **别信这张表的"曾经"**：动手前用 `python -c` 实测该文件 CRLF/LF 计数（下面脚本的 `head_style()` 即可）。
- **严禁**对文件跑"全局统一 CRLF↔LF"脚本——会把每一行都变成改动，真实 diff 被淹没（曾见 ParamPanel 虚高到 1080 行）。
- 正确做法：
  - 优先用 **`edit` 工具**逐处精确改（它保留该文件原有行尾）。
  - 若必须用脚本重写整文件，**先按 HEAD 版本判定该文件的目标行尾再写回**：
    ```python
    def head_style(p):                       # 读 HEAD 判断 CRLF / LF
        b = subprocess.check_output(["git","show",f"HEAD:{p}"])
        cr = b.count(b"\r\n"); lf = b.count(b"\n") - cr
        return "CRLF" if cr >= lf else "LF"   # 新文件默认按同目录兄弟（多为 LF）
    body = open(p,"rb").read().replace(b"\r\n", b"\n")        # 先归一到 LF
    if head_style(p) == "CRLF": body = body.replace(b"\n", b"\r\n")
    open(p,"wb").write(body)
    ```
  - **改完必查** `git diff --stat`：某文件行数远超你的逻辑改动 → 十有八九被行尾搅了，按上法还原。
  - **用脚本改文件时，python 一律 `open(...,'rb')`/`'wb'` 或 `open(..., newline='')`**：
    文本模式读会把 `\r\n` 折成 `\n`、写回就静默把整个 CRLF 文件转成 LF（2026-09-11 一次改 3 个 `.h` 就中招，
    `git diff --numstat` 显示 300+/300- 才发现，只能按 HEAD 风格逐文件还原）。
    改完 **`git diff --numstat` 逐个看**，行数与你的逻辑改动不匹配就是行尾被搅。

**2. `edit` 工具锚点必须唯一**
- 插入函数时用 `return xxx;\n}` 这类短尾锚点，易命中文件里**更早的同名片段**（曾把 `horizontalGradient`
  插进 `ColorMap::map()` 体内，编译炸）。锚点请带**前后各 3–5 行唯一上下文**，或直接 `write` 整文件重写。

**3. JUCE API 惯用坑**
- `ColourGradient::colours` **是私有**（且英式拼写 `colours`）→ 不能 `grad.colors.clear()` 复用同一 gradient 改色。
  逐柱/逐段上色请**每次新建 `ColourGradient` 传进 `setGradientFill`**（160 段/帧，开销可忽略）。
- `reduceClipRegion` / `excludeClipRegion` **只有 int-rect 版**，无 float → `rect.toNearestInt()`（≤1px 偏差，无感）。
- `drawLine` **没有 `(Point,Point,thickness)` 重载** → 用 `drawLine(juce::Line<float>(a,b), t)` 或四 float 版。
- `drawDashedLine(Line<float>, const float* dashes, int numDashes, thickness)`：`numDashes` 是数组段数（如 `{6,4}`+2 = 实6空4）。
- 真高斯模糊直接用 **`image.getPixelData()->applyGaussianBlurEffect(radius)`**（`GlowEffect` 内部即如此），别手搓卷积。
- **全局快捷键收不到键**：无组件持焦时 `ComponentPeer::getTargetForKeyPress` 回退到**顶层窗口**且只向**父链**走，
  永不进入子组件 → 顶层 content（`MainComponent`）的 `keyPressed` 不被调用。要让空格/Delete 即时生效，
  须 `setWantsKeyboardFocus(true)` 并在合适时机（如 `loadFile` 末尾）`grabKeyboardFocus()`。

**4. `--set` 点路径前缀别靠猜**
- 分组前缀固定：`time.` / `visual.` / `transform.` / `output.` / `fft.` / `freq.` / `dynamic.` / `mask.`。
  例：`time.peakDecayAccelDbPerSec2`（**不是** `spectrum.`）、`transform.rotationDeg`（**不是** `spectrum.rotation`）。
- 拿不准就查 `source/core/SpectrumParams.cpp` 的 `setByKey`（`if (key == "...")` 列表是唯一事实源）。

**5. CLI `--help` 糖列表易过期**
- `--style <...>` 帮助串落后于工厂；新增样式时顺手同步 `CliArgs::helpText()` 与 GUI `ParamPanel` 的样式下拉数组（两处须一致）。

**6. `VisTransform` 数学：先探针、再断言（v0.5.4 #3 教训）**
- `buildVisAffine` 语义是 `p → M(p−c) + c + pos`（**枢轴自抵消**，`pos` 就是"元素中心的位移量"）。
  写任何"把元素摆到某处"的公式时，`pos` 补偿**不要再乘 scale**：`makeContainTransform` 旧公式
  `pos = out/2 − s·c` 只在 s=1 时居中，图片缩放适配（s≠1）会整体偏 `(1−s)·c`——潜伏数个版本，
  被"100x50 contain 200x100 恰好 s=2 → pos 算出 0"的自洽假象骗过测试（v0.5.4 修正为 `out/2 − c`）。
- 同类"两套数学必须一致"的地方还有：`compose()` 渲染 ↔ `visCorners()` 手柄（`vis_mask_test` 情形3 锁死）；
  改动 `VisTransform.h`/`SpectrumMask.cpp` 任一侧后务必跑 `vis_anchor_test` + `vis_mask_test`。
- 验证几何的正确姿势：**先打一个最小探针程序看映射值**，别靠脑内推 JUCE `scaled/rotated/translated`
  的"before/after"文档措辞（极易推反）。

**7. 像素缓冲与"轴映射"的三类隐性错（v0.5.4 #1b / #3 连踩，务必自查）**
- **行指针 + 绝对下标 = 二次偏移**：`const int* e = tmp.data() + y*W;` 之后必须写 `e[x]`；
  再套 `idx = y*W + x` 写成 `e[idx]` 就变成读 `tmp[2yW+x]` → 画布下半部**越界读堆**（Windows 勾 Outline 必崩，
  Linux/小图/上半屏看不出问题）。凡"先取行指针再进 x 循环"的代码，下标只允许列号。
- **像素判据别用绝对阈值**：JUCE `PixelARGB` 读回的是**预乘**值，`withAlpha(0.75f)` 的纯蓝线经 AA 后
  `getBlue()` 可能只有 115；测试里判"某色是否存在"请用**两张渲染图做像素差分**（`vis_peaks_test` 的 `diffCount`），
  别写 `getBlue() > 120` 这种一看就过、一跑就假阴的阈值。
- **`baselineTop`/`baselineBottom` 必须成对使用，且反函数要显式减基**：
  `top = a + (1−a)n` 的逆是 `n = ((bottom−y)/H − a)/(1−a)` —— 漏掉 `− a` 会反推出 `n + a/(1−a)`（被抬高并 clamp 到 1）。
  `CrystalStyle::buildMirrorPoints_` 就因此让整个下半屏被填充。
  **能从原始 `normalized[]`/`db[]` 直接算下臂的，就不要从下标/像素反推**（y2k / polyline 用 `nv[]` 正算，天然无此坑）。
- 另：绘制序列里 `setGradientFill/setColour` 是**有状态**的，紧接其后的 `strokePath`/`fillPath` 若忘了重设，
  会静默沿用上一段的颜色与 alpha（y2k 下臂描边"看不见"的真实原因就是继承了 fill 的 0.25f）。

**8. "先改状态、后验证"与"一行只挂一个控件"（v0.5.4 #G/#H 教训：都是静默失效类 bug）**
- **验证必须发生在任何状态写入之前**。反例（都真实存在过）：`applyMaskImageFile` 先无条件写
  `maskImage.path/enabled`、再 `if (im.isValid())` 算均色 → 解不开时留下"已设置但看不见"的死状态，
  下次操作只会撞见"already set"分支；`addImageLayer` 同样不验证就 `images.insert`。
  判据：**任何"失败"分支都不许留下副作用**，且失败要弹**具体**原因（文件名 / 字节数 / 支持清单），
  成功要写明**去向**（图片成了第几层 / 成了蒙版）——"没反应"的观感多半来自静默 return。
- **同一行并排多个控件时，显隐与布局必须整行驱动**。`Row{label, editor}` 单 editor 的结构缺陷是：
  Colors(4)/描边色(2)/W×H(2) 只登记了第一个，其余五个控件**永远不参与切页隐藏** → 用户看到的
  "Use average 切页不消失"只是最显眼的一个。现在 `Row::editors` 是 vector + `addRowGroup`，
  `resized()` 末尾还有**安全网**（不属于任何行、又没登记为常驻 `keepVisibleOnAllTabs()` 的子组件一律隐藏）。
  设计取向：让漏登记的后果是「控件不出现」（开发当场发现）而不是「残留在别的页」（用户替你发现）。
- **两条拖放链（OLE `FileDragAndDropTarget` 与 WM_DROPFILES 直连）不要各写一份路由规则**，
  否则必然漂移：本项目就出现过兜底链把 png/jpg 当作"音频白名单"成员 → 对图片调 `loadFile()`。
  统一入口 `MainComponent::routeDroppedFile(file, 屏幕落点)`。
  另注意 `filesDropped(files, x, y)` 给的 (x,y) 是**本组件内坐标**，与 `Viewport::getScreenBounds()` 比较前要
  `getScreenBounds().getTopLeft() + Point(x,y)` 换成屏幕坐标。
- **别用 `jmax(1.0f, x)` 当"防除零"**：x=0（典型＝解码失败的 0 尺寸图）会被静默当成 1，
  于是 `out/x` 算出画布高度级的荒谬倍数，错误被放大到可视区外而不是被报告。要么显式判 `x<=0` 并退回安全值，
  要么让调用方先验证——两处都做才算闭环。
- **JUCE 8.0.12 里没有的东西**（本轮编译期撞过）：`juce::File::getFileSizeDescription` /
  `getDescriptionForValue`（自己 `String::formatted("%lld bytes")`）、`ListBox::setScrollbarAutoHide` /
  `updateViewport`、`Component::getNumChildren`（用 `getChildren().size()`）、`ListBoxModel::drawRowBackground` /
  `listItemClicked`（真名 `listBoxItemClicked(int, const MouseEvent&)`、`listBoxItemDoubleClicked`）、
  `juce::ImageFileLoader`（廉价探测改图能力用 `ImageFileFormat::findImageFormatForFileExtension`）。
- **脚本改文档必须"按关键词反查落地"**：`str.replace()` 没命中是**静默无操作**，
  只打印脚本自己的 OK 或只看 `git diff --stat` 都发现不了（本轮 PLAN.md 就漏过一整段）。
  正确姿势：`assert old in t` 再 replace，写完后 `grep -c 关键词` 逐条验证。

---

## 8. 完整可调参数表（~50 个，供 AI 快速查询）

**参数注入顺序**（低优先级 → 高优先级，后者覆盖前者）：
```
SpectrumParams.h 默认值
  → --config preset.json (via fromJson)
    → CLI sugar flags (--style, --band-count, etc.)
      → CLI --set dotted.key=value (按出现顺序)
        → GUI ParamPanel 即时改动
```

### 8.1 FFT 组（点前缀：`fft.`，CLI 未暴露糖命令，仅 `--set` 可调）

| 参数（SpectrumParams 字段）| 点路径 | 默认 | 范围 | 说明 |
|---|---|---|---|---|
| fftOrder | `fft.fftOrder` | 11 (size=2048) | 8..14 | 主路 FFT 阶数（size = 1<<order）|
| fftOrderLo | `fft.fftOrderLo` | 13 (size=8192) | 11..15 | 低频增强路 FFT 阶数 |
| enableLowFreqPath | `fft.enableLowFreqPath` | true | bool | 开关低频增强路 |
| crossoverHz | `fft.crossoverHz` | 500.0 | 100..2000 | 高低路 crossfade 频率 |
| windowFunc | `fft.windowFunc` | Hann | Hann / Hamming / Blackman / BlackmanHarris / Rectangular | 加窗函数 |
| hopSize | `fft.hopSize` | 0 (no overlap) | 0..fftSize-1 | 主路 hop（0=no overlap, Y2K 原默认）|
| hopSizeLo | `fft.hopSizeLo` | 0 (= fftSize/4) | 0..fftSizeLo-1 | 低频路 hop |

### 8.2 频率映射组（点前缀：`freq.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| bandCount | `freq.bandCount` | 90（v0.5.4 #25）| "Band count" 滑块（16..512） | 频带数量（越大视觉越密）|
| freqScale | `freq.freqScale` | `Log` | "Freq scale" 下拉 | `log`/`linear`/`mel`/`bark` |
| minHz | `freq.minHz` | 20.0 | "Min Hz" 滑块（20..2000）| 横轴左端频率 |
| maxHz | `freq.maxHz` | 20000.0 | "Max Hz" 滑块（1000..20000）| 横轴右端频率 |

### 8.3 时间响应组（点前缀：`time.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| fps | `time.fps` | 30.0 | "FPS" 下拉（24/30/60） | 输出帧率 |
| attackMs | `time.attackMs` | 30.0 | "Attack ms" 滑块（5..500） | 上升沿时间常数（越小越"跳"）|
| releaseMs | `time.releaseMs` | 200.0 | "Release ms" 滑块（20..2000） | 下降沿时间常数 |
| peakHoldMs | `time.peakHoldMs` | 3500.0 | "Peak hold ms" 滑块（0..10000） | 峰值保持期 ms |
| peakDecayDbPerSec | `time.peakDecayDbPerSec` | 12.0 | "Peak decay dB/s" 滑块（1..60） | 峰值衰减速度 |
| temporalSmoothing | `time.temporalSmoothing` | 0.5 | "Temporal smooth" 滑块（0..1） | 列间 [1:2:1] 模糊强度 |

### 8.4 动态强度组（点前缀：`dynamic.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| dynCurve | `dynamic.curve` | `LinearDyn` | "Y-axis curve" 下拉 | `linear`/`sqrt`/`loglog`/`perceptual` |
| dynGain | `dynamic.gain` | 1.0 | "Gain" 滑块（0.2..3.0） | 乘到 (db-minDb)/range：>1=更跳 |
| dynGamma | `dynamic.gamma` | 1.0 | "Gamma" 滑块（0.3..3.0） | pow(norm, gamma)：gamma<1=压低端整体抬 |
| slopeEnabled | `dynamic.slopeEnabled` | true | "Slope comp" toggle | 开关斜率补偿 |
| slopeDbPerOct | `dynamic.slopeDbPerOct` | 4.5 | "Slope dB/oct" 滑块（0..12） | 每八度斜率补偿（粉红噪声更均匀）|
| minDb | `dynamic.minDb` | -80.0 | （无 GUI，预留）| 纵轴底 |
| maxDb | `dynamic.maxDb` | 0.0 | （无 GUI，预留）| 纵轴顶 |

### 8.5 视觉 / 外观组（点前缀：`visual.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| style | `visual.style` | `y2k-line` | "Render style" 下拉 | `y2k-line`/`bar`/`bar-line`/`polyline`/`crystal`（`bar-mirror` 已于 v0.5.4 #3.1 删除，工厂仍接受该别名→`bar`） |
| colorMap | `visual.colorMap` | `solid` | （CLI `--set` / GUI）| `solid`/`gradient`（按强度）/`rainbow`（按频带相位），已接 bar/line 全样式 |
| primaryColor | `visual.primaryColor` | `#ec4899` | "Primary" 颜色按钮 | 主描边色 |
| secondaryColor | `visual.secondaryColor` | `#f9a8d4` | "Secondary" 颜色按钮 | 填充/网格色 |
| peakColor | `visual.peakColor` | `#be185d` | "Peak" 颜色按钮 | 峰值虚线/帽色 |
| bgColor | `visual.bgColor` | `#00000000` 全透明 | （无 GUI，预留）| 背景色 |
| lineWidth | `visual.lineWidth` | 1.4 | "Line width" 滑块（0.5..5.0） | 主描边粗细 |
| opacity | `visual.opacity` | 1.0 | "Opacity" 滑块（0.05..1.0） | 全局不透明度 |
| transform.set | `transform.set` | false | （画布内首次拖拽自动置 true） | 元素自由变换开关；false=铺满画布（旧行为） |
| transform.centerX | `transform.centerX` | 0 | （画布拖拽） | 元素中心 X（输出分辨率像素） |
| transform.centerY | `transform.centerY` | 0 | （画布拖拽） | 元素中心 Y |
| transform.scaleX | `transform.scaleX` | 1.0 | （画布角/边柄拖拽） | X 轴缩放（非等比即拉伸） |
| transform.scaleY | `transform.scaleY` | 1.0 | （画布角/边柄拖拽） | Y 轴缩放 |
| transform.rotationDeg | `transform.rotationDeg` | 0.0 | （画布顶部圆柄拖拽） | 绕中心旋转角度（度） |
| barPitchRatio | `visual.barPitchRatio` | 1.0 | "Bar pitch %" 滑块（5..250%） | v0.5.4 #25：两柱锚点间距（×slot）；调 pitch 不动 width，gap 联动 |
| barGapRatio | `visual.barGapRatio` | 0.28 | "Bar gap %" 滑块（-248..248%） | #25 三联动：gap = pitch − width，**可为负（重叠）**；调 gap 不动 pitch，width = pitch − gap |
| barWidthRatio | `visual.barWidthRatio` | 0.72 | "Bar width %" 滑块（2..250%） | #25 三联动：柱宽（×slot）；调 width 不动 pitch，gap 联动 |
| drawGrid | `visual.drawGrid` | false | "Draw grid" toggle | 开关网格 |
| drawAxisLabels | `visual.drawAxisLabels` | false | "Axis labels" toggle | 开关坐标轴标签 |
| （非 param，仅 GUI 状态）| — | — | "Checkerboard BG" toggle | 预览画布是否画棋盘格（方便肉眼判断透明区，**不影响导出**）|

### 8.5.1 频谱蒙版图片（点前缀：`mask.`，v0.5.4，GUI 有"Choose mask image..."按钮）

> 图片只在"频谱轮廓"覆盖区可见（频谱=窗口/蒙版）。bar 逐柱（gap 不铺图）、line 整块。实现＝`SpectrumMask::compose`
> 读频谱 base 层 alpha 做像素蒙版（预乘安全）；`VisPipeline` 与 `SpectrumCanvas` 共用，预览即所得。

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| maskImage.enabled | `mask.enabled` | false | "Use spectrum mask" toggle | 蒙版总开关（空 path 时开关无效果） |
| maskImage.path | `mask.path` | "" | "Choose mask image..." 按钮 | 蒙版图片路径（设非空自动置 enabled=true） |
| maskImage.transform | `mask.{centerX,centerY,scaleX,scaleY,rotationDeg,posX,posY}` / `mask.reset` | set=false | 「Edit image position」模式内画布手柄 | 图片在 base 坐标的独立变换；`set=false`=**与其他图片图层一致：等比 contain 适配输出画布并居中**（v0.5.4 #3；与电平无关不漂移），设任一 key 即置 true。`mask.reset` 回默认 |
| maskImage.strokeEnabled | `mask.strokeEnabled` | false | "Outline (auto avg color)" toggle | 沿轮廓内侧勾边开关（选项 C） |
| maskImage.strokeWidth | `mask.strokeWidth` | 2.0 | "Outline width" 滑块（0.5..12） | 描边宽度（≈内侧环像素宽） |
| maskImage.strokeAutoColor | `mask.strokeAutoColor` | true | （设 `mask.strokeColor` 自动置 false）| 描边色=图片平均色 |
| maskImage.strokeColor | `mask.strokeColor` | `#ffffffff` | （CLI `--set`）| 手动描边色（auto 时忽略）|
| maskImage.brightness | `mask.brightness` | 1.0 | "Brightness" 滑块（0..2）| 蒙版图片亮度，1.0=原图（v0.5.4 #4）|
| maskImage.contrast | `mask.contrast` | 1.0 | "Contrast" 滑块（0..2）| 蒙版图片对比度，中灰 128 为轴，1.0=原图 |
| maskImage.saturation | `mask.saturation` | 1.0 | "Saturation" 滑块（0..2）| 蒙版图片饱和度，0=灰度，1.0=原图 |

> 图片随频谱**整体**拖动/缩放/旋转（几何在 base 空间，套用同一 `p.transform`）；「Edit image position」模式给图片**独立手柄**（角=等比、边=单轴拉伸、body=平移、顶圆=旋转，移动可吸附到频谱画框边/中线），点频谱框外自动退出编辑。

### 8.6 输出组（点前缀：`output.`，GUI 部分有控件）

| 字段 | 点路径 | 默认 | GUI / CLI | 说明 |
|---|---|---|---|---|
| width | `output.width` | 1280 | GUI "W" 输入（≥64） | 输出宽 px |
| height | `output.height` | 720 | GUI "H" 输入（≥64） | 输出高 px |
| encoder | `output.encoder` | `PngSeq` | GUI "Encoder" 下拉 | `png-seq`/`webm-vp9`/`mov-qtrle` |
| digits | `output.digits` | 6 | CLI `--digits N` | PNG 文件名零填充位数 |
| baseName | `output.baseName` | `frame_` | CLI `--base-name s` | PNG 文件名前缀 |
| outputDir | `output.outputDir` | `out_frames` | CLI 参数 + GUI "Browse" | 输出目录 |
| outputVideoPath | `output.outputVideoPath` | "" | （预留）| webm/mov 专用文件名 |
| ffmpegPath | `output.ffmpegPath` | "" | CLI `--ffmpeg path` | 空=自动搜索 PATH |
| bgCheckerboardPreview | `output.bgCheckerboardPreview` | false | CLI `--set output.bgCheckerboardPreview=true` | preview-frame 模式叠加棋盘格 |

### 8.7 音频

| 字段 | 点路径 | 默认 | 说明 |
|---|---|---|---|
| audioPath | `audioPath` | "" | CLI 第一个位置参数 / GUI 加载的音频路径 |

---

## 9. GUI ParamPanel 控件 ↔ SpectrumParams 字段映射

（后续 AI 若要"改某 GUI 控件"，直接对照此表即可找到参数字段 / 回调位置）

> **加控件的唯一姿势（v0.5.4 #G）**：`setBuildingTab (Tab::X)` 之后调 `addSlider/addCombo/addToggle/addButton`，
> 同行并排多个控件用 **`addRowGroup (label, { a, b, ... }, h)`**；需要跨页常驻的（页签行、底部导出条）
> 用 **`keepVisibleOnAllTabs (&comp)`**。三者都会自动进入 `rows`/`chrome`，因此切页显隐由 `resized()` 统一驱动。
> **不要**直接 `addAndMakeVisible` 就完事——那样会被安全网隐藏（`findUnownedChildren()` 非空，debug 下 jassert）。

| GUI section | GUI label | 控件类型 | 写入的字段 | 备注 |
|---|---|---|---|---|
| Style | Render style | Combo | params.style | 5 选项（`bar-mirror` 已于 #3.1 删除，工厂仍收该别名→bar） |
| Style | Band count | Slider (int) | params.bandCount | 16..512 |
| Style | Bar width % | Slider (int, %) | params.setBarWidth | 2..250 → ×slot；联动：pitch 不动 gap 变 |
| Style | Bar gap % | Slider (int, %) | params.setBarGap | -248..248；可为负=重叠；联动：pitch 不动 width 变 |
| Style | Bar pitch % | Slider (int, %) | params.setBarPitch | 5..250 → ×slot；联动：width 不动 gap 变 |
| Style | Bar width % | Slider (int, %) | params.barWidthRatio | 5..200 → 0.05..2；仅 bar 样式生效 |
| Style | Reset element transform | Button | params.transform = VisTransform{} | 复位元素变换；画布内双击元素同效 |
| Style | Freq scale | Combo | params.freqScale | 枚举 |
| Style | Min Hz / Max Hz | Slider (float) | params.minHz / maxHz | 最小会钳制最大 |
| Time | FPS | Combo | params.fps | 24/30/60 |
| Time | Attack ms | Slider (float) | params.attackMs | |
| Time | Release ms | Slider (float) | params.releaseMs | |
| Time | Peak hold ms | Slider (float) | params.peakHoldMs | |
| Time | Peak decay dB/s | Slider (float) | params.peakDecayDbPerSec | |
| Time | Temporal smooth | Slider (float) | params.temporalSmoothing | 0..1 |
| Dynamics | Y-axis curve | Combo | params.dynCurve | 枚举 |
| Dynamics | Gain | Slider (float) | params.dynGain | |
| Dynamics | Gamma | Slider (float) | params.dynGamma | |
| Dynamics | Slope comp (dB/oct) | Toggle | params.slopeEnabled | |
| Dynamics | Slope dB/oct | Slider (float) | params.slopeDbPerOct | |
| Appearance | Line width | Slider (float) | params.lineWidth | |
| Appearance | Opacity | Slider (float) | params.opacity | |
| Appearance | Draw grid | Toggle | params.drawGrid | |
| Appearance | Axis labels | Toggle | params.drawAxisLabels | |
| Appearance | Checkerboard BG | Toggle | **GUI-only**（写入 canvas.showCheckerboard，不落盘）|
| Appearance | Primary | Color picker | params.primaryColor | alpha 可改 |
| Appearance | Secondary | Color picker | params.secondaryColor | |
| Appearance | Peak | Color picker | params.peakColor | |
| Layers（Image 页）| **Layer stack（图层栈）** | **ListBox**（ParamPanel 兼任 `ListBoxModel`）| 只读展示 params.images / spectrumPresent / spectrumIndex / style / maskImage | v0.5.4 #H：顶→底列每层 + `[ok\|FILE MISSING\|NO DECODER]` + `scale x…`；点击行 → `onSelectLayerRow(tag)`：`layerTagMask(-2)`=切"编辑图片位置"、`layerTagSpectrum(-1)`=选频谱、`>=0`=选第 N 张图；内容哈希去抖 |
| Export | W / H | Int-only editors | params.width / height | ≥64（#G：与宽度框同属一行 `addRowGroup`） |
| Export | Encoder | Combo | params.encoder | PngSeq/MovQtrle(alpha)/WebmVp9(no alpha) |
| Export | Browse | Button | exportDir (GUI state, not param) | 设置输出目录 |
| Export | Export | Button | (后台 VisPipeline::run) | 按 Encoder 下拉选择导出；进度 % 显示在 progressLabel |
| Export | Export Video | Button | params.encoder→MovQtrle（若为 PngSeq）+ 自动填 outputVideoPath | 一键透明 MOV 导出；目录未设置会先弹 Browse |

---


*文档版本：v0.5.3  ·  最后更新：2026-09-09（v0.5.3 发版：锚定缩放 R·S 修复 / CAD 吸附辅助线 / 范围内外视觉区分 / 空格播放暂停）*
*维护者：AudioVisExport 项目（GPL-3.0）*
*协作规则：任何功能修改后，必须在 `docs/HISTORY.md` §2 变更日志追加一条，并在文档版本号处 bump。*
*发布规则：见文档开头「验证闸门规则」+「发布规则」+「文档地图」；变更日志在 `docs/HISTORY.md`。*
