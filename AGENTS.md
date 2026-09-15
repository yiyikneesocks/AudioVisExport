# AudioVisExport — AI Agent 协作与硬规则

> **提示**：本文件由 OpenCode 自动读取注入。仅包含**最高优先级硬规则与纪律红线**。
> 架构、参数、类字段等细节请**按需 Read** `ARCHITECTURE.md`，严禁预加载或充斥本文件。
>
> **本工程文档分工**（细节"按问题找文档"见 §6 表）：
> - **`AGENTS.md`（本文件）** = 每轮自动注入的**硬规则 + 文档导航入口**（工作流闸门 / 纪律 / INBOX 协议 / 环境 / 编码陷阱）
> - **`ARCHITECTURE.md`** = **代码工程结构**：模块 / 类字段 / 参数表 / 构建 / 版本映射 / 已知限制（按需 Read）
> - **`docs/PLAN.md`** = 本轮滚动计划与当前状态；**`docs/HISTORY.md` §2 + `docs/history/<系列>/<版本>.md`** = 版本变更详录
> - **`docs/ROADMAP.md`** = 长期计划；**`docs/RELEASE_NOTES.md`** = 用户视角公告；**`docs/GUI_GUIDE.md`** = GUI 手册
> - **`docs/inbox/`** = 跨 AI 异步收件箱三件套（**已 gitignore，无历史副本**；读/删唯一入口 `tools/inbox.py`）

---
## 0. 回复语言（**重要，每次回复务必遵守**）

1.默认使用 **English** 正式回复用户。
2.技术术语、命令、文件路径、代码标识符保持原文。
3.如果用户明确要求中文，则切换为中文。
4.用户发送中文时，也默认用 English 回复；无需为了配合用户语言自动切换。
5.使用自然、清晰的技术英语，不刻意使用生僻词或复杂句式。
6.每次用户可见回复正文必须以以下固定分界线开始：

--- AGENT RESPONSE ---

分界线以下的内容才是给用户的正式回复。命令输出、日志、代码、文件内容等可以放在正式回复中，但必须使用 Markdown code block，与正文明确区分。

## 1. 流程闸门与发版纪律（最高红线）

### 1.1 部署与 Push 隔离闸门
- **流转路径**: 本地 commit → 部署 `AudioVisGUI_*.exe` → **停下等待用户明确回复“全部通过”** → 收到用户 `push` 指令 → 执行 `push` + `tag` + GitHub Release。
- **强制禁令**: **未收到用户明确 `push` 指令前，绝对禁止执行 `git push`、打 `tag` 或创建 Release**（即使测试全绿、Build 完全干净也必须等待）。

### 1.2 文档同步红线（防滞后事故）
- **状态同步**: 每次 `git commit`（功能变更）及部署前，**必须**确保 `docs/PLAN.md` 准确反映最新代码状态。
- **INBOX 归档**: 每完成一个 INBOX 任务，必须同步更新 `docs/PLAN.md`「当前状态」与时间戳，并将逐字原文进 `docs/inbox/INBOX_WORKLOG.md`。
- **惩罚机制**: 严禁“代码已推多个 commit 但文档停留在旧版本”。一经发现视同流程事故，立即暂停一切开发优先补全文档。

### 1.3 发版 Checklist（版本号任一位变更时必做）
1. 新建 `docs/history/<系列>/<版本>.md` 条目，并更新 `docs/HISTORY.md` §2 索引表。
2. 更新 `docs/RELEASE_NOTES.md` 用户视角公告（兼 GitHub Release 正文）。
3. 同步修改版本号：`ARCHITECTURE.md` 顶部映射表、`CMakeLists.txt`（`project VERSION` 及两个 target `VERSION`）、`source/gui/Main.cpp::getApplicationVersion`、`source/cli/CliArgs.cpp` 头。
4. 提交 commit 并部署 exe → 进入 §1.1 验证闸门。

---

## 2. INBOX 三文件原子协议 (`docs/inbox/`)

> `docs/inbox/` 已被 gitignore，用于跨 AI 异步对话。**读/删唯一合法入口为 `python tools/inbox.py`**。

### 2.1 读写与清理规则
- **读取指令**: 动手前先执行 `python tools/inbox.py read` 获取全文及首行「状态」。
- **删除指令**: 仅允许执行 `python tools/inbox.py prune --expect "编号=读到的开头片段"`。绝对禁止手写/整文件重写/直接编辑删除编号行。
- **状态锁响应**: 
  - 首行 `状态 1`（用户编辑中）: 仅读，**拒绝任何删除操作**（工具退码 2）。
  - 首行 `状态 0`（空闲）: 可正常处理。
- **两条保护线（严禁删除）**:
  - `🗄️ 历史线`: 上方 = 可清理归档 / 下方 = 进行中。
  - `⏸️ 暂停线`: 上方 = 本轮推进 / 下方 = 暂缓处理。
- **清理与备份**: 清理历史线上方任务前，**必须先将原文逐字备份至 `INBOX_WORKLOG.md`**；暂停线下方要恢复的任务向上移动，编号不可重用。

