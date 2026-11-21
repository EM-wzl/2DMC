/* 2DMC Web — 状态效果 / 药水（纯逻辑，浏览器与 Node 共用） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});

  const DEFS = {
    speed:  { name: '速度',     short: '速', color: '#5ad1ff', good: true },
    jump:   { name: '跳跃提升', short: '跳', color: '#7ee787', good: true },
    haste:  { name: '急迫',     short: '急', color: '#ffd166', good: true },
    regen:  { name: '生命恢复', short: '回', color: '#ff8fab', good: true },
    night:  { name: '夜视',     short: '视', color: '#8f7bff', good: true },
    fire:   { name: '抗火',     short: '火', color: '#ff9f43', good: true },
    poison: { name: '中毒',     short: '毒', color: '#9ccc65', good: false },
    slow:   { name: '缓慢',     short: '慢', color: '#9aa5b1', good: false },
    weak:   { name: '虚弱',     short: '弱', color: '#b08484', good: false }
  };

  class Effects {
    constructor() {
      this.active = {};                 // id -> { ticks, level }
      this.regenTick = 0;
      this.poisonTick = 0;
    }
    apply(id, ticks, level) {
      if (!DEFS[id]) return false;
      const lv = Math.max(1, Math.min(3, level | 0 || 1));
      const t = Math.max(1, ticks | 0 || 1200);
      const cur = this.active[id];
      // 重复饮用：等级取较大、时长取较长（原版行为）
      this.active[id] = { ticks: cur ? Math.max(cur.ticks, t) : t,
                          level: cur ? Math.max(cur.level, lv) : lv };
      return true;
    }
    has(id) { const a = this.active[id]; return !!a && a.ticks > 0; }
    level(id, d) { const a = this.active[id]; return a && a.ticks > 0 ? a.level : (d || 0); }
    clear(id) { if (id) delete this.active[id]; else this.active = {}; }

    /* 派生数值：供 Player / 挖掘 / 战斗读取 */
    get speedMul() { return Math.max(0.4, 1 + 0.28 * this.level('speed') - 0.18 * this.level('slow')); }
    get jumpMul() { return Math.max(0.5, 1 + 0.22 * this.level('jump')); }
    get mineMul() { return Math.max(0.4, 1 + 0.35 * this.level('haste') - 0.15 * this.level('slow')); }
    get attackMul() { return Math.max(0.4, 1 - 0.25 * this.level('weak')); }
    get nightVision() { return this.has('night'); }
    get fireResist() { return this.has('fire'); }

    tick() {
      for (const k of Object.keys(this.active))
        if (--this.active[k].ticks <= 0) delete this.active[k];
    }
    /* 持续型效果：再生回血 / 中毒掉血（不会直接毒死，最低 1 点） */
    potionTick(player) {
      if (this.has('regen')) {
        if (++this.regenTick >= 80) { this.regenTick = 0; player.heal(1); }
      } else this.regenTick = 0;
      if (this.has('poison')) {
        if (++this.poisonTick >= 60) {
          this.poisonTick = 0;
          if (player.life > 1) player.life -= 1;
        }
      } else this.poisonTick = 0;
    }
    list() {
      return Object.keys(this.active).map(k => Object.assign(
        { id: k, ticks: this.active[k].ticks, level: this.active[k].level }, DEFS[k]));
    }
    serialize() {
      const a = {};
      for (const k of Object.keys(this.active))
        a[k] = { ticks: this.active[k].ticks, level: this.active[k].level };
      return { active: a, regenTick: this.regenTick, poisonTick: this.poisonTick };
    }
    static restore(d) {
      const e = new Effects();
      if (!d || !d.active) return e;
      for (const k of Object.keys(d.active)) {
        if (!DEFS[k] || !d.active[k]) continue;
        const ticks = Number(d.active[k].ticks), level = Number(d.active[k].level);
        if (!Number.isFinite(ticks) || ticks <= 0) continue;
        e.active[k] = { ticks: Math.floor(ticks),
                        level: Math.max(1, Math.min(3, Number.isFinite(level) ? level : 1)) };
      }
      e.regenTick = Number(d.regenTick) | 0;
      e.poisonTick = Number(d.poisonTick) | 0;
      return e;
    }
  }

  DMC.Effects = Effects;
  DMC.EFFECT_DEFS = DEFS;
  DMC.EFFECT_IDS = Object.keys(DEFS);
})(typeof globalThis !== 'undefined' ? globalThis : this);
