# AudioVisExport — 工程架构文档

> **面向后续 AI 协作者**：本文档是工程交接文档，帮助你快速理解项目全貌、设计决策、代码结构和后续目标。读完本文档即可开始协作开发。

---

## 1. 项目概述

AudioVisExport 是一个**音频可视化视频生成器**：导入音频文件（WAV/AIFF），通过自写的频谱分析引擎（双路 FFT + 带映射 + 时间平滑 + 峰值保持），以多种可插拔样式（Y2K 曲线 / 柱状图 / 折线 / 水晶效果）渲染到带 alpha 通道的 ARGB 图像序列，最终输出可导入 PR/Vegas/Resolve 等剪辑软件的透明背景视频素材。

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
└── build/                     # CMake 构建目录
    └── AudioVisExport_artefacts/Release/
        ├── AudioVisExport.exe      # 可执行文件
        ├── compare_10s/            # 对比视频目录（6+3 组 10s MP4）
        └── out_pupa_30s_y2k/      # 30s 完整导出示例
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

### 4.5 CLI
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
# 配置（使用 Y2Kmeter 本地 JUCE）
cmake -S . -B build -DAVX_USE_LOCAL_JUCE=ON
# 编译
cmake --build build --config Release
# 产物
build/AudioVisExport_artefacts/Release/AudioVisExport.exe
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

*文档最后更新：2026-09-02*
*维护者：AudioVisExport 项目（GPL-3.0）*
