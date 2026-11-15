/* 从项目内的贴图资源生成网页版贴图包（可复现）
 * 用法：node test/build-textures.cjs
 * 来源：
 *   ../../Default-Java-26.2/assets/minecraft/textures/{block,item}  —— Minecraft Java 26.2 原版
 *   ../../beta1.8/assets/tiles                                      —— 2DMC 自有贴图
 * 产出：
 *   ../assets/tiles/<name>.png   （统一按 DEF.name 命名，动画图集在运行时取第一帧）
 *   ../js/textures.js            （id -> 贴图名映射 + 加载器）
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const WEB = path.join(__dirname, '..');
const ROOT = path.join(WEB, '..');
const MC = path.join(ROOT, 'Default-Java-26.2', 'assets', 'minecraft', 'textures');
const OWN = path.join(ROOT, 'beta1.8', 'assets', 'tiles');
const OUT = path.join(WEB, 'assets', 'tiles');

// 读 DEF（纯逻辑模块，用 vm 加载）
const ctx = { console, Buffer, Math, JSON };
vm.createContext(ctx);
for (const f of ['core', 'items'])
  vm.runInContext(fs.readFileSync(path.join(WEB, 'js', f + '.js'), 'utf8'), ctx, { filename: f });
const DEF = ctx.DMC.DEF, AIR = ctx.DMC.B.AIR;

// name -> 候选源文件（按优先级）。null = 故意保留程序化纹理
const OVERRIDE = {
  grass_block: ['b:grass_block_side'],
  water: ['b:water_still'], lava: ['b:lava_still'],
  nether_portal: ['b:nether_portal'],
  end_portal: null,                       // 程序化紫色漩涡更好看
  crafting_table: ['b:crafting_table_front'],
  chest: null,                            // 箱子是实体贴图，程序化 machine 更清晰
  magma_block: ['b:magma'], snow_block: ['b:snow'],
  purpur_pillar: ['b:purpur_pillar_side'],
  end_portal_frame: ['b:end_portal_frame_side'],
  obsidian_pillar: ['b:obsidian'],
  gold_dandelion: ['b:dandelion'],
  barrel: ['b:barrel_side'], hay_block: ['b:hay_block_side'],
  bone_block: ['b:bone_block_side'],
  enchanting_table: ['b:enchanting_table_side'],
  scaffolding: ['b:scaffolding_side'], honey_block: ['b:honey_block_side'],
  basalt: ['b:basalt_side'], suspicious_sand: ['b:suspicious_sand_0'],
  cactus: ['b:cactus_side'],
  compass: ['i:compass_00'], clock: ['i:clock_00'],
  nether_wart_item: ['i:nether_wart'], candle_item: ['i:candle'],
  gold_dandelion_item: ['i:dandelion', 'b:dandelion'],
  raw_sulfur: ['i:glowstone_dust', 'i:yellow_dye'],
  sulfur_chunk: ['i:yellow_dye', 'i:glowstone_dust'],
  cinnabar_dust: ['i:redstone', 'i:red_dye'],
  // 26.2 自定义方块：项目里没有对应贴图，用最接近的原版方块
  sulfur_block: ['b:yellow_wool', 'b:sandstone'],
  cinnabar_block: ['b:red_wool', 'b:red_concrete'],
  potent_sulfur: ['b:gold_block'],
  sulfur_spike: ['b:pointed_dripstone_tip', 'b:amethyst_cluster'],
  geyser: ['b:bubble_column', 'b:water_still'],
  sulfur_pool: ['b:water_still'],
  end_rod: ['b:end_rod'],
  torch: ['b:torch', 'o:torch']
};

function srcPath(token) {
  const [kind, name] = token.split(':');
  if (kind === 'b') return path.join(MC, 'block', name + '.png');
  if (kind === 'i') return path.join(MC, 'item', name + '.png');
  if (kind === 'o') return path.join(OWN, name + '.png');
  return null;
}
function candidates(name) {
  if (Object.prototype.hasOwnProperty.call(OVERRIDE, name)) {
    return OVERRIDE[name] === null ? [] : OVERRIDE[name].slice();
  }
  return [
    'b:' + name, 'i:' + name, 'o:' + name,
    'b:' + name.replace(/_block$/, ''),
    'b:' + name.replace(/_item$/, ''),
    'i:' + name.replace(/_item$/, '')
  ];
}

fs.mkdirSync(OUT, { recursive: true });
for (const f of fs.readdirSync(OUT)) if (f.endsWith('.png')) fs.unlinkSync(path.join(OUT, f));

const map = {}, credits = {};
let hit = 0, skipped = [], bytes = 0;
const sources = { b: 0, i: 0, o: 0 };

for (const key of Object.keys(DEF)) {
  const id = Number(key);
  const d = DEF[id];
  if (!id || id === AIR || !d) continue;
  let chosen = null;
  for (const token of candidates(d.name)) {
    const p = srcPath(token);
    if (p && fs.existsSync(p)) { chosen = { token, p }; break; }
  }
  if (!chosen) { skipped.push(d.name); continue; }
  const kind = chosen.token.split(':')[0];
  sources[kind]++;
  credits[d.name] = chosen.token + '.png';
  map[id] = d.name;
  const dest = path.join(OUT, d.name + '.png');
  if (!fs.existsSync(dest)) {
    const buf = fs.readFileSync(chosen.p);
    fs.writeFileSync(dest, buf);
    bytes += buf.length;
  }
  hit++;
}

const header = `/* 自动生成，请勿手改：node test/build-textures.cjs
 * 贴图来源：项目内 Default-Java-26.2（MC Java 26.2 原版资源包）与 beta1.8/assets/tiles
 * 共 ${hit} 项，动画图集在运行时只取第一帧 16x16
 */
