# 探针：核实四种生物原版贴图的候选 UV 区域（按 Minecraft 经典箱式 UV 假设采样，用主色验证假设）
from PIL import Image
import os
from collections import Counter

VANILLA = r"D:\cpp\2DMC\Default-Java-26.2\assets\minecraft\textures\entity"

REGIONS = {
    "sheep/sheep.png":   [("head_face", (8, 8, 14, 14)), ("head_top", (8, 0, 14, 8)),
                    ("body_top", (34, 8, 42, 14)), ("body_bottom", (42, 8, 50, 14)),
                    ("body_front", (34, 14, 42, 30))],
    "sheep/sheep_wool.png": [("wool_top", (34, 8, 42, 14)), ("wool_bottom", (42, 8, 50, 14)),
                       ("wool_front", (34, 14, 42, 30)), ("wool_headface", (8, 8, 14, 14))],
    "spider/spider.png":  [("head_front", (38, 10, 46, 18)), ("head_top", (38, 4, 46, 10)),
                    ("body_top", (10, 12, 20, 22)), ("body_bottom", (20, 12, 30, 22)),
                    ("body_front", (10, 22, 20, 30))],
    "spider/spider_eyes.png": [("eyes_front", (38, 10, 46, 18))],
    "creeper/creeper.png": [("head_face", (8, 8, 16, 16)), ("head_top", (8, 0, 16, 8)),
                    ("body_top", (20, 16, 28, 20)), ("body_front", (20, 20, 28, 32))],
    "piglin/zombified_piglin.png": [("head_face", (8, 8, 16, 16)), ("head_top", (8, 0, 16, 8)),
                             ("body_top", (20, 16, 28, 20)), ("body_front", (20, 20, 28, 32)),
                             ("body_bottom", (20, 32, 28, 36))],
}

def stats(img, box):
    region = img.crop(box).convert("RGBA")
    counts = Counter()
    alpha_sum, n = 0, 0
    for x in range(region.size[0]):
        for y in range(region.size[1]):
            p = region.getpixel((x, y))
            alpha_sum += p[3]
            n += 1
            if p[3] > 0:
                counts[p[:3]] += 1
    top = ", ".join(f"{c}x{n_}" for c, n_ in counts.most_common(4))
    return f"avgA={alpha_sum//n:3d} top: {top}"

for fname, regions in REGIONS.items():
    img = Image.open(os.path.join(VANILLA, fname)).convert("RGBA")
    print(f"== {fname} {img.size}")
    for name, box in regions:
        print(f"  {name:12s} {box}: {stats(img, box)}")
