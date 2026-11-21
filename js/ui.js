/* 2DMC Web — HUD、背包/合成、通用面板、命令与提示 */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, DEF, Items, Inv } = DMC;
  const doc = root.document;

  const $ = s => doc.querySelector(s);
  const el = (tag, cls, text) => {
    const e = doc.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined) e.textContent = text;
    return e;
  };
  function itemGlyph(id) {
    const d = DEF[id];
    if (!d) return '??';
    if (id === B.AIR) return '';
    // 用名称首两字作为无贴图时的兜底
    const n = d.cn.replace(/[^\u4e00-\u9fa5A-Za-z0-9]/g, '');
    return n.slice(0, 2) || '?';
  }
  // 槽位图标：优先项目内的原版贴图，没有才退回中文文字 + 颜色块
  function paintGlyph(node, id) {
    node.innerHTML = '';
    if (!id) { node.style.background = 'transparent'; node.style.boxShadow = ''; return; }
    const tp = DMC.texpack, tex = tp && tp.get(id);
    if (tex) {
      const cv = doc.createElement('canvas');
      cv.width = 32; cv.height = 32;
      cv.className = 'tex-img';
      cv.getContext('2d').drawImage(tex, 0, 0);
      node.appendChild(cv);
      node.style.background = 'transparent';
      node.style.boxShadow = 'none';
    } else {
      node.textContent = itemGlyph(id);
      node.style.background = DEF[id].color || '#888';
      node.style.boxShadow = '';
    }
  }

  class UI {
    constructor(game) {
      this.g = game;
      this.panelKind = null;
      this.hot = [];
      this.toastTimer = 0;
      this.iconVersion = 0;
      this.buildHotbar();
      this.bind();
      this.renderCursor();
    }

    buildHotbar() {
      const box = $('#hotbar');
      box.innerHTML = '';
      this.hot = [];
      for (let i = 0; i < 9; i++) {
        const s = el('div', 'slot hot-slot');
        s.dataset.kind = 'inv'; s.dataset.index = String(i);
        const g = el('div', 'glyph'); const n = el('div', 'num');
        s.appendChild(g); s.appendChild(n);
        box.appendChild(s);
        this.hot.push({ root: s, glyph: g, num: n });
      }
    }

    bind() {
      const g = this.g;
      // 面板点击（事件委托）：左键整堆 / 右键一半
      $('#panel').addEventListener('mousedown', e => {
        const s = e.target.closest('.slot');
        if (!s || !this.panelKind) return;
        e.preventDefault();
        if (e.button === 2) e.stopPropagation();
        this.clickSlot(s.dataset.kind, Number(s.dataset.index), e.button === 2);
      });
      $('#panel').addEventListener('contextmenu', e => e.preventDefault());
      $('#btn-close-panel').addEventListener('click', () => this.closePanel());
      $('#btn-help').addEventListener('click', () => this.toggleHelp());
      $('#btn-save').addEventListener('click', () => { g.save(); this.toast('世界已保存'); });
      $('#btn-lan').addEventListener('click', () => this.openCmd('/lan '));
      // 命令
      $('#cmd').addEventListener('keydown', e => {
        if (e.key === 'Enter') {
          const v = $('#cmd').value.trim();
          if (v) { this.log('> ' + v); g.command(v); }
          $('#cmd').value = ''; this.closeCmd();
        } else if (e.key === 'Escape') { $('#cmd').value = ''; this.closeCmd(); }
        e.stopPropagation();
      });
      // 启动页
      $('#btn-new').addEventListener('click', () => {
        const seed = parseInt($('#seed').value, 10);
        g.startNew(isNaN(seed) ? (Date.now() & 0x7fffffff) : seed);
        $('#splash').classList.add('hidden');
      });
      $('#btn-continue').addEventListener('click', () => {
        if (!g.load()) { this.toast('没有找到存档，已创建新世界'); g.startNew(Date.now() & 0x7fffffff); }
        $('#splash').classList.add('hidden');
      });
      $('#splash-help').addEventListener('click', () => this.toggleHelp(true));
      // 光标跟随
      doc.addEventListener('mousemove', e => {
        const c = $('#cursor');
        c.style.left = (e.clientX + 14) + 'px';
        c.style.top = (e.clientY + 14) + 'px';
      });
    }

    /* ---------- HUD ---------- */
    render() {
      const g = this.g, inv = g.inv, p = g.player;
      // 生命
      const hearts = $('#hearts');
      const life = Math.ceil(p.life);
      if (hearts.dataset.v !== String(life)) {
        hearts.dataset.v = String(life);
        hearts.innerHTML = '';
        for (let i = 0; i < 10; i++) {
          const h = el('span', 'heart' + (i * 2 < life ? '' : ' empty'));
          h.textContent = (life - i * 2 >= 2) ? '♥' : (life - i * 2 === 1 ? '♥' : '♡');
          hearts.appendChild(h);
        }
      }
      // 饱食度（10 个鸡腿位，每格 2 点）
      const hungerBar = $('#hunger');
      const hg = Math.ceil(p.hunger || 0);
      if (hungerBar && hungerBar.dataset.v !== String(hg)) {
        hungerBar.dataset.v = String(hg);
        hungerBar.innerHTML = '';
        for (let i = 0; i < 10; i++) {
          const f = el('span', 'food' + (i * 2 < hg ? '' : ' empty'));
          f.textContent = (hg - i * 2 >= 2) ? '◆' : (hg - i * 2 === 1 ? '◈' : '◇');
          hungerBar.appendChild(f);
        }
        hungerBar.title = '饱食度 ' + hg + ' / 20 · 手持食物右键食用';
      }
      // 状态效果（药水）
      const fx = $('#effects');
      if (fx) {
        const list = (p.effects && p.effects.list()) || [];
        const sig = list.map(a => a.id + a.level + Math.ceil(a.ticks / 60)).join(',');
        if (fx.dataset.sig !== sig) {
          fx.dataset.sig = sig;
          fx.innerHTML = '';
          for (const a of list) {
            const b = el('div', 'effect-chip' + (a.good ? '' : ' bad'));
            b.style.borderColor = a.color;
            b.style.color = a.color;
            b.textContent = a.short + (a.level > 1 ? a.level : '') +
                            ' ' + Math.ceil(a.ticks / 60) + 's';
            b.title = a.name + ' Lv' + a.level + ' · ' + Math.ceil(a.ticks / 60) + ' 秒';
            fx.appendChild(b);
          }
        }
      }
      // 等级 / 经验 / 天赋点
      const lv = $('#lvl');
      if (lv) {
        const pr = g.prog;
        const sig = pr.level + '/' + pr.xp + '/' + pr.need + '/' + pr.points;
        if (lv.dataset.sig !== sig) {
          lv.dataset.sig = sig;
          lv.innerHTML = '<span>Lv' + pr.level + '</span>' +
            '<span class="bar"><span class="fill" style="width:' +
            Math.round(100 * pr.xp / pr.need) + '%"></span></span>' +
            (pr.points ? '<span class="pts">+' + pr.points + '点</span>' : '');
        }
      }
      // 快捷栏
      for (let i = 0; i < 9; i++) {
        const s = inv.slots[i], h = this.hot[i];
        h.root.classList.toggle('sel', i === inv.selected);
        if (h.root.dataset.cache !== s.id + ':' + s.count + ':' + (this.iconVersion || 0)) {
          h.root.dataset.cache = s.id + ':' + s.count + ':' + (this.iconVersion || 0);
          paintGlyph(h.glyph, s.id);
          h.num.textContent = s.count > 1 ? String(s.count) : '';
          h.root.title = s.id ? DEF[s.id].cn : '';
        }
      }
      // 信息
      const bio = g.world.dim === 0 ? DMC.BIOME_NAMES[g.world.biomes[Math.floor(p.x)] || 0] : '—';
      const t = g.time;
      const clock = Math.floor((t / 24000 * 24 + 6) % 24);
      $('#info').textContent =
        `${C.DIM_NAMES[g.world.dim]}  ·  x ${Math.floor(p.x)} y ${Math.floor(p.y)}  ·  ${bio}  ·  ` +
        `${String(clock).padStart(2, '0')}:00  ·  ${p.creative ? '创造' : '生存'}` +
        (g.weather && g.world.dim === 0 ? '  ·  ' + g.weather.name : '') +
        (!g.sound || g.sound.enabled ? '' : '  ·  静音') +
        (g.lan.role === 'host' ? '  ·  主机' : g.lan.role === 'client' ? '  ·  客户端' : '');
      // 目标方块名
      const tg = g.target;
      $('#target-name').textContent = tg && tg.valid && tg.id
        ? (DEF[tg.id].cn + '  ' + (DEF[tg.id].hardness < 0 ? '不可破坏' : '')) : '';
      // toast
      if (this.toastTimer > 0 && --this.toastTimer === 0) $('#toast').classList.remove('show');
      // 面板进度
      if (this.panelKind === 'furnace') {
        const bar = $('#panel-progress');
        if (bar) bar.style.width = (g.panel.progress || 0) + '%';
      }
    }

    toast(msg) {
      const t = $('#toast');
      t.textContent = msg; t.classList.add('show');
      this.toastTimer = 190;
    }
    log(msg) {
      const box = $('#log');
      const line = el('div', 'log-line', msg);
      box.appendChild(line);
      while (box.children.length > 40) box.removeChild(box.firstChild);
      box.scrollTop = box.scrollHeight;
      box.classList.add('show');
      clearTimeout(this._logTimer);
      this._logTimer = setTimeout(() => box.classList.remove('show'), 7000);
    }

    /* ---------- 命令输入 ---------- */
    openCmd(pre) {
      const c = $('#cmd');
      c.classList.add('show'); c.value = pre || ''; c.focus();
    }
    closeCmd() { $('#cmd').classList.remove('show'); $('#game').focus(); }
    get cmdOpen() { return $('#cmd').classList.contains('show'); }

    toggleHelp(fromSplash) {
      const h = $('#help');
      h.classList.toggle('show');
      if (fromSplash) h.classList.add('show');
    }

    /* ---------- 面板 ---------- */
    openPanel(kind) {
      this.panelKind = kind;
      $('#panel').classList.add('show');
      $('#panel-title').textContent = ({ inv: '背包 · 2×2 合成', craft: '工作台 · 3×3 合成',
        furnace: '熔炉', enchant: '附魔台', brew: '酿造台', chest: '箱子' })[kind] || '背包';
      this.buildPanel();
    }
    closePanel() {
      if (!this.panelKind) return;
      this.g.inv.dropCursor();
      if (this.g.craft) this.g.craft.clearInto(this.g.inv);
      this.panelKind = null;
      $('#panel').classList.remove('show');
      this.renderCursor();
    }
    get panelOpen() { return !!this.panelKind; }

    slotEl(kind, index, id, count, extraCls) {
      const s = el('div', 'slot ' + (extraCls || ''));
      s.dataset.kind = kind; s.dataset.index = String(index);
      const gl = el('div', 'glyph');
      paintGlyph(gl, id);
      s.appendChild(gl);
      if (count > 1) s.appendChild(el('div', 'num', String(count)));
      s.title = id ? DEF[id].cn : '';
      return s;
    }

    buildPanel() {
      const g = this.g, body = $('#panel-body');
      body.innerHTML = '';
      const kind = this.panelKind;
      if (kind === 'inv' || kind === 'craft') {
        const size = kind === 'craft' ? 3 : 2;
        if (g.craft.size !== size) g.craft = new Inv.CraftGrid(size);
        const wrap = el('div', 'craft-wrap');
        const left = el('div', 'craft-left');
        left.appendChild(el('div', 'sub', size === 3 ? '3×3 合成' : '2×2 合成'));
        const grid = el('div', 'grid g' + size);
        for (let i = 0; i < size * size; i++) {
          grid.appendChild(this.slotEl('grid', i, g.craft.cells[i], 1));
        }
        left.appendChild(grid);
        const arrow = el('div', 'arrow', '➜');
        const res = g.craft.result();
        left.appendChild(arrow);
        left.appendChild(res ? this.slotEl('out', 0, res.out, res.count, 'out-slot ready')
                             : this.slotEl('out', 0, 0, 0, 'out-slot'));
        wrap.appendChild(left);
        const right = el('div', 'craft-right');
        right.appendChild(el('div', 'sub', '配方书 · 点击可自动摆料'));
        const search = el('input', 'recipe-search');
        search.type = 'text';
        search.placeholder = '搜索配方（中文名 / 英文名）…';
        right.appendChild(search);
        const list = el('div', 'recipe-list');
        right.appendChild(list);
        search.addEventListener('input', () =>
          this.renderRecipeList(list, size, search.value));
        this.renderRecipeList(list, size, '');
        const hint = el('div', 'recipe-hint');
        hint.innerHTML = '左键：拿起整堆 / 放下整堆 · 右键：拿一半 / 放一个<br>' +
          '点击右侧「可做」的配方会把材料从背包自动摆进合成格，再点输出格取出产物。';
        right.appendChild(hint);
        wrap.appendChild(right);
        body.appendChild(wrap);
      } else if (kind === 'furnace' || kind === 'brew' || kind === 'enchant') {
        const p = g.panel;
        const wrap = el('div', 'machine-wrap');
        const col = el('div', 'machine-col');
        col.appendChild(el('div', 'sub', '输入'));
        col.appendChild(this.slotEl('panel-input', 0, p.input.id, p.input.count));
        if (kind === 'furnace') {
          col.appendChild(el('div', 'sub', '燃料'));
          col.appendChild(this.slotEl('panel-fuel', 0, p.fuel.id, p.fuel.count));
        }
        wrap.appendChild(col);
        const mid = el('div', 'machine-mid');
        mid.appendChild(el('div', 'sub', kind === 'furnace' ? '烧炼' :
          kind === 'brew' ? '酿造' : '附魔'));
        const bar = el('div', 'progress');
        const inner = el('div', 'progress-in');
        inner.id = 'panel-progress';
        inner.style.width = (p.progress || 0) + '%';
        bar.appendChild(inner); mid.appendChild(bar);
        if (kind === 'enchant') {
          const btn = el('button', 'btn small', '消耗 3 青金石附魔手持物');
          btn.addEventListener('click', () => {
            const inv = g.inv, held = inv.held();
            if (!held.id) { this.toast('先把要附魔的物品拿在手上'); return; }
            if (!inv.remove(I.LAPIS, 3)) { this.toast('青金石不足（需要 3 个）'); return; }
            p.enchantLevel++;
            g.enchantBonus = Math.min(6, (g.enchantBonus || 0) + 1);
            this.toast('附魔成功！攻击 / 挖掘 +1（等级 ' + p.enchantLevel + '）');
          });
          mid.appendChild(btn);
        }
        if (kind === 'brew') {
          const btn = el('button', 'btn small', '酿造药水');
          btn.addEventListener('click', () => {
            if (!inv_remove(g, I.GLOW_BERRIES, 1)) { this.toast('需要发光浆果'); return; }
            const out = g.inv.add(I.POTION, 1);
            this.toast(out ? '背包已满' : '获得 药水');
          });
          mid.appendChild(btn);
        }
        wrap.appendChild(mid);
        const out = el('div', 'machine-col');
        out.appendChild(el('div', 'sub', '输出'));
        out.appendChild(this.slotEl('panel-output', 0, p.output.id, p.output.count));
        wrap.appendChild(out);
        body.appendChild(wrap);
        const tip = el('div', 'recipe-hint', kind === 'furnace'
          ? '把矿物 / 沙子 / 原木放进输入格，煤炭、烈焰棒可作燃料。每 100 帧产出一次。'
          : kind === 'brew' ? '酿造台：发光浆果 → 药水。' : '附魔台：消耗青金石强化手持物。');
        body.appendChild(tip);
      } else if (kind === 'chest') {
        const grid = el('div', 'grid g6');
        for (let i = 0; i < 27; i++) {
          const s = g.panel.chest[i];
          grid.appendChild(this.slotEl('chest', i, s.id, s.count));
        }
        body.appendChild(el('div', 'sub', '容器 · 27 格'));
        body.appendChild(grid);
      }
      // 背包 27 格 + 快捷栏
      body.appendChild(el('div', 'sub', '背包'));
      const bag = el('div', 'grid g9');
      for (let i = 9; i < 36; i++) {
        const s = g.inv.slots[i];
        bag.appendChild(this.slotEl('inv', i, s.id, s.count));
      }
      body.appendChild(bag);
      const hot = el('div', 'grid g9 panel-hot');
      for (let i = 0; i < 9; i++) {
        const s = g.inv.slots[i];
        hot.appendChild(this.slotEl('inv', i, s.id, s.count));
      }
      body.appendChild(hot);
      this.renderCursor();
    }

    // 配方书：按当前背包材料列出可合成配方，点击一键摆料
    renderRecipeList(list, size, query) {
      list.innerHTML = '';
      const q = (query || '').trim().toLowerCase();
      const all = Items.RECIPES.filter(r => Items.fitsSize(r, size) &&
        (!q || DEF[r.out].cn.includes(q) || DEF[r.out].name.includes(q)));
      const can = all.filter(r => Items.canCraft(r, this.g.inv, size));
      const rest = all.filter(r => !Items.canCraft(r, this.g.inv, size));
      list.appendChild(el('div', 'recipe-count',
        '可合成 ' + can.length + ' / ' + all.length + ' 条配方'));
      const addRow = (r, ok) => {
        const row = el('div', 'recipe-row ' + (ok ? 'ok' : 'locked'));
        const icon = el('div', 'recipe-icon');
        paintGlyph(icon, r.out);
        row.appendChild(icon);
        row.appendChild(el('span', 'recipe-name', DEF[r.out].cn + ' ×' + r.count));
        row.appendChild(el('span', 'recipe-tag', ok ? '可做' : '缺材料'));
        row.title = ok ? '点击自动摆放材料' : '材料不足';
        if (ok) row.addEventListener('click', () => this.quickCraft(r));
        list.appendChild(row);
      };
      can.forEach(r => addRow(r, true));
      rest.forEach(r => addRow(r, false));
    }
    quickCraft(r) {
      const g = this.g, inv = g.inv, size = g.craft.size;
      if (!Items.canCraft(r, inv, size)) { this.toast('材料不足'); return; }
      g.craft.clearInto(inv);                    // 网格里原有物品退回背包
      const need = Items.recipeNeeds(r);
      for (const k in need) inv.remove(Number(k), need[k]);
      g.craft.cells = Items.applyRecipe(r, size);
      this.buildPanel();
      this.toast('已摆放 ' + DEF[r.out].cn + ' 的材料，点输出格取出产物');
    }

    clickSlot(kind, i, right) {
      const g = this.g, inv = g.inv;
      let s = null;
      if (kind === 'inv') s = inv.slots[i];
      else if (kind === 'grid') {
        // 合成格：用光标物品往格子里放/取（每格只放 1 个）
        if (right) {
          if (inv.cursor.id && (!g.craft.cells[i] || g.craft.cells[i] === inv.cursor.id)) {
            if (!g.craft.cells[i]) g.craft.cells[i] = inv.cursor.id;
            inv.cursor.count--;
            if (inv.cursor.count <= 0) inv.cursor = Inv.slot(0, 0);
          } else if (!inv.cursor.id && g.craft.cells[i]) {
            inv.cursor = Inv.slot(g.craft.cells[i], 1);
            g.craft.cells[i] = 0;
          }
        } else {
          const t = g.craft.cells[i];
          if (inv.cursor.id === 0 && t) { inv.cursor = Inv.slot(t, 1); g.craft.cells[i] = 0; }
          else if (inv.cursor.id && t === 0) { g.craft.cells[i] = inv.cursor.id; inv.cursor.count--; if (inv.cursor.count <= 0) inv.cursor = Inv.slot(0, 0); }
          else if (inv.cursor.id && t && t !== inv.cursor.id) { g.craft.cells[i] = inv.cursor.id; inv.cursor = Inv.slot(t, 1); }
        }
        this.buildPanel(); return;
      } else if (kind === 'out') {
        const r = g.craft.result();
        if (!r) return;
        if (inv.cursor.id === 0) inv.cursor = Inv.slot(r.out, r.count);
        else if (inv.cursor.id === r.out && inv.cursor.count + r.count <= C.STACK) inv.cursor.count += r.count;
        else return;
        g.craft.consume();
        if (g.ach) g.ach.note('craft');
        this.buildPanel(); return;
      } else if (kind === 'panel-input') s = g.panel.input;
      else if (kind === 'panel-fuel') s = g.panel.fuel;
      else if (kind === 'panel-output') s = g.panel.output;
      else if (kind === 'chest') s = g.panel.chest[i];
      if (!s) return;
      if (kind === 'panel-output') {
        if (inv.cursor.id === 0 && s.id) { inv.cursor = Inv.slot(s.id, s.count); s.id = 0; s.count = 0; }
        this.buildPanel(); return;
      }
      if (right) Inv.cursorRight(inv.cursor, s);
      else Inv.cursorLeft(inv.cursor, s);
      this.buildPanel();
    }

    renderCursor() {
      const c = $('#cursor'), cur = this.g.inv.cursor;
      if (!cur || !cur.id) { c.classList.remove('show'); return; }
      c.classList.add('show');
      c.innerHTML = '';
      const tp = DMC.texpack, tex = tp && tp.get(cur.id);
      if (tex) {
        const cv = doc.createElement('canvas');
        cv.width = 32; cv.height = 32; cv.className = 'tex-img';
        cv.getContext('2d').drawImage(tex, 0, 0);
        c.appendChild(cv);
        c.style.background = 'rgba(20,24,34,.92)';
        c.style.color = '#fff';
      } else {
        c.style.background = DEF[cur.id] ? DEF[cur.id].color : '#888';
        c.style.color = '#10131a';
        c.textContent = itemGlyph(cur.id);
      }
      if (cur.count > 1) c.appendChild(el('span', 'cursor-num', String(cur.count)));
      c.title = DEF[cur.id] ? DEF[cur.id].cn : '';
    }
  }

  function inv_remove(g, id, n) { return g.inv.remove(id, n); }

  DMC.UI = UI;
})(typeof globalThis !== 'undefined' ? globalThis : this);
