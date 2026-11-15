#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""校验：所有 Lua 里被方块/物品引用的贴图文件，是否都存在于客户端实际读取的
texturepacks/minecraft/{blocks,items}/ 目录。

为什么需要它：客户端的 TextureAtlas::loadFromRegistry 遇到缺失贴图会抛
Exception，而 ServerLoadingState::update 里那个调用**没有捕获**（只有
texturePack 不存在时才有 catch），所以一张贴图缺失 = 进世界时客户端直接退出。
"""
import os
import re
import sys

ROOT = r"D:\cpp\2DMC"
MODS = os.path.join(ROOT, "Minecraft", "mods")
TP = os.path.join(ROOT, "Minecraft", "texturepacks", "minecraft")

# 匹配 tiles = "x.png"  或  tiles = {"a.png", "b.png", ...}
RE_TILES_STR = re.compile(r'tiles\s*=\s*"([^"]+\.png)"')
RE_TILES_TBL = re.compile(r'tiles\s*=\s*\{([^}]*)\}')


def collect_lua_files():
    out = []
    for base, _dirs, files in os.walk(MODS):
        for f in files:
            if f.endswith(".lua"):
                out.append(os.path.join(base, f))
    return out


def main():
    refs = {}  # filename -> [来源 lua]
    for path in collect_lua_files():
        try:
            text = open(path, encoding="utf-8", errors="ignore").read()
        except Exception:
            continue
        for m in RE_TILES_STR.finditer(text):
            refs.setdefault(m.group(1), []).append(os.path.basename(path))
        for m in RE_TILES_TBL.finditer(text):
            for s in re.findall(r'"([^"]+\.png)"', m.group(1)):
                refs.setdefault(s, []).append(os.path.basename(path))

    print("被引用的贴图总数: %d" % len(refs))

    missing = []
    for name, srcs in sorted(refs.items()):
        ok = os.path.exists(os.path.join(TP, "blocks", name)) or \
             os.path.exists(os.path.join(TP, "items", name)) or \
             os.path.exists(os.path.join(TP, "gui", name))
        if not ok:
            missing.append((name, sorted(set(srcs))))

    if not missing:
        print("OK: 所有被引用的贴图都在 texturepack 中")
        return 0

    print("缺失 %d 张：" % len(missing))
    for name, srcs in missing:
        print("  %-40s  <- %s" % (name, ", ".join(srcs)))
    return 1


if __name__ == "__main__":
    sys.exit(main())
