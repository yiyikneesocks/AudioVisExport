#!/usr/bin/env bash
# =============================================================================
# ghpush.sh — 通用 GitHub HTTPS 推送助手（**按工程隔离凭据**，杜绝跨仓库互相覆盖）
#
# 为什么隔离（本脚本存在的根因）：
#   git 的 `credential.helper=store` 默认读写全局 `~/.git-credentials`，匹配键是
#   `protocol+host+username`（默认不含 path）。于是"同一账号的多个 HTTPS 仓库"其实
#   **共用同一行凭据**：任一仓库 `git credential reject`（或先写入坏/空 token）都会
#   牵连所有同账号仓库（public 仓库"读能过"掩盖问题，直到 push/Release 才 401）。
#   → 本助手把每个仓库的凭据**落到各自独立文件**，approve/reject 只动自己的文件。
#
# 隔离模型：
#   每仓库一个凭据文件  $HOME/.config/ghpush/<host>-<owner>-<repo>.credentials  (0600)
#   所有 git 调用注入   -c credential.helper= -c credential.helper="store --file=<该文件>"
#   （前一个空值清掉继承的全局/系统 helper，保证绝不误用共享全局文件）
#
# 职责：
#   · 用户（人）：每仓库一次 `--init`（可选）+ `--setup-token`（隐藏录入，永不见明文）
#   · agent/日常：只跑 `ghpush.sh`（+ 可选 --tag）；接触不到 token 明文
#   强烈建议：为每个仓库单独签发 fine-grained PAT（仓库内可即时吊销/轮换），
#   这样某 token 出问题只影响它自己那一个仓库。
#
# 用法：
#   scripts/ghpush.sh --init          # 一次性：把本仓库 local credential.helper 指向独立文件
#   scripts/ghpush.sh --setup-token   # 用户：录入/更新本仓库专属 token（隐藏输入）
#   scripts/ghpush.sh                 # 推送当前分支（写探针预检 + 重试）
#   scripts/ghpush.sh --tag v1.2.3    # 推当前分支 + 推 tag
#   scripts/ghpush.sh --check         # 体检：remote/分支/独立文件是否存在/写探针（不回显 token）
#
# 环境变量 / 覆盖：
#   GIT_BIN               git 路径（默认 git）。TLS 报 gnutls 时填 openssl 版 git。
#   GH_HTTP_VERSION       默认 HTTP/1.1；置空则不加 -c http.version。
#   GH_PUSH_REMOTE        默认 origin
#   GH_PUSH_RETRY         默认 5
#   GH_PUSH_CRED_DIR      独立凭据目录，默认 $HOME/.config/ghpush
#   GH_PUSH_CRED_FILE     直接指定凭据文件路径（覆盖 slug 推导）
#   GH_ALLOW_ANY_TOKEN=1  跳过 token 白名单（GitHub 改格式时用）
# =============================================================================
set -euo pipefail

GIT_BIN="${GIT_BIN:-git}"
HTTP_VERSION="${GH_HTTP_VERSION:-HTTP/1.1}"
REMOTE="${GH_PUSH_REMOTE:-origin}"
RETRY="${GH_PUSH_RETRY:-5}"
ALLOW_ANY="${GH_ALLOW_ANY_TOKEN:-0}"

ACTION="push"; BRANCH=""; TAG=""; USERNAME=""; CRED_FILE="${GH_PUSH_CRED_FILE:-}"

die()  { printf '\033[31m[ghpush] %s\033[0m\n' "$*" >&2; exit 1; }
info() { printf '\033[36m[ghpush] %s\033[0m\n' "$*"; }
warn() { printf '\033[33m[ghpush] %s\033[0m\n' "$*" >&2; }

while [ $# -gt 0 ]; do
  case "$1" in
    --init)        ACTION="init" ;;
    --setup-token) ACTION="setup" ;;
    --check)       ACTION="check" ;;
    --branch)      BRANCH="${2:?--branch 需要值}"; shift ;;
    --tag)         TAG="${2:?--tag 需要值}"; shift ;;
    --username)    USERNAME="${2:?--username 需要值}"; shift ;;
    --credential-file) CRED_FILE="${2:?--credential-file 需要值}"; shift ;;
    --help|-h)     sed -n '2,44p' "$0"; exit 0 ;;
    *)             die "未知参数：$1（--help 看用法）" ;;
  esac
  shift
done

command -v "$GIT_BIN" >/dev/null 2>&1 || die "找不到 git（$GIT_BIN）。TLS 问题用 GIT_BIN 指定 openssl 版。"
"$GIT_BIN" rev-parse --git-dir >/dev/null 2>&1 || die "当前目录不是 git 仓库。"

REMOTE_URL="$("$GIT_BIN" remote get-url "$REMOTE")" || die "没有 remote '$REMOTE'。"
if printf '%s' "$REMOTE_URL" | grep -q '^https://'; then
  printf '%s' "$REMOTE_URL" | grep -Eq 'https://[^/@]+@' \
    && die "remote URL 内嵌了账号/token，请先清干净：git remote set-url $REMOTE https://github.com/OWNER/REPO.git"
  HOST="$(printf '%s'  "$REMOTE_URL" | sed -E 's#https://([^/]+)/.*#\1#')"
  OWNER="$(printf '%s' "$REMOTE_URL" | sed -E 's#https://[^/]+/([^/]+)/?.*#\1#; s#\.git$##')"
  SCHEME="https"
