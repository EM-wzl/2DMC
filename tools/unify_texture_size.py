#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把所有方块贴图统一成同一尺寸（引擎 TextureAtlas 要求全部贴图尺寸一致）。

崩溃现象：
    Fatal error at TextureAtlas.cpp:127:
    Texture size unexpected for texturepacks/minecraft/blocks/debug/f.png.
    Got 16 16 instead of 32 32

原因：原有的方块贴图是 32x32，而后来从 MC 实体图集裁切出来的生物贴图是
16x16，两种尺寸混在同一张图集里 -> 图集构建时直接抛异常 -> 客户端进世界即退出。

做法：以目录里**出现最多的尺寸**为准（这里是 32x32），把其它尺寸用最近邻
放大/缩小到该尺寸。最近邻能保持像素画风格，不会糊。
"""
import glob
import os
import sys
from collections import Counter

from PIL import Image

DIRS = [
    r"D:\cpp\2DMC\gamme_placeholder",  # 占位，实际用下面的
]

TARGET_DIRS = [
    r"D:\cpp\2DMC\gamma\mods\default\textures\blocks",
    r"D:\cpp\2DMC\gamma\mods\default\textures\items",
    r"D:\cpp\2DMC\Minecraft\mods\default\textures\blocks",
    r"D:\cpp\2DMC\Minecraft\mods\default\textures\items",
    r"D:\cpp\2DMC\Minecraft\texturepacks\minecraft\blocks",
    r"D:\cpp\2DMC\Minecraft\texturepacks\minecraft\items",
]

# 这些子目录是调试用贴图，单独处理（不参与统一，但也不能破坏图集）
SKIP_SUBDIR = {"debug"}


def collect(d):
    out = []
    for base, dirs, files in os.walk(d):
        if os.path.basename(base) in SKIP_SUBDIR:
            continue
        for f in files:
            if f.endswith(".png"):
                out.append(os.path.join(base, f))
    return out


def main():
    # 1) 统计主尺寸（只看 blocks 目录，items 单独统计）
    for root in (r"D:\cpp\2DMC\Minecraft\texturepacks\minecraft\blocks",
                 r"D:\cpp\2DMC\Minecraft\texturepacks\minecraft\items"):
        if not os.path.isdir(root):
            continue
        sizes = Counter()
        for p in collect(root):
            try:
                sizes[Image.open(p).size] += 1
            except Exception:
                pass
        if not sizes:
            continue
        # 强制 32x32：项目原有贴图都是 32x32，TextureAtlas 以第一张为准，
        # 统一到 32x32 既不损失原有画质，也不会把新贴图缩糊。
        target = (32, 32)
        print("== %s ==" % os.path.basename(root))
        print("   当前尺寸分布: %s" % dict(sizes))
        print("   统一目标尺寸: %s" % (target,))

        changed = 0
        for p in collect(root):
            try:
                im = Image.open(p).convert("RGBA")
            except Exception as e:
                print("   读取失败 %s: %s" % (p, e))
                continue
            if im.size == target:
                continue
            im2 = im.resize(target, Image.NEAREST)
            im2.save(p)
            changed += 1
        print("   调整了 %d 张" % changed)

    # 2) 把所有目录里同名的贴图按 texturepack 的尺寸对齐（保证 gamma/Minecraft/贴图包三处一致）
    print("== 同步 mods 与 texturepack 尺寸 ==")
    tp_blocks = r"D:\cpp\2DMC\Minecraft\texturepacks\minecraft\blocks"
    tp_items = r"D:\cpp\2DMC\Minecraft\texturepacks\minecraft\items"
    for dst_root, ref_root in [
        (r"D:\cpp\2DMC\Minecraft\mods\default\textures\blocks", tp_blocks),
        (r"D:\cpp\2DMC\gamma\mods\default\textures\blocks", tp_blocks),
        (r"D:\cpp\2DMC\Minecraft\mods\default\textures\items", tp_items),
        (r"D:\cpp\2DMC\gamma\mods\default\textures\items", tp_items),
    ]:
        if not os.path.isdir(dst_root) or not os.path.isdir(ref_root):
            continue
        fixed = 0
        for p in collect(dst_root):
            ref = os.path.join(ref_root, os.path.basename(p))
            if not os.path.exists(ref):
                continue
            want = Image.open(ref).size
            try:
                im = Image.open(p).convert("RGBA")
            except Exception:
                continue
            if im.size != want:
                im.resize(want, Image.NEAREST).save(p)
                fixed += 1
        print("   %s: 修正 %d 张" % (os.path.basename(os.path.dirname(dst_root)) + "/" + os.path.basename(dst_root), fixed))

    # 3) 最终校验
    print("== 最终校验 ==")
    ok = True
    for root in (tp_blocks, tp_items):
        if not os.path.isdir(root):
            continue
        sizes = Counter()
        for p in collect(root):
            try:
                sizes[Image.open(p).size] += 1
            except Exception:
                pass
        print("   %s: %s" % (os.path.basename(root), dict(sizes)))
        if len(sizes) > 1:
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
