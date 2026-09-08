# AudioVisExport — 下一步计划（滚动文件）

> **生命周期**：开版时写计划（经用户确认）→ 执行 → 发版后把执行结果摘要并入
> `docs/HISTORY.md` §2 变更日志，然后**清空本文件**写下一版计划。
> 历史计划快照：`docs/PLAN_v0.5.0.md`（Windows 交叉编译专项）。长期方向见 `docs/ROADMAP.md`。

---

## 工作流模板（含验证闸门，强制）

1. 写计划（本文件）→ 用户确认
2. 执行：代码 + 双端构建 + 部署测试包 + 文档（`HISTORY.md` / `RELEASE_NOTES.md` / 版本号）
3. `git commit`（本地；**此时严禁 push / tag / Release**）
4. **【验证闸门】** 停下请用户验证 GUI/CLI；用户明确回复「全部通过」之前不得推送
5. 用户通过后，**主动提醒用户「尚未推送 GitHub」**，等待用户明确下达推送指令
6. 收到指令后：push + tag + GitHub Release 一并完成（push 命令速查见 `ARCHITECTURE.md` §7.5）

---

## 文档更新触发点（checklist，防遗漏）

| 时刻 | 必须更新的文档 |
|---|---|
| 开版：计划获批 | `PLAN.md` 写入任务拆分与验证方案 |
| 每完成一步 / 发现偏差 | `PLAN.md`「当前状态」+ 最后更新时间戳 |
| 闸门前（本地 commit 时） | `HISTORY.md` §2 版本条目 + `RELEASE_NOTES.md` + `ARCHITECTURE.md` 版本映射表 + 代码版本号（CMake/CLI） |
| 发版推送后（用户指令） | `PLAN.md` 摘要并入 `HISTORY.md` 后清空重写 |

---

## 当前状态（最后更新：2026-09-09）

- **v0.5.2 已完整发版**（commit `0a92842`、tag `v0.5.2`、GitHub Release 已发）
- **v0.5.3 未发版**：主体 B1–B5 + F1 + 真·B3 + B6（旋转拉伸平行四边形）+ Above spectrum UI 移除
  已完成并双端构建通过、GUI 手测主功能基本 OK；本次为 **WIP commit 推送 origin/main，未 tag / 未 GitHub Release**。
- 待发版收尾欠账（全部测完、决定发版时一次做）：`RELEASE_NOTES.md` v0.5.3 节、
  `ARCHITECTURE.md` 版本映射表行 + 页脚版本号、`HISTORY.md` 进度状态块改"已发版"（补 hash/tag/日期）、
  代码 tag `v0.5.3` + GitHub Release。

## 下一步计划（v0.5.3 → 待实施，需用户确认后再动手）

### N1（Bug 优先）仅频谱在场拖动"自我吸附"卡顿

- **现象**：只有频谱、无图片时，拖动频谱手感"黏滞/卡卡的"，像吸附到看不见的东西。
- **根因**：`SpectrumCanvas.cpp:556` 的 Move 吸附块，条件 `params.spectrumPresent && selectedImage < 0`
  ——`selectedImage < 0` 恰好是"正在拖频谱自身"，于是把**被拖频谱自己的实时 corners**
  （`params.transform`，且为上一帧值，与当前 proposed 位置错位一帧）当吸附候选 → 自我吸附抖动。
  代码注释写"若存在且非自身"，但条件实为"自身"，语义反了。
- **修法**（二选一，倾向 A）：
  - A：把该候选块限定为 `selectedImage >= 0`（拖图片时吸附到频谱 AABB），
    与 B5"频谱拖动不吸附"意图一致——拖频谱自身时该块整体跳过。
  - B：保留但排除自身（用 `startTransform` 冻结基线且当 dragged==spectrum 时不加入）。
- **验证**：仅频谱拖动全程顺滑无吸附；有图片时"拖图片→可吸到频谱边/中点"仍生效。

### N2（新功能）吸附辅助线 + 对齐点提示（CAD 风格）

