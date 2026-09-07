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

*维护者：AudioVisExport 项目（GPL-3.0） · 每完成一项请同步勾选*
