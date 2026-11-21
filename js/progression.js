/* 2DMC Web — 怪物等级 / 玩家经验与天赋（纯逻辑，浏览器与 Node 共用） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const clamp = DMC.clamp || ((v, a, b) => Math.max(a, Math.min(b, v)));

  const TALENTS = {
    power:   { name: '力量', short: '力', desc: '每级 +8% 攻击' },
    vigor:   { name: '耐力', short: '耐', desc: '每级 +2 最大生命' },
    agility: { name: '敏捷', short: '敏', desc: '每级 +5% 移动速度' }
  };

  /* 深度越深、维度越危险，怪物等级越高（1..10） */
  function mobLevel(y, dim) {
    const depth = Math.floor((y - 60) / 34);
    const bonus = dim === 1 ? 2 : dim === 2 ? 3 : 0;
    return clamp(1 + depth + bonus, 1, 10);
  }
  function mobScale(lv) {
    return { hp: 1 + 0.18 * (lv - 1), dmg: 1 + 0.12 * (lv - 1) };
  }
  function xpNeed(level) { return 20 + (level - 1) * 14; }
  function xpForKill(lv, elite) {
    return 4 + lv * 3 + (elite ? lv * 3 : 0);
  }

  class Progression {
    constructor() {
      this.level = 1;
      this.xp = 0;
      this.totalXp = 0;
      this.points = 0;
      this.talents = { power: 0, vigor: 0, agility: 0 };
    }
    get need() { return xpNeed(this.level); }
    get attackMul() { return 1 + 0.08 * this.talents.power; }
    get maxLife() { return 20 + 2 * this.talents.vigor; }
    get speedMul() { return 1 + 0.05 * this.talents.agility; }
    /* 返回升了几级 */
    gain(n) {
      n = Math.max(0, Math.floor(Number(n) || 0));
      this.xp += n; this.totalXp += n;
      let ups = 0;
      while (this.xp >= this.need) {
        this.xp -= this.need;
        this.level++; this.points++; ups++;
      }
      return ups;
    }
    spend(id) {
      if (!TALENTS[id] || this.points <= 0) return false;
      this.points--; this.talents[id]++;
      return true;
    }
    /* 洗点：把已投入的点数全部退回 */
    respec() {
      let n = 0;
      for (const k of Object.keys(this.talents)) { n += this.talents[k]; this.talents[k] = 0; }
      this.points += n;
      return n;
    }
    summary() {
      return 'Lv' + this.level + ' ' + this.xp + '/' + this.need + ' XP' +
             (this.points ? ' · 天赋点 ' + this.points : '');
    }
    serialize() {
      return { level: this.level, xp: this.xp, totalXp: this.totalXp,
               points: this.points, talents: Object.assign({}, this.talents) };
    }
    static restore(d) {
      const p = new Progression();
      if (!d) return p;
      p.level = clamp(Math.floor(Number(d.level) || 1), 1, 999);
      p.totalXp = Math.max(0, Math.floor(Number(d.totalXp) || 0));
      p.xp = clamp(Math.floor(Number(d.xp) || 0), 0, xpNeed(p.level) - 1);
      p.points = Math.max(0, Math.floor(Number(d.points) || 0));
      if (d.talents)
        for (const k of Object.keys(p.talents))
          p.talents[k] = Math.max(0, Math.floor(Number(d.talents[k]) || 0));
      return p;
    }
  }

  DMC.Progression = Progression;
  DMC.TALENTS = TALENTS;
  DMC.mobLevel = mobLevel;
  DMC.mobScale = mobScale;
  DMC.xpNeed = xpNeed;
  DMC.xpForKill = xpForKill;
})(typeof globalThis !== 'undefined' ? globalThis : this);
