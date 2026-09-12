# AudioVisExport — 项目历史（里程碑 + 变更日志）

> **定位**：已完成内容的详细记录与解析——新协作 AI 读完本文档即可了解"做过什么、为什么、
> 怎么验证的"，作用等同于交接上下文。长期方向见 `docs/ROADMAP.md`，当前迭代见 `docs/PLAN.md`。
>
> **章节引用约定**：本文档历史条目中的 `§N` 指撰写当时的 `ARCHITECTURE.md` 章节号（原文未改写）。
> 2026-09-08 四拆后 ARCHITECTURE.md 重编号：旧 §6/§7/§12 迁出本文档与 ROADMAP；旧 §9.5→§7.5、
> 旧 §10→§8、旧 §11→§9，§1-§5 不变。读历史条目时按此映射换算。
>
> **发版流程（每次小版本都要走完）**：
> 1. 本文档 §2 顶部新增版本条目（变更内容 + 验证记录）
> 2. 撰写**更新公告** → `docs/RELEASE_NOTES.md`（用户视角：新功能 / 修复 / 升级注意事项）
> 3. `git add -A && git commit`（**本地提交，此时严禁 push**）
> 4. 部署测试包 → **停下请用户验证**；用户明确回复「全部通过」前严禁 push/tag/Release
>    （验证闸门规则全文见 `ARCHITECTURE.md` 开头）
> 5. 用户通过后，**主动提醒用户"尚未推送 GitHub"**，等用户明确下推送指令后：
>    `git push origin main` + 打 tag `git tag vX.Y.Z && git push origin vX.Y.Z`
> 6. 在 GitHub 用 `docs/RELEASE_NOTES.md` 内容创建 Release
>
> **（push 环境事实，实测 2026-09-07）**：
> · 系统 git（gnutls 后端）连 GitHub 必 TLS 中断；须用 conda 环境 gitenv 的 openssl 版 git：
>   ```bash
>   PATH=/home/azulores/miniconda3/envs/gitenv/bin:$PATH git -c http.version=HTTP/1.1 push origin main
>   PATH=/home/azulores/miniconda3/envs/gitenv/bin:$PATH git -c http.version=HTTP/1.1 push origin vX.Y.Z
>   ```
> · TLS 偶发抖动（openssl 后端也偶发 "unexpected eof"），失败就重试 1~5 次；
> · 凭据：GitHub PAT（fine-grained，仅本仓库 Contents 读写）存 `~/.git-credentials`
>   （0600，credential.helper=store），remote URL 保持干净形式（不含 token，防泄漏）；
>   **协作 AI 注意：不要要求用户把 token 贴进对话/命令行明文**——用 credentials 文件机制，
>   token 轮换由用户在 GitHub 网页 Regenerate 后自行写入该文件；
> · push 后发版 = GitHub API 创建 Release（正文取 `docs/RELEASE_NOTES.md` 对应版本节）。

---

## 1. 已完成的里程碑

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

## 2. 变更日志（每发版必更，最新在上）

> 已按版本系列拆分到 `docs/history/<系列>/<版本>.md`（每版一档）。**发新版时**：① 在对应系列目录建 `<version>.md`；② 在下面的表加一行链接（最新在上）。
> 页面结构：§1=里程碑总览；§2=版本索引；发版流程与 push 环境事实见页首。旧章节编号对照：2026-09-08 四拆，旧 §6→HISTORY §1、旧 §7→ROADMAP、旧 §9.5→ARCH §7.5。

| 版本 | 日期 | 主题 | 详见 |
|---|---|---|---|
| v0.5.4 | 2026-09-10 | 频谱样式专项 + 蒙版图片系统 + 基线轴 + 崩溃报告 + 多项UI增强 | [v0.5.4](history/v0.5/v0.5.4.md) |
| v0.5.0 | 2026-09-06 | Windows 原生交付 + UIPI 拖放修复 + GUI/样式增强 | [v0.5.0](history/v0.5/v0.5.0.md) |
| v0.4.2 | 2026-09-05 | 图片图层系统 + bar-line 样式 + 拖放诊断 + 峰值帽开关 | [v0.4.2](history/v0.4/v0.4.2.md) |
| v0.4.1 | 2026-09-05 | 频谱元素自由变换 + 输出分辨率所见即所得 | [v0.4.1](history/v0.4/v0.4.1.md) |
| v0.4.0 | 2026-09-03 | GUI 视频导出 + 编码器实测校准 | [v0.4.0](history/v0.4/v0.4.0.md) |
| v0.3.2 | 2026-09-02 | 文档重写，纯 .md 变更，无代码修改 | [v0.3.2](history/v0.3/v0.3.2.md) |
| v0.3.1 | 2026-09-02 | — | [v0.3.1](history/v0.3/v0.3.1.md) |
| v0.3.0 | 2026-09-02 | 首次提交，c495faf → 9d619f5 | [v0.3.0](history/v0.3/v0.3.0.md) |
