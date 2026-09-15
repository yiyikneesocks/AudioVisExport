# GitHub HTTPS 推送通用流程（**按工程隔离凭据** · PAT · 助手脚本）

> 适用：任何 HTTPS + Personal Access Token 推送的 git 仓库。配套脚本 `scripts/ghpush.sh`（可整文件复制到别的项目）。
> 一句话目标：**每个仓库用各自独立的凭据文件**，任何一个仓库的 token 录入/清除都**不会**牵连其它仓库。

---

## 0. 为什么必须隔离（血泪根因，先读）

`git credential.helper=store` 默认读写**全局** `~/.git-credentials`，其匹配键是
`protocol + host + username`（**默认不含仓库路径**）。于是：

- **同一个 GitHub 账号的多个 HTTPS 仓库，其实共用同一行凭据。**
- 任一仓库执行 `git credential reject`（按 host+username）→ 把**那唯一一行删掉** → **所有**同账号 HTTPS 仓库一起失去凭据。
- 任一仓库先写入了**坏/空 token**（例如粘贴竞争）→ 污染共享行 → 其它仓库随后认证集体 401，直到被覆盖/清除。
- **public 仓库"读"不需要凭据**（`ls-remote`/`clone` 照常成功），所以只有 **`push`/Release API 等写操作**才暴露问题——读探针会"假绿"。

> 本方案把凭据**按仓库落到各自文件**，`approve/reject` 只作用于本仓库文件；并配 **fine-grained per-repo PAT**，把"一次失误炸全网"的半径缩到"只炸这一个仓库"。

---

## 1. 铁律（人和 agent 都要守）

1. **agent / CI 绝不接触 token 明文、绝不做 `--setup-token`**；只有**人**在自己的终端里录入一次。
2. **绝不读写全局 `~/.git-credentials`**：一律用本仓库专属文件（`--init` 设定）。脚本已内置：所有 git 调用注入
   `-c credential.helper=`（清空继承）+ `-c credential.helper="store --file=<本仓库文件>"`，并在目标被指向全局文件时**直接拒绝**。
3. **建议每仓库单独签发 fine-grained PAT**（只勾本仓库、Contents: Read and write）；泄露/轮换只影响该仓库。
4. **自检走写探针** `push --dry-run`，不要用 `ls-remote`（public 仓库读会假绿）。
5. 改凭据文件前**先快照**；reject/approve 前先看"将影响哪些条目（打码）"。

---

## 2. 一次性配置（**用户**，在目标仓库里各跑一次）

```bash
scripts/ghpush.sh --init          # 把本仓库的 credential.helper 指向独立文件（写进 .git/config）
scripts/ghpush.sh --setup-token   # 隐藏录入本仓库专属 token（read -rs，不回显）
```

`--init` 实际做的事（便于核对/别处复刻）：
```bash
slug="$(git remote get-url origin | sed -E 's#^https?://##; s#^git@##; s#[:/]#-#g; s#\.git$##; s#-+#-#g; s#^-##')"
file="$HOME/.config/ghpush/${slug}.credentials"; mkdir -p "$HOME/.config/ghpush"; chmod 700 "$HOME/.config/ghpush"
git config --local credential.helper ""                          # 用空值清掉继承来的全局 store
git config --local --add credential.helper "store --file=$file"  # 本仓库专属
chmod 600 "$file" || true
```
效果：此后**该仓库任何** git 操作（agent、手敲 `git push`、IDE）都用这个专属文件，与别的仓库物理隔离。
（`.git/config` 不进版本库；重新 clone 后对每个仓库再跑一次 `--init` 即可。）

`--setup-token` 的"防粘错/防重复"（脚本内部，供其它 agent 参照实现）：
- `read -rs`：输入**不回显**。
- 清洗：去掉 `\r\n`（Windows 剪贴板）、首尾空白、包裹引号。
- **重复粘贴**：统计 `ghp_`/`github_pat_`/`gh[ousr]_` 前缀次数，>1 直接报错。
- **格式/长度白名单**：经典 `ghp_`/`gh[ousr]_` +36（共40）；fine-grained `github_pat_`+22+`_`+59（共93）；字符集只允许 `[A-Za-z0-9_-]`。
- 只打印**掩码预览**（前5…末4 + 长度），永不回显全 token。
- 先 `cp` 快照该文件，再 `credential reject→approve`——**reject 只作用于本仓库文件**，删不到别的仓库。

---

## 3. 日常推送（agent / 人 同一条命令）

```bash
scripts/ghpush.sh                  # 推当前分支（先 ls-remote 通、push --dry-run 预检，再正式推，失败自动重试）
scripts/ghpush.sh --tag v1.2.3     # 推分支 + 推一个已在本地的 tag
```
网络/TLS（某些环境 HTTP/2 被重置、系统 git gnutls 握手失败）：
```bash
GIT_BIN=/path/to/openssl-git scripts/ghpush.sh         # 用 openssl 后端 git
# 本仓库示例：GIT_BIN=~/miniconda3/envs/gitenv/bin/git scripts/ghpush.sh
```

---

## 4. 建 Release（用本仓库专属文件里的 token，绝不回显）

