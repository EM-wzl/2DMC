/* 2DMC Web — Canvas 渲染：程序化方块纹理、光照、生物、末影龙、粒子 */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, Core, DEF } = DMC;
  const { clamp, lerp } = Core;
  const T = C.TILE;

  class Renderer {
    constructor(canvas) {
      this.cv = canvas;
      this.ctx = canvas.getContext('2d', { alpha: false });
      this.tiles = new Map();
      this.cam = { x: 0, y: 0 };
      this.particles = [];
      this.dpr = 1;
      this.resize();
      root.addEventListener('resize', () => this.resize());
    }
    resize() {
      const rect = this.cv.getBoundingClientRect();
      const w = Math.max(320, Math.floor(rect.width || 960));
      const h = Math.max(240, Math.floor(rect.height || 560));
      this.dpr = Math.min(2, root.devicePixelRatio || 1);
      this.cv.width = Math.floor(w * this.dpr);
      this.cv.height = Math.floor(h * this.dpr);
      this.vw = w; this.vh = h;
      this.cols = Math.ceil(w / T) + 2;
      this.rows = Math.ceil(h / T) + 2;
    }

    /* ---------- 程序化方块纹理缓存 ---------- */
    tile(id) {
      if (this.tiles.has(id)) return this.tiles.get(id);
      if (!id || id === B.AIR) { this.tiles.set(id, null); return null; }
      // 优先用项目内的 MC 26.2 原版贴图（TexturePack 已把 16x16 首帧放大成 32x32）
      const tp = DMC.texpack;
      const tex = tp && tp.get(id);
      if (tex) { this.tiles.set(id, tex); return tex; }
      const d = DEF[id];
      const s = 32;
      const c = document.createElement('canvas');
      c.width = s; c.height = s;
      const g = c.getContext('2d');
      if (!d || d.tex === 'air') { this.tiles.set(id, null); return null; }
      const rnd = (n) => (Math.sin(n * 12.9898 + id * 78.233) * 43758.5453) % 1;
      const grain = (base, alt, n, size) => {
        g.fillStyle = base; g.fillRect(0, 0, s, s);
        for (let i = 0; i < n; i++) {
          const rx = Math.abs(rnd(i * 3 + 1)) * s;
          const ry = Math.abs(rnd(i * 3 + 2)) * s;
          const rw = 1 + Math.abs(rnd(i * 3 + 3)) * size;
          g.fillStyle = alt;
          g.fillRect(Math.floor(rx), Math.floor(ry), Math.ceil(rw), Math.ceil(rw));
        }
      };
      switch (d.tex) {
        case 'grass':
          g.fillStyle = d.color2; g.fillRect(0, 0, s, s);
          g.fillStyle = d.color; g.fillRect(0, 0, s, 9);
          g.fillStyle = '#74b657';
          for (let i = 0; i < 18; i++) g.fillRect(Math.floor(Math.abs(rnd(i)) * s), 0, 2, 3 + Math.abs(rnd(i + 9)) * 7);
          break;
        case 'dirt': grain(d.color, '#63472e', 34, 3); break;
        case 'stone': grain(d.color, 'rgba(255,255,255,.07)', 22, 3); break;
        case 'sand': grain(d.color, 'rgba(255,255,255,.13)', 26, 2); break;
        case 'ore':
          grain('#7c7c7c', 'rgba(0,0,0,.16)', 20, 3);
          g.fillStyle = d.color2;
          for (let i = 0; i < 6; i++) {
            const x = 3 + Math.floor(Math.abs(rnd(i * 5 + 1)) * 24);
            const y = 3 + Math.floor(Math.abs(rnd(i * 5 + 2)) * 24);
            const w = 3 + Math.floor(Math.abs(rnd(i * 5 + 3)) * 4);
            g.fillRect(x, y, w, w);
            g.fillStyle = 'rgba(255,255,255,.28)';
            g.fillRect(x, y, Math.max(1, w - 2), 1);
            g.fillStyle = d.color2;
          }
          break;
        case 'wood':
          grain(d.color, d.color2, 10, 3);
          g.strokeStyle = 'rgba(0,0,0,.22)'; g.lineWidth = 2;
          for (let x = 5; x < s; x += 8) { g.beginPath(); g.moveTo(x, 0); g.lineTo(x, s); g.stroke(); }
          g.strokeStyle = 'rgba(255,255,255,.12)'; g.lineWidth = 1;
          g.strokeRect(2, 2, s - 4, s - 4);
          break;
        case 'plank':
          grain(d.color, 'rgba(0,0,0,.10)', 12, 3);
          g.strokeStyle = 'rgba(0,0,0,.22)'; g.lineWidth = 1;
          for (let y = 8; y < s; y += 8) { g.beginPath(); g.moveTo(0, y); g.lineTo(s, y); g.stroke(); }
          break;
        case 'leaves':
          g.fillStyle = d.color; g.fillRect(0, 0, s, s);
          g.fillStyle = 'rgba(255,255,255,.10)';
          for (let i = 0; i < 24; i++) g.fillRect(Math.floor(Math.abs(rnd(i)) * s), Math.floor(Math.abs(rnd(i + 7)) * s), 4, 3);
          g.fillStyle = 'rgba(0,0,0,.16)';
          for (let i = 0; i < 12; i++) g.fillRect(Math.floor(Math.abs(rnd(i + 3)) * s), Math.floor(Math.abs(rnd(i + 11)) * s), 3, 3);
          break;
        case 'brick':
          grain(d.color, 'rgba(0,0,0,.09)', 10, 3);
          g.strokeStyle = 'rgba(0,0,0,.26)'; g.lineWidth = 2;
          g.strokeRect(0, 0, s, s);
          g.beginPath(); g.moveTo(0, 16); g.lineTo(s, 16);
          g.moveTo(16, 0); g.lineTo(16, 16); g.moveTo(8, 16); g.lineTo(8, 32); g.stroke();
          break;
        case 'pillar':
          grain(d.color, 'rgba(255,255,255,.08)', 12, 2);
          g.fillStyle = 'rgba(255,255,255,.15)'; g.fillRect(0, 0, 5, s);
          g.fillStyle = 'rgba(0,0,0,.20)'; g.fillRect(s - 5, 0, 5, s);
          g.strokeStyle = 'rgba(0,0,0,.25)'; g.lineWidth = 2;
          g.beginPath(); g.moveTo(0, 8); g.lineTo(s, 8); g.moveTo(0, 24); g.lineTo(s, 24); g.stroke();
          break;
        case 'machine':
          grain(d.color, 'rgba(0,0,0,.14)', 14, 3);
          g.strokeStyle = 'rgba(0,0,0,.35)'; g.lineWidth = 2; g.strokeRect(1, 1, s - 2, s - 2);
          g.fillStyle = d.color2 || 'rgba(255,255,255,.25)';
          g.fillRect(9, 9, 14, 5);
          g.fillStyle = 'rgba(0,0,0,.35)'; g.fillRect(9, 18, 14, 5);
          break;
        case 'glass':
          g.fillStyle = 'rgba(190,230,240,.22)'; g.fillRect(0, 0, s, s);
          g.strokeStyle = 'rgba(225,250,255,.85)'; g.lineWidth = 2; g.strokeRect(2, 2, s - 4, s - 4);
          g.beginPath(); g.moveTo(5, 24); g.lineTo(14, 7); g.lineTo(19, 7); g.stroke();
          break;
        case 'liquid':
          g.fillStyle = d.color; g.globalAlpha = 0.78; g.fillRect(0, 0, s, s);
          g.globalAlpha = 1;
          g.fillStyle = 'rgba(255,255,255,.20)';
          for (let x = 0; x < s; x += 8) g.fillRect(x, 5 + Math.abs(rnd(x)) * 4, 5, 2);
          break;
        case 'plant':
          g.clearRect(0, 0, s, s);
          g.fillStyle = d.color;
          g.fillRect(13, 8, 4, 22);
          g.fillRect(7, 14, 8, 3); g.fillRect(18, 10, 8, 3);
          if (d.light) { g.fillStyle = '#fff3b0'; g.fillRect(13, 3, 5, 6); }
          break;
        case 'portal':
          g.fillStyle = '#1a0a30'; g.fillRect(0, 0, s, s);
          for (let i = 0; i < 7; i++) {
            g.fillStyle = 'rgba(196,107,255,' + (0.18 + i * 0.07) + ')';
            const r = 14 - i * 2;
            g.beginPath(); g.arc(16, 16, r, 0, Math.PI * 2); g.fill();
          }
          g.fillStyle = '#e9c6ff'; g.fillRect(14, 14, 4, 4);
          break;
        case 'crystal':
          grain(d.color, 'rgba(255,255,255,.12)', 10, 3);
          g.fillStyle = 'rgba(255,255,255,.55)';
          g.beginPath(); g.moveTo(16, 3); g.lineTo(26, 16); g.lineTo(16, 29); g.lineTo(6, 16); g.closePath(); g.fill();
          g.fillStyle = d.color2 || 'rgba(255,255,255,.35)';
          g.beginPath(); g.moveTo(16, 7); g.lineTo(22, 16); g.lineTo(16, 25); g.lineTo(10, 16); g.closePath(); g.fill();
          break;
        default:
          grain(d.color, 'rgba(255,255,255,.07)', 16, 3);
      }
      // 通用边缘
      if (d.solid && d.tex !== 'glass' && d.tex !== 'plant' && d.tex !== 'portal') {
        g.strokeStyle = 'rgba(0,0,0,.14)'; g.lineWidth = 1;
        g.strokeRect(0.5, 0.5, s - 1, s - 1);
      }
      this.tiles.set(id, c);
      return c;
    }

    /* ---------- 粒子 ---------- */
    emit(x, y, color, n, opt) {
      const o = opt || {};
      for (let i = 0; i < n; i++) {
        this.particles.push({
          x: x + (Math.random() - 0.5) * (o.spread || 0.6),
          y: y + (Math.random() - 0.5) * (o.spread || 0.6),
          vx: (Math.random() - 0.5) * (o.speed || 0.16),
          vy: (Math.random() - 0.7) * (o.speed || 0.16),
          life: o.life || 34, maxLife: o.life || 34,
          color, size: o.size || 3, grav: o.grav === undefined ? 0.012 : o.grav
        });
      }
      if (this.particles.length > 900) this.particles.splice(0, this.particles.length - 900);
    }
    blockBreak(x, y, id) {
      this.emit(x + 0.5, y + 0.5, DEF[id] ? DEF[id].color : '#999', 9, { speed: 0.2, size: 3 });
    }
    explosion(x, y, n) {
      this.emit(x, y, '#ffb347', (n || 3) * 14, { speed: 0.55, life: 45, spread: 2.4, size: 4 });
      this.emit(x, y, '#6b3bd6', (n || 3) * 8, { speed: 0.35, life: 55, spread: 2, size: 5 });
    }
    updateParticles() {
      for (let i = this.particles.length - 1; i >= 0; i--) {
        const p = this.particles[i];
        p.x += p.vx; p.y += p.vy; p.vy += p.grav; p.life--;
        if (p.life <= 0) this.particles.splice(i, 1);
      }
    }

    /* ---------- 天空 ---------- */
    sky(ctx, g, time, dim) {
      const W = this.vw, H = this.vh;
      let top, bot, horizon;
      // 昼夜：0=6:00 日出，6000=12:00 正午，12000=18:00 日落，18000=0:00 午夜
      const isDay = dim === 0 && time < 12000;
      const sunPhase = ((isDay ? time : (time - 12000)) / 12000) * Math.PI;
      const sunHeight = Math.sin(sunPhase);
      if (dim === 1) { top = '#2a0d10'; bot = '#7d2b16'; horizon = '#c85a22'; }
      else if (dim === 2) { top = '#0a0714'; bot = '#241238'; horizon = '#3a1d52'; }
      else if (!isDay) { top = '#0a1030'; bot = '#1b2450'; horizon = '#2c3968'; }
      else if (sunHeight > 0.42) { top = '#3b86d8'; bot = '#a9d9f5'; horizon = '#d8ecfa'; }
      else { top = '#2b3f78'; bot = '#e08a54'; horizon = '#ffc078'; }
      const grd = ctx.createLinearGradient(0, 0, 0, H);
      grd.addColorStop(0, top); grd.addColorStop(0.65, bot); grd.addColorStop(1, horizon);
      ctx.fillStyle = grd; ctx.fillRect(0, 0, W, H);
      // 星星（夜晚 / 末地）
      if (dim === 2 || (dim === 0 && !isDay)) {
        ctx.fillStyle = dim === 2 ? 'rgba(216,170,255,.7)' : 'rgba(255,255,255,.75)';
        const camx = this.cam.x * 2;
        for (let i = 0; i < 90; i++) {
          const sx = ((i * 977 + 131) % 1400) - (camx % 1400);
          const sy = (i * 613 + 77) % Math.max(1, Math.floor(H * 0.7));
          const tw = 0.5 + 0.5 * Math.sin(g.tick * 0.03 + i);
          ctx.globalAlpha = 0.25 + tw * 0.6;
          ctx.fillRect(sx, sy, 2, 2);
        }
        ctx.globalAlpha = 1;
      }
      // 太阳 / 月亮
      if (dim === 0) {
        const sx = W / 2 - Math.cos(sunPhase) * W * 0.45;
        const sy = H * 0.72 - sunHeight * H * 0.62;
        ctx.fillStyle = isDay ? 'rgba(255,239,168,.95)' : 'rgba(226,232,245,.9)';
        ctx.beginPath(); ctx.arc(sx, sy, isDay ? 22 : 17, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = isDay ? 'rgba(255,239,168,.18)' : 'rgba(226,232,245,.12)';
        ctx.beginPath(); ctx.arc(sx, sy, isDay ? 38 : 28, 0, Math.PI * 2); ctx.fill();
      } else if (dim === 2) {
        // 末地虚空里的远岛剪影
        ctx.fillStyle = 'rgba(70,40,100,.35)';
        for (let i = 0; i < 6; i++) {
          const x = ((i * 317 - this.cam.x * 1.5) % (W + 260) + W + 260) % (W + 260) - 130;
          const y = H * 0.25 + (i * 53) % 120;
          ctx.beginPath(); ctx.ellipse(x, y, 60 + i * 9, 12 + i * 2, 0, 0, Math.PI * 2); ctx.fill();
        }
      }
      // 云
      if (dim === 0) {
        ctx.fillStyle = 'rgba(255,255,255,.14)';
        for (let i = 0; i < 7; i++) {
          const x = ((i * 263 - this.cam.x * 3 + g.tick * 0.12) % (W + 320) + W + 320) % (W + 320) - 160;
          const y = 40 + (i * 47) % 130;
          ctx.beginPath();
          ctx.ellipse(x, y, 62, 12, 0, 0, Math.PI * 2);
          ctx.ellipse(x + 38, y - 7, 44, 10, 0, 0, Math.PI * 2);
          ctx.fill();
        }
      }
    }

    /* ---------- 主渲染 ---------- */
    render(g) {
      const ctx = this.ctx;
      const W = this.vw, H = this.vh;
      ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
      ctx.imageSmoothingEnabled = false;
      const world = g.world, p = g.player;
      // 相机（首帧直接到位，避免开局从世界顶部滑下来）
      const targetX = p.x * T - W / 2;
      const targetY = p.y * T - H / 2;
      const wantX = clamp(targetX, 0, C.W * T - W);
      const wantY = clamp(targetY, 0, C.H * T - H);
      if (!this.camInit) { this.cam.x = wantX; this.cam.y = wantY; this.camInit = true; }
      else {
        this.cam.x = lerp(this.cam.x, wantX, 0.16);
        this.cam.y = lerp(this.cam.y, wantY, 0.16);
      }
      const ox = Math.floor(this.cam.x / T), oy = Math.floor(this.cam.y / T);
      const fx = -Math.floor(this.cam.x % T), fy = -Math.floor(this.cam.y % T);
      const sx = (wx) => (wx - ox) * T + fx;
      const sy = (wy) => (wy - oy) * T + fy;

      this.sky(ctx, g, g.time, world.dim);
      // 方块
      const dayFactor = g.world.dim === 0
        ? (g.time < 12000 ? 0.45 + 0.55 * Math.sin(g.time / 12000 * Math.PI) : 0.30)
        : 1;
      for (let j = 0; j < this.rows; j++) {
        const wy = oy + j;
        if (wy < 0 || wy >= C.H) continue;
        for (let i = 0; i < this.cols; i++) {
          const wx = ox + i;
          if (wx < 0 || wx >= C.W) continue;
          const id = world.get(wx, wy);
          if (!id) continue;
          const tile = this.tile(id);
          const px = i * T + fx, py = j * T + fy;
          if (tile) ctx.drawImage(tile, px, py, T + 1, T + 1);
          else { ctx.fillStyle = '#000'; ctx.fillRect(px, py, T + 1, T + 1); }
          const lv = world.light(wx, wy);
          const bright = clamp(lv / 15 * dayFactor, 0.26, 1);
          if (bright < 0.99) {
            ctx.fillStyle = 'rgba(6,8,22,' + (1 - bright).toFixed(3) + ')';
            ctx.fillRect(px, py, T + 1, T + 1);
          }
        }
      }
      // 高亮 / 挖掘裂纹
      if (g.target && g.target.valid) {
        const px = sx(g.target.x), py = sy(g.target.y);
        ctx.strokeStyle = 'rgba(255,255,255,.85)'; ctx.lineWidth = 2;
        ctx.strokeRect(px + 1, py + 1, T - 2, T - 2);
        if (g.breakProgress > 0) {
          const n = Math.min(9, Math.floor(g.breakProgress * 10));
          ctx.strokeStyle = 'rgba(20,20,20,.75)'; ctx.lineWidth = 1.5;
          for (let k = 0; k < n; k++) {
            const a = k * 2.4;
            ctx.beginPath();
            ctx.moveTo(px + T / 2 + Math.cos(a) * 3, py + T / 2 + Math.sin(a) * 3);
            ctx.lineTo(px + T / 2 + Math.cos(a) * (5 + k), py + T / 2 + Math.sin(a) * (5 + k));
            ctx.stroke();
          }
        }
      }
      // 掉落物：用真实物品/方块贴图，带轻微旋转
      for (const d of g.drops) {
        const px = sx(d.x), py = sy(d.y);
        ctx.save(); ctx.translate(px, py);
        ctx.rotate(g.tick * 0.03);
        const tile = this.tile(d.id);
        if (tile) ctx.drawImage(tile, -6, -6, 12, 12);
        else {
          ctx.fillStyle = DEF[d.id] ? DEF[d.id].color : '#fff';
          ctx.fillRect(-5, -5, 10, 10);
        }
        ctx.restore();
      }
      // 生物
      for (const m of g.mobs) this.drawMob(ctx, m, sx, sy, g);
      // 投射物
      for (const pr of g.projectiles) {
        const px = sx(pr.x), py = sy(pr.y);
        ctx.fillStyle = pr.type === 'breath' ? '#c46bff' : '#e8e2c0';
        ctx.beginPath(); ctx.arc(px, py, pr.type === 'breath' ? 7 : 3, 0, Math.PI * 2); ctx.fill();
        if (pr.type === 'breath') {
          ctx.fillStyle = 'rgba(196,107,255,.28)';
          ctx.beginPath(); ctx.arc(px, py, 13, 0, Math.PI * 2); ctx.fill();
        }
      }
      // 龙息滞留云
      for (const c of (g.clouds || [])) {
        const px = sx(c.x), py = sy(c.y);
        const r = c.radius * T;
        const a = clamp(c.life / c.maxLife, 0, 1);
        const grd = ctx.createRadialGradient(px, py, 0, px, py, r);
        grd.addColorStop(0, 'rgba(190,90,240,' + (0.34 * a).toFixed(3) + ')');
        grd.addColorStop(1, 'rgba(150,60,220,0)');
        ctx.fillStyle = grd;
        ctx.beginPath(); ctx.ellipse(px, py, r, r * 0.72, 0, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = 'rgba(220,160,255,' + (0.55 * a).toFixed(3) + ')';
        for (let k = 0; k < 6; k++) {
          const ang = g.tick * 0.02 + k * 1.047;
          ctx.fillRect(px + Math.cos(ang) * r * 0.55,
                       py + Math.sin(ang) * r * 0.38, 3, 3);
        }
      }
      // 末影龙（只在末地维度出现；Boss 血条同样受维度门控）
      if (g.dragon && world.dim === 2) this.drawDragon(ctx, g.dragon, sx, sy, g);
      // 远程玩家（局域网）
      for (const rp of (g.remotePlayers || [])) {
        const px = sx(rp.x), py = sy(rp.y);
        ctx.fillStyle = '#4de0d0';
        ctx.fillRect(px - 7, py - 20, 14, 20);
        ctx.fillStyle = '#0b3b38';
        ctx.font = '11px sans-serif'; ctx.textAlign = 'center';
        ctx.fillText(rp.name || 'P', px, py - 25);
      }
      // 玩家
      this.drawPlayer(ctx, p, sx, sy, g);
      // 粒子
      this.updateParticles();
      for (const q of this.particles) {
        ctx.globalAlpha = clamp(q.life / q.maxLife, 0, 1);
        ctx.fillStyle = q.color;
        ctx.fillRect(sx(q.x), sy(q.y), q.size, q.size);
      }
      ctx.globalAlpha = 1;
      // 创造模式飞行提示 / 水下
      if (world.get(Math.floor(p.x), Math.floor(p.y)) === B.WATER) {
        ctx.fillStyle = 'rgba(40,110,200,.22)'; ctx.fillRect(0, 0, W, H);
      }
      // Boss 血条
      if (g.dragon && g.dragon.alive && world.dim === 2) {
        const bw = Math.min(520, W - 80), bx = (W - bw) / 2, by = 52;
        ctx.fillStyle = 'rgba(0,0,0,.6)'; ctx.fillRect(bx - 2, by - 2, bw + 4, 22);
        ctx.fillStyle = '#3a2150'; ctx.fillRect(bx, by, bw, 18);
        const ratio = clamp(g.dragon.hp / g.dragon.maxHp, 0, 1);
        const grd = ctx.createLinearGradient(bx, 0, bx + bw, 0);
        grd.addColorStop(0, '#8b3bd6'); grd.addColorStop(1, '#e08bff');
        ctx.fillStyle = grd; ctx.fillRect(bx, by, bw * ratio, 18);
        ctx.fillStyle = '#fff'; ctx.font = 'bold 12px sans-serif'; ctx.textAlign = 'center';
        ctx.fillText('末影龙  ' + Math.ceil(g.dragon.hp) + ' / ' + g.dragon.maxHp, W / 2, by + 13);
      }
    }

    drawPlayer(ctx, p, sx, sy, g) {
      const px = sx(p.x), py = sy(p.y);
      const bob = p.onGround ? Math.sin(g.tick * 0.18) * Math.min(1.5, Math.abs(p.vx) * 8) : 0;
      // 项目内 MC 皮肤拼成的正面 sprite（/skin 切换）
      const sp = DMC.spritepack;
      const s = sp && sp.getPlayer();
      if (s) {
        const drawH = p.h * T;
        const drawW = drawH * (20 / 32);
        ctx.save();
        ctx.translate(px, py + bob);
        ctx.scale(p.facing, 1);
        ctx.imageSmoothingEnabled = false;
        ctx.drawImage(s.cv, 0, 0, 20, 32, -drawW / 2, -drawH / 2, drawW, drawH);
        const held2 = g.inv.held();
        if (held2.id) {
          ctx.fillStyle = DEF[held2.id] ? DEF[held2.id].color : '#fff';
          ctx.fillRect(drawW / 2 - 3, -2, 9, 9);
        }
        ctx.restore();
        if (p.invuln > 0 && Math.floor(g.tick / 3) % 2 === 0) {
          ctx.fillStyle = 'rgba(255,60,60,.28)';
          ctx.fillRect(px - 12, py - p.h * T / 2 - 3, 24, p.h * T + 4);
        }
        return;
      }
      ctx.save(); ctx.translate(px, py + bob);
      ctx.scale(p.facing, 1);
      const hh = p.h * T / 2, hw = p.w * T / 2;
      // 腿
      ctx.fillStyle = '#2b3a63';
      ctx.fillRect(-hw + 1, 1, hw - 1, hh - 3);
      ctx.fillRect(0, 1, hw - 1, hh - 3);
      // 身体
      ctx.fillStyle = '#3f9ad6'; ctx.fillRect(-hw, -hh + 8, hw * 2, hh - 6);
      // 头
      ctx.fillStyle = '#d8a277'; ctx.fillRect(-hw - 1, -hh - 2, hw * 2 + 2, 12);
      ctx.fillStyle = '#3a2a20'; ctx.fillRect(-hw - 1, -hh - 2, hw * 2 + 2, 4);
      ctx.fillStyle = '#222'; ctx.fillRect(2, -hh + 3, 2, 2);
      // 手臂 + 手持物
      ctx.fillStyle = '#d8a277'; ctx.fillRect(hw - 1, -hh + 9, 5, 10);
      const held = g.inv.held();
      if (held.id) {
        const d = DEF[held.id];
        ctx.fillStyle = d ? d.color : '#fff';
        ctx.fillRect(hw + 2, -hh + 15, 9, 9);
      }
      ctx.restore();
      // 受击闪红
      if (p.invuln > 0 && Math.floor(g.tick / 3) % 2 === 0) {
        ctx.fillStyle = 'rgba(255,60,60,.28)';
        ctx.fillRect(px - 12, py - p.h * T / 2 - 3, 24, p.h * T + 4);
      }
    }

    drawMob(ctx, m, sx, sy, g) {
      const px = sx(m.x), py = sy(m.y);
      const w = m.w * T, h = m.h * T;
      ctx.save(); ctx.translate(px, py);
      // 生物 skin sprite（zombie / skeleton / creeper / sheep / piglin）
      const sp = DMC.spritepack;
      const s = sp && sp.getMob(m.type);
      if (s) {
        const scale = h / s.contentH;
        const drawW = 20 * scale, drawH = 32 * scale;
        ctx.scale(m.facing, 1);
        ctx.imageSmoothingEnabled = false;
        if (m.hurtFlash > 0 && Math.floor(g.tick / 2) % 2 === 0) ctx.globalAlpha = 0.55;
        ctx.drawImage(s.cv, 0, 0, 20, 32, -drawW / 2, -h / 2, drawW, drawH);
        if (m.type === 'creeper' && m.fuse > 20) {
          ctx.globalAlpha = 0.22 + 0.3 * Math.sin(g.tick * 0.5);
          ctx.fillStyle = '#fff';
          ctx.fillRect(-drawW / 2, -h / 2, drawW, h);
        }
        ctx.globalAlpha = 1;
        if (m.hp < m.maxHp) {
          ctx.fillStyle = 'rgba(0,0,0,.6)'; ctx.fillRect(-10, -h / 2 - 9, 20, 3);
          ctx.fillStyle = '#e05555';
          ctx.fillRect(-10, -h / 2 - 9, 20 * clamp(m.hp / m.maxHp, 0, 1), 3);
        }
        ctx.restore();
        return;
      }
      const col = m.hurtFlash > 0 && Math.floor(g.tick / 2) % 2 === 0 ? '#ffffff' : m.def.color;
      if (m.type === 'spider' || m.type === 'cube') {
        ctx.fillStyle = col; ctx.fillRect(-w / 2, -h / 2, w, h);
        if (m.type === 'spider') {
          ctx.strokeStyle = '#1a1016'; ctx.lineWidth = 2;
          for (let i = -1; i <= 1; i += 2) for (let k = -1; k <= 1; k++) {
            ctx.beginPath(); ctx.moveTo(i * w / 2, k * 5); ctx.lineTo(i * (w / 2 + 7), k * 9 - 4); ctx.stroke();
          }
          ctx.fillStyle = '#e04040'; ctx.fillRect(-5, -2, 2, 2); ctx.fillRect(3, -2, 2, 2);
        } else {
          ctx.fillStyle = 'rgba(255,255,255,.35)'; ctx.fillRect(-w / 2 + 3, -h / 2 + 3, w - 6, 4);
          ctx.fillStyle = '#5a4a10'; ctx.fillRect(-4, -2, 3, 3); ctx.fillRect(3, -2, 3, 3);
          if (m.held) { ctx.fillStyle = '#888'; ctx.fillRect(-4, -h / 2 - 7, 9, 9); }
        }
      } else if (m.type === 'creeper') {
        ctx.fillStyle = col; ctx.fillRect(-w / 2, -h / 2 + 6, w, h - 6);
        ctx.fillRect(-w / 2 + 2, -h / 2 - 2, w - 4, 10);
        ctx.fillStyle = '#16210f';
        ctx.fillRect(-6, -h / 2 + 1, 3, 4); ctx.fillRect(3, -h / 2 + 1, 3, 4);
        ctx.fillRect(-3, -h / 2 + 5, 6, 4);
        if (m.fuse > 20) {
          ctx.fillStyle = 'rgba(255,255,255,' + (0.2 + 0.4 * Math.sin(g.tick * 0.5)) + ')';
          ctx.fillRect(-w / 2 - 3, -h / 2 - 5, w + 6, h + 8);
        }
      } else if (m.type === 'enderman') {
        ctx.fillStyle = col;
        ctx.fillRect(-4, -h / 2, 8, h);
        ctx.fillRect(-w / 2, -h / 2 - 3, w, 10);
        ctx.fillStyle = '#c46bff'; ctx.fillRect(-5, -h / 2 - 1, 3, 2); ctx.fillRect(2, -h / 2 - 1, 3, 2);
      } else if (m.type === 'sheep') {
        ctx.fillStyle = col; ctx.fillRect(-w / 2, -h / 2 + 3, w, h - 6);
        ctx.fillStyle = '#333'; ctx.fillRect(w / 2 - 5, -h / 2 + 5, 7, 7);
        ctx.fillStyle = '#ddd'; ctx.fillRect(-w / 2 + 2, 2, 4, h / 2 - 3); ctx.fillRect(w / 2 - 6, 2, 4, h / 2 - 3);
      } else {
        ctx.fillStyle = col;
        ctx.fillRect(-w / 2, -h / 2 + 7, w, h - 10);
        ctx.fillRect(-w / 2 - 2, -h / 2 + 8, 4, 12);
        ctx.fillRect(w / 2 - 2, -h / 2 + 8, 4, 12);
        ctx.fillStyle = m.type === 'skeleton' ? '#e8e6da' : '#8fbf68';
        ctx.fillRect(-w / 2 + 1, -h / 2 - 3, w - 2, 11);
        ctx.fillStyle = '#111';
        ctx.fillRect(-4, -h / 2, 2, 2); ctx.fillRect(2, -h / 2, 2, 2);
      }
      // 血条
      if (m.hp < m.maxHp) {
        ctx.fillStyle = 'rgba(0,0,0,.6)'; ctx.fillRect(-10, -h / 2 - 9, 20, 3);
        ctx.fillStyle = '#e05555'; ctx.fillRect(-10, -h / 2 - 9, 20 * clamp(m.hp / m.maxHp, 0, 1), 3);
      }
      ctx.restore();
    }

    drawDragon(ctx, d, sx, sy, g) {
      if (!d.alive && d.deathTimer <= 0) return;
      const segs = d.segs.map(s => ({ x: sx(s.x), y: sy(s.y) }));
      const wing = Math.sin(d.wing) * 0.5 + 0.5;
      const alpha = d.alive ? 1 : clamp(d.deathTimer / 90, 0, 1);
      ctx.save(); ctx.globalAlpha = alpha;
      // 翅膀（画在身体后面）
      const h = segs[2] || segs[0];
      for (const side of [-1, 1]) {
        ctx.fillStyle = 'rgba(48,20,72,.86)';
        ctx.beginPath();
        ctx.moveTo(h.x, h.y);
        ctx.quadraticCurveTo(h.x + side * (40 + wing * 26), h.y - 50 - wing * 24,
                             h.x + side * (78 + wing * 34), h.y - 18 - wing * 20);
        ctx.lineTo(h.x + side * (52 + wing * 20), h.y + 12);
        ctx.quadraticCurveTo(h.x + side * 34, h.y + 4, h.x, h.y + 8);
        ctx.closePath(); ctx.fill();
        ctx.strokeStyle = 'rgba(160,90,220,.55)'; ctx.lineWidth = 1.5; ctx.stroke();
      }
      // 身体：从尾到头画渐变椭圆（链式结构可视化）
      for (let i = segs.length - 1; i >= 1; i--) {
        const s = segs[i], prev = segs[i - 1];
        const r = 5 + (1 - i / segs.length) * 9;
        ctx.fillStyle = i % 2 ? '#2a153d' : '#3a1c52';
        ctx.beginPath(); ctx.ellipse(s.x, s.y, r, r * 0.78,
          Math.atan2(prev.y - s.y, prev.x - s.x), 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = 'rgba(170,100,230,.42)';
        ctx.beginPath(); ctx.ellipse(s.x, s.y - r * 0.25, r * 0.55, r * 0.25, 0, 0, Math.PI * 2); ctx.fill();
      }
      // 头
      const head = segs[0], neck = segs[1] || segs[0];
      const ang = Math.atan2(head.y - neck.y, head.x - neck.x);
      ctx.save(); ctx.translate(head.x, head.y); ctx.rotate(ang);
      ctx.fillStyle = d.hurtFlash > 0 && Math.floor(g.tick / 2) % 2 === 0 ? '#ffffff' : '#241134';
      ctx.fillRect(-12, -11, 27, 22);
      ctx.fillStyle = '#3a1c52'; ctx.fillRect(8, -6, 16, 12);   // 口鼻
      ctx.fillStyle = '#c46bff'; ctx.fillRect(-5, -7, 4, 3);    // 眼
      ctx.fillStyle = '#8b5bd6';                               // 角
      ctx.beginPath(); ctx.moveTo(-8, -9); ctx.lineTo(-16, -20); ctx.lineTo(-3, -11); ctx.fill();
      ctx.beginPath(); ctx.moveTo(-8, 9); ctx.lineTo(-16, 20); ctx.lineTo(-3, 11); ctx.fill();
      ctx.restore();
      // 栖息时脚下的阴影
      if (d.state === 'PERCH' && d.landed) {
        ctx.fillStyle = 'rgba(0,0,0,.28)';
        ctx.beginPath(); ctx.ellipse(head.x, head.y + 22, 32, 6, 0, 0, Math.PI * 2); ctx.fill();
      }
      ctx.restore();
    }
  }

  DMC.Renderer = Renderer;
})(typeof globalThis !== 'undefined' ? globalThis : this);
