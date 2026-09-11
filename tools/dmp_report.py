#!/usr/bin/env python3
# =============================================================================
# dmp_report.py — 把 Windows 崩溃转储翻成"哪个函数的第几字节"
#
#   背景：本工程的 CrashReporter 只写 MiniDump + 一个很粗的 .txt（只有异常码和地址），
#   真正的定位要靠链接期 /MAP 文件。v0.5.4 我手工做过两次（compose 描边越界读 #1b 就是
#   这样定位到的），这个脚本把那套流程固化下来，别再靠记忆重做。
#
#   /MAP 怎么来（交叉构建时打开链接器映射输出）：
#     cmake -B build_win ... -DCMAKE_EXE_LINKER_FLAGS_INIT="/MAP"   （详见 ARCHITECTURE.md §7.8）
#     产物：build_win/AudioVisGUI.map（注意：map 是**每次链接**生成的，符号化必须用
#     和出事 exe **同一次构建**的 map，否则偏移会错位——脚本会比对两边大小并警告。）
#
# 用法：
#   tools/.../python tools/dmp_report.py <crash.dmp> [--map build_win/AudioVisGUI.map]
#   python tools/dmp_report.py crash_20260911_001832.dmp --map ../build_win/AudioVisGUI.map
# =============================================================================
from __future__ import annotations

import argparse
import bisect
import logging
import os
import re
import struct
import sys

logging.disable(logging.CRITICAL)   # minidump 读 PEB 失败会打整段 traceback；那是噪音，
                                    # 模块表/异常记录照样解析得出来（MiniDumpWithoutAuxiliaryState）

try:
    from minidump.minidumpfile import MinidumpFile
except ImportError:
    sys.exit("[dmp_report] 需要 minidump 包：先跑 `bash tools/setup_env.sh`，"
             "或用该 venv 的 python 运行本脚本")

# MSVC 链接器 map 的符号行： " 0001:00019a30  ?compose@SpectrumMask@@...  000000014001aa30  SpectrumMask.cpp.obj"
MAP_SYM = re.compile(r'^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{16})\s+(\S+)\s*$')
MAP_BASE = re.compile(r'Preferred load address is\s+([0-9a-f]{16})', re.I)
MAP_STAMP = re.compile(r'Timestamp is\s+([0-9a-f]{8})', re.I)


def demangle_msvc(sym: str) -> str:
    """`?compose@SpectrumMask@@YA...` → `SpectrumMask::compose`（只还原限定名，不解析参数表）。

    MSVC 规则：`?` 之后到**第一个 `@@`** 之间是 `名字@外层@更外层...`（由内向外），
    `@@` 之后才是调用约定与参数编码 —— 所以按 '@' 切开再反转即可。
    构造/析构/运算符（`??...`）与不认识的形态一律退回原文，避免瞎猜出假名字。
    """
    if not sym.startswith('?') or sym.startswith('??'):
        return sym
    body = sym[1:]
    end = body.find('@@')
    if end < 0:
        return sym
    parts = [p for p in body[:end].split('@') if p]
    if len(parts) < 2:
        return parts[0] if parts else sym
    return '::'.join(reversed(parts))


def load_map(path: str):
    # 注意：不能用 "base 还是默认值" 当作跳过表头的判据 —— 绝大多数 exe 的 preferred
    # base 就等于默认值 0x140000000，那样会把整个文件 continue 掉、解析出 0 个符号。
    base, stamp, base_seen = 0x140000000, None, False
    addrs, syms, objs = [], [], []
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            if not base_seen:
                m = MAP_BASE.search(line)
                if m:
                    base, base_seen = int(m.group(1), 16), True
                    continue
            if stamp is None:
                m = MAP_STAMP.search(line)
                if m:
                    stamp = m.group(1)
            m = MAP_SYM.match(line)
            if not m:
                continue
            va = int(m.group(4), 16)
            addrs.append(va - base)                 # 存成 RVA，便于和 dump 地址对齐
            syms.append(demangle_msvc(m.group(3)))
            objs.append(m.group(5))
    order = sorted(range(len(addrs)), key=lambda i: addrs[i])
    return base, stamp, [addrs[i] for i in order], [syms[i] for i in order], [objs[i] for i in order]


DESCRIPTION = "把 Windows 崩溃转储(.dmp) + 链接期 /MAP 翻成『哪个函数 + 偏移多少』"


