/* 2DMC Web — 成就与统计（纯逻辑，浏览器 / Node 共用） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I } = DMC;

  /* 所有条件只依赖 stats，第二个参数 g 可选（用于读取背包等运行时状态） */
  const LIST = [
    { id: 'first_mine',  name: '开局一镐',   desc: '挖掉第一个方块',       check: s => s.mined >= 1 },
    { id: 'lumberjack',  name: '伐木工',     desc: '挖到 8 个原木',        check: s => s.logs >= 8 },
    { id: 'crafter',     name: '合成入门',   desc: '完成一次合成',         check: s => s.crafted >= 1 },
    { id: 'iron_age',     name: '铁器时代',   desc: '挖到铁矿石',           check: s => s.ironOre >= 1 },
    { id: 'diamond',      name: '钻石！',     desc: '挖到钻石矿石',         check: s => s.diamondOre >= 1 },
    { id: 'builder',      name: '建筑师',     desc: '放置 32 个方块',       check: s => s.placed >= 32 },
    { id: 'hunter',       name: '怪物猎人',   desc: '击杀 10 只生物',       check: s => s.mobs >= 10 },
    { id: 'foodie',       name: '补充能量',   desc: '吃下第一份食物',       check: s => s.eaten >= 1 },
    { id: 'alchemist',    name: '炼金术士',   desc: '喝下第一瓶药水',       check: s => s.potions >= 1 },
    { id: 'nether',       name: '下界来客',   desc: '进入下界',             check: s => !!s.dims[1] },
    { id: 'end',          name: '末路之地',   desc: '进入末地',             check: s => !!s.dims[2] },
    { id: 'dragon',       name: '屠龙者',     desc: '击败末影龙',           check: s => !!s.dragonDead },
    { id: 'storm',        name: '风雨交加',   desc: '遇上雷暴天气',         check: s => !!s.storm },
    { id: 'survivor',     name: '老玩家',     desc: '累计游玩 6000 帧',     check: s => s.ticks >= 6000 }
  ];
  const BY_ID = {};
  LIST.forEach(a => { BY_ID[a.id] = a; });

  function blankStats() {
    return { mined: 0, logs: 0, ironOre: 0, diamondOre: 0, placed: 0,
             crafted: 0, mobs: 0, eaten: 0, potions: 0,
             dims: [false, false, false],
             dragonDead: false, storm: false, ticks: 0 };
  }

  class Achievements {
    constructor() {
      this.unlocked = {};       // id -> 解锁时的帧数
      this.stats = blankStats();
    }
    get count() { return Object.keys(this.unlocked).length; }
    get total() { return LIST.length; }
    has(id) { return Object.prototype.hasOwnProperty.call(this.unlocked, id); }
    unlock(id) {
      if (this.has(id) || !BY_ID[id]) return false;
      this.unlocked[id] = this.stats.ticks;
      return true;
    }
    /* 事件计数；event 见下方 game.js 的调用点 */
    note(event, value) {
      const s = this.stats;
      switch (event) {
        case 'mine':
          s.mined++;
          if (value === B.LOG || value === B.CHERRY_LOG || value === B.MANGROVE_LOG ||
              value === B.PALE_LOG) s.logs++;
          if (value === B.IRON_ORE || value === B.IRON_ORE_DEEP) s.ironOre++;
          if (value === B.DIAMOND_ORE || value === B.DIAMOND_ORE_DEEP) s.diamondOre++;
          break;
        case 'place':   s.placed++; break;
        case 'craft':  s.crafted++; break;
        case 'mob':    s.mobs++; break;
        case 'eat':    s.eaten++; break;
        case 'potion': s.potions++; break;
        case 'dim':    if (value >= 0 && value <= 2) s.dims[value] = true; break;
        case 'dragon': s.dragonDead = true; break;
        case 'storm':  s.storm = true; break;
        case 'tick':   s.ticks++; break;
        default: break;
      }
    }
    /* 每帧/定期调用，返回本次新解锁的成就数组 */
    check(g) {
      const got = [];
      for (const a of LIST) {
        if (this.has(a.id)) continue;
        let ok = false;
        try { ok = !!a.check(this.stats, g); } catch (e) { ok = false; }
        if (ok && this.unlock(a.id)) got.push(a);
      }
      return got;
    }
    progress() {
      return LIST.map(a => ({ id: a.id, name: a.name, desc: a.desc,
                              done: this.has(a.id) }));
    }
    serialize() {
      return { unlocked: Object.assign({}, this.unlocked),
               stats: Object.assign({}, this.stats, { dims: this.stats.dims.slice() }) };
    }
    static restore(d) {
      const a = new Achievements();
      if (!d) return a;
      if (d.unlocked && typeof d.unlocked === 'object')
        for (const k of Object.keys(d.unlocked))
          if (BY_ID[k]) a.unlocked[k] = d.unlocked[k] | 0;
      if (d.stats && typeof d.stats === 'object') {
        for (const k of Object.keys(blankStats())) {
          if (k === 'dims') continue;
          const v = d.stats[k];
          if (v === true || v === false) a.stats[k] = !!v;
          else if (Number.isFinite(v)) a.stats[k] = v;
        }
        if (Array.isArray(d.stats.dims))
          for (let i = 0; i < 3; i++) a.stats.dims[i] = !!d.stats.dims[i];
      }
      return a;
    }
  }

  DMC.Achievements = Achievements;
  DMC.ACHIEVEMENTS = LIST;
  DMC.ACHIEVEMENT_BY_ID = BY_ID;
})(typeof globalThis !== 'undefined' ? globalThis : this);
