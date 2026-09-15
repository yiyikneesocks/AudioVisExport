# GitHub HTTPS 推送通用流程（PAT + 凭据助手）

> 适用范围：**任何**用 HTTPS + Personal Access Token 推送的 git 仓库（不止本工程）。
> 配套脚本：`scripts/ghpush.sh`（本仓库示例；可原样复制到其它工程使用）。
> 目标：**用户只需配一次 token**（隐藏输入、防粘错），**之后 agent / 日常一条命令即可推送**。

---

## 0. 谁做什么（职责边界）

| 角色 | 做一次 | 每次推送 |
|---|---|---|
| **用户（人）** | 在 GitHub 生成 fine-grained PAT；运行 `scripts/ghpush.sh --setup-token` 把 token **隐藏录入**本机凭据库 | 无（token 不经过 agent） |
| **Agent / CI / 日常** | 无 | 运行 `scripts/ghpush.sh`（可加 `--tag vX.Y.Z`）。脚本用**已存凭据**推送，全程接触不到 token 明文 |

关键点：**token 只在"人 → 本机凭据库"这一步出现一次**，且录入时终端不回显；推送阶段只引用凭据库，agent 与日志、shell 历史都不含 token。

---

## 1. 前置：把 remote 配成"干净 URL"（不含 token）

```bash
git remote -v                          # 确认现状
# 正确形态（用户名/密码/token 都不写进 URL）：
git remote set-url origin https://github.com/<OWNER>/<REPO>.git
```
`ghpush.sh` 会**主动拒绝**内嵌了 `user@` 的 remote（避免 token/账号被塞进 URL、泄露在 `.git/config` 与 `remote -v`）。

可选全局项（一次性）：
```bash
git config --global credential.helper store     # 凭据存 ~/.git-credentials（脚本也会按调用强制生效）
git config --global http.version HTTP/1.1        # 见 §4：某些网络 HTTP/2 会被重置
```

---

## 2. 创建 fine-grained PAT（用户操作）

GitHub 网页：
1. **Settings**（右上角头像）→ **Developer settings** → **Personal access tokens** → **Fine-grained tokens** → **Generate new token**。
2. **Repository access** 选 **Only select repositories**，勾目标仓库（最小权限，别用 All repositories）。
3. **Permissions → Repository permissions**：
   - **Contents = Read and write**（推送 commit / tag 必需）。
   - **Metadata = Read and write**（用 API 创建/编辑 Release 才需要；仅推代码可不给）。
4. **Expiration** 按需；点 **Generate token**，复制那串 `github_pat_...`。

> 若仓库是组织所有，生成后可能要在 token 页点 **Install/Authorize**（GitHub App 授权）才会真正生效。

---

## 3. 安全录入 token（隐藏、且防粘错）——核心

### 推荐：一条命令搞定（内部即下述全部校验）
```bash
scripts/ghpush.sh --setup-token
#   粘贴 GitHub token（隐藏）: ▓▓▓▓▓▓▓▓▓▓▓▓   ← 屏幕上不会显示任何字符
#   [ghpush] 校验通过：githu…z9Kf (len=93)
#   [ghpush] 已保存且可访问 origin/main。以后直接：scripts/ghpush.sh
```

### 它到底做了什么（也解释"复制不显示"和"防多次/粘错"）
1. **不显示输入**：`read -rs`（`-s` = silent，不回显；`-r` 不转义）。粘贴后直接回车即可。
   - 通用等价手写：
     ```bash
     read -rs -p "Paste token: " T; echo; printf '%s' "$T" | ...   # T 全程不回显
     unset T                                                        # 用完清掉
     ```
2. **清洗**：去掉粘贴常混入的 `\r\n`（Windows/换行）、首尾空格/制表、包裹的引号 `"` `'`。
3. **防"多次复制/重复粘贴"**：统计 `ghp_` / `github_pat_` / `gh?_` 前缀出现次数，>1 直接报错（提示"只粘贴一次"）。
4. **防"粘错/粘漏"**：字符集只允许 `[A-Za-z0-9_-]`；按前缀校验长度：
   - 经典 PAT：`ghp_` + 36 = **40**；OAuth/其它 `gh[ousr]_` + 36。
   - Fine-grained：`github_pat_` + 22 + `_` + 59 = **93**。
   不符即报错（GitHub 若改格式可用 `GH_ALLOW_ANY_TOKEN=1` 放行）。
5. **不回显明文**：只打印掩码预览 `前5位…末4位 (len=…)`。
6. **安全落盘**：用 `git credential reject`（清旧）→ `git credential approve`（写新），
   把凭据交给 git 的 `store` 助手管理，**先删后加避免 `~/.git-credentials` 里堆重复/错条目**；随后 `chmod 600`。
   - token 不进命令行参数、不进 URL、不进 shell 历史（因为是交互输入 + stdin 管道）。

### 手写等价（不想用脚本时）
```bash
read -rs -p "token: " T; echo
printf 'protocol=https\nhost=github.com\nusername=%s\npassword=%s\n\n' "$OWNER" "$T" \
  | git credential reject 2>/dev/null || true
printf 'protocol=https\nhost=github.com\nusername=%s\npassword=%s\n\n' "$OWNER" "$T" \
  | git credential approve
chmod 600 ~/.git-credentials; unset T
```

