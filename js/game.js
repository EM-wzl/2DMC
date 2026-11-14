/* 2DMC Web — 主循环、输入、挖掘/放置、维度、命令、存档 */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, Core, DEF, Items, Inv, World, Entities, Renderer, UI, LAN } = DMC;
  const { clamp, dist } = Core;
  const T = C.TILE;
  const $ = s => document.querySelector(s);

  class Game {
    constructor() {
      this.canvas = $('#game');
      this.renderer = new Renderer(this.canvas);
      this.seed = (Date.now() & 0x7fffffff);
      this.world = new World(this.seed);
      this.world.generateOverworld();
      this.player = new Entities.Player(C.W / 2 + 0.5, 0);
      this.player.respawn(this.world);
      this.inv = new Inv.Inventory();
      this.starterKit();
      this.craft = new Inv.CraftGrid(2);
      this.panel = new Inv.PanelState('furnace');
      this.mobs = [];
      this.projectiles = [];
      this.drops = [];
      this.remotePlayers = [];
      this.dragon = new Entities.EnderDragon(this.world);
      this.lan = new LAN(this);
      this.ui = new UI(this);
      this.input = { left: 0, right: 0, up: 0, down: 0, jump: 0 };
      this.mouse = { x: 0, y: 0, left: false, right: false };
      this.target = { x: 0, y: 0, valid: false, id: 0 };
      this.breakProgress = 0;
      this.breakKey = '';
      this.backPos = { x: this.player.x, y: this.player.y };
      this.time = 6000;
      this.tick = 0;
      this.enchantBonus = 0;
      this.paused = false;
      this._deathHandled = false;
      this._dragonAPI = this.makeDragonAPI();
      this.bindInput();
      // 其余维度延迟生成，避免阻塞首帧
      setTimeout(() => {
        this.world.generateNether();
        this.world.generateEnd();
        if (this.world.dragonDead) this.dragon.alive = false;
      }, 60);
      this.loop = this.loop.bind(this);
      requestAnimationFrame(this.loop);
      this.ui.log('2DMC Web 已启动 · 按 H 查看操作说明');
    }

    starterKit() {
      const inv = this.inv;
      inv.add(I.DIAMOND_PICK, 1);
      inv.add(I.DIAMOND_SWORD, 1);
      inv.add(B.CRAFTING_TABLE, 16);
      inv.add(B.FURNACE, 4);
      inv.add(B.TORCH, 32);
      inv.add(B.OBSIDIAN, 24);
      inv.add(I.ENDER_PEARL, 8);
      inv.add(B.END_PORTAL_FRAME, 12);
      inv.add(B.END_CRYSTAL, 4);
      inv.add(I.RAW_IRON, 16);
      inv.add(I.COAL, 16);
      inv.selected = 0;
    }

    startNew(seed) {
      this.seed = seed >>> 0;
      this.world = new World(this.seed);
      this.world.generateAll();
      this.player.respawn(this.world);
      this.inv = new Inv.Inventory();
      this.starterKit();
      this.craft = new Inv.CraftGrid(2);
      this.mobs = []; this.projectiles = []; this.drops = [];
      this.dragon = new Entities.EnderDragon(this.world);
      this.time = 6000; this.tick = 0;
      this._deathHandled = false;
      this.renderer.tiles.clear();
      this.ui.toast('新世界已生成 · 种子 ' + this.seed);
    }

    /* ---------- 输入 ---------- */
    bindInput() {
      const cv = this.canvas;
      document.addEventListener('keydown', e => {
        if (this.ui.cmdOpen) return;
        const k = e.key.toLowerCase();
        if (['arrowup', 'arrowdown', 'arrowleft', 'arrowright', ' ', 'tab'].includes(k)) e.preventDefault();
        if (k >= '1' && k <= '9') { this.inv.selected = Number(k) - 1; return; }
        if (k === 'a' || k === 'arrowleft') this.input.left = 1;
        if (k === 'd' || k === 'arrowright') this.input.right = 1;
        if (k === 'w' || k === 'arrowup' || k === ' ') this.input.jump = 1;
        if (k === 's' || k === 'arrowdown') this.input.down = 1;
        if (k === 'e') { this.ui.panelOpen ? this.ui.closePanel() : this.ui.openPanel('inv'); }
        if (k === 'f') {
          this.player.creative = !this.player.creative;
          this.player.fly = this.player.creative;
          this.ui.toast(this.player.creative ? '创造模式（空格上升 / Shift 下降）' : '生存模式');
        }
        if (k === 'escape') this.ui.closePanel();
        if (k === 'h') this.ui.toggleHelp();
        if (k === 'enter' || k === '/') this.ui.openCmd(k === '/' ? '/' : '');
        if (k === 'shift' || k === 'control') this.input.down = 1;
      });
      document.addEventListener('keyup', e => {
        const k = e.key.toLowerCase();
        if (k === 'a' || k === 'arrowleft') this.input.left = 0;
        if (k === 'd' || k === 'arrowright') this.input.right = 0;
        if (k === 'w' || k === 'arrowup' || k === ' ') this.input.jump = 0;
        if (k === 's' || k === 'arrowdown') this.input.down = 0;
        if (k === 'shift' || k === 'control') this.input.down = 0;
      });
      cv.addEventListener('mousemove', e => this.updateMouse(e));
      cv.addEventListener('mousedown', e => {
        if (this.ui.panelOpen) return;
        this.updateMouse(e);
        if (e.button === 0) { this.mouse.left = true; this.tryHit(true); }
        if (e.button === 2) { this.rightClick(); }
      });
      document.addEventListener('mouseup', e => { if (e.button === 0) { this.mouse.left = false; this.breakProgress = 0; } });
      cv.addEventListener('contextmenu', e => e.preventDefault());
      cv.addEventListener('wheel', e => {
        e.preventDefault();
        this.inv.selected = (this.inv.selected + (e.deltaY > 0 ? 1 : 8)) % 9;
      }, { passive: false });
    }

    updateMouse(e) {
      const r = this.canvas.getBoundingClientRect();
      const mx = e.clientX - r.left, my = e.clientY - r.top;
      this.mouse.x = mx; this.mouse.y = my;
      const wx = Math.floor((this.renderer.cam.x + mx) / T);
      const wy = Math.floor((this.renderer.cam.y + my) / T);
      const d = dist(this.player.x, this.player.y, wx + 0.5, wy + 0.5);
      this.target = { x: wx, y: wy, valid: d <= C.REACH && Core.inWorld(wx, wy),
                      id: Core.inWorld(wx, wy) ? this.world.get(wx, wy) : 0 };
    }

    /* ---------- 战斗 / 挖掘 ---------- */
    tryHit(first) {
      const g = this, p = g.player;
      if (p.attackCd > 0 && first) return;
      const wx = (this.renderer.cam.x + this.mouse.x) / T;
      const wy = (this.renderer.cam.y + this.mouse.y) / T;
      // 先判龙首
      if (this.world.dim === 2 && this.dragon && this.dragon.alive) {
        const h = this.dragon.head;
        if (dist(wx, wy, h.x, h.y) < 2.2 && dist(p.x, p.y, h.x, h.y) < 5.5) {
          if (p.attackCd <= 0) {
            const dmg = p.attackDamage(this.inv) + (this.enchantBonus || 0);
            const dead = this.dragon.hurt(dmg);
            this.renderer.emit(h.x, h.y, '#e08bff', 10, { speed: 0.3 });
            p.attackCd = 22;
            if (dead) this.dragon.die(this._dragonAPI, this.world);
            else this.ui.toast('命中末影龙！它被打落开始栖息');
          }
          return;
        }
      }
      // 再判生物
      for (const m of this.mobs) {
        if (dist(wx, wy, m.x, m.y) < Math.max(0.9, m.w) &&
            dist(p.x, p.y, m.x, m.y) < 3.6) {
          if (p.attackCd <= 0) {
            const killed = m.hurt(p.attackDamage(this.inv) + (this.enchantBonus || 0));
            m.vx = (m.x > p.x ? 1 : -1) * 3; m.vy = -3;
            p.attackCd = 22;
            this.renderer.emit(m.x, m.y, '#ff6666', 6);
            if (killed) this.killMob(m);
          }
          return;
        }
      }
    }

    killMob(m) {
      const drops = {
        zombie: I.LEATHER, skeleton: I.BONE, creeper: I.GUNPOWDER,
        spider: I.STRING, piglin: I.GOLD_INGOT, cube: I.RAW_SULFUR,
        sheep: I.LEATHER, enderman: I.ENDER_PEARL
      };
      const id = drops[m.type];
      if (id) this.drops.push({ x: m.x, y: m.y, id, count: 1 });
      this.renderer.emit(m.x, m.y, m.def.color, 12);
    }

    mine(dt) {
      if (!this.mouse.left || this.ui.panelOpen || !this.target.valid) { this.breakProgress = 0; return; }
      const { x, y } = this.target;
      const id = this.world.get(x, y);
      if (!id) { this.breakProgress = 0; return; }
      const d = DEF[id];
      if (!d || d.hardness < 0) { this.breakProgress = 0; return; }
      const key = x + ',' + y + ',' + id + ',' + this.world.dim;
      if (key !== this.breakKey) { this.breakKey = key; this.breakProgress = 0; }
      if (this.player.creative) this.breakProgress = 1;
      else {
        const tier = this.player.toolTier(this.inv) + (this.enchantBonus || 0);
        const speed = 1 + tier * 0.85;
        this.breakProgress += (speed * 0.016) / Math.max(1, d.hardness / 12);
      }
      if (Math.random() < 0.12)
        this.renderer.emit(x + 0.5, y + 0.5, d.color, 1, { speed: 0.08, life: 18, size: 2 });
      if (this.breakProgress >= 1) {
        this.breakBlock(x, y, id, d);
        this.breakProgress = 0;
      }
    }

    breakBlock(x, y, id, d) {
      const p = this.player;
      const tier = p.toolTier(this.inv) + (this.enchantBonus || 0);
      this.world.set(x, y, B.AIR);
      this.renderer.blockBreak(x, y, id);
      this.lan.block(x, y, B.AIR);
      // 末影水晶被破坏 → 停止回血
      if (id === B.END_CRYSTAL) {
        for (const c of this.world.crystals) if (c.x === x && c.y === y) c.alive = false;
        this.ui.toast('末影水晶已破坏：末影龙不再回血（剩 ' + this.world.crystalsAlive() + ' 枚）');
      }
      if (p.creative) return;
      if (d.tier > tier) return;   // 工具等级不足不掉东西
      const dropId = d.drop === id ? id : d.drop;
      this.drops.push({ x: x + 0.5, y: y + 0.5, id: dropId, count: 1 });
    }

    /* ---------- 右键：交互 / 放置 ---------- */
    rightClick() {
      if (!this.target.valid || this.ui.panelOpen) return;
      const { x, y } = this.target;
      const id = this.world.get(x, y);
      const held = this.inv.held();
      // 交互方块
      if (id === B.CRAFTING_TABLE) { this.ui.openPanel('craft'); return; }
      if (id === B.FURNACE) { this.panel.kind = 'furnace'; this.ui.openPanel('furnace'); return; }
      if (id === B.CHEST || id === B.BARREL) { this.panel.kind = 'chest'; this.ui.openPanel('chest'); return; }
      if (id === B.ENCHANTING) { this.panel.kind = 'enchant'; this.ui.openPanel('enchant'); return; }
      if (id === B.BREWING) { this.panel.kind = 'brew'; this.ui.openPanel('brew'); return; }
      if (id === B.CHORUS_FLOWER && this.world.dim === 2) { this.chorusTeleport(); return; }
      if (id === B.END_PORTAL || id === B.NETHER_PORTAL) { this.usePortal(id, x, y); return; }
      if (id === B.END_PORTAL_FRAME && held.id === B.DRAGON_EGG) {
        this.world.set(x, y, B.END_PORTAL);
        this.inv.consumeHeld(1);
        this.ui.toast('龙蛋激活了末地传送门！右键传送门进入末地');
        return;
      }
      if (id === B.DRAGON_EGG && this.world.dim === 2) {
        this.world.set(x, y, B.AIR);
        this.inv.add(B.DRAGON_EGG, 1);
        this.lan.block(x, y, B.AIR);
        this.ui.toast('拾取龙蛋（带回家可激活末地传送门框架）');
        return;
      }
      // 放置
      if (!held.id || !Items.isBlock(held.id)) return;
      let px = x, py = y;
      if (this.world.get(x, y) !== 0 && DEF[this.world.get(x, y)].solid) {
        const cand = [[x, y - 1], [x - 1, y], [x + 1, y], [x, y + 1]];
        let found = false;
        for (const [cx, cy] of cand) {
          if (!Core.inWorld(cx, cy) || this.world.get(cx, cy) !== 0) continue;
          if (dist(this.player.x, this.player.y, cx + 0.5, cy + 0.5) < 0.8) continue;
          px = cx; py = cy; found = true; break;
        }
        if (!found) return;
      }
      if (dist(this.player.x, this.player.y, px + 0.5, py + 0.5) < 0.8) return;
      if (this.world.get(px, py) !== 0 && this.world.get(px, py) !== B.WATER) return;
      this.world.set(px, py, held.id);
      this.lan.block(px, py, held.id);
      this.renderer.emit(px + 0.5, py + 0.5, DEF[held.id].color, 4, { speed: 0.1 });
      if (!this.player.creative) this.inv.consumeHeld(1);
    }

    usePortal(id, x, y) {
      const w = this.world;
      if (id === B.NETHER_PORTAL) {
        if (w.dim === 0) {
          this.backPos = { x: this.player.x, y: this.player.y };
          this.switchDim(1);
          this.ui.toast('进入下界');
        } else if (w.dim === 1) this.switchDim(0);
      } else if (id === B.END_PORTAL) {
        if (w.dim === 2) {
          this.switchDim(0);
          this.player.x = this.backPos.x; this.player.y = this.backPos.y;
          this.ui.toast('通过退出传送门回到主世界');
        } else {
          this.backPos = { x: this.player.x, y: this.player.y };
          this.switchDim(2);
          this.ui.toast('进入末地：浮空岛下方是虚空，末影龙盘旋在中央岛上空');
        }
      }
    }
    switchDim(d) {
      this.world.switchTo(d);
      const sp = this.world.spawnPoint();
      this.player.x = sp.x + 0.5; this.player.y = sp.y - 0.4;
      this.player.vx = 0; this.player.vy = 0;
      this.renderer.tiles.clear();
      this.lan.dimension(d);
    }
    chorusTeleport() {
      const w = this.world;
      for (let i = 0; i < 30; i++) {
        const x = Math.floor(C.W / 2 + (Math.random() - 0.5) * 500);
        const y = w.groundY(x, 100);
        if (x > 4 && x < C.W - 4 && y > 4) {
          this.player.x = x + 0.5; this.player.y = y - 1;
          this.player.vy = 0;
          this.renderer.emit(x, y, '#c46bff', 18);
          this.ui.toast('紫颂花把你传送到了末地的另一处');
          return;
        }
      }
    }

    /* ---------- 龙回调 ---------- */
    makeDragonAPI() {
      const g = this;
      return {
        spawnBreath(x, y, px, py) {
          const dx = px - x, dy = py - y;
          const d = Math.hypot(dx, dy) + 0.001;
          g.projectiles.push(new Entities.Projectile('breath', x, y, dx / d * 0.36, dy / d * 0.36, 3));
          g.renderer.emit(x, y, '#c46bff', 4, { speed: 0.12, life: 24, grav: 0 });
        },
        onDragonDeath(x, y) {
          if (g._deathHandled) return;
          g._deathHandled = true;
          g.renderer.explosion(x, y, 4);
          g.inv.add(B.DRAGON_EGG, 1);
          const ep = g.world.endPortal;
          g.world.set(ep.x, ep.y, B.END_PORTAL);
          g.world.set(ep.x, ep.y - 1, B.DRAGON_EGG);
          g.lan.block(ep.x, ep.y, B.END_PORTAL);
          g.ui.toast('末影龙被击败！龙蛋已掉入背包，退出传送门已激活');
          g.ui.log('[成就] 末影龙已被击败 · 龙蛋 ×1');
        }
      };
    }

    /* ---------- 生物 / 掉落物 ---------- */
    spawnMobs() {
      if (this.tick % 150 !== 0 || this.mobs.length > 16) return;
      const w = this.world, p = this.player;
      if (w.dim === 2) return;
      const night = this.time >= 12000;
      if (w.dim === 0 && !night && p.y < w.heights[Math.floor(p.x)] - 12) return;
      const types = w.dim === 0
        ? ['zombie', 'skeleton', 'creeper', 'spider', 'enderman', 'sheep', 'cube']
        : ['piglin', 'cube', 'cube'];
      const type = types[Math.floor(Math.random() * types.length)];
      const x = Math.floor(p.x + (Math.random() < 0.5 ? -1 : 1) * (14 + Math.random() * 20));
      if (x < 4 || x > C.W - 4) return;
      const y = w.groundY(x, clamp(p.y - 6, 10, C.H - 12));
      if (w.solid(x, y) || !w.solid(x, y + 1)) return;
      this.mobs.push(new Entities.Mob(type, x + 0.5, y));
    }
    updateDrops() {
      for (let i = this.drops.length - 1; i >= 0; i--) {
        const d = this.drops[i];
        if (dist(d.x, d.y, this.player.x, this.player.y) < 1.3) {
          this.inv.add(d.id, d.count || 1);
          this.renderer.emit(d.x, d.y, '#ffe9a8', 5, { speed: 0.1 });
          this.drops.splice(i, 1);
        }
      }
    }

    /* ---------- 命令 ---------- */
    command(raw) {
      const s = raw.trim().replace(/^\//, '');
      const [cmd, ...args] = s.split(/\s+/);
      const g = this;
      switch (cmd.toLowerCase()) {
        case 'help':
          g.ui.log('命令：/help /end /nether /overworld /lan host|join|stop|status /say <消息> ' +
                   '/give <名称|id> [数量] /tp <x> <y> /gamemode /time <0-24000> /seed /save /load ' +
                   '/spawn <生物> /dragon /selftest /clear');
          break;
        case 'end':
          if (g.world.dim === 2) {
            g.switchDim(0); g.player.x = g.backPos.x; g.player.y = g.backPos.y;
            g.ui.toast('回到主世界');
          } else { g.backPos = { x: g.player.x, y: g.player.y }; g.switchDim(2); g.ui.toast('传送到末地：小心末影龙！'); }
          break;
        case 'nether': g.backPos = { x: g.player.x, y: g.player.y }; g.switchDim(1); break;
        case 'overworld': g.switchDim(0); break;
        case 'lan': {
          const sub = (args[0] || '').toLowerCase();
          let r;
          if (sub === 'host') r = g.lan.host();
          else if (sub === 'join') r = g.lan.join();
          else if (sub === 'stop') r = g.lan.stop();
          else if (sub === 'status') r = g.lan.status();
          else r = '用法: /lan host | /lan join | /lan stop | /lan status';
          g.ui.log('[LAN] ' + r); g.ui.toast(r);
          break;
        }
        case 'say': g.lan.chat(args.join(' ')); break;
        case 'give': {
          const q = args[0] || '';
          const n = clamp(parseInt(args[1], 10) || 1, 1, 64);
          let id = parseInt(q, 10);
          if (isNaN(id)) {
            id = 0;
            for (const k in DEF) {
              const d = DEF[k];
              if (d.cn === q || d.name === q || d.name.includes(q.toLowerCase())) { id = Number(k); break; }
            }
          }
          if (!id || !DEF[id]) { g.ui.log('找不到物品：' + q); break; }
          g.inv.add(id, n); g.ui.toast('获得 ' + DEF[id].cn + ' ×' + n);
          break;
        }
        case 'tp': {
          const x = parseFloat(args[0]), y = parseFloat(args[1]);
          if (!isNaN(x) && !isNaN(y)) { g.player.x = clamp(x, 2, C.W - 2); g.player.y = clamp(y, 2, C.H - 2); g.player.vy = 0; }
          break;
        }
        case 'gamemode': {
          const m = (args[0] || '').toLowerCase();
          g.player.creative = m === 'creative' || m === '1' || (!m && !g.player.creative);
          g.player.fly = g.player.creative;
          g.ui.toast(g.player.creative ? '创造模式' : '生存模式'); break;
        }
        case 'time': { const t = parseInt(args[0], 10); if (!isNaN(t)) g.time = ((t % 24000) + 24000) % 24000; break; }
        case 'seed': g.ui.log('种子：' + g.world.seed); break;
        case 'save': g.save(); g.ui.toast('世界已保存'); break;
        case 'load': g.ui.toast(g.load() ? '存档已读取' : '没有找到存档'); break;
        case 'spawn': {
          const t = (args[0] || 'zombie').toLowerCase();
          if (!Entities.MOB_DEFS[t]) { g.ui.log('未知生物：' + t); break; }
          g.mobs.push(new Entities.Mob(t, g.player.x + 3, g.player.y - 1));
          g.ui.toast('生成 ' + Entities.MOB_DEFS[t].name); break;
        }
        case 'dragon':
          if (g.world.dim !== 2) { g.backPos = { x: g.player.x, y: g.player.y }; g.switchDim(2); }
          g.player.x = g.dragon.head.x; g.player.y = g.dragon.head.y + 5;
          g.dragon.hp = g.dragon.maxHp; g.dragon.alive = true;
          g.world.dragonDead = false; g._deathHandled = false;
          g.ui.toast('已传送到末影龙附近，龙已恢复满血'); break;
        case 'selftest': {
          const r = DMC.SelfTest.run();
          g.ui.log('[自检] ' + r.passed + ' 通过 / ' + r.failed + ' 失败');
          r.lines.filter(l => l.startsWith('FAIL')).forEach(l => g.ui.log(l));
          g.ui.toast('自检完成：' + r.passed + '/' + (r.passed + r.failed));
          break;
        }
        case 'clear': g.mobs = []; g.projectiles = []; g.drops = []; g.ui.toast('已清空实体'); break;
        default: g.ui.log('未知命令: ' + cmd + '（输入 /help 查看）');
      }
    }

    /* ---------- 存档 ---------- */
    save() {
      try {
        const data = {
          world: this.world.serialize(),
          player: { x: this.player.x, y: this.player.y, life: this.player.life,
                    creative: this.player.creative },
          inv: this.inv.toJSON(), craft: this.craft.toJSON(),
          panel: this.panel.toJSON(), dragon: this.dragon.serialize(),
          time: this.time, tick: this.tick, backPos: this.backPos,
          enchantBonus: this.enchantBonus
        };
        localStorage.setItem(C.SAVE_KEY, JSON.stringify(data));
        return true;
      } catch (e) { this.ui.toast('保存失败：' + e.message); return false; }
    }
    load() {
      try {
        const raw = localStorage.getItem(C.SAVE_KEY);
        if (!raw) return false;
        const d = JSON.parse(raw);
        const w = World.deserialize(d.world);
        if (!w) return false;
        this.world = w;
        this.seed = w.seed;
        this.inv = Inv.Inventory.fromJSON(d.inv);
        this.craft = Inv.CraftGrid.fromJSON(d.craft);
        this.panel = Inv.PanelState.fromJSON(d.panel);
        this.player.creative = !!d.player.creative;
        this.player.fly = this.player.creative;
        this.player.x = d.player.x; this.player.y = d.player.y;
        this.player.life = d.player.life; this.player.dead = false;
        this.time = d.time || 6000; this.tick = d.tick || 0;
        this.backPos = d.backPos || { x: this.player.x, y: this.player.y };
        this.enchantBonus = d.enchantBonus || 0;
        this.dragon = Entities.EnderDragon.restore(d.dragon, this.world);
        this._deathHandled = !!(d.dragon && d.dragon.alive === false);
        this.mobs = []; this.projectiles = []; this.drops = [];
        this.renderer.tiles.clear();
        this.ui.buildHotbar();
        this.ui.toast('存档已读取（种子 ' + w.seed + '）');
        return true;
      } catch (e) { this.ui.toast('读档失败：' + e.message); return false; }
    }

    /* ---------- 主循环 ---------- */
    loop() {
      requestAnimationFrame(this.loop);
      if (this.ui.cmdOpen || this.ui.panelOpen) {
        this.renderer.render(this);
        this.ui.render();
        return;
      }
      this.tick++;
      this.time = (this.time + 1) % 24000;
      const p = this.player;
      p.update(this.world, this.input);
      if (p.dead) {
        p.respawn(this.world);
        this.ui.toast('你死了，已重生');
      }
      this.mine(1);
      // 生物
      this.spawnMobs();
      const mobAPI = {
        explode: (x, y, r) => {
          this.renderer.explosion(x, y, r);
          for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) {
            const qx = Math.floor(x) + dx, qy = Math.floor(y) + dy;
            if (dx * dx + dy * dy > r * r) continue;
            if (this.world.get(qx, qy) && DEF[this.world.get(qx, qy)].hardness >= 0)
              this.breakBlock(qx, qy, this.world.get(qx, qy), DEF[this.world.get(qx, qy)]);
          }
          if (dist(x, y, p.x, p.y) < r + 1) p.hurt(9, x);
        }
      };
      for (let i = this.mobs.length - 1; i >= 0; i--) {
        const m = this.mobs[i];
        m.update(this.world, p, mobAPI);
        if (m.dead) { this.killMob(m); this.mobs.splice(i, 1); }
      }
      // 投射物
      for (let i = this.projectiles.length - 1; i >= 0; i--) {
        const pr = this.projectiles[i];
        pr.update(this.world, p);
        if (pr.type === 'breath' && this.tick % 3 === 0)
          this.renderer.emit(pr.x, pr.y, '#b06be0', 1, { speed: 0.03, grav: 0, life: 20, size: 3 });
        if (pr.dead) this.projectiles.splice(i, 1);
      }
      this.updateDrops();
      // 龙
      if (this.world.dim === 2 && this.dragon) this.dragon.step(this.world, p, this._dragonAPI);
      // 通用面板
      if (this.ui.panelKind === 'furnace') {
        if (this.panel.tick()) this.ui.buildPanel();
      }
      // 局域网
      this.lan.update();
      // 自动保存
      if (this.tick % 1800 === 0) this.save();
      this.renderer.render(this);
      this.ui.render();
    }
  }

  DMC.Game = Game;
  root.addEventListener('DOMContentLoaded', () => {
    if (root.DMC && !root.DMC_NO_AUTOSTART) {
      try { root.game = new Game(); }
      catch (e) {
        console.error(e);
        const t = document.querySelector('#toast');
        if (t) { t.textContent = '启动失败：' + e.message; t.classList.add('show'); }
      }
    }
  });
})(typeof globalThis !== 'undefined' ? globalThis : this);
