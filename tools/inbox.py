#!/usr/bin/env python3
# =============================================================================
# inbox.py — docs/inbox/INBOX.md 的**唯一**编辑入口（v0.5.5 新 #4：读+删原子化）
#
#   背景（用户明确要求）：INBOX.md 由用户随时手改，AI "先删后读"或"边改边并行删"极易
#   误删用户刚写的新条目（本轮就因两条脚本并行改同一处险些丢数据）。故把"读"与"删"
#   绑成一个不可分割的动作：AI 只能通过本工具读、只能通过本工具删，删除必须提交
#   "它刚才读到的原文"作为凭据，工具再读一遍磁盘、逐条核对后才删——用户改过的自动跳过。
#
# 规则（与本工具行为一一对应）：
#   · 首行「状态 1」→ 用户编辑中 → 本工具**只读、拒绝任何删除**。
#   · 首行「状态 0」→ 空闲 → 允许删，但仅限 --expect 中"当前文本与你读到的完全一致"的行。
#   · 删除前自动整文件备份到同目录 .INBOX.md.bak（该目录已 gitignore）。
#
# 用法：
#   python tools/inbox.py read
#   python tools/inbox.py prune \
#       --expect "1=A/B/C/D 通过。但是B图层列表" \
#       --expect "2=E我差不多懂了"
#     #   冒号左边是"编号前缀"（匹配 `^编号.` 的行），右边是你 read 到的该行**开头片段**；
#     #   只有当该行仍以该片段开头时才删；被用户改过 → 不删并打印 MISMATCH。
# 退出码：0 成功；2 = 状态为 1，拒绝删除（AI 应改为只读、把内容并入 todo，不动文件）。
# 依赖：仅标准库（不进 tools/requirements.txt，也不需要 venv）。
# =============================================================================
from __future__ import annotations

import argparse
import io
import os
import re
import sys

_DESC = "INBOX.md 唯一编辑入口：read 读全文；prune 凭'读到的原文'原子删已完成行"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INBOX = os.path.join(REPO, "docs", "inbox", "INBOX.md")
BACKUP = os.path.join(REPO, "docs", "inbox", ".INBOX.md.bak")


def read_raw() -> str:
    if not os.path.exists(INBOX):
        sys.exit("[inbox] 找不到 " + INBOX + "（三件套缺失需按 WORKLOG 头部模板重建）")
    return io.open(INBOX, "r", encoding="utf-8", newline="").read()


def status_of(text: str) -> int:
    first = text.split("\n", 1)[0]
    m = re.search(r"状态\s*([01])", first)
    if not m:
        raise SystemExit("[inbox] 首行没有「状态 0/1」标记，拒绝操作（文件可能已损坏/被换）")
    return int(m.group(1))


def emit(text: str) -> None:
    # 打印时去掉可能的 \r，方便 AI 读；写回时用原始串
    sys.stdout.write(text if text.endswith("\n") else text + "\n")


def cmd_read(_: argparse.Namespace) -> int:
    text = read_raw()
    print("=== INBOX.md（状态 %d）——以下为磁盘当前全文，逐字为准 ===" % status_of(text))
    emit(text)
    return 0


def cmd_prune(args: argparse.Namespace) -> int:
    text = read_raw()                       # 原子：先读
    st = status_of(text)
    if st == 1:
        print("[inbox] 状态=1（用户编辑中）→ 只读返回、**不删任何行**；请把下面内容并入 todo。")
        emit(text)
        return 2

    lines = text.split("\n")
    kept, deleted, mismatch = [], [], []

    # 解析 --expect "id=snippet"
    want = []
    for e in args.expect or []:
        if "=" not in e:
            sys.exit("[inbox] --expect 需形如 '编号=你读到的行首片段'：" + e)
        k, v = e.split("=", 1)
        want.append((k.strip(), v))

    consumed = set()
    for ln in lines:
        m = re.match(r"^\s*(\d+)\.", ln)
        hit = None
        if m:
            num = m.group(1)
            for k, snip in want:
                if k == num and (num, snip) not in consumed:
                    if snip and snip in ln:
                        hit = (num, k, "del")
                        consumed.add((num, snip))
                        break
                    hit = (num, k, "mismatch")   # 编号在、但内容被用户改过 → 绝不删
                    consumed.add((num, snip))
                    break
        if hit and hit[2] == "del":
            deleted.append(ln)
        elif hit and hit[2] == "mismatch":
            mismatch.append(ln)
            kept.append(ln)
        else:
            kept.append(ln)

    found_nums = {re.match(r"^\s*(\d+)\.", l).group(1)
                  for l in lines if re.match(r"^\s*(\d+)\.", l)}
    missing = [k for k, _ in want if k not in found_nums]

    out = "\n".join(kept)
    if out != text:
        io.open(BACKUP, "w", encoding="utf-8", newline="").write(text)   # 改前整文件备份
        io.open(INBOX, "w", encoding="utf-8", newline="").write(out)

    print("=== prune 结果 ===")
    print("删除 %d 行 / MISMATCH 保留 %d 行 / 未找到 %d 项；原文件已备份→ %s"
          % (len(deleted), len(mismatch), len(missing), os.path.relpath(BACKUP, REPO)))
    for l in deleted:  print("  DEL: " + l[:80])
    for l in mismatch: print("  MISMATCH(用户已改，未删): " + l[:80])
    for k in missing:  print("  MISSING(无此编号): " + k)
    print("=== 删除后的全文 ===")
    emit(out)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=_DESC)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("read").set_defaults(fn=cmd_read)
    pr = sub.add_parser("prune")
    pr.add_argument("--expect", action="append",
                    help="可重复：'编号=你read到的该行开头片段'")
    pr.set_defaults(fn=cmd_prune)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