### 2.2 轮次 rhythm 与收尾（最高优先级）
- **一轮做完**: 一轮内连续完成所有可做任务，中途不停顿、不逐条清文档。
- **强制收尾 (用户 #11)**: 本轮做完**必须**重新读取 `INBOX.md`。若有加急/新任务，清理已完成项后**不清场、不打断，直接开下一轮**。
- **唯一合法停顿点**: 下一步**确需依赖用户实测或决策**而无法继续推进。
- **写入安全规范**:
  - 改写三件套前先备份: `cp docs/inbox/*.md /tmp/<tag>/`
  - 禁止“切片重组整写”，新轮次仅在文件**顶部插入**，旧内容原样保留。
  - 必须逐字备份用户原话（严禁摘要）。
  - 修改后执行 `grep -c ''` 对比行数，确认未意外缩水。
- **REPLY 结构**: 开头必须为 **待办速览**（待办 / 待用户实测 / 关键汇报），普通汇报附于后。
- **任务优先级 (用户 #10)**: 加急(`!!`) > 无争议/实现简单/可自证 > 需用户实测确认 > 需拍板的设计项。被“缺用户测试”卡住时直接跳过，不阻塞后续任务。

---

## 3. 环境与构建规范

### 3.1 Python 环境
- **项目 venv 路径**: `~/CodingProgram/AudioVisualizer/tools-venv`（System Python 3.10，非 Conda base）。
- **环境修复**: 若环境缺失，执行 `bash tools/setup_env.sh`。
- **工具调用**: `tools/*.py` 已配置 auto-reexec 进 venv。详见 `~/CodingProgram/PYTHON_ENVIRONMENT.md`。

### 3.2 编译与测试指令
- **Windows 交叉编译部署**:
  ```bash
  bash scripts/build_win_cross.sh --deploy
  # 部署目标: /mnt/c/Users/yiyikneesocks/Desktop/AudioVisExport_test/
  ```
- **Linux 本地构建**:
  ```bash
  cmake --build build
  ```
- **回归测试（每次改动必跑全套）**:
  ```bash
  ninja -C build vis_mask_test vis_peaks_test vis_tabs_test vis_anchor_test
  ```

---

## 4. 编码与 Git 安全陷阱

### 4.1 换行符（Newline）严格隔离
- **CRLF 文件**: `SpectrumCanvas.h`
- **LF 文件**: `SpectrumCanvas.cpp`, `MainComponent.cpp`, `SpectrumStyle.cpp`, `SpectrumMask.cpp`, `ParamPanel.*`, 各 style 模块, 所有 `docs/*.md`, `ARCHITECTURE.md`。
- **Python 修改文件标准代码片段**:
  ```python
  # 必须显式声明 newline='' 以保留原文件的 \r\n，禁止使用默认 open()
  import io

  with io.open(path, "r", encoding="utf-8", newline="") as f:
      content = f.read()

  with io.open(path, "w", encoding="utf-8", newline="") as f:
      f.write(content)
  ```
- **提交抽查**: 修改前后的 `git diff --numstat` 必须检查改动行数，防止整个文件的 CRLF/LF 被污染。

### 4.2 Git 暂存与认证防爆
- **禁止 `git add -A`**: 严禁批量暂存！**仅精确 stage (`git add <file>`) 明确修改的路径**。
- **Windows Push 认证**: 使用 HTTP/1.1 + PAT，详见 `ARCHITECTURE.md §7.5`（凭据存在 `~/.git-credentials`）。

---

## 5. 参考优先原则（预研逻辑）

- 在实现新功能或算法前，只要网络连接正常，**必须优先检索同类实现**：
  1. 通过 GitHub REST API 进行搜索。
  2. 使用 `webfetch` 读取 README/源码。
  3. 必要时 `git clone --depth 1` 至 `/tmp/opencode`。
- 借鉴点/坑点需记录至 `docs/PLAN.md`「当前状态」或代码注释中，并注明来源 `owner/repo`（GPL 合规）。无网络时注明即可跳过。

---

## 6. 文档按需查阅索引（禁止预加载）

| 查阅需求 | 对应目标文件 |
| :--- | :--- |
| 架构 / 模块 / 类字段 / 循环 / 参数 / 构建 | `ARCHITECTURE.md`（按章节跳读） |
| 当前推进进度 / 下一步计划 | `docs/PLAN.md`（重点看「当前状态」） |
| 版本历史 / 发版详录 | `docs/HISTORY.md` §2 / `docs/history/<系列>/<版本>.md` |
| 长期规划 / 视角公告 / GUI 手册 | `docs/ROADMAP.md` / `docs/RELEASE_NOTES.md` / `docs/GUI_GUIDE.md` |
| INBOX 台账 / 给用户回复历史 | `docs/inbox/INBOX_WORKLOG.md` / `INBOX_REPLY.md` |

> **改动前原则**: 先 Read 相关文档，与源码交叉比对，绝对禁止将未验证的推断写入文档。