---

## 4. 日常推送（agent 只跑这一条）

```bash
scripts/ghpush.sh                    # 推当前分支
scripts/ghpush.sh --tag v1.2.3       # 推当前分支 + 推 tag（tag 需已 git tag 本地打好）
```
脚本流程：探测凭据 → **只读 `ls-remote` 预检**（不提交任何变更）→ `git push`（失败自动重试，默认 5 次）。

### TLS / 网络坑（本类环境常见）
- 报错 `gnutls_handshake() failed` 或连接被重置：多半是**系统 git 的 TLS 后端(gnutls) 或 HTTP/2** 的问题。
- 解法（脚本已内置其一，另一靠 `GIT_BIN`）：
  - `http.version=HTTP/1.1`（脚本默认已加）。
  - 用 **openssl 后端的 git**：`GIT_BIN=/path/to/openssl-git scripts/ghpush.sh`。
  - 本仓库示例：`GIT_BIN=~/miniconda3/envs/gitenv/bin/git scripts/ghpush.sh`。

> 给别的工程：把 openssl-git 路径写进项目自己的快捷方式即可，例如
> `alias ghp='GIT_BIN=/opt/gitssl/bin/git /repo/scripts/ghpush.sh'`，或让 `ghpush.sh` 读项目内 `.ghpushrc`。

---

## 5. 建 Release（用已存凭据，绝不回显 token）

纯推代码后，如需 GitHub Release（可选上传产物）：
```bash
# 从 git 凭据库取回 token 到内存变量（不打印、不落盘）
TOK=$(printf 'protocol=https\nhost=github.com\nusername=%s\n\n' "$OWNER" \
      | GIT_TERMINAL_PROMPT=0 git credential fill | sed -n 's/^password=//p')

curl -fsS -X POST "https://api.github.com/repos/<OWNER>/<REPO>/releases" \
  -H "Authorization: Bearer $TOK" \
  -H "Accept: application/vnd.github+json" \
  -d "{\"tag_name\":\"v1.2.3\",\"name\":\"v1.2.3\",\"body\":\"见 CHANGELOG\",\"prerelease\":true,
       \"target_commitish\":\"main\"}"
unset TOK          # 用完立刻清掉内存里的 token
```
- 预发布（beta/preview）加 `"prerelease": true`；草稿加 `"draft": true`。
- 传二进制：先建 release 拿 `upload_url`，再
  `curl -T file --header "Content-Type: application/octet-stream" "$UPLOAD_URL?name=file"`。
- **别开 `set -x`**（会把 `$TOK` 打进日志）；别把 token 写进脚本/环境变量导出。

---

## 6. 其它工程如何落地（照搬清单）

1. 复制 `scripts/ghpush.sh` 到目标仓库 `scripts/`（它靠 `git remote get-url` 自适应仓库，无需改代码）。
2. 若目标环境 TLS/网络敏感：确认 openssl 版 git 路径，设成该项目的 `GIT_BIN`（或 alias）。
3. 用户各自跑一次 `scripts/ghpush.sh --setup-token`。
4. 团队日常/CI 用 `scripts/ghpush.sh [--tag ...]`。
5. CI 场景**不要用交互录入**：直接在 secrets 里注入，或用
   `printf '...\npassword=$%TOKEN%...\n' | git credential approve`（token 来自 CI secret，勿 echo）。

---

## 7. 排错速查

| 现象 | 原因 / 处理 |
|---|---|
| `remote URL 内嵌了账号/token` | `git remote set-url origin https://github.com/OWNER/REPO.git` |
| `ls-remote 失败（凭据缺失）` | 让用户跑 `--setup-token`；或 `--check` 看是否有凭据 |
| `ls-remote 失败（TLS）` | `GIT_BIN=<openssl-git>`；确保 `http.version=HTTP/1.1` |
| `token 含非法字符` | 粘贴混入了空格/换行/引号 → 重新只粘一次 |
| `疑似重复粘贴（检测到 2 个前缀）` | 复制了两遍 → 清空重粘一次 |
| `token 格式不识别` | 长度/前缀不符；确认完整复制；GitHub 改格式时 `GH_ALLOW_ANY_TOKEN=1` |
| push 403 / `Write permission denied` | fine-grained token 未勾本仓库，或 Contents 不是 Read and write |
| Release API 403 | 缺 Metadata: Read and write，或 org 未授权该 GitHub App |

---

## 8. 安全红线（务必遵守）

- **绝不**把 token 写进 remote URL、`.git/config`、脚本、commit、Issue、日志、shell 历史。
- token 用 **最小权限 + 单仓库 scope + 合理过期**；泄露立即在 GitHub 吊销重发，再 `--setup-token` 覆盖。
- `~/.git-credentials` 权限 `0600`（脚本会自动收权限）。
- 只读的连通性/凭据自检用 `ghpush.sh --check`（内部 `ls-remote` + `credential fill`，不改任何东西、不回显密钥）。

---

*本仓库另有项目专属"推送速查"见 `ARCHITECTURE.md §7.5`（含 openssl-git 具体路径）；本文件是其通用化版本 + 交互录入/校验/安全落盘的完整实现（`scripts/ghpush.sh`）。*
