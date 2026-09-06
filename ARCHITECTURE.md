# AudioVisExport — 工程架构文档

> **面向后续 AI 协作者**：本文档是工程交接文档，帮助你快速理解项目全貌、设计决策、代码结构和后续目标。读完本文档即可开始协作开发。注意：你后续推进任何操作以后都要写入并更新此文档。先读当前 ARCHITECTURE.md，和 GUI 源码实际内容交叉比对，避免“把推断当事实写入文档”。
>
> **文档版本 ↔ 项目版本映射（严格对齐）**：每次代码发布都要在此处更新版本行号，确保任意 AI 拿到文档后能准确定位到对应 tag / commit。
>
> | 文档 / 项目版本 | 日期 | tag（可）| 里程碑 |
> |---|---|---|---|
> | v0.5.0 | 2026-09-06 | —（工作区待提交）| **Windows 原生 exe 交叉编译打通**（Clang 18 + xwin + lld-link-18）+ UIPI 拖放彻底修复（WM_DROPFILES 兼容层）+ Colors 4 按钮免滚动（新增 BG 色）+ 峰值帽参数上移 Style 区 + bar-line 斜面柱顶重构 |
> | v0.4.2 | 2026-09-05 | —（工作区未提交，tag 待补）| 图片图层系统 + bar-line 样式 + 峰值帽开关 + 拖放 UIPI 诊断 + Windows→WSL 迁移（文档/环境）+ CLI UTF-8 修复 + 版本号同步至 v0.4.2 |
> | v0.4.1 | 2026-09-05 | `v0.4.1`（推荐打 tag）| 频谱元素自由变换（拖动/缩放/拉伸/旋转）+ 两段式合成渲染，导出所见即所得 |
> | v0.4.0 | 2026-09-03 | `v0.4.0`（推荐打 tag）| GUI 新增一键 "Export Video" 按钮（默认透明 MOV QTRLE）；PngSequenceEncoder 实现编码器（MOV QTRLE alpha 实测 argb / WebM VP9 实测无 alpha）；视频模式自动补全输出路径；新增 docs/GUI_GUIDE.md 使用说明 |
> | v0.3.2 | 2026-09-02 | `v0.3.2`（推荐打 tag）| ARCHITECTURE.md GUI 章节重写：精确成员字段、类成员同步流程、导出线程原子轮询、面板回调、已知限制 |
> | v0.3.1 | 2026-09-02 | `v0.3.1`（已 push）| GUI 上线 + 全英文 UI + 4 种 style + 双 target |
> | v0.3.0 | 2026-09-02 | `v0.3.0`（已 push）| Engine + 4 styles + CLI + GUI skeleton |
>
> 协作时：若要修改功能，请先新建分支；文档末尾版本号 / 变更日志必须与代码 commit 同步更新。
>
> **发布规则（强制，2026-09-05 起）**：每次小版本更新（版本号任一位变化）必须完成——
> ① §12 变更日志写好对应条目；② 单独撰写**更新公告**（Release Notes，写入 `docs/RELEASE_NOTES.md`
> 并作为 GitHub Release 正文）；③ commit + push 到 `origin`（GitHub）；④ 打对应版本 tag 并 push。

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
| **音频输入**（`PcmSource`）| WAV (PCM), AIFF | JUCE `AudioFormatManager::registerBasicFormats()`, 单/多声道（多声道自动转立体声）。FLAC/MP3/OGG 未启用，因为 JUCE 核心未含第三方解码库。**如需支持需自行加 mp3/ogg/flac 插件**（`AudioFormatManager::registerFormat`）|
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
| L1 | **音频输入格式仅 WAV/AIFF**（MP3/FLAC/OGG/M4A 未启用）| 无法拖入 FLAC 等 | `PcmSource::load` 扩展：在 `formatManager.registerBasicFormats()` 之后注册 `OggVorbisAudioFormat / FlacAudioFormat / MP3AudioFormat`（JUCE 需要 `juce::ogg_vorbis` / `juce::flac` 模块；MP3 需 `dr_mp3` 格式）|
| L2 | **GUI 仅单音频替换，不支持 playlist 或 multi-clip timeline** | 每次 loadFile 会释放旧 readerSource + transport.setSource(nullptr) | 未来 timeline editing（ARCHITECTURE.md §7.2）|
| L3 | **参数改动不回放历史 PCM** → 改参后，曲线需要 `attackMs+releaseMs` 秒才能稳定 | N/A（设计决策：避免重新解码全音频）| 若用户要"立即到达对应视觉稳态"，可在 advanceCoreTo 内部跳过前 N 帧不渲染 |
| L4 | **多 pass 合成目前在单 Graphics 上叠加**，CrystalStyle::renderPass 的 glow 层没有真 blur | 水晶效果 bloom 是多层粗描边近似而非 GaussianBlur | ARCHITECTURE.md §7.4 多 pass 扩展：每 pass 到独立 Image + juce::ImageEffectFilter GaussianBlur |
| L5 | **导出时 GUI 播放引擎不暂停**：后台线程开独立的 `VisPipeline::run` 再次解码同一个音频文件（当前是 OK 的，因为音频只读）| CPU 峰值略高 | 可加 if (exporting) transport.stop(); exportDone exchange 后可选恢复 |
| L6 | **面板 paramsDirty 粒度是"任意字段修改即重建 core/style 全量"**：改颜色/画网格不需要重建 core，只用重绘 | 轻微性能浪费（30fps 下无感，但 60fps + 512 band 时会有影响）| 加细分 dirty flag：`dirtyEngine / dirtyStyle / dirtyRepaintOnly` |
| L7 | **GUI 导出目录不落盘（仅本次会话有效）**：下次启动需重新选 | N/A | 加 `juce::ApplicationProperties` + OptionsPage 记忆最后导出目录 |
| L8 | **颜色选择弹层的 OK/Cancel 不是显式按钮**：JUCE ColourSelector 是实时 change broadcaster，点外部关闭后最后一次选择的颜色立即生效（但如果用户"后悔"，没有 undo）| UX | 加 "Preset colours" combos 与 "Reset to default" 按钮 |
| L9 | **minDb/maxDb/bgColor 没有 GUI 控件**（参见 §10 表中标注"无 GUI，预留"的 3 个字段）| 改 minDb/maxDb 必须用 CLI `--set dynamic.minDb=-96` 或 JSON | 补 2 个 Slider + 1 个 ColourPicker 到 Appearance section 末尾 |
| L10 | **进度条 seekBar 没有播放头刻度样式**，只是标准 LinearHorizontal Slider | UX | 自定义 LookAndFeel method：drawLinearSlider 画一个带圆角的轨迹 + 拖动圆点 |



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

