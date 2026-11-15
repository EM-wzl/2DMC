#
# make_mob_textures2.py — 第二批生物（sheep/spider/creeper/zombified_piglin）32x32 方块贴图
#
# 与第一批 make_mob_textures.py 相同的引擎约束与拼装方式：
#   * TextureAtlas 要求全图集同尺寸正方形 tile（32x32）
#   * 成品必须写入 mods/default/textures/blocks/（引擎只从该目录加载方块贴图）
#   * 裁剪区域 → 居中贴到方形画布（<=8px 用 8px 画布，否则 16px），空隙填区域主色，
#     NEAREST 整数倍放大到 32x32
#
# UV 区域先用 probe_uv.py 像素探针验证过（主色与模型几何吻合）：
#   sheep.png/sheep_wool.png (64x32)：head 6x6x8 @(0,0)（脸 (8,8)-(14,14)，裸脸+粉鼻），
#       body 8x16x6 @(28,8)（top (34,8)-(42,14)，bottom (42,8)-(50,14)，front (34,14)-(42,30)）
#   spider.png (64x32)：head 8x8x6 @(32,4)（脸 (38,10)-(46,18)），body 10x8x10 @(0,12)
#       （top (10,12)-(20,22)，bottom (20,12)-(30,22)，front (10,22)-(20,30)）
#   spider_eyes.png：红眼覆盖层，与 spider.png 脸同区域 (38,10)-(46,18)，需 alpha 合成
#   creeper.png (64x32)：head 8x8x8 @(0,0)（脸 (8,8)-(16,16)，top (8,0)-(16,8)），
#       body 8x12x4 @(16,16)（top (20,16)-(28,20)，bottom (28,16)-(36,20)，front (20,20)-(28,32)）
#   zombified_piglin.png (64x64)：head 8x8x8 @(0,0)（脸 (8,8)-(16,16)），body 8x12x4 @(16,16)
#       （top (20,16)-(28,20)，front (20,20)-(28,32)），leg 4x12x4 @(0,16)（front (4,20)-(8,32)）
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

def src(name):
    return Image.open(os.path.join(VANILLA, *name.split("/"))).convert("RGBA")

SHEEP, SHEEP_WOOL = src("sheep/sheep.png"), src("sheep/sheep_wool.png")
SPIDER, SPIDER_EYES = src("spider/spider.png"), src("spider/spider_eyes.png")
CREEPER = src("creeper/creeper.png")
ZPIGLIN = src("piglin/zombified_piglin.png")

def blend(base, overlay):
    """alpha 合成两张同尺寸图（spider 脸 + 红眼层）。"""
    out = base.copy()
    out.alpha_composite(overlay)
    return out

REGIONS = {
    "sheep_top":    SHEEP_WOOL.crop((34, 8, 42, 14)),    # 羊毛体 top
    "sheep_bottom": SHEEP_WOOL.crop((42, 8, 50, 14)),    # 羊毛体 bottom
    "sheep_side":   SHEEP_WOOL.crop((34, 14, 42, 30)),   # 羊毛体侧
    "sheep_face":   SHEEP.crop((8, 8, 14, 14)),          # 裸脸（粉鼻）

    "spider_top":    SPIDER.crop((10, 12, 20, 22)),      # 腹部 top
    "spider_bottom": SPIDER.crop((20, 12, 30, 22)),      # 腹部 bottom
    "spider_side":   SPIDER.crop((10, 22, 20, 30)),      # 腹部侧
    "spider_face":   blend(SPIDER.crop((38, 10, 46, 18)),   # 头脸 + 红眼覆盖层
                           SPIDER_EYES.crop((38, 10, 46, 18))),

    "creeper_top":    CREEPER.crop((8, 0, 16, 8)),       # 头顶（绿色斑驳）
    "creeper_bottom": CREEPER.crop((28, 16, 36, 20)),    # 体 bottom
    "creeper_side":   CREEPER.crop((20, 20, 28, 32)),    # 体侧
    "creeper_face":   CREEPER.crop((8, 8, 16, 16)),      # 经典苦力怕脸

    "zpiglin_top":    ZPIGLIN.crop((20, 16, 28, 20)),    # 体 top
    "zpiglin_bottom": ZPIGLIN.crop((4, 20, 8, 32)),      # 腿 front
    "zpiglin_side":   ZPIGLIN.crop((20, 20, 28, 32)),    # 体侧
    "zpiglin_face":   ZPIGLIN.crop((8, 8, 16, 16)),      # 头脸
}

def most_common_opaque_color(img):
    counts = Counter()
    px = img.load()
    for x in range(img.size[0]):
        for y in range(img.size[1]):
            p = px[x, y]
            if p[3] > 0:
                counts[p[:3]] += 1
    return counts.most_common(1)[0][0] if counts else (0, 0, 0)

def make_tile(region):
    w, h = region.size
    side = 8 if max(w, h) <= 8 else 16
    fill = most_common_opaque_color(region)
    canvas = Image.new("RGBA", (side, side), fill + (255,))
    canvas.paste(region, ((side - w) // 2, (side - h) // 2), region)
    return canvas.resize((TILE, TILE), Image.NEAREST)

def main():
    made = []
    for name, region in REGIONS.items():
        tile = make_tile(region)
        out_blocks = os.path.join(GAMMA_MOD, "textures", "blocks", name + ".png")
        tile.save(out_blocks)
        made.append(out_blocks)
        # textures/entities/ 副本（与第一批一致；引擎实际不从此目录加载方块贴图）
        mob = name.rsplit("_", 1)[0]
        out_entities = os.path.join(GAMMA_MOD, "textures", "entities", mob + "_" + name + ".png")
        tile.save(out_entities)
        made.append(out_entities)

    # 预览拼图（自查用）：4 行 × 4 列，每格 64x64（2x 放大显示）
    names = list(REGIONS.keys())
    preview = Image.new("RGBA", (4 * 66, 4 * 66), (40, 40, 40, 255))
    for i, name in enumerate(names):
        img = Image.open(os.path.join(GAMMA_MOD, "textures", "blocks", name + ".png"))
        x, y = (i % 4) * 66 + 1, (i // 4) * 66 + 1
        preview.paste(img.resize((64, 64), Image.NEAREST), (x, y))
    out_preview = os.path.join(REPO, "gamma", "_mobs2_research", "mob_tiles_preview2.png")
    preview.save(out_preview)
    made.append(out_preview)

    # 同步到 Minecraft/mods/default/
    for f in made:
        rel = os.path.relpath(f, GAMMA_MOD)
        if rel.startswith("_mobs2_research"):
            continue
        dst = os.path.join(MC_MOD, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(f, dst)
        print("synced:", rel)

    print("done,", len(made), "files")

if __name__ == "__main__":
    main()
