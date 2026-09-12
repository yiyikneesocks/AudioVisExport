# AudioVisExport — 计划 v0.5.6（用户 2026-09-12 口述：先记账，暂不动工）

> **状态：仅记录 + 出计划，未改任何代码。** 用户不在电脑边，明确要求"记好几条不要动工"，
> 待其回来确认优先级后再实施。权威滚动计划见 `docs/PLAN.md`；长期方向见 `docs/ROADMAP.md`。

## 0. 用户原话（2026-09-12，chat 口述，逐字留档）

1. 目前感觉 bug 不少。**暂时禁用 Ctrl+Z，bug 特别多。**
2. **大幅度优化实时渲染**，现在不管是效果还是按钮选项都比较乱。
3. **实现 Ctrl+A 全选所有图层 / 按住 Ctrl 多选图层 / 多选以后可以一键删除和一起移动（禁用一起旋转拉伸）。**
4. 后续再处理；先记好 + 看代码列计划。

---

## 1. 禁用 Ctrl+Z（小改动，最先做）

**现状代码定位**
- `source/gui/MainComponent.cpp::keyPressed()` 内约 940–946 行：`Ctrl/Cmd+Z → undoOnce()` 分支。
- 快照注入点 5 处：`onGestureStart`（画布 `mouseDrag` 首次）、`addImageLayer`、`addSpectrumLayer`、
  `removeSelectedLayer`、（结构操作）。

**已发现的真实 bug（解释"bug 特别多"）**
- **P0｜`addSpectrumLayer` 里 `pushUndoSnapshot()` 被调用两次**（第 821、822 行连续两句）——
  是 v0.5.5 那轮"两条脚本并行改同一文件"留下的重复接线。→ 每次"恢复频谱"压入两份快照，
  撤回一次只退半步、且栈被污染。**这条本身就是要禁 Ctrl+Z 的原因之一。**
- 快照粒度粗：拖拽期间 `onGestureStart` 只压一次（靠 `gestureReported`），但**滑块改参数**（颜色/bandCount/
  baseline 等）走 `notify() → paramsDirty`，**不压快照** → undo 只覆盖"结构/几何"，不覆盖数值 →
  用户直觉"我改了这么多怎么撤不回/撤错"。语义不一致 = 体感 bug。
- 快照恢复用 `canvas.selectSpectrum()` 粗暴复位选中 → 撤回后选中态丢失，像"撤乱了"。
- 只回滚 `params`，不回滚导出/播放态，边界不清晰。

**计划**
- **第一步（本次）**：把 `keyPressed` 的 Ctrl/Cmd+Z 分支用 `constexpr bool avxEnableUndo = false;` 关掉，
  保留 `pushUndoSnapshot/undoOnce` 代码与调用点（不删，留待重做），并在 REPLY/§9 标注"暂时禁用"。
  —— 纯注释式旁路，零删除，风险最低。
- **重做方向（后续）**：见 §4"撤销系统重做"，要么做成**命令式**（每个操作一个可逆命令），要么做成
  **全字段 params 快照 + 事件级合并**，语义统一后才重新开放。

---

## 2. 实时渲染"效果 + 按钮选项都乱"——大幅优化

### 2A 渲染性能（效果卡顿/负载高）
**现状主链路（30fps）**
- `MainComponent::timerCallback()` @30Hz → `core->getBandFrame(canvas.frame)` → `canvas.repaint()`。
- `SpectrumCanvas::paint()` 每帧：`fillAll 灰底` → 新建 **整分辨率 base ARGB Image（ow×oh）** → `style->render()`
  →（若开蒙版）`loadCached` + `adjustedImageCached` + `SpectrumMask::composeWithPlan(...)`。
- `composeWithPlan` 每帧对 base 做：`mA` 全图 alpha 提取(O(W·H)) → 图片仿射绘制 → clip(O(W·H)) →
  描边**四方向单调队列滑窗**(4×O(W·H)) + `makeStrokePlan` 全图预乘求和(O(W·H))。
  → 1280×720 一帧约"7~8 遍全图遍历 + 一次大图分配"，30fps 且开了描边时明显吃力。

**优化计划（按收益排序，逐项可验证）**
1. **分辨率解耦**：预览渲染按 `dispRect`（视口可见尺寸）而不是固定输出 `ow×oh`；导出仍走 `VisPipeline` 全分辨率。
   两者已同源 style/render，只差画布尺寸 → 给 RenderParams/paint 传一个"渲染尺度"，预览降采样渲染。
2. **蒙版描边降频**：描边（erosion/滑窗/平均色）本就允许低帧率——把"几何裁剪结果"与"描边"拆成两层，
   裁剪可逐帧、描边按 `outlinePreviewFps` 缓存合成图（`composeWithPlan` 已有 plan 缓存，扩到整张 mask 图层缓存）。
3. **避免每帧分配大图**：`base`/`out` 复用 thread_local scratch（同 `compose` 里 buf 的做法），
   尺寸不变不 resize。
4. **脏区/增量**：电平未变化的列不重算；`getBandFrame` 已有平滑，paint 只在 frame 变化时重画 base。
5. **timer 分级**：30Hz 主 tick 只推进音频/同步；重绘制用独立更低帧率（如 20fps）与 `repaint(rect)` 局部刷新。