else
  HOST="$(printf '%s'  "$REMOTE_URL" | sed -E 's#.*@([^:]+):.*#\1#')"
  OWNER="$(printf '%s' "$REMOTE_URL" | sed -E 's#.*:([^/]+)/.*#\1#')"
  SCHEME="https"
  info "remote 是 SSH URL；本助手面向 HTTPS+PAT（SSH 无需 token）。"
fi
[ -n "$USERNAME" ] || USERNAME="$OWNER"
[ -n "$BRANCH" ]   || BRANCH="$("$GIT_BIN" rev-parse --abbrev-ref HEAD)"
[ "$BRANCH" != "HEAD" ] || die "处于 detached HEAD，用 --branch 指定分支。"

# ---- 独立凭据文件（每仓库一个）----
if [ -z "$CRED_FILE" ]; then
  CRED_DIR="${GH_PUSH_CRED_DIR:-$HOME/.config/ghpush}"
  SLUG="$(printf '%s' "$REMOTE_URL" | sed -E 's#^https?://##; s#^git@##; s#^ssh://##; s#[:/]#-#g; s#\.git$##; s#-+#-#g; s#^-##')"
  mkdir -p "$CRED_DIR"; chmod 700 "$CRED_DIR"
  CRED_FILE="$CRED_DIR/${SLUG}.credentials"
fi
# 防呆：绝不允许把独立文件指到全局共享文件
case "$CRED_FILE" in
  "$HOME/.git-credentials") die "目标凭据文件是全局共享文件（会被别的仓库 reject 牵连）。请换一个（默认即可）。" ;;
esac
touch "$CRED_FILE" 2>/dev/null || true; chmod 600 "$CRED_FILE" 2>/dev/null || true

# ---- 只注入本仓库专属 helper（空值先清继承）----
gitx() {
  local args=()
  [ -n "$HTTP_VERSION" ] && args+=("-c" "http.version=$HTTP_VERSION")
  args+=("-c" "credential.helper=" "-c" "credential.helper=store --file=$CRED_FILE")
  "$GIT_BIN" "${args[@]}" "$@"
}

# 是否已有该 host+username 凭据（限定到本文件；只读、非交互）
has_credential() {
  local out
  out="$(printf 'protocol=https\nhost=%s\nusername=%s\n\n' "$HOST" "$USERNAME" \
        | GIT_TERMINAL_PROMPT=0 gitx credential fill 2>/dev/null || true)"
  printf '%s' "$out" | grep -q '^password='
}

# 掩码打印某文件里的条目（只显示 user@host，永不显示 password）
list_masked() {
  local f="$1"
  [ -s "$f" ] || { echo "  (空)"; return; }
  sed -E 's#(://[^:/@]+):[^@]*@#\1:***@#' "$f" | sed 's/^/  /'
}
# 快照（打码前的真实文件备份，权限 600）
snapshot() {
  [ -s "$CRED_FILE" ] || return 0
  local bak="$CRED_FILE.bak.$(date +%Y%m%d%H%M%S)"
  cp -p "$CRED_FILE" "$bak" 2>/dev/null && chmod 600 "$bak" 2>/dev/null || true
  info "已快照旧凭据 → $(basename "$bak")"
}

