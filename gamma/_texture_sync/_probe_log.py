# -*- coding: utf-8 -*-
"""临时排查：分析日志中的加载失败分布与上下文。"""
import re
from pathlib import Path

log = Path(r'Minecraft\logs\openminer.log')
txt = log.read_text(encoding='utf-8', errors='replace')
lines = txt.splitlines()
idx = [i for i, l in enumerate(lines) if 'Failed to load texture' in l]
print('log lines:', len(lines), 'failure lines:', len(idx))
print('first failure line no:', idx[0] if idx else None, 'last:', idx[-1] if idx else None)

blocks, items = [], []
for i in idx:
    m = re.search(r'texturepacks/[^/]+/(blocks|items)/([^"]+)', lines[i])
    if m:
        (blocks if m.group(1) == 'blocks' else items).append(m.group(2))
print('blocks failures:', len(blocks), 'unique', len(set(blocks)))
print('items failures:', len(items), 'unique', len(set(items)))
print('\nitems unique:')
for n in sorted(set(items)):
    print('  ', n)

# 打印失败块前后的非失败行，看上下文（比如 texture pack 名、加载阶段）
lo, hi = max(0, idx[0] - 25), min(len(lines), idx[0] + 5)
print('\n--- context before first failure ---')
for l in lines[lo:hi]:
    print(l)