### 2B 面板"按钮选项乱"
**现状问题（已在代码核实）**
- **Mask 页**：四个方向 width 滑块标签**都叫 "  width"**（源里同一 `addSlider("  width", …)` 生成 4 个，无法区分上下左右）；
  `Outline colour` 下拉、四边 toggle、四 width 滑块、`Outline fps`、`Outline smoothing`、`Brightness/Contrast/Saturation`、
  `Outline width` 主滑块 —— 一屏 10+ 控件**无分组、无缩进、命名重复**，非常乱。
- "Outline width" 主滑块与四边 width 语义重叠（主滑块会覆盖四边），用户不知道谁生效。
- 色彩调整(B/C/S) 在 Mask 页也出现在图片层，职责重叠。

**整理计划**
1. 用**折叠分区**重排 Mask 页：`Mask image` / `Outline(开关·颜色模式)` / `Edges(四边：开关+各自宽度，标签写全 "Top width"…)` / `Preview(perf)` / `Colour grade`。
2. 四边 width 标签改成 **Top/Bottom/Left/Right width**；"Outline width" 改名 **"All edges width"**，拖动时同步四边（现逻辑已是统一预设，改名即可消除歧义），并在 tooltip 写明"随后单独调某缘会覆盖"。
3. 与 §3 多选复用同一套"图层区"组件语言，保持视觉一致。
4. 加"高级/简化"两档显示（默认简化，勾 #5 那些边缘/性能项收进"高级"）。

---

## 3. 多选系统（Ctrl+A 全选 / Ctrl+点击 增选 / 批量删除 / 一起移动，禁一起旋转拉伸）

**现状**：选中是**单选整数** `int SpectrumCanvas::selectedImage`（-1=频谱，>=0=图片下标）；
`selectSpectrum()/selectImage(idx)`；命中用 `visCorners`+z 序拾取；拖拽只驱动一个 `activeTransform`。

**改造计划**
1. **数据模型**：`struct Selection { std::vector<int> images; bool spectrum=false; }`（蒙版图片是频谱子层，
   选频谱即隐含可进 mask 编辑，与多选分离）。提供 `selectAll()`（Ctrl+A）、`toggle(idx)`（Ctrl+点击）、
   `replace(idx)`（普通点击）、`clear()`。兼容现有 `selectedImage` 读取处（改为"主选中=集合末位"）。
2. **命中/绘制**：`paintOverlay` 对集合内每个元素都画手柄框；主选（末位）画满手柄，其余画细框+"已多选"标识。
3. **一起移动**：`mouseDrag` 的 Move 分支——当 `size>1` 且非 Ctrl+点击背景时，
   把位移 `dx,dy` **同时加到每个选中元素的 `transform.posX/posY`**（用统一的 base/输出位移换算）。
   单次 `onGestureStart` 压一个快照（撤销重做恢复后见 §4）。
4. **禁用批量旋转/拉伸**：多选时**只显示移动能力**——隐藏角/边/旋转手柄（`hitHandle` 在多选态直接返回 None，
   仅 Move 生效），从交互上杜绝"一起旋转拉伸"，符合用户要求。
5. **一键删除**：`removeSelectedLayer()` 扩为 `removeSelectedLayers()`（遍历集合删除 + 修 `spectrumIndex`），
   面板 Remove 按钮 / Delete 键 / 列表多选删除都走它；一次一个快照。
6. **图层列表联动**：`ParamPanel` 图层栈支持多选高亮 + Ctrl+点击；与画布共享 Selection（经回调，单向数据源=canvas）。
7. **键盘**：Ctrl+A=全选（从"选频谱"升级）、Esc=清空多选回到单选、Delete=删集合。**Ctrl+Z 仍禁用（§1）**。
8. **边界**：频谱与图片层同级、蒙版子层随频谱；批量移动时蒙版随其父频谱一起动（因为它绑定在频谱 transform 内）。

---

## 4. 撤销系统重做（解禁 Ctrl+Z 的前置条件）

- 采用**命令栈**而非全量 JSON：`struct UndoCmd { beforeJsonField, afterJsonField, apply(bool reverse) }`，
  每个用户动作（拖动结束、滑块一次提交、增删层、多选操作）压**一条**命令；合并同字段连续变更（如拖一个滑块）。
- 覆盖"选中态"回滚（记住操作前 Selection），修掉现版"撤回后强制 selectSpectrum"的丢状态问题。
- 修 `addSpectrumLayer` 双快照；栈设上限 + 单测：push/undo/redo 序列断言最终态=期望态。
- 全部通过后再把 §1 的 `avxEnableUndo` 打开。

---

## 5. 建议实施顺序与验证

1. **禁 Ctrl+Z + 修双快照**（10 分钟级，止血）→ 用户可继续测其它。
2. **多选系统 §3**（独立、体感强，不依赖渲染改造）。
3. **面板整理 §2B**（纯 UI，风险低，缓解"乱"）。
4. **渲染优化 §2A + 撤销重做 §4**（较硬，需要逐项像素/帧率回归）。

**回归口径（避免又"记了很多 bug"）**
- 渲染：新增计时断言（单帧 paint 预算 < 一帧周期），1280×720+蒙版+描边压测；预览分辨率解耦后导出逐帧一致性对拍。
- 多选：`vis_anchor_test` 式纯函数断言（集合运算、位移换算、手柄隐藏），列表/画布双向一致。
- 面板：沿用 `vis_tabs_test` 不变式（新控件必须走 `addRowGroup`，杜绝切页残留）。
- 撤销：命令栈单测（push/undo/redo/合并/边界），先于解禁。

---

*维护：本文件为 v0.5.6 计划草案，实施前需用户回来确认优先级；当前不改任何源码。*
