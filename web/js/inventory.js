/* 2DMC Web — 背包、原版光标拿取、合成网格（纯逻辑） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { C } = DMC;

  function slot(id, count) { return { id: id | 0, count: (id ? count | 0 : 0) }; }
  const empty = () => slot(0, 0);

  /* 原版光标拿取：左键整堆 / 右键一半或一个（直接修改传入对象，便于单测） */
  function cursorLeft(cur, s) {
    if (!s) return;
    if (cur.id === 0) {
      if (s.id !== 0) { cur.id = s.id; cur.count = s.count; s.id = 0; s.count = 0; }
      return;
    }
    if (s.id === 0) { s.id = cur.id; s.count = cur.count; cur.id = 0; cur.count = 0; return; }
    if (s.id === cur.id) {
      const room = C.STACK - s.count;
      const move = Math.min(room, cur.count);
      s.count += move; cur.count -= move;
      if (cur.count === 0) { cur.id = 0; }
      return;
    }
    const ti = cur.id, tc = cur.count;
    cur.id = s.id; cur.count = s.count;
    s.id = ti; s.count = tc;
  }
  function cursorRight(cur, s) {
    if (!s) return;
    if (cur.id === 0) {
      if (s.id !== 0) {
        const half = Math.ceil(s.count / 2);
        cur.id = s.id; cur.count = half;
        s.count -= half;
        if (s.count === 0) s.id = 0;
      }
      return;
    }
    if (s.id === 0) {
      s.id = cur.id; s.count = 1;
      cur.count--; if (cur.count === 0) cur.id = 0;
      return;
    }
    if (s.id === cur.id && s.count < C.STACK) {
      s.count++; cur.count--;
      if (cur.count === 0) cur.id = 0;
    }
    // 异类 / 已满：右键不做任何事
  }

  class Inventory {
    constructor() {
      this.slots = [];
      for (let i = 0; i < 36; i++) this.slots.push(empty());
      this.cursor = empty();
      this.selected = 0;
    }
    static fromJSON(o) {
      const inv = new Inventory();
      if (!o) return inv;
      (o.slots || []).forEach((s, i) => {
        if (i < 36 && s) inv.slots[i] = slot(s.id, s.count);
      });
      if (o.cursor) inv.cursor = slot(o.cursor.id, o.cursor.count);
      inv.selected = Math.max(0, Math.min(8, o.selected | 0));
      return inv;
    }
    toJSON() {
      return { slots: this.slots.map(s => ({ id: s.id, count: s.count })),
               cursor: { id: this.cursor.id, count: this.cursor.count },
               selected: this.selected };
    }
    held() { return this.slots[this.selected]; }
    countOf(id) {
      let n = 0;
      for (const s of this.slots) if (s.id === id) n += s.count;
      return n;
    }
    // 返回没能放下的数量
    add(id, count) {
      if (!id || count <= 0) return 0;
      for (const s of this.slots) {
        if (s.id === id && s.count < C.STACK) {
          const n = Math.min(C.STACK - s.count, count);
          s.count += n; count -= n;
          if (count === 0) return 0;
        }
      }
      for (const s of this.slots) {
        if (s.id === 0) {
          const n = Math.min(C.STACK, count);
          s.id = id; s.count = n; count -= n;
          if (count === 0) return 0;
        }
      }
      return count;
    }
    remove(id, count) {
      if (this.countOf(id) < count) return false;
      for (let i = this.slots.length - 1; i >= 0 && count > 0; i--) {
        const s = this.slots[i];
        if (s.id !== id) continue;
        const n = Math.min(s.count, count);
        s.count -= n; count -= n;
        if (s.count === 0) s.id = 0;
      }
      return true;
    }
    consumeHeld(n) {
      const s = this.held();
      if (!s.id) return false;
      s.count -= (n || 1);
      if (s.count <= 0) { s.id = 0; s.count = 0; }
      return true;
    }
    // 关闭界面时把光标物品退回背包（原版行为）
    dropCursor() {
      if (!this.cursor.id) return;
      const left = this.add(this.cursor.id, this.cursor.count);
      if (left > 0) this.cursor = slot(this.cursor.id, left);
      else this.cursor = empty();
    }
  }

  /* 合成网格：2x2 随身 / 3x3 工作台 */
  class CraftGrid {
    constructor(size) { this.size = size; this.cells = new Array(size * size).fill(0); }
    static fromJSON(o) {
      const g = new CraftGrid(o && o.size === 3 ? 3 : 2);
      if (o && o.cells) o.cells.forEach((v, i) => { if (i < g.cells.length) g.cells[i] = v | 0; });
      return g;
    }
    toJSON() { return { size: this.size, cells: this.cells.slice() }; }
    result() {
      return DMC.Items.craftMatch(this.cells, this.size);
    }
    // 成功合成后每格 -1
    consume() {
      for (let i = 0; i < this.cells.length; i++) {
        if (this.cells[i]) this.cells[i] = 0;
      }
    }
    clearInto(inv) {
      for (let i = 0; i < this.cells.length; i++) {
        if (this.cells[i]) inv.add(this.cells[i], 1);
        this.cells[i] = 0;
      }
    }
  }

  /* 通用面板：熔炉 / 附魔台 / 酿造台 / 箱子 */
  class PanelState {
    constructor(kind) {
      this.kind = kind;                    // 'furnace' | 'enchant' | 'brew' | 'chest'
      this.input = slot(0, 0);
      this.fuel = slot(0, 0);
      this.output = slot(0, 0);
      this.progress = 0;
      this.burn = 0;
      this.chest = new Array(27).fill(0).map(empty);
      this.enchantLevel = 0;
      this.brewTimer = 0;
    }
    toJSON() {
      return { kind: this.kind, input: this.input, fuel: this.fuel,
               output: this.output, progress: this.progress, burn: this.burn,
               chest: this.chest.map(s => ({ id: s.id, count: s.count })),
               enchantLevel: this.enchantLevel, brewTimer: this.brewTimer };
    }
    static fromJSON(o) {
      const p = new PanelState(o ? o.kind : 'furnace');
      if (!o) return p;
      if (o.input) p.input = slot(o.input.id, o.input.count);
      if (o.fuel) p.fuel = slot(o.fuel.id, o.fuel.count);
      if (o.output) p.output = slot(o.output.id, o.output.count);
      p.progress = o.progress | 0; p.burn = o.burn | 0;
      p.enchantLevel = o.enchantLevel | 0; p.brewTimer = o.brewTimer | 0;
      if (o.chest) o.chest.forEach((s, i) => { if (i < 27) p.chest[i] = slot(s.id, s.count); });
      return p;
    }
    // 熔炉/附魔/酿造每帧推进；返回 true 表示有状态变化
    tick() {
      const { SMELT } = DMC.Items;
      let changed = false;
      if (this.kind === 'furnace') {
        const recipe = SMELT[this.input.id];
        const canBurn = this.burn > 0 || (this.fuel.id && DMC.DEF[this.fuel.id] && DMC.DEF[this.fuel.id].fuel);
        if (recipe && this.input.count > 0 && canBurn &&
            (this.output.id === 0 || (this.output.id === recipe[0] && this.output.count < C.STACK))) {
          if (this.burn <= 0) {
            this.burn = DMC.DEF[this.fuel.id].fuel;
            this.fuel.count--;
            if (this.fuel.count <= 0) this.fuel.id = 0;
            changed = true;
          }
          this.progress++;
          if (this.progress >= 100) {
            this.progress = 0;
            this.input.count--; if (this.input.count <= 0) this.input.id = 0;
            if (this.output.id === 0) { this.output.id = recipe[0]; this.output.count = 0; }
            this.output.count += recipe[1];
            changed = true;
          }
          this.burn = Math.max(0, this.burn - 1);
        } else if (this.progress > 0) { this.progress = 0; changed = true; }
      } else if (this.kind === 'brew') {
        if (this.input.id === I_POTION_BASE && this.brewTimer < 120) this.brewTimer++;
      }
      return changed;
    }
  }
  const I_POTION_BASE = DMC.I.GLOW_BERRIES;

  DMC.Inv = { slot, empty, cursorLeft, cursorRight, Inventory, CraftGrid, PanelState };
})(typeof globalThis !== 'undefined' ? globalThis : this);