---

## 6. 已完成的里程碑

| 步骤 | 内容 | 状态 |
|------|------|------|
| Step 1 | 骨架可编译（删 projectM、CMake、空骨架、CLI、VisPipeline） | ✅ |
| Step 2 | SpectrumCore 算法实现（双路 FFT + band mapping + 平滑 + 峰值） | ✅ |
| Step 3 | Y2KLineStyle 完整实现（Catmull-Rom 平滑曲线 + 填充 + 描边 + 峰值） | ✅ |
| Step 4 | 透明背景验证（ARGB PNG alpha=0，PR 可叠加） | ✅ |
| Step 5 | 真实音频端到端测试（PUPA 30s 完整导出 + WebM alpha） | ✅ |
| Step 6 | 参数对比验证（6 组 10s 不同参数 MP4） | ✅ |
| Step 7 | BarStyle 完整实现（柱状图 + 渐变填充 + 峰值帽） | ✅ |
| Step 8 | PolylineStyle 完整实现（折线 + 填充 + 描边 + 峰值） | ✅ |
| Step 9 | CrystalStyle 完整实现（3-pass 水晶效果：辉光 + 玻璃体 + 高光） | ✅ |
| Step 10 | AudioVisGUI 实时预览界面（拖入音频 + 播放同步 + 参数面板 + 一键导出） | ✅ |

---

## 7. 后续目标

### 7.1 短期（样式 + 效果增强）
- [ ] CrystalStyle v2：真 GaussianBlur（`juce::ImageEffectFilter`）替换多层粗描边模拟 bloom
- [ ] CrystalStyle v2：折射/色散效果（RGB 通道分别偏移）
- [ ] ColorMap 实现：`gradient`（按强度上色）+ `rainbow`（全频段彩虹）
- [ ] 样式组合：支持同一帧叠加多个样式（如 crystal + bar 底层）

### 7.2 中期（编码 + 工作流）
- [x] MOV QTRLE alpha 编码器内置（PngSequenceEncoder::finalizeAndMux，v0.4.0；实测 argb 无损保留）
- [x] 编码器实测校准：WebM VP9 的 yuva420p 在主流 ffmpeg 构建中**实际丢 alpha**（v0.4.0 实测降级为 yuv420p），故 WebM 定位为"无 alpha 小体积预览片"；若未来获得开启 VP9-alpha / VP8-alpha 的 ffmpeg 构建可复刻 alpha
- [ ] 时间轴编辑界面（JUCE GUI）：timeline-based 多段频谱图编辑
- [ ] 实时预览窗口（JUCE GUI + OpenGL 或 软件渲染）

### 7.3 长期（大工程衔接）
- [ ] 将 `source/core/` 提取为独立静态库，供剪辑软件引用
- [ ] 剪辑软件 UI：SpectrumParams 暴露为滑块组，满意后导出 preset.json
- [ ] 多轨道支持：每个轨道独立 style + params，时间轴可裁剪/拼接
- [ ] 粒子系统：沿曲线流动的光斑/粒子（延伸 multi-pass 架构）
- [ ] 音频实时输入支持（从离线管线扩展为实时管线）

### 7.4 多 pass 架构扩展路
CrystalStyle 已验证 `getNumPasses() / renderPass()` 机制可用。后续扩展方向：
- **真多图层合成**：VisPipeline::renderFrame() 检测 `getNumPasses() > 1`，每 pass 渲染到独立 ARGB Image，用 blend mode 合成（支持 blur / color-dodge / screen 等）
- **滤镜链**：每个 pass 可附加 ImageEffectFilter（GaussianBlur / DropShadow / InnerShadow）
- **动画化 pass 参数**：pass 的透明度/粗细随时间变化（如辉光呼吸效果）

---

## 8. 关键设计决策记录

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

## 9. 常见问题

**Q: 为什么绝对 dB 值偏低（1kHz -6dBFS 音显示 -35dB）？**
A: 不是 bug。FFT 归一化与 Y2Kmeter 逐字节一致（`2.0f / fftSize`）。偏低是 bin 间泄漏 + band 取值所致。用户明确"可视化工具不需严格纵轴精度"。

**Q: 导出的 PNG 导入 PR 后透明区显示黑色？**
A: PR 需要带 alpha 的视频格式。实测最可靠：**MOV QTRLE（rgba / argb）** 无损保留 alpha；
   WebM VP9 的 `yuva420p` 在主流 ffmpeg（含 gyan.dev 2026 发布版）中实际不编码 alpha，会静默降级为 `yuv420p`。建议透明素材用 MOV QTRLE 或 PNG 序列。

**Q: 构建报错找不到 JUCE？**
A: 需指定 `-DAVX_USE_LOCAL_JUCE=ON`，或确保 `third_party/JUCE/` 存在。沙箱环境无法从 GitHub clone。

**Q: ffmpeg 子进程卡死？**
A: 不能先 wait exit 再 drain stdout/stderr（会死锁）。PngSequenceEncoder::finalizeAndMux 用 200ms 轮询 + 增量读取。

---

## 10. 完整可调参数表（~50 个，供 AI 快速查询）

