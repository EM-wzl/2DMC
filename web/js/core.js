/* 2DMC Web — 核心常量、随机数、噪声、RLE（纯逻辑，可 Node 加载） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});

  const C = {
    W: 1024,                    // 世界宽（列）
    H: 256,                     // 世界高（行）
    TILE: 26,                   // 每格像素
    SEA: 118,                   // 海平面行号
    SURFACE: 96,                // 主世界基准地表
    GRAVITY: 0.44,
    MAX_FALL: 11,
    REACH: 5.5,
    SAVE_KEY: '2dmc-web-save-v1',
    SAVE_VERSION: 1,
    DIM_OVERWORLD: 0,
    DIM_NETHER: 1,
    DIM_END: 2,
    DIM_NAMES: ['主世界', '下界', '末地'],
    STACK: 64
  };

  // 方块 id（1..199）；物品 id 从 200 起
  const B = {
    AIR: 0,
    STONE: 1, GRASS: 2, DIRT: 3, COBBLE: 4, BEDROCK: 5,
    SAND: 6, SANDSTONE: 7, LOG: 8, LEAVES: 9, PLANK: 10,
    WATER: 11, LAVA: 12,
    COAL_ORE: 13, IRON_ORE: 14, GOLD_ORE: 15, DIAMOND_ORE: 16,
    LAPIS_ORE: 17, REDSTONE_ORE: 18, EMERALD_ORE: 19, COPPER_ORE: 20,
    DEEPSLATE: 21, AMETHYST: 22, SCULK: 23,
    TORCH: 24, CRAFTING_TABLE: 25, FURNACE: 26, CHEST: 27, GLASS: 28,
    OBSIDIAN: 29, NETHER_PORTAL: 30, NETHERRACK: 31, GLOWSTONE: 32,
    SOUL_SAND: 33, NETHER_GOLD: 34, ANCIENT_DEBRIS: 35, NETHER_BRICKS: 36,
    MAGMA: 37, SULFUR: 38, CINNABAR: 39, POTENT_SULFUR: 40, SULFUR_SPIKE: 41,
    GEYSER: 42, CHERRY_LOG: 43, CHERRY_LEAVES: 44, MANGROVE_LOG: 45,
    MANGROVE_LEAVES: 46, PALE_LOG: 47, PALE_LEAVES: 48,
    MOSS: 49, AZALEA: 50, CACTUS: 51, SUGAR_CANE: 52, SNOW: 53, ICE: 54,
    GRAVEL: 55, CLAY: 56,
    PURPUR: 57, PURPUR_PILLAR: 58, CHORUS_PLANT: 59, CHORUS_FLOWER: 60,
    DRAGON_EGG: 61, END_PORTAL: 62, END_PORTAL_FRAME: 63, END_CRYSTAL: 64,
    OBSIDIAN_PILLAR: 65, END_ROD: 66, END_STONE: 67, END_STONE_BRICKS: 68,
    GOLD_DANDELION: 69, BARREL: 70, SMITHING: 71, CAMPFIRE: 72, WOOL: 73,
    HAY: 74, BONE_BLOCK: 75, IRON_BLOCK: 76, GOLD_BLOCK: 77, DIAMOND_BLOCK: 78,
    LAPIS_BLOCK: 79, EMERALD_BLOCK: 80, STONE_BRICKS: 81, BRICKS: 82,
    BOOKSHELF: 83, ENCHANTING: 84, BREWING: 85, SEA_LANTERN: 86,
    PRISMARINE: 87, SPONGE: 88, TERRACOTTA: 89, LANTERN: 90, SCAFFOLDING: 91,
    BAMBOO: 92, HONEY: 93, NETHER_WART: 94, SHROOMLIGHT: 95,
    CRIMSON_STEM: 96, WARPED_STEM: 97, NETHER_SPROUTS: 98,
    BASALT: 99, BLACKSTONE: 100, TUFF: 101, CALCITE: 102,
    COPPER_BLOCK: 103, CANDLE: 104, AMETHYST_CLUSTER: 105,
    CHERRY_PLANK: 106, MUD: 107, SUSPICIOUS_SAND: 108,
    IRON_ORE_DEEP: 109, DIAMOND_ORE_DEEP: 110, REDSTONE_ORE_DEEP: 111,
    SULFUR_POOL: 112
  };

  // 物品 id（>=200）
  const I = {
    STICK: 200, COAL: 201, RAW_IRON: 202, RAW_GOLD: 203, RAW_COPPER: 204,
    IRON_INGOT: 205, GOLD_INGOT: 206, COPPER_INGOT: 207, DIAMOND: 208,
    LAPIS: 209, REDSTONE_DUST: 210, EMERALD: 211, NETHERITE_SCRAP: 212,
    NETHERITE_INGOT: 213, FLINT: 214, BONE: 215, STRING: 216,
    GUNPOWDER: 217, LEATHER: 218, APPLE: 219, BREAD: 220,
    WOOD_PICK: 221, WOOD_AXE: 222, WOOD_SHOVEL: 223, WOOD_SWORD: 224,
    STONE_PICK: 225, STONE_AXE: 226, STONE_SHOVEL: 227, STONE_SWORD: 228,
    IRON_PICK: 229, IRON_AXE: 230, IRON_SHOVEL: 231, IRON_SWORD: 232,
    DIAMOND_PICK: 233, DIAMOND_AXE: 234, DIAMOND_SHOVEL: 235, DIAMOND_SWORD: 236,
    NETHERITE_SWORD: 237, SHEARS: 238, BUNDLE: 239, NAME_TAG: 240,
    ENDER_PEARL: 241, BLAZE_ROD: 242, GHAST_TEAR: 243, CHORUS_FRUIT: 244,
    POPPED_CHORUS: 245, ELYTRA: 246, TOTEM: 247, SHULKER_SHELL: 248,
    BRICK: 249, PAPER: 250, BOOK: 251, BUCKET: 252, WATER_BUCKET: 253,
    LAVA_BUCKET: 254, FLINT_STEEL: 255, COMPASS: 256, CLOCK: 257,
    GOLDEN_APPLE: 258, POTION: 259, HONEYCOMB: 260, SWEET_BERRIES: 261,
    GLOW_BERRIES: 262, SULFUR_CHUNK: 263, AMETHYST_SHARD: 264, ECHO_SHARD: 265,
    GOLD_DANDELION_ITEM: 266, NETHER_WART_ITEM: 267, CANDLE_ITEM: 268,
    BRICK_STAIRS: 269, RAW_SULFUR: 270, CINNABAR_DUST: 271
  };

  // ---- 可复现随机数（xorshift32）----
  function makeRNG(seed) {
    let s = (seed >>> 0) || 0x9e3779b9;
    return function rnd() {
      s ^= s << 13; s >>>= 0;
      s ^= s >>> 17;
      s ^= s << 5; s >>>= 0;
      return (s >>> 0) / 4294967296;
    };
  }
  function hash2(x, y, seed) {
    let h = (x | 0) * 374761393 + (y | 0) * 668265263 + (seed | 0) * 1442695041;
    h = (h ^ (h >>> 13)) >>> 0;
    h = (h * 1274126177) >>> 0;
    return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
  }
  const smooth = t => t * t * (3 - 2 * t);
  function noise1(x, seed) {
    const i = Math.floor(x), f = x - i;
    return hash2(i, 0, seed) * (1 - smooth(f)) + hash2(i + 1, 0, seed) * smooth(f);
  }
  function noise2(x, y, seed) {
    const ix = Math.floor(x), iy = Math.floor(y);
    const fx = smooth(x - ix), fy = smooth(y - iy);
    const a = hash2(ix, iy, seed), b = hash2(ix + 1, iy, seed);
    const c = hash2(ix, iy + 1, seed), d = hash2(ix + 1, iy + 1, seed);
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
  }
  function fbm(x, y, seed, oct) {
    let v = 0, amp = 1, tot = 0, f = 1;
    for (let k = 0; k < (oct || 4); k++) {
      v += noise2(x * f, y * f, seed + k * 7919) * amp;
      tot += amp; amp *= 0.5; f *= 2;
    }
    return v / tot;
  }
  const clamp = (v, a, b) => v < a ? a : (v > b ? b : v);
  const lerp = (a, b, t) => a + (b - a) * t;
  const dist = (ax, ay, bx, by) => Math.hypot(ax - bx, ay - by);
  const idx = (x, y) => y * C.W + x;
  const inWorld = (x, y) => x >= 0 && x < C.W && y >= 0 && y < C.H;

  // ---- RLE：把 Uint16Array 压成 [runLen, id] 序列，再 Base64 ----
  function rleEncode(arr) {
    const out = [];
    let i = 0;
    while (i < arr.length) {
      const v = arr[i]; let n = 1;
      while (i + n < arr.length && arr[i + n] === v && n < 65535) n++;
      out.push(n, v); i += n;
    }
    const u16 = new Uint16Array(out);
    const bytes = new Uint8Array(u16.buffer);
    let s = '';
    for (let k = 0; k < bytes.length; k += 8192)
      s += String.fromCharCode.apply(null, bytes.subarray(k, k + 8192));
    return b64.enc(s);
  }
  function rleDecode(str, len) {
    const bin = b64.dec(str);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    const u16 = new Uint16Array(bytes.buffer);
    const arr = new Uint16Array(len);
    let p = 0;
    for (let k = 0; k + 1 < u16.length && p < len; k += 2) {
      const n = u16[k], v = u16[k + 1];
      arr.fill(v, p, Math.min(len, p + n));
      p += n;
    }
    return arr;
  }

  // 浏览器与 Node 都能用的 Base64
  const b64 = (() => {
    const g = typeof globalThis !== 'undefined' ? globalThis : {};
    if (typeof g.btoa === 'function' && typeof g.atob === 'function')
      return { enc: g.btoa.bind(g), dec: g.atob.bind(g) };
    return {
      enc: s => Buffer.from(s, 'binary').toString('base64'),
      dec: s => Buffer.from(s, 'base64').toString('binary')
    };
  })();

  // 简易事件总线
  function makeBus() {
    const map = {};
    return {
      on(k, fn) { (map[k] || (map[k] = [])).push(fn); },
      emit(k, data) { (map[k] || []).forEach(fn => fn(data)); }
    };
  }

  DMC.C = C; DMC.B = B; DMC.I = I;
  DMC.Core = {
    makeRNG, hash2, noise1, noise2, fbm, clamp, lerp, dist, idx, inWorld,
    rleEncode, rleDecode, makeBus, smooth
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