**目标**：拖动命中吸附时，实时画出对齐辅助线，并标注对齐到的**点**（角点/中点额外打点），
像 CAD 的 object snap 提示。仅 `snapEnabled` 为真时生效。

**现状**：Move 吸附把候选压成标量 `SnapVal{ pos, target }`（SpectrumCanvas.cpp:522 起），
丢失了"是哪个元素的哪个特征点"的信息，无法据此画线/标点。

**设计（分阶段）**：

1. **数据模型**：候选从标量升级为富描述
   `SnapCand { float dragVal; float targetVal; juce::Point<float> dragPtOut; juce::Point<float> targetPtOut; Axis axis; StringRef label; }`
   - axis = Vertical（X 吸附 → 竖线）/ Horizontal（Y 吸附 → 横线）。
   - `dragPtOut` = 被拖元素该特征点输出坐标；`targetPtOut` = 目标特征点输出坐标。
   - 生成点：画布中心/四边、其它图片 AABB 的角点×4 / 边中点×4 / 中心、频谱 AABB 同上。
2. **求解**：X、Y 各自在阈值内取最优候选；命中则把该候选记入瞬时成员
   `std::vector<SnapGuide> activeSnapGuides;`（mouseDrag 每次重算，mouseUp 清空）。
3. **渲染**：在 `paintOverlay`（SpectrumCanvas.cpp:155，手柄同一层）末尾画 guide：
   - 线：命中方向画一条贯穿画布（或在两特征点之间）的细虚线，`PathStrokeType` + `Path::LineInfo`
     虚线；建议青/品红高对比色，与现有 cyan 旋转柄区分。
   - 点：在 `dragPtOut` 与 `targetPtOut` 各画一个小空心圆/十字；角点与中点用不同小图标区分
     （角点方框、中点菱形/圆）。
   - 可选文本：线端小标签 `label`（如 "Center" / "Edge" / "Img2 ⌐"），字号 10–11。
4. **性能**：仅拖拽期间持有极少量 guide（≤4），每帧重绘，代价可忽略。

**范围与分期**：
- **MVP（先做）**：Move 吸附的竖/横辅助线 + 两端特征点标记（角点/中点/中心图标化）。
- **P2（后议）**：缩放（角/边）时的对边/中点对齐辅助线（当前缩放无吸附）；旋转 90° 吸附时提示。
- 与 N1 一并落地（N1 修完才有干净的手感验证基线）。

**验收清单（GUI 手测）**：
- 拖图片中心接近另一图片角点 → 出现竖线 + 两点标记，且真的对齐。
- 接近画布水平中线 → 横线 + 中心点标记。
- 关闭 Snapping → 无任何辅助线，自由拖动。
- 仅频谱拖动 → 顺滑无自我吸附（N1）。

### N3（小功能，已定方案 A）空格 播放/暂停

- 焦点在画布/滑块/无焦点时空格切换；文本框内空格照常输入（JUCE 按钮焦点抢占空格的已知小限制，方案 A 接受）。
- 实现：`MainComponent.h` 加 `void togglePlayPause();`；`MainComponent.cpp` 把 `playBtn.onClick`
  lambda 体抽成该方法并复用；`keyPressed` 加 `if (key == juce::KeyPress::spaceKey) { togglePlayPause(); return true; }`。
- 顺序：建议与 N1/N2 同批实现构建，减少部署轮次。

## 候选下一版（引自 `docs/ROADMAP.md`，待用户挑选）

- CrystalStyle v2：真 GaussianBlur bloom（替换多层描边模拟）
- ColorMap：`gradient` / `rainbow` 两色图实现（字段已预留）
- 样式叠加：同帧渲染多个样式
- 缩放拖拽时的吸附（v0.5.2 仅做了移动 + 旋转吸附，缩放吸附列为可选）
- （若上述手测发现问题 → 优先修复，暂缓新功能）

---

*维护者：AudioVisExport 项目（GPL-3.0） · 发版后本文件清空重写*