**参数注入顺序**（低优先级 → 高优先级，后者覆盖前者）：
```
SpectrumParams.h 默认值
  → --config preset.json (via fromJson)
    → CLI sugar flags (--style, --band-count, etc.)
      → CLI --set dotted.key=value (按出现顺序)
        → GUI ParamPanel 即时改动
```

### 10.1 FFT 组（点前缀：`fft.`，CLI 未暴露糖命令，仅 `--set` 可调）

| 参数（SpectrumParams 字段）| 点路径 | 默认 | 范围 | 说明 |
|---|---|---|---|---|
| fftOrder | `fft.fftOrder` | 11 (size=2048) | 8..14 | 主路 FFT 阶数（size = 1<<order）|
| fftOrderLo | `fft.fftOrderLo` | 13 (size=8192) | 11..15 | 低频增强路 FFT 阶数 |
| enableLowFreqPath | `fft.enableLowFreqPath` | true | bool | 开关低频增强路 |
| crossoverHz | `fft.crossoverHz` | 500.0 | 100..2000 | 高低路 crossfade 频率 |
| windowFunc | `fft.windowFunc` | Hann | Hann / Hamming / Blackman / BlackmanHarris / Rectangular | 加窗函数 |
| hopSize | `fft.hopSize` | 0 (no overlap) | 0..fftSize-1 | 主路 hop（0=no overlap, Y2K 原默认）|
| hopSizeLo | `fft.hopSizeLo` | 0 (= fftSize/4) | 0..fftSizeLo-1 | 低频路 hop |

### 10.2 频率映射组（点前缀：`freq.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| bandCount | `freq.bandCount` | 160 | "Band count" 滑块（16..512） | 频带数量（越大视觉越密）|
| freqScale | `freq.freqScale` | `Log` | "Freq scale" 下拉 | `log`/`linear`/`mel`/`bark` |
| minHz | `freq.minHz` | 20.0 | "Min Hz" 滑块（20..2000）| 横轴左端频率 |
| maxHz | `freq.maxHz` | 20000.0 | "Max Hz" 滑块（1000..20000）| 横轴右端频率 |

### 10.3 时间响应组（点前缀：`time.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| fps | `time.fps` | 30.0 | "FPS" 下拉（24/30/60） | 输出帧率 |
| attackMs | `time.attackMs` | 30.0 | "Attack ms" 滑块（5..500） | 上升沿时间常数（越小越"跳"）|
| releaseMs | `time.releaseMs` | 200.0 | "Release ms" 滑块（20..2000） | 下降沿时间常数 |
| peakHoldMs | `time.peakHoldMs` | 3500.0 | "Peak hold ms" 滑块（0..10000） | 峰值保持期 ms |
| peakDecayDbPerSec | `time.peakDecayDbPerSec` | 12.0 | "Peak decay dB/s" 滑块（1..60） | 峰值衰减速度 |
| temporalSmoothing | `time.temporalSmoothing` | 0.5 | "Temporal smooth" 滑块（0..1） | 列间 [1:2:1] 模糊强度 |

### 10.4 动态强度组（点前缀：`dynamic.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| dynCurve | `dynamic.curve` | `LinearDyn` | "Y-axis curve" 下拉 | `linear`/`sqrt`/`loglog`/`perceptual` |
| dynGain | `dynamic.gain` | 1.0 | "Gain" 滑块（0.2..3.0） | 乘到 (db-minDb)/range：>1=更跳 |
| dynGamma | `dynamic.gamma` | 1.0 | "Gamma" 滑块（0.3..3.0） | pow(norm, gamma)：gamma<1=压低端整体抬 |
| slopeEnabled | `dynamic.slopeEnabled` | true | "Slope comp" toggle | 开关斜率补偿 |
| slopeDbPerOct | `dynamic.slopeDbPerOct` | 4.5 | "Slope dB/oct" 滑块（0..12） | 每八度斜率补偿（粉红噪声更均匀）|
| minDb | `dynamic.minDb` | -80.0 | （无 GUI，预留）| 纵轴底 |
| maxDb | `dynamic.maxDb` | 0.0 | （无 GUI，预留）| 纵轴顶 |

### 10.5 视觉 / 外观组（点前缀：`visual.`，GUI 有控件）

| 字段 | 点路径 | 默认 | GUI 控件 | 说明 |
|---|---|---|---|---|
| style | `visual.style` | `y2k-line` | "Render style" 下拉 | `y2k-line`/`bar`/`polyline`/`crystal` |
| colorMap | `visual.colorMap` | `solid` | （预留，下拉骨架）| 未来 gradient/rainbow |
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
| barGapRatio | `visual.barGapRatio` | 0.28 | "Bar gap %" 滑块（0..100%） | bar 样式：柱间空隙占 slot 比例 |
| barWidthRatio | `visual.barWidthRatio` | 1.0 | "Bar width %" 滑块（5..200%） | bar 样式：柱宽占 (slot-gap) 比例，>1 相邻柱重叠 |
| drawGrid | `visual.drawGrid` | false | "Draw grid" toggle | 开关网格 |
| drawAxisLabels | `visual.drawAxisLabels` | false | "Axis labels" toggle | 开关坐标轴标签 |
| （非 param，仅 GUI 状态）| — | — | "Checkerboard BG" toggle | 预览画布是否画棋盘格（方便肉眼判断透明区，**不影响导出**）|

### 10.6 输出组（点前缀：`output.`，GUI 部分有控件）

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

### 10.7 音频

| 字段 | 点路径 | 默认 | 说明 |
|---|---|---|---|
| audioPath | `audioPath` | "" | CLI 第一个位置参数 / GUI 加载的音频路径 |

---

## 11. GUI ParamPanel 控件 ↔ SpectrumParams 字段映射

（后续 AI 若要"改某 GUI 控件"，直接对照此表即可找到参数字段 / 回调位置）

