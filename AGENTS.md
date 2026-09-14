# AudioVisExport — 协作硬规则（每次会话自动注入；保持精简，细节按需查 ARCHITECTURE.md）

本文件被 OpenCode 每次会话自动加载。**这里只放"任何改动都必须遵守"的硬规则**；架构/参数/构建细节、类字段速查等
一律放在 `ARCHITECTURE.md` 按需 Read，不要预加载，也不要往本文件塞成长篇说明。

## 0. 工作流闸门（不可跳）
本地 commit → 部署 `AudioVisGUI_*.exe` → **停下等用户明确回复"全部通过"** → 用户下达 push 指令 → `push` + `tag` + GitHub Release。
**未收到用户 push 指令前，绝不 push / tag / 建 Release**（即便测试全绿、build 干净）。

## 1. 纪律红线（防文档滞后）
- 每完成一个 INBOX 任务 → 立刻更新 `docs/PLAN.md`「当前状态」+ 时间戳；逐字备份 + 指纹进 `docs/inbox/INBOX_WORKLOG.md`。
- 每次 `git commit`（功能变更）前 → 检查 `docs/PLAN.md` 是否反映最新代码状态。
- 每次部署前 → 确认 `docs/PLAN.md` 已同步。
- 绝对禁止"代码已推 N 个 commit 但文档停留在上个版本"（v0.5.4 连推 20 commit 未同步的反面案例）。违反 = 流程事故，立即暂停补文档。

## 2. 发版 checklist（版本号任一位变化时全部必做）
① `docs/history/<系列>/<版本>.md` 新建本版条目 + `docs/HISTORY.md` §2 索引表加行（**不再有 `PLAN_vX.Y.Z.md` 快照**，见 ARCHITECTURE 顶部）。
② `docs/RELEASE_NOTES.md` 用户视角公告（同时作为 GitHub Release 正文）。
③ `ARCHITECTURE.md` 顶部版本映射表 + 代码版本号：CMakeLists（`project VERSION` + 两个 target `VERSION`）/ `source/gui/Main.cpp::getApplicationVersion` / `source/cli/CliArgs.cpp` 头。
④ 本地 commit + 部署 exe → 见 §0 验证闸门。

## 3. INBOX 三文件协议（`docs/inbox/`，已 gitignore；跨 AI 异步对话）
**读/删唯一入口 = `python tools/inbox.py`**（把"读+删"绑成原子步骤，防误删用户刚写的行）：
- `read` — 动手前先读全文 + 首行「状态」。
- `prune --expect "编号=你刚 read 到的该行开头片段"` — 工具重读盘逐条核对，只有当前文本与你读到的**完全一致**才删；用户改过的自动 MISMATCH 保留。删前自动 `.bak` 备份。
- **禁止**用 `write` 整文件重写、也不要手 `edit` 删编号行——一律走 `prune`。
- 首行「状态 1」= 用户编辑中 → 只读、拒绝任何删除（工具退码 2）。首行「状态 0」= 空闲。
- **两条分隔线（不得删除）**：`🗄️ 历史线`（上=可清理归档 / 下=进行中）与 `⏸️ 暂停线`（上=本轮推进 / 下=暂缓别动）。
- 到暂停线就暂停 + 汇报 + 提醒发版；清理上方已完成前**必须**先把原文（逐字）备份进 `INBOX_WORKLOG.md`；暂时无法解决的移到历史线上方；把暂停线下方要恢复的上移，编号不重用。
- **每完成一条立刻重读文档 1**（防漏 `!!` 加急/新任务）。
- **收尾强制规则（用户 #11，最高优先）**：本轮做完**必须**重读 `INBOX.md`；有新任务 → 清理已处理后**不停、直接开下一轮**；
  唯一合法停顿点 = 下一步确实依赖用户测试/决策而无法推进。