def main() -> int:
    ap = argparse.ArgumentParser(description=DESCRIPTION)
    ap.add_argument('dmp')
    ap.add_argument('--map', dest='mapfile', default=None,
                    help='linker .map（默认在同目录与 ../build_win 里找同名 exe 的 map）')
    ap.add_argument('--exe', default=None, help='按名字挑模块（默认取 dump 里含主 exe 的那个）')
    args = ap.parse_args()

    mf = MinidumpFile.parse(args.dmp)
    if mf.exception is None or not mf.exception.exception_records:
        print("[dmp_report] 这个 dump 里没有异常记录（可能不是崩溃转储）")
        return 1

    rec = mf.exception.exception_records[0]
    er = rec.ExceptionRecord
    code = getattr(er.ExceptionCode, 'name', str(er.ExceptionCode))
    addr = int(er.ExceptionAddress)

    mods = mf.modules.modules if hasattr(mf.modules, 'modules') else []
    if not mods:
        print("[dmp_report] 解析不到模块表")
        return 1

    # 出事的那个 exe（名字里的时间戳能告诉我们是**哪一次部署**崩的）
    want = args.exe or 'AudioVis'
    host = next((m for m in mods if want.lower() in str(m.name).lower()), None)
    host = host or next((m for m in mods if m.inrange(addr)), None)
    if host is None:
        print("[dmp_report] 找不到包含异常地址 0x%x 的模块" % addr)
        return 1

    base = int(host.baseaddress)
    rva = addr - base
    # dump 里的模块名是 Windows 路径（反斜杠），Linux 的 os.path.basename 不认 → 自己切
    win_name = str(host.name).replace('\\', '/').rsplit('/', 1)[-1]

    print("dump      : %s" % os.path.abspath(args.dmp))
    txt = os.path.splitext(args.dmp)[0] + '.txt'
    if os.path.exists(txt):
        first = ''
        for line in open(txt, encoding='utf-8', errors='replace'):
            if line.startswith('build'):
                first = line.strip()
        if first:
            print("crash.txt : %s" % first)
    print("exception : %s" % code)
    print("thread    : %d" % int(rec.ThreadId))
    print("module    : %s" % win_name)
    print("            base=0x%x  size=0x%x  (→ 这是哪一次部署的文件名)" % (base, int(host.size)))
    print("addr      : 0x%x" % addr)
    print("RVA       : 0x%x" % rva)

    info = [int(x) for x in (er.ExceptionInformation or [])]
    if 'ACCESS_VIOLATION' in code and len(info) >= 2:
        kind = {0: 'READ', 1: 'WRITE', 8: 'EXECUTE/DEP'}.get(info[0], str(info[0]))
        print("AV        : %s at 0x%x%s" % (kind, info[1],
              "   <-- 空指针/近零地址" if info[1] < 0x10000 else ""))

    # ---- .map 定位 ----
    mapfile = args.mapfile
    if not mapfile:
        here = os.path.dirname(os.path.abspath(args.dmp))
        cand = []
        stem = win_name.split('_')[0].rsplit('.', 1)[0]              # AudioVisGUI_09110001.exe → AudioVisGUI
        for d in (here, '.', '../build_win', '../../build_win',
                  os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build_win')):
            cand.append(os.path.join(d, stem + '.map'))
        mapfile = next((c for c in cand if os.path.exists(c)), None)

    if not mapfile or not os.path.exists(mapfile or ''):
        print("\n[!] 没找到 .map（--map 指定，或交叉构建时打开 /MAP）；RVA 已给出，可自行查表")
        return 0

    mb, stamp, addrs, syms, objs = load_map(mapfile)
    print("map       : %s   (image base 0x%x, %d 个符号%s)"
          % (mapfile, mb, len(addrs), ", stamp %s" % stamp if stamp else ""))
    # map 必须是**出事那一次构建**的，否则偏移会静默错位 —— 用链接时间戳硬比对
    try:
        dump_stamp = int(host.timestamp)
        if stamp and dump_stamp:
            if dump_stamp == int(stamp, 16):
                print("stamp     : dump 与 map 时间戳一致 (0x%s) → 同一次构建，可放心采信" % stamp)
            else:
                print("[!] 警告：map 时间戳 0x%s != dump 模块时间戳 0x%x"
                      % (stamp, dump_stamp))
                print("    即：**不是同一次构建**。下面的符号只能当粗略参考，"
                      "要准确定位请用出事那版源码重新加 /MAP 链接一次。")
    except (TypeError, ValueError):
        pass

    i = bisect.bisect_right(addrs, rva) - 1
    if i < 0:
        print("            RVA 在 map 符号表之前（可能是 PE 头 / 导入 thunk）")
        return 0
    print("\n=> 定位  : %s + 0x%x" % (syms[i], rva - addrs[i]))
    print("   绝对VA: 0x%x   来自 %s" % (addrs[i] + mb, objs[i]))
    if i + 1 < len(addrs):
        print("   （符号区间 0x%x .. 0x%x，本地址落在其中%s）"
              % (addrs[i] + mb, addrs[i + 1] + mb,
                 "" if rva < addrs[i + 1] else " —— 注意：越过下一个符号，可能是内联/尾部"))
    print("\n下一步建议（用同一次构建的 exe，地址用绝对 VA）：")
    print("  llvm-objdump-18 -d --start-address=0x%x --stop-address=0x%x \"<同版本 exe>\""
          % (addrs[i] + mb, addrs[i] + mb + 0x400))
    return 0


if __name__ == '__main__':
    sys.exit(main())