`;
const body = `(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  DMC.TextureMap = ${JSON.stringify(map, null, 2)};
  DMC.TextureCredits = ${JSON.stringify(credits, null, 2)};

  class TexturePack {
    constructor() {
      this.tiles = {};      // name -> 32x32 canvas（已裁剪第一帧）
      this.images = {};
      this.loaded = 0; this.failed = 0; this.total = 0;
      this.enabled = true;
    }
    get totalCount() { return Object.keys(DMC.TextureMap).length; }
    load(cb) {
      const names = Array.from(new Set(Object.values(DMC.TextureMap)));
      this.total = names.length;
      if (!this.total) { cb && cb(this); return; }
      let done = 0;
      names.forEach(name => {
        const img = new Image();
        img.onload = () => {
          const cv = document.createElement('canvas');
          cv.width = 32; cv.height = 32;
          const g = cv.getContext('2d');
          g.imageSmoothingEnabled = false;
          // 绝大多数是 16x16；动画图集(16xN)只取最上面一帧
          const sw = Math.min(16, img.width), sh = Math.min(16, img.height);
          g.drawImage(img, 0, 0, sw, sh, 0, 0, 32, 32);
          this.tiles[name] = cv;
          this.images[name] = img;
          if (++done === this.total) cb && cb(this);
        };
        img.onerror = () => {
          this.failed++;
          if (++done === this.total) cb && cb(this);
        };
        img.src = 'assets/tiles/' + name + '.png';
      });
    }
    get(id) { return this.enabled ? (this.tiles[DMC.TextureMap[id]] || null) : null; }
    nameOf(id) { return DMC.TextureMap[id] || null; }
  }
  DMC.TexturePack = TexturePack;
})(typeof globalThis !== 'undefined' ? globalThis : this);
`;
fs.writeFileSync(path.join(WEB, 'js', 'textures.js'), header + body);

console.log(`贴图包生成完成：${hit}/${hit + skipped.length} 项命中`);
console.log('来源统计：MC block =', sources.b, ' MC item =', sources.i, ' 2DMC own =', sources.o);
console.log('文件数 =', fs.readdirSync(OUT).filter(f => f.endsWith('.png')).length,
            ' 总大小 =', (bytes / 1024).toFixed(1) + ' KB');
console.log('保留程序化纹理（' + skipped.length + '）：', skipped.join(', '));
