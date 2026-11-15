# -*- coding: utf-8 -*-
"""临时排查：items.lua 中相关条目及贴图名引用。"""
from pathlib import Path
import re

txt = Path(r'Minecraft\mods\default\items.lua').read_text(encoding='utf-8', errors='replace')
for name in ['netherite_axe', 'netherite_hoe', 'netherite_shovel', 'netherite_ingot',
             'nether_quartz', 'netherite_sword', 'netherite_pickaxe', 'netherite_scrap']:
    i = txt.find('"' + name + '"')
    print('---', name, 'idx', i)
    if i >= 0:
        print(txt[max(0, i - 150):i + 260].replace('\n', ' | '))
    else:
        print('NOT FOUND')

print()
print('=== 所有 png 引用 in items.lua (dedup) ===')
refs = sorted(set(re.findall(r'([A-Za-z0-9_\-]+\.png)', txt)))
print(len(refs), refs)
