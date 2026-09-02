# AudioVisExport — 工程架构文档

> **面向后续 AI 协作者**：本文档是工程交接文档，帮助你快速理解项目全貌、设计决策、代码结构和后续目标。读完本文档即可开始协作开发。
>
> **文档版本 ↔ 项目版本映射（严格对齐）**：每次代码发布都要在此处更新版本行号，确保任意 AI 拿到文档后能准确定位到对应 tag / commit。
>
> | 文档 / 项目版本 | 日期 | tag（可）| 里程碑 |
> |---|---|---|---|
> | v0.3.1 | 2026-09-02 | `v0.3.1`（推荐打 tag）| GUI 上线 + 全英文 UI + 4 种 style + 双 target |
> | v0.3.0 | 2026-09-02 | `v0.3.0`（首次 commit c495faf + docs 9d619f5）| Engine + 4 styles + CLI + GUI skeleton |
>
> 协作时：若要修改功能，请先新建分支；文档末尾版本号 / 变更日志必须与代码 commit 同步更新。

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
| **GUI** (AudioVisGUI) | 拖放加载（WAV/AIFF）/ 点击选择；实时预览（30fps，与播放位置同步）；播放/暂停/seek；参数面板（滑块+数字输入双方式、下拉、开关、颜色选择、编码选择）；后台导出线程 + 进度条 | 全英文 UI；默认字体 Segoe UI；无代码页依赖 |
| **编码器** (PngSequenceEncoder) | PNG 序列（默认，单图 ARGB 100% 保真）；WebM VP9 yuva420p 透明通道；MOV QTRLE 透明通道 | WebM/MOV 调用系统 ffmpeg，需 `ffmpeg` 可见于 PATH |

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

### 核心设计原则
- **纯函数式离线管线**：不依赖 UI / 系统时钟 / OpenGL / 实时音频设备，所有渲染离线完成
- **引擎与 UI 解耦**：`source/core/` 是可独立复用的引擎，后续剪辑软件可作为子目录或静态库引入
- **参数全可配**：一份 `SpectrumParams` 结构承载所有参数（FFT / 频率映射 / 时间 / 动态 / 视觉 / 输出），支持 JSON 预设 + CLI 覆盖
- **样式可插拔**：`SpectrumStyle` 抽象基类 + 工厂模式，新增样式只需实现 `render()` 并注册

### 许可证
GPL-3.0（因参考了 Y2Kmeter 代码）

---

## 2. 参考来源

### 2.1 Y2Kmeter（主要参考）
- **路径**：`d:\Study 'n' Work\Program\Y2Kmeter\`
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
    ├── AudioVisExport_artefacts/Release/AudioVisExport.exe   # CLI
    └── AudioVisGUI_artefacts/Release/AudioVisGUI.exe          # GUI
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

### 4.5 GUI 实时预览（AudioVisGUI，v0.3 新增）
**文件**：`source/gui/`（独立 target `AudioVisGUI`，与 CLI 共享全部引擎源）

**架构（纯软件渲染，无 OpenGL）**：
```
拖入/选择音频 ──┬─ PcmSource（频谱用离线随机读取）
                └─ AudioFormatReaderSource → AudioTransportSource → 扬声器

juce::Timer @30fps:
  1. paramsDirty? → 重建 SpectrumStyle/SpectrumCore（不回放历史，曲线自然恢复）
  2. target = transport 位置 × fps
  3. target > coreFramePos → advanceCoreTo(): 从 PcmSource 逐帧读 PCM
     → SpectrumCore::pushInterleavedStereo + advanceTime（追赶同步，天然抗漂移）
  4. getBandFrame → SpectrumCanvas 重绘（ARGB Image + style.render，与导出同源）
```

**关键设计**：
- **预览即所得**：画布与导出管线走同一 `SpectrumStyle::render`，参数改完导出结果一致
- **同步模型**：以 transport 播放位置为唯一时钟，每 tick 推进 core 到目标帧（跳变时批量补帧）；回退 seek = 重建 core + 静默快进
- **参数实时生效**：ParamPanel 直接写 `SpectrumParams` + `paramsDirty=true`，下一 tick 重建 core（重建成本 = 2 次 FFT 分配，30fps 下无感）
- **导出**：后台 `std::thread` 跑 `VisPipeline::run`，原子进度轮询回 UI，完成后显示输出目录

**GUI 文件职责**：
- `SpectrumCanvas` — Component + FileDragAndDropTarget；paint() 里建 ARGB Image → 棋盘格（可选）→ style.render；空文件时显示提示文案
- `ParamPanel` — 行式布局（label + editor）；滑块（可拖+数字输入双方式）/下拉/开关/颜色弹层（ColourSelector via CallOutBox）/导出区
- `MainComponent` — 组装一切：布局（左画布/右参数面板/底部传输条）、音频设备生命周期、导出线程

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

### 构建
```powershell
cmake -S . -B build
# 编译（两个 target：CLI + GUI）
cmake --build build --config Release
# 产物
build/AudioVisExport_artefacts/Release/AudioVisExport.exe   # CLI
build/AudioVisGUI_artefacts/Release/AudioVisGUI.exe         # GUI 实时预览
```
> JUCE 8.0.12 通过 FetchContent 自动从 GitHub 拉取（首次配置需联网）。

### GUI 使用
```
AudioVisGUI.exe   # 双击启动
1. 把 WAV/AIFF 拖进左侧画布（或点击画布选择文件）
2. 点"播放"，频谱曲线随音乐实时跳动
3. 右侧面板直接拖滑块 / 输数字 / 换颜色，画面即时响应
4. 选导出目录 + 宽高 + 编码器 → 点"导出"，完成后提示 PNG/WebM 输出位置
```

### 运行示例
```powershell
# 生成 10s 透明背景柱状图视频
AudioVisExport --export PUPA_10s.wav out_bar --style bar --width 960 --height 540 --fps 30

# 生成水晶效果单帧预览（棋盘格背景验证透明度）
AudioVisExport --preview-frame preview_crystal.png --audio PUPA_10s.wav --frame-index 300 --style crystal --width 960 --height 540 --set output.bgCheckerboardPreview=true

# 数值调试
AudioVisExport --probe-spectrum PUPA_10s.wav 300 --band-count 64
```

### 透明背景视频合成
```powershell
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
- [ ] WebM VP9 alpha 编码器内置（PngSequenceEncoder::finalizeAndMux 已预留）
- [ ] MOV QTRLE alpha 编码器内置
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
A: PR 需要 WebM (VP9 + yuva420p) 或 MOV (QTRLE) 格式才支持 alpha。PNG 序列需先 ffmpeg 合成为带 alpha 的视频。`-auto-alt-ref 0` 是 VP9 alpha 的关键参数。

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
| Export | Encoder | Combo | params.encoder | PngSeq/WebmVp9/MovQtrle |
| Export | Browse | Button | exportDir (GUI state, not param) | 设置输出目录 |
| Export | Export | Button | (后台 VisPipeline::run) | 进度 % 显示在 progressLabel |

---

## 12. 变更日志（每发版必更，同步文档末尾版本号）

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

*文档版本：v0.3.1  ·  最后更新：2026-09-02*
*维护者：AudioVisExport 项目（GPL-3.0）*
*协作规则：任何功能修改后，必须在 §12 变更日志追加一条，并在文档版本号处 bump。*
