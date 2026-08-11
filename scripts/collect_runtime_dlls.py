#!/usr/bin/env python3
"""递归 ldd 收集非系统 DLL 到目标目录。"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path


def is_system(path: str) -> bool:
    p = path.replace("\\", "/").lower()
    return any(m in p for m in ("/windows/", "system32", "winsxs", "c:/windows/"))


def to_native(path: str) -> Path:
    """把 MSYS 路径转成 Python 可访问的路径。"""
    p = path.strip().replace("\\", "/")
    try:
        win = subprocess.check_output(["cygpath", "-w", p], text=True, stderr=subprocess.DEVNULL).strip()
        return Path(win)
    except Exception:
        pass
    # 手工兜底
    if re.match(r"^/[a-zA-Z]/", p):
        return Path(f"{p[1].upper()}:{p[2:]}")
    if p.startswith("/ucrt64/"):
        return Path("D:/msys64/ucrt64/" + p[len("/ucrt64/") :])
    if p.startswith("/mingw64/"):
        return Path("D:/msys64/mingw64/" + p[len("/mingw64/") :])
    return Path(p)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: collect_runtime_dlls.py <exe> <dist_dir>")
        return 2

    exe = to_native(sys.argv[1]).resolve()
    dist = to_native(sys.argv[2]).resolve()
    ucrt = to_native("/ucrt64/bin")

    needed: set[Path] = set()
    queue = [exe]
    seen: set[str] = set()

    while queue:
        cur = queue.pop()
        key = str(cur).lower()
        if key in seen:
            continue
        seen.add(key)
        try:
            out = subprocess.check_output(["ldd", str(cur)], text=True, stderr=subprocess.STDOUT)
        except Exception as exc:
            # ldd 在 MSYS 下更吃 POSIX 路径
            try:
                posix = subprocess.check_output(["cygpath", "-u", str(cur)], text=True).strip()
                out = subprocess.check_output(["ldd", posix], text=True, stderr=subprocess.STDOUT)
            except Exception as exc2:
                print(f"ldd fail {cur}: {exc2}")
                continue

        for line in out.splitlines():
            m = re.search(r"=>\s+(\S+)\s+\(", line)
            dep: Path | None = None
            if m:
                dep = to_native(m.group(1))
            elif "not found" in line:
                name = line.strip().split()[0]
                cand = ucrt / name
                if cand.exists():
                    dep = cand
                else:
                    print(f"MISSING {name}")
                    continue
            if dep is None:
                continue
            if is_system(str(dep)):
                continue
            if not dep.exists():
                print(f"missing file {dep}")
                continue
            resolved = dep.resolve()
            needed.add(resolved)
            queue.append(resolved)

    copied = 0
    for p in sorted(needed):
        if p.name.lower() == exe.name.lower():
            continue
        dest = dist / p.name
        if (not dest.exists()) or dest.stat().st_size != p.stat().st_size:
            shutil.copy2(p, dest)
            copied += 1
            print(f"  + {p.name}")

    print(f"deps: {len(needed)}, copied: {copied}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
