#!/usr/bin/env bash
# =============================================================================
# ghpush.sh — 通用 GitHub HTTPS 推送助手（一次配好凭据，之后一条命令推送）
#
#   设计目标（跨工程通用）：
#   · 用户只需一次 `--setup-token`：粘贴 fine-grained PAT（终端不回显），脚本负责
#     格式校验（防漏粘 / 重复粘贴 / 混入空格换行引号），并以 git credential 方式
#     安全落盘（绝不把 token 打进 URL、命令行、日志或 shell 历史）。
#   · Agent / 日常只需 `ghpush.sh`（可选 `--tag`）：用已存凭据推送，全程无需接触 token。
#   · 针对某些网络 HTTP/2 / gnutls 会 TLS 中断：默认 HTTP/1.1，可指定 openssl 版 git。
#
#   仅用系统自带：bash + git + coreutils。无需 gh / python。
#
# 用法：
#   scripts/ghpush.sh --setup-token             # 用户：录入/更新 token（隐藏输入）
#   scripts/ghpush.sh                           # 推送当前分支（体检→重试推）
#   scripts/ghpush.sh --tag v1.2.3              # 推送当前分支 + 附打/推该 tag
#   scripts/ghpush.sh --branch main --tag v1.2.3
#   scripts/ghpush.sh --check                   # 只体检：remote/分支/凭据是否在（不回显 token）
#   scripts/ghpush.sh --setup-token --username me   # 指定凭据 username（默认取 remote 的 owner）
#
# 可选环境变量：
#   GIT_BIN           git 路径（默认 git）。TLS 报 gnutls_handshake 时填 openssl 版 git。
#   GH_HTTP_VERSION   默认 HTTP/1.1；设为空串则不加 -c http.version。
#   GH_PUSH_REMOTE    默认 origin
#   GH_PUSH_RETRY     默认 5
#   GH_ALLOW_ANY_TOKEN=1  跳过 token 白名单（GitHub 改格式时用）
# =============================================================================
set -euo pipefail

GIT_BIN="${GIT_BIN:-git}"
HTTP_VERSION="${GH_HTTP_VERSION:-HTTP/1.1}"
REMOTE="${GH_PUSH_REMOTE:-origin}"
RETRY="${GH_PUSH_RETRY:-5}"
ALLOW_ANY="${GH_ALLOW_ANY_TOKEN:-0}"

ACTION="push"; BRANCH=""; TAG=""; USERNAME=""

die()  { printf '\033[31m[ghpush] %s\033[0m\n' "$*" >&2; exit 1; }
info() { printf '\033[36m[ghpush] %s\033[0m\n' "$*"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --setup-token) ACTION="setup" ;;
    --check)       ACTION="check" ;;
    --branch)      BRANCH="${2:?--branch 需要值}"; shift ;;
    --tag)         TAG="${2:?--tag 需要值}"; shift ;;
    --username)    USERNAME="${2:?--username 需要值}"; shift ;;
    --help|-h)     sed -n '2,34p' "$0"; exit 0 ;;
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
else
  HOST="$(printf '%s'  "$REMOTE_URL" | sed -E 's#.*@([^:]+):.*#\1#')"
  OWNER="$(printf '%s' "$REMOTE_URL" | sed -E 's#.*:([^/]+)/.*#\1#')"
  info "remote 是 SSH URL；本助手面向 HTTPS+PAT。若坚持 SSH（deploy key），无需 --setup-token。"
fi
[ -n "$USERNAME" ] || USERNAME="$OWNER"
if [ -z "$BRANCH" ]; then
  BRANCH="$("$GIT_BIN" rev-parse --abbrev-ref HEAD)"
  [ "$BRANCH" != "HEAD" ] || die "处于 detached HEAD，请用 --branch 指定分支。"
fi

gitx() {
  if [ -n "$HTTP_VERSION" ]; then
    "$GIT_BIN" -c "http.version=$HTTP_VERSION" -c credential.helper=store "$@"
  else
    "$GIT_BIN" -c credential.helper=store "$@"
  fi
}

# 只读、非交互地判断是否已有该 host+username 的凭据
has_credential() {
  local out
  out="$(printf 'protocol=https\nhost=%s\nusername=%s\n\n' "$HOST" "$USERNAME" \
        | GIT_TERMINAL_PROMPT=0 gitx credential fill 2>/dev/null || true)"
  printf '%s' "$out" | grep -q '^password='
}

