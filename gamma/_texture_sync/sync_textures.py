#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""同步自定义材质到 16x16 材质包（只补缺，绝不覆盖）。

背景
----
`Minecraft/mods/default/textures/{blocks,items}` 里的自定义贴图是 32x32，
而 `Minecraft/texturepacks/minecraft/{blocks,items}` 是 16x16 的材质包。
引擎的 TextureAtlas 要求同一图集内所有贴图尺寸一致，因此不能直接复制
32x32 的图进去，必须用 NEAREST 缩放到 16x16 再写入。

规则
----
1. 递归扫描源目录（mods/default/textures/blocks、items）下的每个 .png。
2. 若目标材质包目录中**不存在同名（同相对路径）文件**，则 NEAREST 缩放到
   16x16 并写入；已存在的文件绝不覆盖。
3. 目标材质包：
   - Minecraft/texturepacks/minecraft/{blocks,items}（必须存在）
   - gamma/texturepacks/minecraft/{blocks,items}（仅当 gamma 包存在时才处理，
     不存在则忽略，不新建整个目录）
4. 最后用 Pillow 校验目标目录所有 png 都是 16x16，并列出仍然缺失的文件。

用法
----
    python gamma/_texture_sync/sync_textures.py            # 执行补齐
    python gamma/_texture_sync/sync_textures.py --dry-run  # 只统计，不写文件
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image

# Windows 控制台默认 GBK/936，统一输出 UTF-8 避免中文乱码（失败则保持原样）。
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

TARGET_SIZE = (16, 16)
PNG_SUFFIX = ".png"

# 脚本位于 <repo>/gamma/_texture_sync/sync_textures.py
REPO_ROOT = Path(__file__).resolve().parents[2]

SOURCE_DIRS = {
    "blocks": REPO_ROOT / "Minecraft" / "mods" / "default" / "textures" / "blocks",
    "items": REPO_ROOT / "Minecraft" / "mods" / "default" / "textures" / "items",
}

# (显示名, 目录)。gamma 包整体不存在时整条跳过，不创建目录。
TARGET_PACKS = [
    ("Minecraft", REPO_ROOT / "Minecraft" / "texturepacks" / "minecraft"),
    ("gamma", REPO_ROOT / "gamma" / "texturepacks" / "minecraft"),
]


def iter_source_pngs(src_dir: Path):
    """递归产出源目录下所有 png，返回 (相对路径, 绝对路径)。"""
    for path in sorted(src_dir.rglob("*")):
        if path.is_file() and path.suffix.lower() == PNG_SUFFIX:
            yield path.relative_to(src_dir), path


def collect_missing(src_dir: Path, dst_dir: Path):
    """返回目标目录中缺失的 (相对路径, 源绝对路径) 列表。"""
    missing = []
    for rel, src in iter_source_pngs(src_dir):
        if not (dst_dir / rel).exists():
            missing.append((rel, src))
    return missing


def resize_nearest(src: Path, dst: Path) -> None:
    """把 src 以 NEAREST 缩放到 16x16 写入 dst（保留透明通道）。"""
    with Image.open(src) as im:
        im.load()
        if im.size == TARGET_SIZE:
            out = im.copy()
        else:
            out = im.convert("RGBA").resize(TARGET_SIZE, Image.NEAREST)
        dst.parent.mkdir(parents=True, exist_ok=True)
        out.save(dst, format="PNG")


