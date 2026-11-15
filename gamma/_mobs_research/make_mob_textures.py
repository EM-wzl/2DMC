#
# make_mob_textures.py — 从原版资源包实体 UV 展开图生成引擎可用的 32x32 方块贴图
#
# 引擎约束（gamma/source/client/graphics/TextureAtlas.cpp）：
#   * 所有方块贴图必须同尺寸正方形（图集 tile 尺寸取首个加载的贴图，本仓库为 32x32）
#   * 方块 tiles 只会从 mods/<mod>/textures/blocks/ 加载，因此成品必须写入 textures/blocks/
#
# 原版贴图为 UV 展开图（64x64）。经典箱式 UV 布局（已用像素采样验证）：
#   pig_temperate.png：head box @(0,0) 8x8x8（脸在 (8,8)-(16,16)），
#                      body box @(28,8) 10x16x8（top (36,8)-(46,16)，bottom (46,8)-(56,16)，
#                      front/side (36,16)-(46,32)）
#   zombie.png：head box @(0,0)（脸 (8,8)-(16,16)），body box @(16,16) 8x12x4
#               （top (20,16)-(28,20)，front (20,20)-(28,32)），leg box @(0,16)（front (4,20)-(8,32)）
#
# 拼装方式：裁剪区域 → 居中贴到方形画布（<=8px 用 8px 画布，否则 16px），空隙填充区域主色，
#           再 NEAREST 整数倍放大到 32x32（8px→4x，16px→2x，保持像素对齐）
#

from PIL import Image
import os
import shutil
from collections import Counter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VANILLA = os.path.join(REPO, "Default-Java-26.2", "assets", "minecraft", "textures", "entity")
GAMMA_MOD = os.path.join(REPO, "gamma", "mods", "default")
MC_MOD = os.path.join(REPO, "Minecraft", "mods", "default")
TILE = 32

REGIONS = {
    "pig": {
        "source": os.path.join(VANILLA, "pig", "pig_temperate.png"),
        "tiles": {
            "pig_top":    (36, 8, 46, 16),   # body top
            "pig_bottom": (46, 8, 56, 16),   # body bottom（肚皮）
            "pig_side":   (36, 16, 46, 32),  # body side
            "pig_face":   (8, 8, 16, 16),    # head face（鼻吻脸）
        },
    },
    "zombie": {
        "source": os.path.join(VANILLA, "zombie", "zombie.png"),
        "tiles": {
            "zombie_top":    (20, 16, 28, 20),  # body top（衣肩）
            "zombie_bottom": (4, 20, 8, 32),    # leg front（裤腿）
            "zombie_side":   (20, 20, 28, 32),  # body front（上衣）
            "zombie_face":   (8, 8, 16, 16),    # head face
        },
    },
}


def most_common_opaque_color(img):
    """取区域内最常见的不透明颜色，用作画布填充色。"""
    counts = Counter()
    px = img.load()
    for x in range(img.size[0]):
        for y in range(img.size[1]):
            p = px[x, y]
            if p[3] > 0:
                counts[p[:3]] += 1
    return counts.most_common(1)[0][0] if counts else (0, 0, 0)


def make_tile(region):
    """region: PIL Image → 32x32 RGBA tile。"""
    w, h = region.size
    side = 8 if max(w, h) <= 8 else 16
    fill = most_common_opaque_color(region)
    canvas = Image.new("RGBA", (side, side), fill + (255,))
    canvas.paste(region, ((side - w) // 2, (side - h) // 2), region)
    return canvas.resize((TILE, TILE), Image.NEAREST)


def main():
    made = []
    for mob, spec in REGIONS.items():
        sheet = Image.open(spec["source"]).convert("RGBA")
        for name, box in spec["tiles"].items():
            tile = make_tile(sheet.crop(box))
            out_blocks = os.path.join(GAMMA_MOD, "textures", "blocks", name + ".png")
            os.makedirs(os.path.dirname(out_blocks), exist_ok=True)
            tile.save(out_blocks)
            made.append(out_blocks)
            # 任务要求的 textures/entities/ 副本（引擎实际不从此目录加载方块贴图）
            out_entities = os.path.join(GAMMA_MOD, "textures", "entities", mob + "_" + name + ".png")
            os.makedirs(os.path.dirname(out_entities), exist_ok=True)
            tile.save(out_entities)
            made.append(out_entities)

    # 预览拼图（自查用）：两行 × 四列，每格 64x64（2x 放大显示）
    names = ["pig_top", "pig_bottom", "pig_side", "pig_face",
             "zombie_top", "zombie_bottom", "zombie_side", "zombie_face"]
    preview = Image.new("RGBA", (4 * 66, 2 * 66), (40, 40, 40, 255))
    for i, name in enumerate(names):
        img = Image.open(os.path.join(GAMMA_MOD, "textures", "blocks", name + ".png"))
        x = (i % 4) * 66 + 1
        y = (i // 4) * 66 + 1
        preview.paste(img.resize((64, 64), Image.NEAREST), (x, y))
    out_preview = os.path.join(REPO, "gamma", "_mobs_research", "mob_tiles_preview.png")
    os.makedirs(os.path.dirname(out_preview), exist_ok=True)
    preview.save(out_preview)
    made.append(out_preview)

    # 同步到 Minecraft/mods/default/
    for src in made:
        rel = os.path.relpath(src, GAMMA_MOD)
        if rel.startswith("_mobs_research"):
            continue
        dst = os.path.join(MC_MOD, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        print("synced:", rel)

    print("done,", len(made), "files")


if __name__ == "__main__":
    main()
