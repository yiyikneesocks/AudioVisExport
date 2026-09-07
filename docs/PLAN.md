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

## 当前状态（最后更新：2026-09-08）

- **v0.5.2 已完整发版**（commit `0a92842`、tag `v0.5.2`、GitHub Release 已发）
- 文档四拆重构完成（ARCHITECTURE / HISTORY / ROADMAP / PLAN），无代码变更
- 进行中任务：无

## 待用户验证（v0.5.2 GUI 手测清单）

1. 加图片后点击频谱区域 → 应能选中频谱并拖动（统一 z 序命中）
2. 选中频谱按 Delete / Backspace → 频谱消失、音乐照常 → [Add spectrum] 一键加回
3. 拖图片左下角 → 右上角应纹丝不动；拖下边 → 上边钉死（对边锚定缩放）
4. 旋转接近 90°×n 自动吸正；移动接近其他元素边缘/画布中线自动对齐（Snapping 开关可关）

## 候选下一版（引自 `docs/ROADMAP.md`，待用户挑选）

- CrystalStyle v2：真 GaussianBlur bloom（替换多层描边模拟）
- ColorMap：`gradient` / `rainbow` 两色图实现（字段已预留）
- 样式叠加：同帧渲染多个样式
- 缩放拖拽时的吸附（v0.5.2 仅做了移动 + 旋转吸附，缩放吸附列为可选）
- （若上述手测发现问题 → 优先修复，暂缓新功能）

---

*维护者：AudioVisExport 项目（GPL-3.0） · 发版后本文件清空重写*