- **轮次节奏（用户 #0）**：一轮内连续做完所有能做的、中途不停、不逐条清文档；只有"没用户信息推不动"或"必须等用户测"才停。
- **REPLY 结构**：开头 = **待办速览**（待办 / 待用户实测 / 关键汇报——仅用户明确等回或必须知悉的）；普通工作汇报放后面；三件套 AI 均可按逻辑清理。
- **写入安全（2026-09-11 事故后，强制）**：`docs/inbox/` 不入 git、无历史副本。改三件套前 ① 先整体备份 `cp docs/inbox/*.md /tmp/<tag>/`；
  ② **禁止**"读全文→切片重组→整写" REPLY/WORKLOG 归档尾部——新轮只在其**顶部插入**、旧内容原样保留；
  ③ 用户原话必须**逐字**备份（不得摘要）；④ 改完 `grep -c ''` 对比行数确认未意外缩水，任何截断/丢内容如实说明。
- **任务排序（用户 #10）**：加急(`!!`) > 无争议/实现简单/可自证的 > 需用户实测确认的 > 需拍板的设计项（列计划）。被"缺用户测试"卡住→跳过不阻塞。

## 4. Python 环境（跑 `tools/*.py`）
项目 venv：`~/CodingProgram/AudioVisualizer/tools-venv`（system python 3.10，非 conda base）。缺失时 `bash tools/setup_env.sh`。
`tools/*.py` 已 auto-reexec 进 venv。权威说明 `~/CodingProgram/PYTHON_ENVIRONMENT.md`。

## 5. 编码 / Git 陷阱（本项目专属，违反必出事）
- **混合换行符**：`SpectrumCanvas.h` = **CRLF**；`SpectrumCanvas.cpp` / `MainComponent.cpp` / `SpectrumStyle.cpp` / `SpectrumMask.cpp` / `ParamPanel.*` / 各 style / 所有 `docs/*.md` / `ARCHITECTURE.md` = **LF**。
  用 Python 改文件必须 `io.open(p, 'r|w', encoding='utf-8', newline='')`（**不要**用 `open()` 默认模式，会剥 `\r` 造成 CRLF 文件被整体改行污染 diff）。
  编辑前后 `git diff --numstat` 抽查改动行数是否符合预期；`edit` 工具改 CRLF 文件风险高，尽量走 Python 脚本 + 保留 `\r\n`。
- **禁止 `git add -A`**（尤其 pull 别人 commit 之后）——曾把用户新加的 `update_plan.py` 误删（远端未受影响，本地 88affdd 恢复）。**只 stage 明确改动的路径**。
- 交叉编译部署：`bash scripts/build_win_cross.sh --deploy`（可选 `--clean` / `--no-build`）→ `/mnt/c/Users/yiyikneesocks/Desktop/AudioVisExport_test/`。
- Linux 本机：`cmake --build build`。回归测试是 `EXCLUDE_FROM_ALL`，改了要单独 `ninja -C build <test_target>`。
- 回归四件套（每次改动跑全）：`vis_mask_test / vis_peaks_test / vis_tabs_test / vis_anchor_test`。
- Windows push 认证（HTTP/1.1 + PAT）：见 `ARCHITECTURE.md §7.5`；凭据在 `~/.git-credentials`。

## 6. 参考优先原则（每次动手前的默认动作，非阻断）
实现某功能/算法前，**只要环境能联网就先搜同类实现/范式**：GitHub REST 搜索 → `webfetch` 读 README/源码 → 必要时 `git clone --depth 1` 到 `/tmp/opencode`。
借鉴点/坑记进 `docs/PLAN.md`「当前状态」或代码注释，注明来源 `owner/repo`（GPL 合规追溯）。仅"无网络"可跳过并注明。

## 7. 按需查阅（**不要**预加载，按需 Read）
- 架构 / 模块 / 类字段 / 同步主循环 / 已知限制 / 参数表 / 快捷键 / 构建详录 / 版本映射：`ARCHITECTURE.md`（大文档，按 §号跳读）。
- 项目当前推进到哪 / 下一步：`docs/PLAN.md`「当前状态」。
- 版本索引：`docs/HISTORY.md` §2；发版详录：`docs/history/<系列>/<版本>.md`。
- 长期计划：`docs/ROADMAP.md`。用户视角公告：`docs/RELEASE_NOTES.md`。GUI 手册：`docs/GUI_GUIDE.md`。
- 协作细节（INBOX 台账 / 给用户回话）：`docs/inbox/INBOX_WORKLOG.md` / `INBOX_REPLY.md`（**用户专用，AI 之间不当文档读**）。

> 任何修改前：先 Read 相关文档 + 源码交叉比对，避免"把推断当事实写入文档"。