```bash
slug="$(git remote get-url origin | sed -E 's#^https?://##; s#^git@##; s#[:/]#-#g; s#\.git$##; s#-+#-#g; s#^-##')"
file="$HOME/.config/ghpush/${slug}.credentials"
# 从专属文件读回 token 到内存变量（不打印）：
TOK="$(git -c "credential.helper=" -c "credential.helper=store --file=$file" \
       credential fill <<<"protocol=https
host=github.com
username=<OWNER>
" | sed -n 's/^password=//p')"

curl -fsS -X POST "https://api.github.com/repos/<OWNER>/<REPO>/releases" \
  -H "Authorization: Bearer $TOK" -H "Accept: application/vnd.github+json" \
  -d '{"tag_name":"v1.2.3","name":"v1.2.3","body":"见 CHANGELOG","prerelease":false,"draft":false,"target_commitish":"main"}'
unset TOK
```
- 传二进制：拿返回的 `upload_url`，再
  `curl -T file -H "Authorization: Bearer $TOK" -H "Content-Type: application/octet-stream" "$UPLOAD_URL?name=file"`。
- 预发布加 `"prerelease": true`；草稿 `"draft": true`。
- **别开 `set -x`**（会把 `$TOK` 打进日志）；token 不进 URL/argv/commit/Issue。

---

## 5. 隔离方式对比（都可用，本方案默认 A）

| 方案 | 做法 | 隔离粒度 | 备注 |
|---|---|---|---|
| **A. per-repo store 文件**（默认） | `credential.helper=store --file=~/.config/ghpush/<slug>` | 每仓库一文件 | reject 只动自己文件；`.git/config` 本地生效 |
| B. useHttpPath | `git config --global credential.useHttpPath true` | 每 `host+user+path` 一 **行** | 仍是一份全局文件、一行一仓库；reject 需带 path |
| C. 不同 username 标签 | 同 token、username 用 `owner+repo` | 每 username 一行 | GitHub 对 PAT 忽略 username，仅作 store 键 |
| D. SSH / deploy key | 每仓库 SSH key | 天然独立 | 不碰 credential store；CI 友好 |

> 关键：**别让多个仓库共享同一行**（A/C 用不同键，B 用 path，D 用 SSH）。最稳是 **A + 每仓库单独 PAT**。

---

## 6. 恢复被误删/污染的凭据

- 现象：某仓库 `push` 报 401，但 public 仓库 `ls-remote`/`clone` 仍正常（读不需要凭据）。
- 恢复（用户，在该仓库里）：`scripts/ghpush.sh --setup-token` 重新录入即可（PAT 若没作废就直接用）。
- 核对"哪些仓库被牵连"（对每个用同一全局 store 的仓库跑）：
  ```bash
  cd <仓库> && GIT_TERMINAL_PROMPT=0 git push --dry-run origin HEAD   # 认证失败=受影响；成功或 up-to-date=没事
  ```
  或看 `~/.git-credentials` 里 `github.com` 行是否还在/是否为空。
- 迁移到隔离：受影响仓库各自 `--init` + `--setup-token` 一次，从此不再共享全局行。

---

## 7. 其它工程落地清单

1. 复制 `scripts/ghpush.sh` 到目标仓库 `scripts/`（它靠 `git remote get-url` 自适应，无需改代码）。
2. 该仓库跑一次 `scripts/ghpush.sh --init`（+ 可选：为它单独签发 fine-grained PAT）。
3. 用户跑一次 `scripts/ghpush.sh --setup-token`（隐藏录入到专属文件）。
4. 日常/CI/agent：`scripts/ghpush.sh [--tag vX.Y.Z]`；TLS 敏感环境设 `GIT_BIN=<openssl-git>`。
5. CI：**不跑交互式 `--setup-token`**；用 CI secret 注入，写进该 job 的专属文件：
   ```bash
   git config --local credential.helper ""
   git config --local credential.helper "store --file=$RUNNER_TEMP/gh-cred"
   printf 'protocol=https\nhost=github.com\nusername=x-access-token\npassword=%s\n\n' "$GH_TOKEN" | git credential approve
   ```
   （token 来自 secrets，勿 echo；job 结束文件即弃。）

---

## 8. 排错

| 现象 | 处理 |
|---|---|
| `目标凭据文件是全局共享文件` | 别把 `--credential-file` 指到 `~/.git-credentials`；用默认专属文件 |
| 读能过、push 401 | 该仓库专属文件没凭据：用户 `--setup-token`；或 token 无本仓库 Contents:write |
| `疑似重复粘贴`/`含非法字符`/`格式不识别` | 重新只粘一次；确认完整；确要放行 `GH_ALLOW_ANY_TOKEN=1` |
| `gnutls_handshake failed` / 连接重置 | `GIT_BIN=<openssl-git>`；确认 `http.version=HTTP/1.1`（脚本默认已加） |
| 换了 PAT/被 reject 后别的仓库也坏 | 说明还在共享全局行——给各仓库 `--init` 迁到专属文件 |

---

## 9. 安全红线

- token 只在 `--setup-token` 时经人手输入一次，隐藏回显、不落命令行/URL/日志/历史/commit。
- 专属凭据文件与 `~/.git-credentials` 一律 `0600`，目录 `0700`。
- fine-grained + 单仓库 scope + 合理过期；泄露立刻在 GitHub 吊销重发，再对**该仓库** `--setup-token`。
- 任何 agent 只调用 `ghpush.sh`（push/check/tag），不碰 token、不跑 `--setup-token`。

---

*本仓库具体 openssl-git 路径见 `ARCHITECTURE.md §7.5`。本文件是"按工程隔离"的通用版；旧的全局 store 写法已废弃（见事故记录 `~/CodingProgram/github-credential-incident-2026-09-15.md`）。*