| GUI section | GUI label | 控件类型 | 写入的字段 | 备注 |
|---|---|---|---|---|
| Style | Render style | Combo | params.style | 4 选项 |
| Style | Band count | Slider (int) | params.bandCount | 16..512 |
| Style | Bar gap % | Slider (int, %) | params.barGapRatio | 0..100 → 0..1；仅 bar 样式生效 |
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
| Export | W / H | Int-only editors | params.width / height | ≥64 |
| Export | Encoder | Combo | params.encoder | PngSeq/MovQtrle(alpha)/WebmVp9(no alpha) |
| Export | Browse | Button | exportDir (GUI state, not param) | 设置输出目录 |
| Export | Export | Button | (后台 VisPipeline::run) | 按 Encoder 下拉选择导出；进度 % 显示在 progressLabel |
| Export | Export Video | Button | params.encoder→MovQtrle（若为 PngSeq）+ 自动填 outputVideoPath | 一键透明 MOV 导出；目录未设置会先弹 Browse |

---

## 12. 变更日志（每发版必更，同步文档末尾版本号）

> **发版流程（每次小版本都要走完）**：
> 1. 本 §12 顶部新增版本条目（变更内容 + 验证记录）
> 2. 撰写**更新公告** → `docs/RELEASE_NOTES.md`（用户视角：新功能 / 修复 / 升级注意事项）
> 3. `git add -A && git commit`，commit message 以版本号开头
> 4. `git push origin main` + 打 tag `git tag vX.Y.Z && git push origin vX.Y.Z`
> 5. 在 GitHub 用 `docs/RELEASE_NOTES.md` 内容创建 Release
>
> （2026-09-05 注：当前环境 git 访问 GitHub 实测 TLS 中断，push 需在网络恢复/代理就绪后补做。）

### v0.5.0 — 2026-09-06
**变更（Windows 原生交付 + UIPI 拖放修复 + GUI/样式增强；实施计划见 `docs/PLAN_v0.5.0.md`）**：

- **Windows 原生 exe 交叉编译打通（§5 新章节）**：
  - 工具链：Clang 18（MSVC ABI，`scripts/clang-cl-wrapper.sh` 包 `--target=x86_64-w64-windows-msvc`）
    + lld-link-18 + xwin 拉取的 Windows SDK 10.0.22621 / MSVC CRT 14.44（`~/.xwin-sysroot/`）
  - 新增 `cmake/toolchains/clang-cl-msvc.cmake`（旧 MinGW 版失效原因：缺 d2d1_2/3.h，JUCE 8 Direct2D 无法编译）
  - 新增 `scripts/build_win_cross.sh`（环境检查 / configure / build / 验证 / `--deploy` 到 Windows 测试目录 / `--clean`）
  - CMakeLists.txt：`avx_configure_target` 兼容 Clang-MSVC 前端；CRT 静态链接免 VC_redist（依赖校验仅系统 DLL）
  - **修复 JUCE 源残留**：恢复 `juce_graphics.cpp` 的 `d2d1_3.h` 等 6 处 MinGW 时期补丁（共享 checkout 回到原版 8.0.12）
  - 排错记录：STL1000 版本检查（`_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH`）、大小写符号链接（Dbghelp.h/D2d1.lib 等）、
    SSE intrinsics 不内联（clang resource dir `-isystem` 首位）——三个坑的完整分析在 §5 交叉编译章节
- **UIPI 拖放彻底修复（`source/gui/WinDragCompat.{h,cpp}` 新增）**：
  - 根因链：JUCE 无条件 `RegisterDragDrop` → Explorer 只走 OLE → 提权进程被 UIPI 拦截 → 禁止符不回退
  - 修复：提权进程 `RevokeDragDrop` + `DragAcceptFiles` 切换 WM_DROPFILES 老协议（JUCE 已 `ChangeWindowMessageFilterEx`
    放行该消息，官方允许跨 UIPI）；`SetWindowSubclass` 拦截 WM_DROPFILES → 转 `ComponentPeer::DragInfo`
    → 复用 JUCE `handleDragDrop` 分发链（画布/兜底/图层逻辑零改动）
  - 普通权限下 OLE 全功能保留（含拖放悬停 HUD）；画布红色横幅改为提示"legacy drag-drop active"
