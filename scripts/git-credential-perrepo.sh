#!/usr/bin/env bash
# =============================================================================
# git-credential-perrepo — 全局 git credential 助手：把凭据按"当前仓库"分文件存
#
#   装一次（全局）即可让**任意仓库**的 git 凭据读写都落到各自独立文件，物理上杜绝
#   "同账号多仓库共用 ~/.git-credentials 一行 → 一个仓库 reject 牵连全部" 的事故。
#   这是"按工程隔离"的**强制执行层**，与任何文档/agent 是否遵守无关。
#
#   安装（见 docs/GITHUB_PUSH.md / 事故记录）：
#     git config --global credential.helper ""
#     git config --global credential.helper '!/home/azulores/.config/git/git-credential-perrepo'
#
#   作为 git shell helper 被调用：git 传入动作 get|store|erase 于 $1，凭据字段走 stdin。
#   本脚本据当前仓库 origin URL 推导唯一文件，再转交给内置 credential-store 处理。
#   slug 规则与 scripts/ghpush.sh 完全一致（复用同一批文件）。
# =============================================================================
set -euo pipefail

action="${1:-}"
case "$action" in get|store|erase) : ;; *) exit 0 ;; esac   # 只处理标准动作，其余原样交给 git

# 解析当前仓库 origin URL（helper 由 git 在仓库上下文里调用）
top="$(git rev-parse --show-toplevel 2>/dev/null || true)"
url=""
if [ -n "$top" ]; then
  url="$(git -C "$top" config --get remote.origin.url 2>/dev/null || true)"
fi
[ -n "$url" ] || url="$(git config --get remote.origin.url 2>/dev/null || true)"

if [ -n "$url" ]; then
  slug="$(printf '%s' "$url" \
    | sed -E 's#^https?://##; s#^ssh://##; s#^git@##; s#[:/]#-#g; s#\.git$##; s#-+#-#g; s#^-##')"
else
  # 不在仓库内 / 无 origin：落到一个与任何具体仓库无关的兜底文件（绝不共享别的仓库的 key），
  # 也**不去读 stdin**（否则会掏空交给 credential-store 的请求）。
  slug="_no-repo"
fi

dir="${XDG_CONFIG_HOME:-$HOME/.config}/ghpush"
umask 077
mkdir -p "$dir"; chmod 700 "$dir" 2>/dev/null || true
file="$dir/${slug}.credentials"
[ -e "$file" ] || : > "$file"
chmod 600 "$file" 2>/dev/null || true

# 转交给内置 store（它读 stdin 的凭据请求；仅作用于 $file）
exec git credential-store --file="$file" "$action"