def verify_pack(name: str, pack_dir: Path) -> list[str]:
    """校验材质包目录下所有 png 都是 16x16，返回异常描述列表。"""
    problems = []
    total = 0
    for sub in ("blocks", "items"):
        d = pack_dir / sub
        if not d.is_dir():
            problems.append(f"{name}/{sub}: 目录不存在")
            continue
        for png in sorted(d.rglob("*.png")):
            total += 1
            try:
                with Image.open(png) as im:
                    if im.size != TARGET_SIZE:
                        problems.append(f"{png.relative_to(REPO_ROOT)}: {im.size} != 16x16")
            except Exception as exc:  # noqa: BLE001
                problems.append(f"{png.relative_to(REPO_ROOT)}: 无法读取 ({exc})")
    print(f"  校验 {name}: 共 {total} 个 png，异常 {len(problems)} 个")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description="补缺自定义材质并缩放到 16x16（不覆盖已有文件）")
    parser.add_argument("--dry-run", action="store_true", help="只统计缺失数量，不写文件")
    args = parser.parse_args()

    print(f"仓库根目录: {REPO_ROOT}")
    if not SOURCE_DIRS["blocks"].is_dir() or not SOURCE_DIRS["items"].is_dir():
        print("错误: 找不到源贴图目录 Minecraft/mods/default/textures/{blocks,items}", file=sys.stderr)
        return 1

    total_missing = 0
    total_added = 0
    failures: list[str] = []
    # (pack_name, sub, 已补文件名列表)
    added_report: list[tuple[str, str, list[str]]] = []
    remaining: list[tuple[str, str, str]] = []

    for pack_name, pack_dir in TARGET_PACKS:
        if not pack_dir.is_dir():
            print(f"跳过 {pack_name}: {pack_dir} 不存在（不新建整个目录）")
            continue

        for sub, src_dir in SOURCE_DIRS.items():
            dst_dir = pack_dir / sub
            if not dst_dir.is_dir():
                print(f"跳过 {pack_name}/{sub}: 目标目录不存在")
                continue

            missing = collect_missing(src_dir, dst_dir)
            total_missing += len(missing)
            added: list[str] = []

            for rel, src in missing:
                dst = dst_dir / rel
                if args.dry_run:
                    added.append(str(rel).replace("\\", "/"))
                    continue
                try:
                    resize_nearest(src, dst)
                    added.append(str(rel).replace("\\", "/"))
                except Exception as exc:  # noqa: BLE001
                    failures.append(f"{pack_name}/{sub}/{rel}: {exc}")

            if not args.dry_run:
                total_added += len(added)
                added_report.append((pack_name, sub, added))

            # 补完后复查，仍缺失的记录下来
            for rel, _src in collect_missing(src_dir, dst_dir):
                remaining.append((pack_name, sub, str(rel).replace("\\", "/")))

            print(f"{pack_name}/{sub}: 缺失 {len(missing)} 个，已补 {0 if args.dry_run else len(added)} 个")

    print()
    if args.dry_run:
        print(f"缺失 {total_missing} 个，已补 0 个（dry-run，未写文件）")
    else:
        print(f"缺失 {total_missing} 个，已补 {total_added} 个")

    if added_report:
        print("\n补齐明细:")
        for pack_name, sub, added in added_report:
            print(f"  [{pack_name}/{sub}] {len(added)} 个")
            for name in added:
                print(f"    + {name}")

    if failures:
        print("\n写入失败:")
        for f in failures:
            print(f"  ! {f}")

    print("\n=== 尺寸校验 ===")
    problems: list[str] = []
    for pack_name, pack_dir in TARGET_PACKS:
        if pack_dir.is_dir():
            problems.extend(verify_pack(pack_name, pack_dir))

    print("\n=== 仍然缺失 ===")
    if remaining:
        for pack_name, sub, rel in remaining:
            print(f"  缺失 {pack_name}/{sub}/{rel}")
    else:
        print("  0 个（全部已补齐）")

    print("\n=== 结果 ===")
    print(f"缺失 {total_missing} 个，已补 {total_added if not args.dry_run else 0} 个，"
          f"仍缺 {len(remaining)} 个，尺寸异常 {len(problems)} 个，写入失败 {len(failures)} 个")
    if problems:
        for p in problems:
            print(f"  尺寸异常: {p}")
    for f in failures:
        print(f"  写入失败: {f}")

    return 0 if not (remaining or problems or failures) else 1


if __name__ == "__main__":
    raise SystemExit(main())