- **GUI 参数面板（ParamPanel）**：
  - Colors 行（Primary/Secondary/Peak/**新增 BG**）从 Appearance 末尾上移到区首（原位置 y≈852px 超首屏需滚动）
  - BG 按钮接入 `params.bgColor`（JSON/CLI 原已支持，补 GUI 控件）；透明底显示为半透明灰以免不可见
  - "Peak hold ms" / "Peak decay dB/s" 两滑块从 Time 区上移到 Style 区 Peak caps 开关之后（峰值帽三件套聚合），
    加 tooltip 说明"下落间隔/下落速度"语义；面板新增 TooltipWindow
- **bar-line 样式重构（斜面柱顶，`BarLineStyle.{h,cpp}` 重写）**：
  - 柱体从矩形改为五边形梯形：顶边斜线（柱左缘高 = 相邻带归一化中点插值，右缘同）；同高带 → 水平顶
  - 首柱左缘/末柱右缘用自身值；空带柱不画但边缘插值仍参与（无悬空跳变）；gap 保留（间隙断开，用户确认）
  - 删除旧"柱顶中点连线"与水平描边；峰值帽逻辑不变
  - 像素级验证：扫频音频下 bar-line 顶缘平台占比 26% vs bar 84%（斜面连续 vs 全平顶），柱宽/空隙序列与 bar 逐位一致
- **验证记录**：Linux 双 target 编译零错误 + GUI 冒烟通过；交叉编译全量通过并已部署
  `C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`（COMCTL32 导入确认，无 CRT DLL 依赖）；
  Windows 侧 UIPI 拖放 / 颜色 / bar-line / 峰值帽实测待用户回归（S6）

**追加（v0.5.0 同日，实测反馈修复）**：
- **拖放实测结论 + 修复升级**：用户实测提权下禁止符消失但 WM_DROPFILES drop 无反应
  → 方案升级为两层：①**首选 = 自动降权重启**（`relaunchDeElevated()`，经 explorer.exe 代理
  重启为 Medium 完整性级别，OLE 拖放完整可用含 HUD；`initialise` 提权时自动触发，旧实例 `quit()`）；
  ②兜底 = WM_DROPFILES relay 保留（降权失败时生效），新增 `onStatus` 诊断回调 → 面板进度文本
  实时显示"WM_DROPFILES arrived / relaying / delivered"每一步（远程排障可见）
- **峰值帽斜面化（BarLineStyle v0.5.1 增强）**：每带形状捕获状态机——峰值未在下落时每帧
  重置帽形状 = 当时柱顶斜率（与柱顶贴合）；下落开始冻结形状、整条斜线随 peakDb 刚性下落。
  像素验证：保持期/下落期 peak 线全部呈斜线段（0 平段）
- **暂停峰值抖动修复（SpectrumCore::setPeaksFrozen）**：根因 = getBandFrame 峰值段是有状态的
  （GUI 暂停时 timer 仍 30Hz 调用 → hold 倒计时/衰减随 wall-clock dt 持续推进 → 峰值帽继续下落）。
  修复 = 冻结标志跳过衰减（保留新峰跟随，seek 快进峰值建立不受影响）；MainComponent 每 tick
  按 `transport.isPlaying()` 设置；离线导出默认不冻结零影响
- **第二轮实测反馈修复（同日）**：
  - **拖放方案再升级**：`installDragCompat` 改为**无条件 DragAcceptFiles**（假设 B：OLE 注册失败时
    Explorer 自动回退 WM_DROPFILES，此前仅提权分支启用是漏洞）；relaunchDeElevated 加
    `--avx-deelevated` 防死循环标志（UAC 最低档环境下降权必然失败，防无限重启）；画布右下角
    常显诊断行 `[drag] elevated= oleRevoked= fallback=on wmDropFiles=N`（不靠滚动找）
  - **bar-line 峰值帽状态机 v2**（修复用户实测"看不到峰值帽"）：v1 两缺陷——帽端点画在柱宽内
    与柱顶描边重叠视觉淹没；保持期每帧重捕获导致下落前冻结的形状≈水平。v2：仅峰值刷新
    （peakDb 上升）时捕获形状；帽 x 伸出 gap 两侧 + 斜率线性外推。像素验证：下落期帽悬停
    柱顶上方 9-11px、斜线保持
  - **音频文件管理（用户需求）**：传输条新增 `[Load...]`（随时换曲）/ `[Eject]`（移除当前音频：
    停止播放 + transport/readerSource/pcm 全清 + 画布回空拖放提示态）；加载后显示
    `♪ 文件名`（左下角 nowPlayingLabel，悬停 helpText 显示完整路径）
  - **拖放 relay 链最终修复（T3/T4 实测驱动）**：T3 证实 explorer.exe 代理**无法传命令行参数**
    （args 里的 --avx-deelevated 被 explorer 当作导航位置解析，打开 Documents 而非程序）→
    改用**环境变量 AVX_DEELEVATED** 传标志（ShellExecuteW 子进程继承环境块）；T4 证实
    WM_DROPFILES 到达子类（wmDropFiles+1）但 JUCE `handleDragMove/findDragAndDropTarget`
    分发链跨协议 relay 不可靠（静默吞 drop）→ relayDrop 改为**直连
    onNativeFilesDropped 回调**（同消息线程确定性交付：图片→图层 / 音频→loadFile / 其他→提示），
    JUCE 链降为后备。T2 对照同时证实：非管理员禁止符已消失（无条件 DragAcceptFiles 生效，
    Explorer 全程走 WM_DROPFILES —— 即 OLE RegisterDragDrop 在该环境静默失败，与杀软无关）

### v0.4.2 — 2026-09-05
**变更（图片图层系统 + bar-line 样式 + 拖放诊断 + 峰值帽开关）**：

- **外部拖放问题定性（UIPI 提权隔离，非程序 bug）**：
  - 深入排查结论：代码侧 OLE 注册（`RegisterDragDrop` 无条件调用）与目标命中逻辑均正常；
    禁止符的真因是 **Windows UIPI**——以管理员身份运行的进程（GUI 主进程完整性级别 High）
    **收不到普通权限 Explorer（Medium）发起的 OLE 拖放**，整条拖放消息链被内核静默拦截，
    表现恰好是全程禁止符（`ChangeWindowMessageFilterEx` 只放行指定消息，OLE 拖放链不在放行之列）。
  - **程序内新增两个诊断装置**（`SpectrumCanvas`）：
    - 启动时检测提权状态（`CheckTokenMembership`/`IsUserAnAdmin` 路径）：若进程为 Elevated，
      画布顶部显示**红色警告横幅**"Running as Administrator — file drag-drop from Explorer is blocked by Windows (UIPI)"
    - 拖放悬停 HUD：拖文件进入窗口时画布显示"Drag detected: <文件名>"——
      若能看到 HUD 说明消息链已通（此时禁止符不存在）；看不到 HUD + 禁止符 = UIPI 拦截
  - **用户自查步骤**：任务管理器 → 详细信息 → 添加"Elevated"列 → 看 AudioVisGUI.exe 是否 Elevated；
    或直接看画布是否出现红色横幅。**解决：不要以管理员身份运行 GUI**（也不要从提权终端 start 它）
- **图片图层系统（第一阶段：静态图片）**：
  - `SpectrumParams::ImageLayer`（namespace 级结构，`images` 数组成员）：path / centerX / centerY /
    scaleX / scaleY / rotationDeg / opacity / aboveSpectrum / visible，复用 `VisTransform`
  - JSON 持久化（`images: [ {...} ]`，像素坐标空间与频谱变换一致）；`--set` 暂不支持数组元素
  - **VisPipeline 合成**：belowSpectrum 组 → 频谱 → aboveSpectrum 组，图片默认铺满输出画布再套
    `VisTransform`（与频谱同基准），导出与预览共用同一合成函数（所见即所得）
  - **画布交互**：拖图片文件进画布即创建图层（默认铺满）；点击切换选中（频谱/图片）；
    选中图片后与频谱共用同一套 8 角柄/旋转柄/移动/双击复位交互；按图层 z 序做命中测试
  - **面板新增 Layers 区**：[Add image]（文件选择）/ [Up] / [Down] / [Remove] / Opacity 滑块
  - CLI `--preview-frame` 与导出全链路验证：img_on/img_off 对照字节不同 ✅
- **新增样式 `bar-line`**（柱体 + 柱顶直线连接）：
  - `styles/BarLineStyle.{h,cpp}`：逐柱画柱体（沿用 bar 的 gap/width 布局参数），
    相邻柱顶点用直线段连接（可选择是否带填充），峰值帽同样受 barParticles 开关控制
  - 工厂注册 + CMake + 面板下拉第 5 项；CLI 渲染验证 ✅
- **`barParticles` 参数（柱样式"粒子"开关，默认开）**：
  - 用户所称"粒子"实为**峰值帽**（peak caps，随峰值缓慢下落的小横线），命名沿用引擎术语
  - `visual.barParticles`：JSON 写/读/CLI `--set` 三处贯通；GUI Appearance 区新增
    **"Peak caps"** 开关；BarStyle 与 BarLineStyle 双双接入；开/关对照渲染字节不同 ✅
- **颜色选择器确认未丢失**：Primary/Secondary/Peak 三色按钮 + 取色器一直在 Appearance 区
  （面板变长后需要**滚动**才能看到）；在 GUI 指南中注明位置
- **修复**：`SpectrumCanvas::fileDragExit` 签名对齐 JUCE 基类（`const StringArray&`）；
  `SpectrumParams::ImageLayer` 限定名统一；`isProcessElevated` 补 `<windows.h>`；
  ParamPanel 重复的 `addHeader("Export")` 去重；MainComponent `buildRp` 补接 `rp.barParticles`
  （否则 GUI 预览峰值帽永远关闭、与导出不一致）
- **CLI UTF-8 修复 + 版本号同步**：
  - 根因：`juce::String(const char*)` 按 ASCII 处理 UTF-8 中文，多字节被逐字节重编码成双重编码乱码
    （Release 下 jassert 被禁用无提示）；正确做法是用 `CharPointer_UTF8` 包装
  - `source/cli/CliArgs.cpp` helpText() 改用 `juce::String(juce::CharPointer_UTF8(...))`；
    版本号从 `v0.2.0` → `v0.4.2`（与文档一致）
  - `CMakeLists.txt`：`project(VERSION)` 与两个 target 的 `VERSION` 全部从 `0.2.0`/`0.3.0` 同步到 `0.4.2`
  - **已重新交付** Windows 测试包（`C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\` 内二进制更新时间戳）
- **验证记录**：双 target 构建零错误；CLI 四组对照（bar-line / 图层 on-off / 峰值帽 on-off）
  全部通过；GUI 启动冒烟通过；CLI `--help` 中文版本号显示正确（UTF-8 修复验证）

**追加（v0.4.2 同日）— Windows → WSL Ubuntu 22.04 迁移（仅文档/环境，无功能代码变更）**：
- **工程迁移**：`d:\Study 'n' Work\Program\AudioVisExport` → `~/CodingProgram/AudioVisualizer/AudioVisExport`；
  Y2Kmeter 现位于同级 `~/CodingProgram/AudioVisualizer/Y2Kmeter`
- **文档路径修正**：
  - `ARCHITECTURE.md`：§2.1 Y2Kmeter 参考路径、§3 产物路径（Linux 无 `.exe`）、
    §4.5.7 启动命令、§5 构建章节整体重写为 bash + Ninja（含 WSL 依赖一键安装命令 + 本地 JUCE 说明）、
    §6 手动 ffmpeg 命令代码块、示例输出路径 `D:\...` → `<dir>`
  - `docs/GUI_GUIDE.md`：§1 构建与启动重写为 WSL/Linux（apt 依赖、Ninja 构建、WSLg 说明）、
    §3.6 UIPI 节标注"仅 Windows"、FAQ Q1 的 FFMPEG_PATH 说明按平台区分
- **third_party/JUCE 符号链接重建** → `../../Y2Kmeter/third_party/JUCE`
  （复用 Y2Kmeter 的 JUCE 8.0.12 本地 checkout；构建需 `-DAVX_USE_LOCAL_JUCE=ON`，见 §5/FAQ）
- **Windows MSVC 旧 `build/` 已删除**（`.sln/.vcxproj` 与指向 `d:/` 的 CMakeCache 对 Linux 无效），Linux 下重新 configure
- **代码跨平台核查结论（无需改动）**：
  - `SpectrumCanvas::isProcessElevated` 与提权横幅已有 `#if JUCE_WINDOWS` 守卫——非 Windows 恒 false，横幅不显示
  - `PngSequenceEncoder::findFfmpeg_` 的 PATH 分隔符（`;` vs `:`）、可执行名（`ffmpeg.exe` vs `ffmpeg`）、
    `.exe` 后缀补全逻辑全部已分平台处理，Linux 下开箱即用
- **环境补齐（WSL 侧，一次性）**：apt 安装 cmake / ninja-build / ffmpeg / pkg-config +
  JUCE Linux 依赖（libasound2-dev、libx11-dev、libxext-dev、libxrandr-dev、libxinerama-dev、
  libxcursor-dev、libxcomposite-dev、libxrender-dev、libfreetype6-dev、libfontconfig1-dev、
  libgl1-mesa-dev），完整命令见 §5
- **运行时形态**：GUI 显示走 WSLg（`DISPLAY=:0` / `WAYLAND_DISPLAY=wayland-0`），
  音频走 WSLg PulseAudio；ffmpeg 用 apt 版（Windows 期 gyan.dev 下载包的说法仅适用于旧环境）
- **验证记录（2026-09-05 WSL 构建，Ninja + 本地 JUCE 8.0.12）**：
  - 双 target 编译链接零错误（⚠️ 实测 12 核 / 7.7GB 内存下 `-j$(nproc)` 会在 JUCE 大编译单元上
    OOM（`cc1plus Killed`），须 `-j2` 续编；已写入 §5 构建命令备查）
  - CLI 冒烟全过：`--gen-tone`（44.1k 立体声 5s 1kHz -6dBFS）→ `--probe-pcm` →
    `--probe-spectrum` → `--preview-frame`（crystal / bar-line 两样式出图）→
    `--export` png-seq（480x270@15，75 帧 / 0.9s）
  - `--export --encoder mov-qtrle` 全链路：Linux PATH 自动找到 apt ffmpeg 4.4.2，
    产物 ffprobe 实测 `codec=qtrle / pix_fmt=argb`（alpha 无损保留，与 Windows 期结论一致）
  - GUI 冒烟：WSLg 下启动运行正常（timeout 8s 退出码 124 = 全程存活；
    `ALSA seq` 提示为 WSL 无 MIDI 设备，无害）
  - **已交付 Windows 测试包**：`C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`
    （AudioVisExport / AudioVisGUI 二进制 + test_tone.wav + sample_mov/test_tone_vis.mov
    + Run_AudioVisGUI.bat + README_测试说明.txt；bat 经 `wsl -e` 拉起，WSLg 显示窗口）

### v0.4.1 — 2026-09-05
**变更（频谱元素自由变换 + 输出分辨率所见即所得）**：
- **新增核心变换类型 `VisTransform`（`source/core/VisTransform.h`）**：
  - 坐标空间 = 输出分辨率像素；`set/centerX/centerY/scaleX/scaleY/rotationDeg`
  - `buildVisAffine()`：绕中心（平移→旋转→缩放→平移回中心），GUI 与导出共用同一仿射；
    `visCorners() / visContains() / visDistanceToSegment()` 供画布命中测试与手柄绘制，
    后续图片/视频图层可直接复用
- **`SpectrumParams` 新增 `transform` 成员**：JSON 序列化/解析（`transform.*` 键）+
  CLI `--set transform.centerX=640` 等覆盖
- **`VisPipeline::renderFrame` 改为两段式合成**：
  1) 基础层：频谱按输出分辨率渲染到透明 ARGB（不带变换）
  2) 合成层：棋盘/半透明 bg + `buildVisAffine(transform)` 叠加 → **导出应用与预览相同的变换**，所见即所得
- **`SpectrumCanvas` 全重写为"元素式"交互画布**：
  - 基础层固定渲染到 `params.width×params.height`，画布显示区按 letterbox 居中适配
  - 鼠标交互：拖动主体 = 移动；四角手柄 = 等比缩放；四边手柄 = 单轴拉伸（非等比）；
    顶部青色圆柄 = 绕中心自由旋转；双击元素 = 复位铺满画布
  - 画布坐标 ↔ 输出坐标经 `displayAffine()`（逆变换）互换，全部命中测试在输出坐标系完成
  - 悬停光标区分：移动/角缩放/边拉伸/旋转/默认
- **GUI 新增 "Reset element transform" 按钮**（Style 组，全宽按钮行 `addButton` 布局支持）
- **文档同步**：§4.5.6 渲染管线（两段式）、§10 参数表（transform.* 6 项）、
  §11 控件映射（Reset 按钮）、`docs/GUI_GUIDE.md`（第 4 步"摆放频谱元素自由变换"）
- **已知限制**：变换是"整元素仿射"，网格与坐标轴文字随元素一起旋转/拉伸（符合所见即所得）；
  改变 W×H 后元素保持绝对像素位置，双击复位即可回正

### v0.4.0 — 2026-09-03
**变更（GUI 视频导出 + 编码器实测校准）**：
- **GUI 新增 "Export Video" 一键按钮**（ParamPanel Export 区，玫红色）：
  - 默认把 `params.encoder` 切到 `MovQtrle`（用户已在 Encoder 下拉选了则尊重）
  - 自动生成输出文件名 `<导出目录>/<音频名>_vis.mov`（无需手填 `outputVideoPath`）
  - 目录未设时先弹目录选择，选完回调继续导出（`exportKindPending` 标志）
  - 导出期间与普通 Export 按钮一起禁用；完成消息显示 `Done: video -> <路径>`
- **PngSequenceEncoder 编码器真正实现**（修复原 buildFfmpegArgs_ 写死 libx264→mp4 的遗留问题）：
  - `Config` 新增 `encoder` 字段；`buildFfmpegArgs_` 按 encoder 分支：
    - MOV：`qtrle + rgba`（**实测输出 pix_fmt=argb，无损 alpha**），音轨 `pcm_s16le`
    - WebM：`libvpx-vp9 + crf 32 + b:v 0 + row-mt 1`（**实测不保留 alpha**，定位为小体积预览片），音轨 `libopus`
    - PngSeq：跳过 ffmpeg（finalizeAndMux 提前返回）
  - `finalizeAndMux` 增加空 `outputVideoPath` / 空 args 的防御性校验
- **重要实测结论（写入本变更日志备查）**：`libvpx-vp9` 官方像素格式表声明支持 `yuva420p`，
  但 gyan.dev 2026-01 ffmpeg 实测编码后 ffprobe 显示 `yuv420p`，alpha 被静默丢弃
  （加 `-auto-alt-ref 0` 亦无效）。因此透明视频导出以 **MOV QTRLE** 为准。
- **VisPipeline::run**：视频模式自动补全 `outputVideoPath`（`<outputDir>/<音频名>_vis.<mov|webm>`），结果键 `mp4_path/mp4_status` → `video_path/video_status`
- **GUI Encoder 下拉**：调整为 `png-seq` / `mov-qtrle (alpha)` / `webm-vp9 (no alpha)`，避免误导
- **新增 `docs/GUI_GUIDE.md`**：中文 GUI 使用说明（构建 / 布局 / 5 步上手 / 格式选择 / FAQ）
- **待办勾销**：§7.2 编码器条目按实测结果校准（MOV alpha 完成；WebM alpha 记录为不可行）
- **注意**：需要系统 `ffmpeg.exe`（hint / `FFMPEG_PATH` / PATH 任一命中）；推荐 gyan.dev 构建

**追加（v0.4.0 同日）— 外部拖放修复 + Bar 样式参数**：
- **外部文件拖放修复**：`MainComponent` 原先未实现 `FileDragAndDropTarget`，窗口内唯一的拖放目标
  是 `SpectrumCanvas` 且 `isInterestedInFileDrag` 只对 `.wav/.aif/.aiff` 返回 true——
  拖非音频文件时 `ComponentPeer::handleDragMove` 沿父链找不到任何目标，
  全程 `DROPEFFECT_NONE`（禁止符），表现为"外部文件拖不进来"。修复：
  - `SpectrumCanvas::isInterestedInFileDrag` 改为接受任意文件；非音频走新回调
    `onNonAudioDropped` → `MainComponent` 弹警告框
  - `MainComponent` 补实现 `FileDragAndDropTarget` 兜底（画布外的面板/按钮上方也能接住拖放）
- **Bar 样式新增两参数**（全管线贯通：SpectrumParams → RenderParams →
  VisPipeline::buildRenderParams / GUI buildRp → BarStyle 布局）：
  - `barGapRatio`（0..1，默认 0.28）：柱间空隙占每带 slot 宽度的比例
  - `barWidthRatio`（0.05..2，默认 1.0）：柱宽占 (slot - gap) 的比例，>1 时相邻柱重叠
  - 柱改为在 slot 内**居中**；默认值组合与旧版（bar=0.72*slot）视觉完全一致
  - JSON 键 `visual.barGapRatio` / `visual.barWidthRatio`；CLI `--set visual.barGapRatio=0.5`
  - GUI 新滑块 **"Bar gap %"**（0..100）与 **"Bar width %"**（5..200），位于 Band count 下方

### v0.3.2 — 2026-09-02
**变更（文档重写，纯 .md 变更，无代码修改）**：
- 完全重写 **§4.5 GUI 章节**（v0.3 新增的 GUI skeleton 章节过于粗），拆成 8 个子节：
  - 4.5.1 源文件 ↔ 类 ↔ 关键成员 映射表（5 个源文件，精确字段名）
  - 4.5.2 MainComponent 成员字段速查表（6 大类 × 字段/类型/行为，全部源自 .h/.cpp）
  - 4.5.3 `timerCallback @30Hz` 主循环伪代码 + `currentTargetFrame` / `rebuildCoreLight` / `advanceCoreTo` 子函数行为
  - 4.5.4 ParamPanel ↔ MainComponent 5 条回调通道 + 2 种特殊行布局约定
  - 4.5.5 后台导出线程（`exporting/exportPct/exportDone` 原子变量 + `exportMsgMtx` + UI tick 轮询 join）
  - 4.5.6 SpectrumCanvas 渲染管线逐行（ARGB Image + checkerboard only-in-GUI）+ 拖放扩展名过滤
  - 4.5.7 基于当前状态的 GUI 快速上手指南（5 步 + 窗口分区说明）
  - 4.5.8 当前实现 10 条已知限制（L1-L10），附扩展建议，避免后续 AI 误报为 bug
- §5 "GUI 使用" 章节替换为英文步骤 + Checkerboard toggle 行为说明
- 顶部版本映射表新增 v0.3.2（本次）；末尾协作声明保留。
- **协作注意**：本版本是纯文档修订，未涉及 `source/`、`CMakeLists.txt` 等代码文件，因此不触发 GUI target 重新编译。

### v0.3.1 — 2026-09-02
**变更**：
- GUI 文案全部改为英文（避免非 Unicode locale 下的乱码），全局 LookAndFeel 固定字体为 "Segoe UI"
- 新增 §1-已实现功能 / §10-完整可调参数表 / §11-GUI 控件映射 / §12-变更日志 四节文档
- CMake: `juce_add_gui_app()` target 正确链接 `juce_gui_extra`（ColourSelector）
- 所有 `source/gui/` 成员按钮、标签文字与实际 addToggle/addCombo 参数一致

### v0.3.0 — 2026-09-02（首次提交，c495faf → 9d619f5）
**变更**：
- Engine: SpectrumCore (双路 FFT)、SpectrumStyle (y2k-line/bar/polyline/crystal)、SpectrumParams JSON+Override、VisPipeline、PcmSource、PngSequenceEncoder
- CLI: AudioVisExport (`--export` / `--preview-frame` / `--probe-spectrum` / etc.)
- GUI skeleton: AudioVisGUI (drag-drop, transport, ParamPanel, export thread)
- 6 组参数对比视频（10s）+ 4 种 style 10s 对比视频 生成于 `build/compare_10s/`（不入 git）

---

*文档版本：v0.5.0  ·  最后更新：2026-09-06*
*维护者：AudioVisExport 项目（GPL-3.0）*
*协作规则：任何功能修改后，必须在 §12 变更日志追加一条，并在文档版本号处 bump。*
*发布规则：每次小版本更新 → §12 条目 + `docs/RELEASE_NOTES.md` 更新公告 + commit/push + tag，缺一不可（见文档开头「发布规则」与 §12「发版流程」）。*
