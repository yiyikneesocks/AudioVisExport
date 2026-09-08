# AudioVisExport — 下一步计划（滚动文件）

> **生命周期**：开版时写计划（经用户确认）→ 执行 → 发版后把执行结果摘要并入
> `docs/HISTORY.md` §2 变更日志，然后**清空本文件**写下一版计划。
> 历史计划快照：`docs/PLAN_v0.5.0.md`（Windows 交叉编译专项）。长期方向见 `docs/ROADMAP.md`。

---

## 工作流模板（含验证闸门，强制）

1. 写计划（本文件）→ 用户确认
2. **GitHub 参考检索（务必优先，非阻断）**：本步动手前，只要环境能联网，就先搜同类实现 / 范式
   （REST API 搜索 → `webfetch` 读 README/源码 → 必要时 `git clone --depth 1` 到 `/tmp/opencode`），
   在「当前状态」记可借鉴点 / 坑 + 来源 `owner/repo`；**无网才跳过并注明**。详见 `ARCHITECTURE.md`「参考优先原则」
3. 执行：代码 + 双端构建 + 部署测试包 + 文档（`HISTORY.md` / `RELEASE_NOTES.md` / 版本号）
4. `git commit`（本地；**此时严禁 push / tag / Release**）
5. **【验证闸门】** 停下请用户验证 GUI/CLI；用户明确回复「全部通过」之前不得推送
6. 用户通过后，**主动提醒用户「尚未推送 GitHub」**，等待用户明确下达推送指令
7. 收到指令后：push + tag + GitHub Release 一并完成（push 命令速查见 `ARCHITECTURE.md` §7.5）

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
- **v0.5.3 已完整发版**（2026-09-09，tag `v0.5.3` + GitHub Release 已发）：
  B1–B5 + F1 + 真·B3 + B6（旋转后拉伸平行四边形，S·R→R·S）+ Above spectrum UI 移除
  + N1 频谱自吸附修复 + N2 CAD 吸附辅助线（9 特征点↔9 特征点，边-边/角-角全对齐）
  + N3 空格播放/暂停（含键盘焦点修复）+ N4 范围内外视觉区分（超范围内容变暗发灰）。
  执行详情见 `docs/HISTORY.md`「v0.5.3」条目与 `docs/RELEASE_NOTES.md` v0.5.3 节。
- **发版后遗留（回归/可选）**：
  - ⏳ 导出侧范围内裁剪回归待用户复测（本轮仅改 GUI `SpectrumCanvas`，`VisPipeline` 未动，理论上输出不变）。
  - P-verify 缩放 / 旋转时的吸附辅助线（v0.5.3 仅做移动吸附辅助线，缩放辅助线列为可选）。

## 候选下一版（v0.5.4 → 引自 `docs/ROADMAP.md`，待用户挑选）

- CrystalStyle v2：真 GaussianBlur bloom（替换多层描边模拟）
- ColorMap：`gradient` / `rainbow` 两色图实现（字段已预留）
- 样式叠加：同帧渲染多个样式
- 缩放拖拽时的吸附 + 辅助线（v0.5.3 仅移动吸附辅助线，缩放吸附列为可选 P2）
- FLAC / MP3 / OGG 解码支持（JUCE 核心未含第三方解码库，需挂 `registerFormat`）
- （若上述手测发现问题 → 优先修复，暂缓新功能）

---

*维护者：AudioVisExport 项目（GPL-3.0） · 发版后本文件清空重写*

