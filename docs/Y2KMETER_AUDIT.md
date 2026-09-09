# Y2Kmeter 借鉴审计（v0.5.4 · 任务 #100）

> 背景：AudioVisExport 的频谱引擎 / 配色 / 峰值曲线源自隔壁 Y2Kmeter（GPL-3.0）。
> 本文档回答两问：**已经借鉴了什么（附证据）**、**还有什么值得借鉴（按价值/成本排序）**。
> Y2Kmeter 位置：`~/CodingProgram/AudioVisualizer/Y2Kmeter`（v2.6.8，JUCE 8.0.12 + juce_opengl + libprojectM 4）。

## 1. 已借鉴清单（本工程中可查的证据）

| 借鉴项 | Y2Kmeter 出处 | 本工程落点 |
|---|---|---|
| **双路 FFT 架构**（2048 主路 + 8192 低频路，500Hz 交叉） | `source/ui/modules/SpectrumModule.*` / `AnalyserHub.*` | `SpectrumCore`（fftOrder=11 / fftOrderLo=13 / crossoverHz） |
| **PinkXP 配色命名与三色体系**（primary/secondary/peak + Y2K 粉主题） | `source/ui/PinkXPStyle.*` | `RenderParams::primary/secondary/peak`、ParamPanel 四色按钮 |
| **峰值保持/下落曲线参数**（hold → decay → accel 二阶下落） | Y2Kmeter 频谱模块峰值线 | `peakHoldMs / peakDecayDbPerSec / peakDecayAccelDbPerSec2`（注释"Y2K 原值"） |
| **对数轴 + minHz/maxHz/minDb/maxDb 口径** | SpectrumModule | `FreqMap` + RenderParams |
| **Y2K 峰值虚线**（Catmull-Rom 平滑 + createDashedStroke） | SpectrumModule 峰值线 | `Y2KLineStyle` 峰值虚线（#9+ 峰帽讨论的参照物） |
| **动态库外置**（projectM dll 运行时下载不入 git） | Y2Kmeter third_party 方案 | `.gitignore` `/third_party/projectm/bin/*.dll`（已预留，未接线） |
| 参数表/架构文档的**AI 导航文档模式** | `PROJECT_OVERVIEW.md` | `ARCHITECTURE.md` 文档地图 |

## 2. 可借鉴候选（按 价值/成本 排序）

### 高价值 · 中成本
1. **Milkdrop 图层（libprojectM 4）**：Y2Kmeter 的 `MilkdropModule` + `ProjectMApi` + FBO 零拷贝 blit 管线是现成模板；
   我们已预留 dll 目录。可做成"频谱驱动的全屏可视化图层"（可被频谱蒙版裁剪 → 与本工程蒙版体系天然契合）。
   注意 LGPL-2.1 合规（动态链接 ✓，与现方案一致）。
2. **WasapiLoopbackCapture（系统声卡循环内录）**：`source/standalone/WasapiLoopbackCapture.*`。
   让 GUI 能**实时监听系统声音**直接出频谱（不必先有 wav 文件）——直播/录屏场景刚需，实现成本低（单文件）。
3. **PGO/LTO 构建选项**（`Y2K_ENABLE_LTO` / `Y2K_PGO_MODE`）：4K 导出是 CPU 密集，PGO 白拿 10-20%。

### 中价值 · 低成本
4. **Spectrogram 瀑布图**（`SpectrogramModule`，像素方格复古风）：新增样式即可，与现有 BandFrame 数据流完全兼容。
5. **示波器图层**（`OscilloscopeModule`：波形 / X-Y / Lissajous）：作独立图层元素，套用现有 VisTransform/蒙版体系。
6. **模拟 VU 指针表**（VuMeterModule）：复古观感图层，和蒙版图片体系可叠加。
7. **UiFrameClock**（帧率节流）+ **PerformanceCounterSystem**（性能计数 HUD）：预览掉帧诊断。

### 低优先 / 参考
8. **GpuCompositor/GpuRenderer**（全 GPU 合成）：架构级迁移，成本高；当前 CPU 管线 + PGO 先撑，等样式数量爆炸再议。
9. **Tamagotchi 宠物 / 拼豆像素画**：娱乐性图层，社区传播卖点，技术上=音频驱动状态机+小图渲染。
10. **Inno Setup 安装器**（`Y2kmeter_installer.iss`）+ **自动更新对话框**（UpdateDialog）：发版体验，v1.0 前再做。
11. **Silkscreen 像素字体** + `juce_add_binary_data`：Y2K 视觉资产打包方案，若做"标题/时间戳文字图层"可参考。
12. **模块化工作区**（ModuleWorkspace/ModulePanel：模块可拖拽停靠）：比我们的 4 页签重，观望。

## 3. 合规注意
- Y2Kmeter 为 **GPL-3.0**：直接抄代码需本工程同样 GPL-3.0（目前就是）✓；libprojectM 为 **LGPL-2.1**：须动态链接。
- 隔壁代码"抄思路/抄参数值"安全；整段移植时在文件头注明来源（现 peakDecay 注释模式即可）。

---
*生成于 2026-09-10，任务 #100。下版候选方向建议：Milkdrop 图层 + WasapiLoopbackCapture。*
