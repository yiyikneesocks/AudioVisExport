# AudioVisExport — 长期计划（Roadmap）

> 迁自 `ARCHITECTURE.md` 原 §7。完成一项即把 `[ ]` 改 `[x]`，并在 `docs/HISTORY.md` §2 对应版本条目记录。
> 架构参考见 `ARCHITECTURE.md`，当前迭代计划见 `docs/PLAN.md`。


### 短期（样式 + 效果增强）
- [ ] CrystalStyle v2：真 GaussianBlur（`juce::ImageEffectFilter`）替换多层粗描边模拟 bloom
- [ ] CrystalStyle v2：折射/色散效果（RGB 通道分别偏移）
- [ ] ColorMap 实现：`gradient`（按强度上色）+ `rainbow`（全频段彩虹）
- [ ] 样式组合：支持同一帧叠加多个样式（如 crystal + bar 底层）

### 中期（编码 + 工作流）

> **已完成**（未列在原清单，详见 `docs/HISTORY.md`）：图片图层系统 v0.4.2 落地、
> v0.5.1 等比显示重构、v0.5.2 统一图层模型（频谱=真图层：z 序自由/删除恢复/键盘删除）+
> 对边锚定缩放 + 吸附系统。
- [x] MOV QTRLE alpha 编码器内置（PngSequenceEncoder::finalizeAndMux，v0.4.0；实测 argb 无损保留）
- [x] 编码器实测校准：WebM VP9 的 yuva420p 在主流 ffmpeg 构建中**实际丢 alpha**（v0.4.0 实测降级为 yuv420p），故 WebM 定位为"无 alpha 小体积预览片"；若未来获得开启 VP9-alpha / VP8-alpha 的 ffmpeg 构建可复刻 alpha
- [ ] 时间轴编辑界面（JUCE GUI）：timeline-based 多段频谱图编辑
- [ ] 实时预览窗口（JUCE GUI + OpenGL 或 软件渲染）

### 长期（大工程衔接）
- [ ] 将 `source/core/` 提取为独立静态库，供剪辑软件引用
- [ ] 剪辑软件 UI：SpectrumParams 暴露为滑块组，满意后导出 preset.json
- [ ] 多轨道支持：每个轨道独立 style + params，时间轴可裁剪/拼接
- [ ] 粒子系统：沿曲线流动的光斑/粒子（延伸 multi-pass 架构）
- [ ] 音频实时输入支持（从离线管线扩展为实时管线）

### 多 pass 架构扩展路
CrystalStyle 已验证 `getNumPasses() / renderPass()` 机制可用。后续扩展方向：
- **真多图层合成**：VisPipeline::renderFrame() 检测 `getNumPasses() > 1`，每 pass 渲染到独立 ARGB Image，用 blend mode 合成（支持 blur / color-dodge / screen 等）
- **滤镜链**：每个 pass 可附加 ImageEffectFilter（GaussianBlur / DropShadow / InnerShadow）
- **动画化 pass 参数**：pass 的透明度/粗细随时间变化（如辉光呼吸效果）

---

## 长期计划：图层复制粘贴 & 撤销重做（用户 2026-09-12，待排期）

### Ctrl+C / Ctrl+V 复制粘贴图层（v0.5.6 task5）
- **数据**：剪贴板成员 `std::vector<ImageLayer> clipboard;`（ImageLayer 已是纯数据结构、含 path+transform，
  可直接值拷贝）。`ImageLayer` 无资源句柄（图片走 `ImageCache`/`loadCached(path)`），拷贝安全。
- **复制**：Ctrl+C → 把"当前多选 `selectedSet` 里所有图片层"快照进 clipboard（**频谱层当前跳过**，见下）。
- **粘贴**：Ctrl+V → 逐个 `params.images.insert(...)` 到新副本，transform **整体偏移一点**（如 +24,+24 px）
  避免与源完全重叠看不见；插在频谱下方（复用 `addImageLayer` 的 spectrumIndex 规则）；粘贴后自动选中新副本集合。
  重复粘贴继续累加偏移。粘贴前 `pushUndoSnapshot`（撤销就绪后）。
- **命名/槽位**：无数量上限（不同于蒙版/音频单槽）；文件缺失时 `loadValidatedImage` 仍兜底。
- **未来：多频谱可复制**：现在频谱是唯一实例（`spectrumPresent` + 顶层 `params.transform`）。要支持"复制频谱并单独调"，
  需把频谱从"全局唯一"改造成**图层列表里的一类可重复元素**：给 ImageLayer 加 `isSpectrum` 变体或新增
  `std::vector<SpectrumLayer>{transform, styleOverride?, ...}`，渲染按 z 序逐个画。这是**较大重构**（触及
  VisPipeline/GUI 合成/蒙版绑定），故 v0.5.6 只做图片层复制粘贴，多频谱留待单独立项讨论。

### 撤销/重做命令化（v0.5.6 task4，当前 Ctrl+Z 已禁用）
- 弃用"整份 params 快照"（语义粗、易和滑块连续变更打架）。改**命令栈**：每个用户动作=一条命令
  （含 `apply()/revert()` + 前/后 Selection），滑块连续变更按字段+时间合并成一条，redo=Ctrl+Shift+Z。
- 配套单测（push/undo/redo/合并/边界）通过后，再翻 `avxEnableUndo=true` 解禁。

*（本节为待排期项；开工前需用户确认优先级，勿擅自启动。）*

---

*维护者：AudioVisExport 项目（GPL-3.0） · 每完成一项请同步勾选*
