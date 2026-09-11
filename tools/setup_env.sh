#!/usr/bin/env bash
# =============================================================================
# setup_env.sh — 建/修复 AudioVisExport 的**主机侧辅助** python 虚拟环境
#
#   为什么单独建：这些包（minidump / pillow）只服务于人工排障与做素材，
#   装进 conda base 会污染环境、且换机器就丢；工程本身（CMake/Ninja/clang-cl）
#   完全不依赖 python，所以这里做的是一套独立、可一键重建的小 venv。
#
#   为什么放仓库**外面**：venv 里有成千上万个 .py，落在工程目录会污染
#   find/grep（协作 AI 每次搜代码都要分辨哪些是自己的）。放在工程同级目录，
#   仓库里只留 tools/*.py 这几个"我们自己的"脚本。
#
#   为什么用 venv 而不是再开一个 conda env：conda 环境会复制整套解释器+库目录
#   （几百 MB 起）；venv 只是个薄壳，装多少算多少。需要非 python 二进制时才值得
#   用 conda（例：gitenv 里那个 openssl 版 git）。
#
#   环境选型的**权威说明**是 ~/CodingProgram/PYTHON_ENVIRONMENT.md（用户维护）：
#   base 只管 conda 不装包；日常通用环境是 common311；venv 首选基于**系统 python**
#   （不依赖 conda，文档"方式 A"），本脚本即按该优先级挑选解释器。
#
# 用法：  bash tools/setup_env.sh            # 建环境 + 装依赖（幂等，可重复跑）
#         source tools/setup_env.sh --print  # 只打印激活路径，不动任何东西
# =============================================================================
set -euo pipefail

VENV_DIR="${AVX_TOOLS_VENV:-$HOME/CodingProgram/AudioVisualizer/tools-venv}"
REQ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/requirements.txt"

if [[ "${1:-}" == "--print" ]]; then
    echo "source $VENV_DIR/bin/activate"
    exit 0
fi

# 挑一个能用 ensurepip 的解释器，按 PYTHON_ENVIRONMENT.md 的优先级：
#   ① 系统 python3（文档"方式 A"，与 conda 完全解耦）
#   ② common311（用户的日常通用 conda 环境，3.11）
#   ③ conda base 的 python（最后兜底；venv 与 base 隔离，不会把包装进 base）
pick_python() {
    for p in python3 "$HOME/miniconda3/envs/common311/bin/python" \
             "$HOME/miniconda3/bin/python" /usr/bin/python3 python; do
        command -v "$p" >/dev/null 2>&1 || continue
        if "$p" -c "import ensurepip" >/dev/null 2>&1; then echo "$p"; return 0; fi
    done
    return 1
}

if [[ ! -x "$VENV_DIR/bin/python" ]]; then
    PY="$(pick_python || true)"
    if [[ -z "${PY:-}" ]]; then
        cat >&2 <<'ERR'
[setup_env] 找不到带 ensurepip 的 python。任选其一：
  sudo apt-get install -y python3-venv        # 推荐（系统 python）
  conda create -n avxtools python=3.11 -y     # 或走 conda，然后 AVX_TOOLS_VENV 指过去
ERR
        exit 1
    fi
    echo "[setup_env] 用 $("$PY" -V 2>&1) ($PY) 创建 venv → $VENV_DIR"
    "$PY" -m venv "$VENV_DIR"
else
    echo "[setup_env] venv 已存在：$VENV_DIR"
fi

# Ubuntu 的 python3.10-venv 缺 ensurepip 时会建出没有 pip 的 venv → 补一次
if ! "$VENV_DIR/bin/python" -m pip --version >/dev/null 2>&1; then
    echo "[setup_env] venv 内无 pip，执行 ensurepip"
    "$VENV_DIR/bin/python" -m ensurepip --upgrade
fi

"$VENV_DIR/bin/python" -m pip install --quiet --upgrade pip
"$VENV_DIR/bin/python" -m pip install --quiet -r "$REQ"

echo "[setup_env] 已装入依赖："
"$VENV_DIR/bin/python" -m pip list --format=columns 2>/dev/null | grep -iE "minidump|pillow" || true
echo ""
echo "激活：  source $VENV_DIR/bin/activate"
echo "用法：  $VENV_DIR/bin/python tools/dmp_report.py <crash.dmp> --map build_win/AudioVisGUI.map"
