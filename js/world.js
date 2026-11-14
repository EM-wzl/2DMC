/* 2DMC Web — 三维度世界生成、光照、RLE 存档（纯逻辑） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, Core, DEF } = DMC;
  const { W, H } = C;
  const { clamp, fbm, noise2, hash2, makeRNG, idx, inWorld, rleEncode, rleDecode } = Core;

  const BIOMES = ['平原', '雪原', '斑驳林', '沙漠', '丛林', '樱花林', '红树林', '苍白之园'];
  const BIOME_NAMES = BIOMES;

  class World {
    constructor(seed) {
      this.seed = (seed >>> 0) || 20261114;
      this.planes = [new Uint16Array(W * H), new Uint16Array(W * H), new Uint16Array(W * H)];
      this.lights = [new Uint8Array(W * H), new Uint8Array(W * H), new Uint8Array(W * H)];
      this.dim = 0;
      this.heights = new Int16Array(W);
      this.biomes = new Uint8Array(W);
      this.endSpawn = { x: W >> 1, y: 80 };
      this.crystals = [];
      this.endPortal = { x: W >> 1, y: 80 };
      this.endReady = false;
      this.dragonDead = false;
      this.stronghold = { x: 0, y: 0 };
      this.generated = [false, false, false];
    }

    /* ---------- 基础存取（默认当前维度） ---------- */
    plane(dim) { return this.planes[dim === undefined ? this.dim : dim]; }
    lightPlane(dim) { return this.lights[dim === undefined ? this.dim : dim]; }
    get(x, y, dim) {
      if (!inWorld(x, y)) return B.BEDROCK;
      return this.plane(dim)[idx(x, y)];
    }
    getAt(dim, x, y) { return this.get(x, y, dim); }
    set(x, y, v, dim) {
      if (!inWorld(x, y)) return;
      const d = dim === undefined ? this.dim : dim;
      this.planes[d][idx(x, y)] = v;
      this.relight(x, y, d);
    }
    solid(x, y, dim) {
      const v = this.get(x, y, dim);
      return v !== 0 && !!DEF[v] && DEF[v].solid;
    }
    opaque(x, y, dim) {
      const v = this.get(x, y, dim);
      return v !== 0 && !!DEF[v] && DEF[v].opaque;
    }
    light(x, y, dim) {
      if (!inWorld(x, y)) return 0;
      return this.lightPlane(dim)[idx(x, y)];
    }
    hardness(x, y) { const d = DEF[this.get(x, y)]; return d ? d.hardness : 0; }

    /* ---------- 光照：列式天光 + 局部点光 ---------- */
    // 只重算若干列的天光（便宜，挖掘时每帧调用）
    relightColumns(x0, x1, dim) {
      const d = dim === undefined ? this.dim : dim;
      const p = this.planes[d], lp = this.lights[d];
      const ambient = d === 1 ? 7 : (d === 2 ? 10 : 15);
      const a = Math.max(0, x0), b = Math.min(W - 1, x1);
      for (let cx = a; cx <= b; cx++) {
        let sky = ambient;
        for (let cy = 0; cy < H; cy++) {
          const v = p[idx(cx, cy)];
          const def = DEF[v];
          let val = sky;
          if (def && def.light) val = Math.max(val, def.light);
          lp[idx(cx, cy)] = clamp(val, 0, 15);
          if (def && def.opaque) sky = Math.max(0, sky - (d === 0 ? 3 : 4));
        }
      }
    }
    // 把某个矩形区域的光按点光源重新叠加（只在发光方块变动时调用）
    addPointLights(x0, y0, x1, y1, dim) {
      const d = dim === undefined ? this.dim : dim;
      const p = this.planes[d], lp = this.lights[d];
      const r = 7;
      for (let cy = Math.max(0, y0 - r); cy <= Math.min(H - 1, y1 + r); cy++) {
        for (let cx = Math.max(0, x0 - r); cx <= Math.min(W - 1, x1 + r); cx++) {
          const def = DEF[p[idx(cx, cy)]];
          if (!def || !def.light) continue;
          const rad = def.light > 10 ? 7 : 5;
          for (let dy = -rad; dy <= rad; dy++) {
            for (let dx = -rad; dx <= rad; dx++) {
              const qx = cx + dx, qy = cy + dy;
              if (!inWorld(qx, qy)) continue;
              const dd = Math.abs(dx) + Math.abs(dy);
              if (dd > rad) continue;
              const k = idx(qx, qy);
              const val = def.light - dd;
              if (val > lp[k]) lp[k] = val;
            }
          }
        }
      }
    }
    relight(x, y, dim) {
      const d = dim === undefined ? this.dim : dim;
      const r = 9;
      this.relightColumns(x - r, x + r, d);
      this.addPointLights(x - r, y - r, x + r, y + r, d);
    }
    relightAll(dim) {
      const d = dim === undefined ? this.dim : dim;
      this.relightColumns(0, W - 1, d);
      this.addPointLights(0, 0, W - 1, H - 1, d);
    }

    /* ---------- 主世界生成 ---------- */
    generateOverworld() {
      if (this.generated[0]) return;
      const p = this.planes[0], rnd = makeRNG(this.seed);
      const S = this.seed;
      // 1. 高度图 + 群系
      for (let x = 0; x < W; x++) {
        const base = C.SURFACE
          - (fbm(x / 72, 0.3, S, 4) - 0.5) * 46
          - (fbm(x / 220, 1.7, S + 991, 3) - 0.5) * 34;
        this.heights[x] = clamp(Math.round(base), 48, 165);
        const bv = fbm(x / 150, 9.1, S + 313, 3);
        this.biomes[x] = bv < 0.30 ? 1 : bv < 0.40 ? 3 : bv < 0.50 ? 2 :
                         bv < 0.58 ? 4 : bv < 0.66 ? 5 : bv < 0.73 ? 6 :
                         bv < 0.80 ? 7 : 0;
      }
      // 2. 地形填充
      for (let x = 0; x < W; x++) {
        const h = this.heights[x], bio = this.biomes[x];
        const desert = bio === 3, snowy = bio === 1;
        for (let y = 0; y < H; y++) {
          const k = idx(x, y);
          if (y >= H - 2) { p[k] = B.BEDROCK; continue; }
          if (y < h) { p[k] = 0; continue; }
          const deep = y > 158;
          let v;
          if (y === h) v = desert ? B.SAND : snowy ? B.SNOW : B.GRASS;
          else if (y < h + 5) v = desert ? B.SAND : snowy ? B.DIRT : B.DIRT;
          else v = deep ? B.DEEPSLATE : B.STONE;
          // 洞穴与岩浆湖
          const cav = fbm(x / 19, y / 13, S + 7717, 3);
          if (y > h + 6 && y < H - 6 && cav > 0.635) v = 0;
          const cav2 = noise2(x / 34, y / 27, S + 4409);
          if (y > 175 && cav2 > 0.79) v = 0;
          // 矿脉
          if (v === B.STONE || v === B.DEEPSLATE) {
            const o = hash2(x, y, S + 55);
            const db = v === B.DEEPSLATE;
            if (o < 0.014 && y > h + 12) v = B.COAL_ORE;
            else if (o < 0.024 && y > h + 20) v = db ? B.IRON_ORE_DEEP : B.IRON_ORE;
            else if (o < 0.030 && y > 105) v = B.COPPER_ORE;
            else if (o < 0.034 && y > 130) v = db ? B.REDSTONE_ORE_DEEP : B.REDSTONE_ORE;
            else if (o < 0.037 && y > 125) v = B.LAPIS_ORE;
            else if (o < 0.040 && y > 150) v = B.GOLD_ORE;
            else if (o < 0.043 && y > 175) v = db ? B.DIAMOND_ORE_DEEP : B.DIAMOND_ORE;
            else if (o < 0.0448 && y > 190) v = B.EMERALD_ORE;
            else if (o < 0.047 && y > 200) v = B.SCULK;
          }
          // 26.2 硫磺洞穴
          if ((v === B.STONE || v === 0) && y > 150 && y < 220 &&
              fbm(x / 46, y / 30, S + 8823, 3) > 0.69) {
            v = hash2(x, y, S + 17) < 0.28 ? B.CINNABAR : B.SULFUR;
          }
          p[k] = v;
        }
        // 海 / 岩浆
        for (let y = h; y < C.SEA; y++) {
          const k = idx(x, y);
          if (p[k] === 0) p[k] = B.WATER;
        }
        if (h + 8 < H - 4) {
          for (let y = H - 22; y < H - 4; y++) {
            if (p[idx(x, y)] === 0 && hash2(x, y, S + 31) < 0.10) p[idx(x, y)] = B.LAVA;
          }
        }
      }
      // 3. 紫水晶晶洞
      for (let n = 0; n < 26; n++) {
        const cx = 30 + Math.floor(rnd() * (W - 60));
        const cy = 150 + Math.floor(rnd() * 70);
        const r = 3 + Math.floor(rnd() * 3);
        for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) {
          const x = cx + dx, y = cy + dy;
          if (!inWorld(x, y) || dx * dx + dy * dy > r * r) continue;
          const edge = dx * dx + dy * dy > (r - 1) * (r - 1);
          p[idx(x, y)] = edge ? B.AMETHYST : (hash2(x, y, S) < 0.14 ? B.AMETHYST_CLUSTER : 0);
        }
      }
      // 4. 植被 / 树
      for (let x = 4; x < W - 4; x++) {
        const h = this.heights[x], bio = this.biomes[x];
        if (h >= C.SEA) continue;
        const r = hash2(x, 3, S + 123);
        const treeRate = (bio === 3) ? 0.004 : (bio === 4 ? 0.055 : 0.024);
        if (r < treeRate) {
          const hgt = 4 + Math.floor(hash2(x, 5, S) * 3);
          let log = B.LOG, leaf = B.LEAVES;
          if (bio === 5) { log = B.CHERRY_LOG; leaf = B.CHERRY_LEAVES; }
          else if (bio === 6) { log = B.MANGROVE_LOG; leaf = B.MANGROVE_LEAVES; }
          else if (bio === 7) { log = B.PALE_LOG; leaf = B.PALE_LEAVES; }
          else if (bio === 2) { log = B.LOG; leaf = B.AZALEA; }
          for (let j = 1; j <= hgt; j++) if (inWorld(x, h - j)) p[idx(x, h - j)] = log;
          const cy = h - hgt;
          for (let dy = -2; dy <= 2; dy++) for (let dx = -3; dx <= 3; dx++) {
            const qx = x + dx, qy = cy + dy;
            if (!inWorld(qx, qy)) continue;
            if (dx * dx / 9 + dy * dy / 4 > 1.05) continue;
            if (p[idx(qx, qy)] === 0) p[idx(qx, qy)] = leaf;
          }
        } else if (bio === 3 && r < 0.035) {
          for (let j = 1; j <= 2 + Math.floor(hash2(x, 7, S) * 2); j++)
            if (inWorld(x, h - j)) p[idx(x, h - j)] = B.CACTUS;
        } else if (r > 0.965) {
          if (inWorld(x, h - 1)) p[idx(x, h - 1)] = bio === 1 ? B.SNOW : B.GOLD_DANDELION;
        }
        // 26.2 间歇泉 / 硫磺尖刺
        if (this.get(x, h, 0) === B.SULFUR && hash2(x, 9, S) < 0.03 && inWorld(x, h - 1))
          p[idx(x, h - 1)] = B.SULFUR_SPIKE;
      }
      // 5. 要塞：地下末地传送门框架（供龙蛋激活）
      const sx = clamp(Math.floor(W * 0.73), 20, W - 20), sy = 175;
      this.stronghold = { x: sx, y: sy };
      for (let dy = -3; dy <= 3; dy++) for (let dx = -5; dx <= 5; dx++) {
        const qx = sx + dx, qy = sy + dy;
        if (!inWorld(qx, qy)) continue;
        const border = Math.abs(dx) === 5 || Math.abs(dy) === 3;
        if (border) p[idx(qx, qy)] = B.STONE_BRICKS;
        else p[idx(qx, qy)] = 0;
      }
      for (let dx = -1; dx <= 1; dx++) for (let dy = -1; dy <= 1; dy++) {
        if (Math.abs(dx) === 1 && Math.abs(dy) === 1) continue;
        p[idx(sx + dx, sy + 1)] = B.END_PORTAL_FRAME;
      }
      p[idx(sx, sy)] = B.CHEST;
      p[idx(sx - 2, sy + 1)] = B.TORCH;
      this.generated[0] = true;
      this.relightAll(0);
    }

    /* ---------- 下界生成 ---------- */
    generateNether() {
      if (this.generated[1]) return;
      const p = this.planes[1], S = this.seed + 4242;
      for (let x = 0; x < W; x++) {
        for (let y = 0; y < H; y++) {
          const k = idx(x, y);
          if (y < 3 || y >= H - 2) { p[k] = B.BEDROCK; continue; }
          let v = B.NETHERRACK;
          if (y > 150 && fbm(x / 26, y / 22, S + 11, 3) > 0.58) v = 0;
          if (fbm(x / 40, y / 28, S + 71, 3) > 0.66) v = 0;
          if (v === B.NETHERRACK) {
            const o = hash2(x, y, S + 5);
            if (o < 0.010) v = B.NETHER_GOLD;
            else if (o < 0.0135 && y > 170) v = B.ANCIENT_DEBRIS;
            else if (o < 0.020 && y < 60) v = B.GLOWSTONE;
            else if (o < 0.028) v = B.SULFUR;
            else if (o < 0.033) v = B.CINNABAR;
            else if (o < 0.038 && y > 190) v = B.BASALT;
            else if (o < 0.042) v = B.MAGMA;
          }
          if (v === 0 && y > 205) v = B.LAVA;
          p[k] = v;
        }
        // 岩浆海
        for (let y = 214; y < H - 2; y++) if (p[idx(x, y)] === 0) p[idx(x, y)] = B.LAVA;
      }
      // 菌柄 / 菌光体
      for (let x = 8; x < W - 8; x += 3) {
        if (hash2(x, 2, S + 991) > 0.10) continue;
        const top = 40 + Math.floor(hash2(x, 4, S) * 70);
        const stem = hash2(x, 6, S) < 0.5 ? B.CRIMSON_STEM : B.WARPED_STEM;
        const hgt = 5 + Math.floor(hash2(x, 8, S) * 6);
        for (let j = 0; j < hgt; j++) {
          const y = top + j;
          if (!inWorld(x, y) || p[idx(x, y)] !== 0) continue;
          p[idx(x, y)] = stem;
        }
        for (let dx = -2; dx <= 2; dx++) for (let dy = -2; dy <= 0; dy++) {
          const qx = x + dx, qy = top + hgt + dy;
          if (!inWorld(qx, qy) || p[idx(qx, qy)] !== 0) continue;
          if (dx * dx + dy * dy > 5) continue;
          p[idx(qx, qy)] = hash2(qx, qy, S) < 0.12 ? B.SHROOMLIGHT : B.NETHER_SPROUTS;
        }
      }
      this.generated[1] = true;
      this.relightAll(1);
    }

    /* ---------- 末地生成 ---------- */
    generateEnd() {
      if (this.generated[2]) return;
      const p = this.planes[2], S = this.seed + 8888;
      const cx = W >> 1, baseY = 118;
      const RX = 95, RY = 20;
      p.fill(0);
      // 中央岛（椭圆台地）
      for (let dx = -RX - 22; dx <= RX + 22; dx++) {
        const x = cx + dx;
        if (!inWorld(x, 0)) continue;
        const t = 1 - (dx * dx) / ((RX + 22) * (RX + 22));
        if (t <= 0) continue;
        const ry = Math.max(3, Math.round(RY * (0.45 + 0.55 * t)));
        for (let y = baseY - ry; y <= baseY + ry; y++) {
          if (y < 1 || y >= H - 1) continue;
          p[idx(x, y)] = (y >= baseY + ry - 2) ? B.END_STONE_BRICKS : B.END_STONE;
        }
      }
      // 四根黑曜石柱 + 柱顶末影水晶
      const DX = [-54, 54, -54, 54], DY = [-12, -12, 12, 12];
      this.crystals = [];
      for (let i = 0; i < 4; i++) {
        const px = cx + DX[i], py = baseY + DY[i];
        const ph = 14 + Math.floor(hash2(px, 3, S) * 14);
        for (let y = py - ph; y <= py; y++) {
          if (!inWorld(px, y)) continue;
          p[idx(px, y)] = B.OBSIDIAN_PILLAR;
          if (inWorld(px - 1, y)) p[idx(px - 1, y)] = B.OBSIDIAN_PILLAR;
          if (inWorld(px + 1, y)) p[idx(px + 1, y)] = B.OBSIDIAN_PILLAR;
        }
        const ty = py - ph - 1;
        if (ty > 1) {
          p[idx(px, ty)] = B.END_CRYSTAL;
          this.crystals.push({ x: px, y: ty, alive: true });
        }
      }
      // 退出传送门 + 龙蛋
      const ex = cx, ey = baseY - RY - 6;
      this.endPortal = { x: ex, y: ey };
      for (let dx = -1; dx <= 1; dx++) for (let dy = 0; dy <= 1; dy++) {
        const qx = ex + dx, qy = ey + dy;
        if (inWorld(qx, qy)) p[idx(qx, qy)] = B.END_PORTAL_FRAME;
      }
      p[idx(ex, ey)] = B.END_PORTAL;
      if (ey - 1 > 0) p[idx(ex, ey - 1)] = B.DRAGON_EGG;
      // 外围岛屿链
      let ix = cx + (RX + 60);
      for (let k = 0; k < 8 && ix < W - 40; k++) {
        const rx = 14 + Math.floor(hash2(ix, 1, S) * 15);
        const ry = 8 + Math.floor(hash2(ix, 2, S) * 9);
        const iy = baseY + Math.floor((hash2(ix, 3, S) - 0.5) * 50);
        for (let dx = -rx; dx <= rx; dx++) {
          const x = ix + dx;
          if (!inWorld(x, 0)) continue;
          const t = 1 - dx * dx / (rx * rx);
          if (t <= 0) continue;
          const h = Math.round(ry * (0.45 + 0.55 * t));
          for (let y = iy - h; y <= iy; y++)
            if (y > 0 && y < H - 1) p[idx(x, y)] = B.END_STONE;
          if (inWorld(x, iy - h - 1) && hash2(x, 5, S) < 0.14)
            p[idx(x, iy - h - 1)] = hash2(x, 6, S) < 0.5 ? B.CHORUS_PLANT : B.END_ROD;
        }
        ix += rx + 22 + Math.floor(hash2(ix, 7, S) * 24);
      }
      // 中央岛紫颂植物
      for (let k = 0; k < 16; k++) {
        const x = cx + Math.floor((hash2(k, 11, S) - 0.5) * RX * 1.8);
        if (!inWorld(x, 0)) continue;
        let top = -1;
        for (let y = baseY - RY - 8; y < baseY; y++)
          if (p[idx(x, y)] !== 0) { top = y; break; }
        if (top > 2) {
          p[idx(x, top - 1)] = B.CHORUS_PLANT;
          if (hash2(x, 12, S) < 0.5 && top - 2 > 0) p[idx(x, top - 2)] = B.CHORUS_FLOWER;
        }
      }
      this.endSpawn = { x: cx, y: baseY - RY - 2 };
      this.generated[2] = true;
      this.endReady = true;
      this.relightAll(2);
    }

    generateAll() {
      this.generateOverworld(); this.generateNether(); this.generateEnd();
    }
    ensure(dim) {
      if (dim === 1) this.generateNether();
      else if (dim === 2) this.generateEnd();
      else this.generateOverworld();
    }
    switchTo(dim) {
      this.dim = clamp(dim | 0, 0, 2);
      this.ensure(this.dim);
    }
    crystalsAlive() {
      let n = 0;
      for (const c of this.crystals)
        if (c.alive && this.getAt(2, c.x, c.y) === B.END_CRYSTAL) n++;
      return n;
    }
    // 找某列从上往下的第一个实心方块上方（站立点）
    groundY(x, fromY) {
      const start = clamp(Math.floor(fromY) - 4, 1, H - 3);
      for (let y = start; y < H - 2; y++) {
        if (this.solid(x, y) && !this.solid(x, y - 1)) return y - 1;
      }
      return clamp(Math.floor(fromY), 1, H - 3);
    }
    spawnPoint() {
      if (this.dim === 2) return { x: this.endSpawn.x, y: this.endSpawn.y };
      if (this.dim === 1) {
        for (let y = 20; y < H - 4; y++)
          if (!this.solid(W >> 1, y) && this.solid(W >> 1, y + 1)) return { x: W >> 1, y: y - 1 };
        return { x: W >> 1, y: 40 };
      }
      return { x: W >> 1, y: clamp(this.heights[W >> 1] - 3, 4, H - 4) };
    }

    /* ---------- 存档 ---------- */
    serialize() {
      return {
        version: C.SAVE_VERSION,
        seed: this.seed,
        dim: this.dim,
        planes: this.planes.map(p => rleEncode(p)),
        heights: Array.from(this.heights),
        biomes: Array.from(this.biomes),
        endSpawn: this.endSpawn,
        endPortal: this.endPortal,
        crystals: this.crystals,
        endReady: this.endReady,
        dragonDead: this.dragonDead,
        stronghold: this.stronghold,
        generated: this.generated
      };
    }
    static deserialize(o) {
      if (!o || o.version !== C.SAVE_VERSION) return null;
      const w = new World(o.seed);
      const len = W * H;
      for (let d = 0; d < 3; d++) {
        if (!o.planes || !o.planes[d]) continue;
        try { w.planes[d] = rleDecode(o.planes[d], len); }
        catch (e) { return null; }
      }
      if (o.heights) w.heights = Int16Array.from(o.heights);
      if (o.biomes) w.biomes = Uint8Array.from(o.biomes);
      if (o.endSpawn) w.endSpawn = o.endSpawn;
      if (o.endPortal) w.endPortal = o.endPortal;
      if (o.crystals) w.crystals = o.crystals;
      if (o.stronghold) w.stronghold = o.stronghold;
      w.endReady = !!o.endReady;
      w.dragonDead = !!o.dragonDead;
      w.generated = o.generated || [true, true, true];
      w.switchTo(o.dim | 0);
      w.relightAll(0); w.relightAll(1); w.relightAll(2);
      return w;
    }
  }

  DMC.World = World;
  DMC.BIOME_NAMES = BIOME_NAMES;
})(typeof globalThis !== 'undefined' ? globalThis : this);