# 清洗 + 校验 token：设 VALIDATED_TOKEN / MASK_PREVIEW；失败 die（主 shell，可中止）
validate_token() {
  local raw="$1" tok hits
  tok="$(printf '%s' "$raw" | tr -d '\r\n')"
  tok="${tok#\"}"; tok="${tok%\"}"; tok="${tok#\'}"; tok="${tok%\'}"
  tok="$(printf '%s' "$tok" | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//')"
  [ -n "$tok" ] || die "token 为空。"
  printf '%s' "$tok" | grep -Eq '^[A-Za-z0-9_-]+$' \
    || die "token 含非法字符（空格/制表/中文/引号等）——多半没粘全或混入多余内容。"
  hits="$(printf '%s' "$tok" | grep -Eo 'ghp_|github_pat_|gh[ousr]_' | wc -l | tr -d '[:space:]')"
  [ "$hits" -le 1 ] || die "疑似重复粘贴（检测到 $hits 个 token 前缀）。清空后只粘贴一次。"
  if [ "$ALLOW_ANY" != "1" ]; then
    if printf '%s' "$tok" | grep -Eq '^(ghp_|gh[ousr]_)[A-Za-z0-9]{36}$' \
       || printf '%s' "$tok" | grep -Eq '^github_pat_[A-Za-z0-9]{22}_[A-Za-z0-9]{59}$'; then :
    else
      die "token 格式不识别（长度/前缀不符）。确认从 GitHub 原样复制？确要接受用 GH_ALLOW_ANY_TOKEN=1 重跑。"
    fi
  fi
  VALIDATED_TOKEN="$tok"
  MASK_PREVIEW="$(printf '%s…%s (len=%d)' "${tok:0:5}" "${tok: -4}" "${#tok}")"
}

# 写入本仓库专属文件（先清 host+username 旧项再 approve，不会碰别的仓库/全局）
store_credential() {
  local tok="$1"
  printf 'protocol=https\nhost=%s\nusername=%s\npassword=%s\n\n' "$HOST" "$USERNAME" "$tok" | gitx credential reject  2>/dev/null || true
  printf 'protocol=https\nhost=%s\nusername=%s\npassword=%s\n\n' "$HOST" "$USERNAME" "$tok" | gitx credential approve
  chmod 600 "$CRED_FILE" 2>/dev/null || true
}

case "$ACTION" in
  init)
    info "为 $REMOTE 设置**本仓库专属**凭据文件（写进该仓库 .git/config，普通 git push 也走它）："
    info "  file = $CRED_FILE"
    "$GIT_BIN" config --local credential.helper ""
    "$GIT_BIN" config --local --add credential.helper "store --file=$CRED_FILE"
    info "已生效的 local helper："
    "$GIT_BIN" config --local --get-all credential.helper | sed 's/^/    /'
    info "下一步（用户，隐藏输入）：$0 --setup-token"
    ;;

  check)
    info "remote : $REMOTE -> $SCHEME://$HOST/$OWNER/<repo>"
    info "branch : $BRANCH"
    info "git    : $GIT_BIN   http.version=${HTTP_VERSION:-<default>}"
    info "credfile: $CRED_FILE（本仓库专属，与其它仓库隔离）"
    info "该文件现有条目（已打码）："; list_masked "$CRED_FILE"
    has_credential && info "credential: 有（user=$USERNAME）" || info "credential: 无 → 用户跑：$0 --setup-token"
    # 写探针（public 仓库读会假绿，必须探写路径；--dry-run 不改远程）
    if GIT_TERMINAL_PROMPT=0 gitx push --dry-run "$REMOTE" "$BRANCH" >/dev/null 2>&1; then
      info "write-probe (push --dry-run): 通过（可推送）"
    else
      info "write-probe (push --dry-run): 失败（缺凭据/权限/网络/TLS）"
    fi
    ;;

  setup)
    info "目标仓库 : $OWNER/<repo> @ $HOST"
    info "凭据文件 : $CRED_FILE   （仅本仓库，不动全局 ~/.git-credentials）"
    info "建议：GitHub 为**本仓库**单独签发 fine-grained PAT（Contents: Read and write；建 Release 再加 Metadata）。"
    snapshot
    tok=""
    read -rs -p "粘贴 GitHub token（输入隐藏）: " tok </dev/tty || die "读取失败（需终端交互）。"
    printf '\n'
    validate_token "$tok"
    info "校验通过：$MASK_PREVIEW"
    store_credential "$VALIDATED_TOKEN"
    unset tok VALIDATED_TOKEN MASK_PREVIEW
    if GIT_TERMINAL_PROMPT=0 gitx push --dry-run "$REMOTE" "$BRANCH" >/dev/null 2>&1; then
      info "已保存，写探针通过。以后直接：$0（可加 --tag vX.Y.Z）"
    else
      info "已保存，但写探针失败——检查 token 是否含**本仓库** Contents:write、或网络/TLS（可 GIT_BIN=<openssl-git>）。"
    fi
    ;;

  push)
    has_credential || die "本仓库无凭据（$CRED_FILE）。请让用户先跑：$0 --setup-token"
    GIT_TERMINAL_PROMPT=0 gitx ls-remote --heads "$REMOTE" >/dev/null 2>&1 \
      || die "ls-remote 失败（凭据/网络/TLS）。可 GIT_BIN=<openssl-git> 重跑，或 --setup-token。"
    ahead="$("$GIT_BIN" rev-list --count "$REMOTE/$BRANCH..$BRANCH" 2>/dev/null || echo 0)"
    if [ "${ahead:-0}" -gt 0 ]; then
      n=0
      until gitx push "$REMOTE" "$BRANCH"; do
        n=$((n+1)); [ "$n" -ge "$RETRY" ] && die "push $BRANCH 连续失败 $n 次（多为网络/TLS）。"
        info "push 失败，第 $((n+1))/$RETRY 次重试…"; sleep 1
      done
      info "已推送分支 $BRANCH（$ahead 个 commit）"
    else
      info "分支 $BRANCH 无待推送 commit（已同步）"
    fi
    if [ -n "$TAG" ]; then
      "$GIT_BIN" rev-parse -q --verify "refs/tags/$TAG" >/dev/null \
        || die "本地无 tag '$TAG'（先：git tag -a $TAG -m '...'）。"
      n=0
      until gitx push "$REMOTE" "$TAG"; do
        n=$((n+1)); [ "$n" -ge "$RETRY" ] && die "push tag $TAG 连续失败 $n 次。"
        info "push tag 失败，重试 $((n+1))/$RETRY…"; sleep 1
      done
      info "已推送 tag $TAG（建 Release 见 docs/GITHUB_PUSH.md）"
    fi
    ;;
esac
info "完成。"
