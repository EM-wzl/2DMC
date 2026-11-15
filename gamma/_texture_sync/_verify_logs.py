# -*- coding: utf-8 -*-
"""临时校验：日志里所有 'Failed to load texture' 路径现在是否都能解析。"""
import re
from pathlib import Path

pat = re.compile(r'Failed to load texture: "([^"]+)"')
names = {}
for lg in Path(r'Minecraft\logs').glob('*.log'):
    txt = lg.read_text(encoding='utf-8', errors='replace')
    for m in pat.finditer(txt):
        names[m.group(1)] = names.get(m.group(1), 0) + 1

print('distinct failed texture paths in logs:', len(names))
packs = [Path(r'Minecraft\texturepacks'), Path(r'gamma\texturepacks')]
still, fixed = [], 0
for rel in sorted(names):
    if rel.startswith('texturepacks/'):
        ok = any((p / rel[len('texturepacks/'):]).exists() for p in packs)
    else:
        ok = (Path('Minecraft') / rel).exists()
    fixed += 1 if ok else 0
    if not ok:
        still.append(rel)

print('now resolvable:', fixed, ' still missing:', len(still))
for s in still:
    print('  MISSING', s)
print('\ntop failures:')
for k, v in sorted(names.items(), key=lambda x: -x[1])[:10]:
    print(f'  {v:4d}  {k}')