# 清洗 + 校验 token；成功则设 VALIDATED_TOKEN / MASK_PREVIEW，失败直接 die（在主 shell，能中止）
validate_token() {
  local raw="$1" tok hits
  tok="$(printf '%s' "$raw" | tr -d '\r\n')"          # 去掉换行/CR（Windows 剪贴板）
  tok="${tok#\"}"; tok="${tok%\"}"; tok="${tok#\'}"; tok="${tok%\'}"   # 去包裹引号
  tok="$(printf '%s' "$tok" | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//')"  # 去首尾空白
  [ -n "$tok" ] || die "token 为空。"
  printf '%s' "$tok" | grep -Eq '^[A-Za-z0-9_-]+$' \
    || die "token 含非法字符（空格/制表/中文/引号等）——多半没粘全或混入多余内容。"
  # 重复粘贴检测：已知前缀出现 >1 次
  hits="$(printf '%s' "$tok" | grep -Eo 'ghp_|github_pat_|gh[ousr]_' | wc -l | tr -d '[:space:]')"
  [ "$hits" -le 1 ] \
    || die "疑似重复粘贴（检测到 $hits 个 token 前缀）。请清空后只粘贴一次。"
  if [ "$ALLOW_ANY" != "1" ]; then
    # 白名单用正则（不是 case-glob）：经典 ghp_/ghX_ +36；fine-grained github_pat_+22_+59
    if printf '%s' "$tok" | grep -Eq '^(ghp_|gh[ousr]_)[A-Za-z0-9]{36}$' \
       || printf '%s' "$tok" | grep -Eq '^github_pat_[A-Za-z0-9]{22}_[A-Za-z0-9]{59}$'; then
      :
    else
      die "token 格式不识别（长度/前缀不符）。确认是从 GitHub 原样复制的 PAT？确要接受用 GH_ALLOW_ANY_TOKEN=1 重跑。"
    fi
  fi
  VALIDATED_TOKEN="$tok"
  MASK_PREVIEW="$(printf '%s…%s (len=%d)' "${tok:0:5}" "${tok: -4}" "${#tok}")"
}

# 保存凭据：先 reject 旧条目再 approve，避免 store 里堆多份/错值；随后清掉内存里的 token
store_credential() {
  local tok="$1"
  printf 'protocol=https\nhost=%s\nusername=%s\npassword=%s\n\n' "$HOST" "$USERNAME" "$tok" \
    | gitx credential reject  2>/dev/null || true
  printf 'protocol=https\nhost=%s\nusername=%s\npassword=%s\n\n' "$HOST" "$USERNAME" "$tok" \
    | gitx credential approve
  [ -f "${HOME}/.git-credentials" ] && chmod 600 "${HOME}/.git-credentials" || true
}

case "$ACTION" in
  check)
    info "remote : $REMOTE -> https://$HOST/$OWNER/<repo>"
    info "branch : $BRANCH"
    info "git    : $GIT_BIN   http.version=${HTTP_VERSION:-<default>}"
    has_credential && info "credential: 已存在（user=$USERNAME）——可直接 push" \
                   || info "credential: 未找到（user=$USERNAME）——请先：$0 --setup-token"
    gitx ls-remote --heads "$REMOTE" "$BRANCH" >/dev/null 2>&1 \
      && info "ls-remote : 成功（凭据+网络 OK）" \
      || info "ls-remote : 失败（凭据缺失/网络/TLS；TLS 可 GIT_BIN=openssl-git，网络多重试）"
    ;;

  setup)
    info "为 https://$HOST (user=$USERNAME) 配置 fine-grained PAT。"
    info "GitHub → Settings → Developer settings → Fine-grained tokens："
    info "  · Scope 只勾本仓库；Permissions：Contents = Read and write"
    info "    （要建 Release 再给仓库加 Metadata = Read and write，或用下方 API 另配）"
    info "复制 token 后在下面粘贴——输入不回显，粘好直接回车。"
    local tok
    read -rs -p "粘贴 GitHub token（隐藏）: " tok </dev/tty || die "读取输入失败（需终端交互）。"
    printf '\n'
    validate_token "$tok"
    info "校验通过：$MASK_PREVIEW"
    store_credential "$VALIDATED_TOKEN"
    unset tok VALIDATED_TOKEN MASK_PREVIEW
    if gitx ls-remote --heads "$REMOTE" "$BRANCH" >/dev/null 2>&1; then
      info "已保存且可访问 $REMOTE/$BRANCH。以后直接：$0（或加 --tag vX.Y.Z）"
    else
      info "已保存，但 ls-remote 失败——检查 token 是否含本仓库 Contents:write，或网络/TLS。"
    fi
    ;;

  push)
    has_credential || die "未检测到 $HOST 的凭据（user=$USERNAME）。请让用户先跑：$0 --setup-token"
    info "precheck: ls-remote $REMOTE/$BRANCH ..."
    gitx ls-remote --heads "$REMOTE" "$BRANCH" >/dev/null 2>&1 \
      || die "precheck 失败（凭据/网络/TLS）。可 GIT_BIN=<openssl-git> 重跑，或先 $0 --setup-token。"

    ahead="$("$GIT_BIN" rev-list --count "$REMOTE/$BRANCH..$BRANCH" 2>/dev/null || echo 0)"
    if [ "${ahead:-0}" -gt 0 ]; then
      n=0
      until gitx push "$REMOTE" "$BRANCH"; do
        n=$((n+1))
        [ "$n" -ge "$RETRY" ] && die "push $BRANCH 连续失败 $n 次，中止（多为网络/TLS）。"
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
