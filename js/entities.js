/* 2DMC Web — 玩家物理、生物、投射物、末影龙链式 Boss（纯逻辑） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, Core, DEF } = DMC;
  const { clamp, dist, makeRNG } = Core;

  /* ============ 共享 AABB 物理（玩家与生物同一套，避免穿墙/卡方块） ============ */
  function boxOf(e) {
    return { x0: e.x - e.w / 2, x1: e.x + e.w / 2,
             y0: e.y - e.h / 2, y1: e.y + e.h / 2 };
  }
  function hitsSolid(world, b) {
    const x0 = Math.floor(b.x0 + 0.001), x1 = Math.floor(b.x1 - 0.001);
    const y0 = Math.floor(b.y0 + 0.001), y1 = Math.floor(b.y1 - 0.001);
    for (let y = y0; y <= y1; y++)
      for (let x = x0; x <= x1; x++)
        if (world.solid(x, y)) return true;
    return false;
  }
  // 分轴、小步长推进；返回碰撞信息。noClip 用于创造模式飞行。
  function moveAABB(ent, world, dx, dy, noClip) {
    const r = { hitX: false, hitY: false, ground: false };
    if (noClip) { ent.x += dx; ent.y += dy; return r; }
    const STEP = 0.18;
    const axisMove = (axis, delta) => {
      const n = Math.max(1, Math.ceil(Math.abs(delta) / STEP));
      const s = delta / n;
      for (let i = 0; i < n; i++) {
        const b = boxOf(ent);
        if (axis === 0) { b.x0 += s; b.x1 += s; }
        else { b.y0 += s; b.y1 += s; }
        if (hitsSolid(world, b)) {
          if (axis === 0) { r.hitX = true; ent.vx = 0; }
          else { r.hitY = true; if (s > 0) r.ground = true; ent.vy = 0; }
          return;
        }
        if (axis === 0) ent.x += s; else ent.y += s;
      }
    };
    axisMove(0, dx);
    axisMove(1, dy);
    return r;
  }
  // 已经嵌进方块时把实体推回空气里；实在救不回来就返回 false（由调用方移除）
  function unstick(ent, world) {
    if (!hitsSolid(world, boxOf(ent))) return true;
    const ox = ent.x, oy = ent.y;
    for (const dir of [-1, 1]) {
      for (let k = 1; k <= 16; k++) {
        ent.y = oy + dir * k * 0.25;
        if (!hitsSolid(world, boxOf(ent))) { ent.vy = 0; return true; }
      }
    }
    ent.y = oy;
    for (const dx of [-1.5, 1.5, -3, 3]) {
      ent.x = ox + dx;
      if (!hitsSolid(world, boxOf(ent))) { ent.vy = 0; return true; }
    }
    ent.x = ox; ent.y = oy;
    return false;
  }
  // 找一个「头顶够高 + 脚下有支撑」的生成点，返回实体中心 y
  function findSpawn(world, x, fromY, w, h) {
    const x0 = Math.floor(x - w / 2), x1 = Math.floor(x + w / 2);
    const start = clamp(Math.floor(fromY) - 6, 3, C.H - 8);
    for (let sy = start; sy < C.H - 3; sy++) {
      let support = true, clear = true;
      for (let cx = x0; cx <= x1; cx++) {
        if (!world.solid(cx, sy + 1)) support = false;
        for (let cy = sy - Math.ceil(h) + 1; cy <= sy; cy++)
          if (world.solid(cx, cy)) clear = false;
      }
      if (support && clear) return sy + 1 - h / 2 - 0.001;
    }
    return null;
  }
  const Physics = { boxOf, hitsSolid, moveAABB, unstick, findSpawn };

  /* ============ 玩家 ============ */
  class Player {
    constructor(x, y) {
      this.x = x; this.y = y;
      this.vx = 0; this.vy = 0;
      this.w = 0.62; this.h = 1.78;
      this.onGround = false;
      this.facing = 1;
      this.life = 20; this.maxLife = 20;
      // 饥饿：20 点饱食度 + 隐藏饱和度 + 消耗度（原版三段式）
      this.hunger = 20; this.maxHunger = 20;
      this.saturation = 5; this.exhaustion = 0;
      this.regenTick = 0; this.starveTick = 0;
      this.air = 0;
      this.invuln = 0;
      this.creative = false;
      this.fly = false;
      this.dead = false;
      this.attackCd = 0;
      this.effects = new DMC.Effects();   // 状态效果（药水 / 环境）
    }
    get cx() { return this.x; }
    get cy() { return this.y; }
    box() {
      return { x0: this.x - this.w / 2, x1: this.x + this.w / 2,
               y0: this.y - this.h / 2, y1: this.y + this.h / 2 };
    }
    static hitsSolid(world, b) { return hitsSolid(world, b); }
    move(world, dx, dy) {
      const r = moveAABB(this, world, dx, dy, !!(this.fly && this.creative));
      if (r.ground) this.onGround = true;
      return r;
    }
    update(world, input) {
      if (this.invuln > 0) this.invuln--;
      if (this.attackCd > 0) this.attackCd--;
      if (this.dead) return;
      const ox = this.x, oy = this.y;
      const inWater = world.get(Math.floor(this.x), Math.floor(this.y)) === B.WATER;
      if (this.creative && this.fly) {
        const sp = C.FLY_SPEED * this.effects.speedMul *
                   (this.prog ? this.prog.speedMul : 1);
        this.vx = (input.right - input.left) * sp;
        this.vy = (input.down - input.jump) * sp;
        this.facing = this.vx < 0 ? -1 : this.vx > 0 ? 1 : this.facing;
        this.move(world, this.vx, this.vy);
        this.onGround = false;
      } else {
        const accel = this.onGround ? 0.020 : 0.010;
        const target = (input.right - input.left) *
                       (inWater ? C.WATER_SPEED : C.WALK_SPEED) *
                       this.effects.speedMul * (this.prog ? this.prog.speedMul : 1);
        this.vx += clamp(target - this.vx, -accel, accel);
        this.vx *= this.onGround ? 0.80 : 0.94;
        if (input.left) this.facing = -1;
        if (input.right) this.facing = 1;
        this.vy = Math.min(C.MAX_FALL,
                           this.vy + (inWater ? C.GRAVITY * 0.35 : C.GRAVITY));
        if (inWater) this.vy *= 0.86;
        if (input.jump && this.onGround) {
          this.vy = C.JUMP_V * this.effects.jumpMul; this.onGround = false;
          this.exhaust(0.15);
        }
        if (input.jump && inWater) this.vy = C.WATER_JUMP;
        this.onGround = false;
        this.move(world, this.vx, this.vy);
      }
      // 虚空 / 世界边界
      if (world.dim === 2 && this.y > C.H - 3) {
        this.hurt(2, this.x);
        if (this.y > C.H + 2) this.dead = true;
      }
      if (this.y < 1) { this.y = 1; this.vy = Math.max(0, this.vy); }
      if (this.x < 1) { this.x = 1; this.vx = 0; }
      if (this.x > C.W - 2) { this.x = C.W - 2; this.vx = 0; }
      // 岩浆伤害：抗火效果期间完全免疫
      if (world.get(Math.floor(this.x), Math.floor(this.y)) === B.LAVA &&
          !this.effects.fireResist && this.invuln <= 0 && !this.creative)
        this.hurt(2, this.x);
      this.effects.tick();
      this.effects.potionTick(this);
      this.metabolism(Math.abs(this.x - ox) + Math.abs(this.y - oy));
    }

    /* ---- 饥饿 / 代谢 ---- */
    exhaust(n) {
      if (this.creative) return;
      this.exhaustion += n;
    }
    eat(food) {
      if (!food || (this.hunger >= this.maxHunger && this.saturation >= 5))
        return false;
      this.hunger = clamp(this.hunger + food, 0, this.maxHunger);
      this.saturation = clamp(this.saturation + food * 0.6, 0, this.maxHunger);
      return true;
    }
    metabolism(moved) {
      if (this.creative) return;
      // 基础代谢 + 移动消耗；消耗度每满 4 先扣饱和度，饱和度见底才扣饥饿
      this.exhaustion += 0.004 + moved * 0.02;
      while (this.exhaustion >= 4) {
        this.exhaustion -= 4;
        if (this.saturation > 0) this.saturation = Math.max(0, this.saturation - 1);
        else this.hunger = Math.max(0, this.hunger - 1);
      }
      // 饱食时缓慢自然回血（同时消耗额外饱食度）
      if (this.hunger >= 18 && this.life < this.maxLife) {
        if (++this.regenTick >= 80) {
          this.regenTick = 0;
          this.life = Math.min(this.maxLife, this.life + 1);
          this.exhaustion += 6;
        }
      } else this.regenTick = 0;
      // 饿空后持续掉血，但不会直接饿死（最低 1 点）
      if (this.hunger <= 0 && this.life > 1) {
        if (++this.starveTick >= 100) { this.starveTick = 0; this.life -= 1; }
      } else this.starveTick = 0;
    }
    hurt(dmg, fromX) {
      if (this.invuln > 0 || this.creative) return false;
      this.life -= dmg;
      this.exhaust(0.6);
      this.invuln = 40;
      this.vx = (this.x < fromX ? -1 : 1) * C.KNOCK_X;
      this.vy = C.KNOCK_Y;
      if (this.life <= 0) { this.life = 0; this.dead = true; }
      return true;
    }
    heal(n) { this.life = Math.min(this.maxLife, this.life + n); }
    attackDamage(inv) {
      const h = inv.held();
      const d = h.id ? DEF[h.id] : null;
      return (d && d.dmg ? d.dmg : 1) * this.effects.attackMul *
             (this.prog ? this.prog.attackMul : 1);
    }
    toolTier(inv) {
      const h = inv.held();
      const d = h.id ? DEF[h.id] : null;
      return d && d.tier ? d.tier : 0;
    }
    respawn(world) {
      const p = world.spawnPoint();
      this.x = p.x + 0.5; this.y = p.y - 0.4;
      this.vx = 0; this.vy = 0;
      this.life = this.maxLife; this.dead = false; this.invuln = 60;
      this.hunger = this.maxHunger; this.saturation = 5; this.exhaustion = 0;
      this.regenTick = 0; this.starveTick = 0;
    }
  }

  /* ============ 生物 ============ */
  const MOB_DEFS = {
    zombie:    { hp: 20, dmg: 3, speed: 0.075, color: '#4f7a48', name: '僵尸', w: 0.7, h: 1.7 },
    skeleton:  { hp: 20, dmg: 3, speed: 0.065, color: '#d8d5c0', name: '骷髅', w: 0.7, h: 1.7 },
    creeper:   { hp: 20, dmg: 9, speed: 0.085, color: '#5aa14a', name: '苦力怕', w: 0.7, h: 1.6 },
    spider:    { hp: 16, dmg: 2, speed: 0.12,  color: '#3b2b33', name: '蜘蛛', w: 0.9, h: 0.9 },
    piglin:    { hp: 24, dmg: 4, speed: 0.08,  color: '#a8743b', name: '僵尸猪灵', w: 0.7, h: 1.7 },
    cube:      { hp: 8,  dmg: 2, speed: 0.10,  color: '#e0cc55', name: '硫磺方块怪', w: 0.9, h: 0.9 },
    sheep:     { hp: 8,  dmg: 0, speed: 0.06,  color: '#eeeeee', name: '羊', w: 0.8, h: 1.1 },
    enderman:  { hp: 40, dmg: 6, speed: 0.10,  color: '#1d1424', name: '末影人', w: 0.7, h: 2.4 }
  };
  class Mob {
    constructor(type, x, y, dim) {
      const d = MOB_DEFS[type] || MOB_DEFS.zombie;
      // 怪物等级：越深 / 越危险的维度越高；8% 概率出精英
      const baseLv = DMC.mobLevel ? DMC.mobLevel(y, dim | 0) : 1;
      const elite = Math.random() < 0.08;
      const lv = Math.min(12, baseLv + (elite ? 2 : 0));
      const sc = DMC.mobScale ? DMC.mobScale(lv) : { hp: 1, dmg: 1 };
      this.lv = lv; this.elite = elite;
      this.hpScale = sc.hp * (elite ? 2 : 1);
      this.dmgScale = sc.dmg * (elite ? 1.3 : 1);
      this.type = type; this.def = d;
      this.x = x; this.y = y; this.vx = 0; this.vy = 0;
      this.hp = Math.max(1, Math.round(d.hp * this.hpScale));
      this.maxHp = this.hp;
      this.cd = 0; this.fuse = 0; this.wander = Math.random() * 200;
      this.facing = 1; this.held = 0; this.arche = Math.floor(Math.random() * 6);
      this.hurtFlash = 0; this.dead = false; this.onGround = false;
    }
    get w() { return this.def.w; }
    get h() { return this.def.h; }
    update(world, player, api) {
      if (this.dead) return;
      if (this.cd > 0) this.cd--;
      if (this.hurtFlash > 0) this.hurtFlash--;
      const distP = dist(this.x, this.y, player.x, player.y);
      const hostile = this.type !== 'sheep';
      let dir = 0;
      if (hostile && distP < 16) dir = player.x > this.x ? 1 : -1;
      else {
        this.wander += 1;
        if (Math.floor(this.wander) % 140 === 0) this.facing = Math.random() < 0.5 ? -1 : 1;
        dir = this.facing;
        if (this.type === 'cube' && Math.random() < 0.02) dir = -dir;
      }
      // 已经嵌进方块先推回空气里；救不回来就移除，避免永久卡住
      if (!unstick(this, world)) { this.dead = true; return; }
      this.vx = dir * this.def.speed;
      this.facing = dir < 0 ? -1 : 1;
      // 与玩家共用同一套 AABB 分轴碰撞，速度单位统一为格/帧
      this.vy = Math.min(C.MAX_FALL, this.vy + C.MOB_GRAVITY);
      const r = moveAABB(this, world, this.vx, this.vy, false);
      this.onGround = r.ground;
      if (r.hitX && this.onGround && dir !== 0) {
        // 前方正好是 1 格台阶 → 起跳越过去
        const ahead = Math.floor(this.x + dir * (this.w / 2 + 0.35));
        const foot = Math.floor(this.y + this.h / 2 + 0.05);
        if (world.solid(ahead, foot) && !world.solid(ahead, foot - 1) &&
            !world.solid(ahead, foot - 2)) this.vy = C.MOB_JUMP;
        else this.facing *= -1;
      }
      this.x = clamp(this.x, 2, C.W - 2);
      this.y = clamp(this.y, 2, C.H - 2);
      // 苦力怕引信
      if (this.type === 'creeper' && distP < 2.2) {
        this.fuse++;
        if (this.fuse > 60) { api.explode(this.x, this.y, 3); this.dead = true; }
      } else if (this.type !== 'creeper') this.fuse = 0;
      // 接触伤害
      if (hostile && distP < 1.1 && this.cd <= 0) {
        player.hurt(this.def.dmg * this.dmgScale, this.x);
        this.cd = 55;
      }
      // 硫磺方块怪：吞方块（26.2）
      if (this.type === 'cube' && this.held === 0 && Math.random() < 0.004) this.held = B.STONE;
    }
    hurt(n) { this.hp -= n; this.hurtFlash = 12; if (this.hp <= 0) this.dead = true; return this.dead; }
  }

  /* ============ 投射物（龙息 / 箭） ============ */
  class Projectile {
    constructor(type, x, y, vx, vy, dmg) {
      this.type = type; this.x = x; this.y = y;
      this.vx = vx; this.vy = vy; this.dmg = dmg || 3;
      this.life = 200; this.dead = false;
    }
    update(world, player) {
      this.life--;
      this.x += this.vx; this.y += this.vy;
      if (this.type === 'arrow') this.vy += 0.002;
      if (this.life <= 0 || !Core.inWorld(this.x, this.y)) { this.dead = true; return; }
      if (world.solid(Math.floor(this.x), Math.floor(this.y))) {
        this.dead = true; return;   // 龙息撞到方块就落地形成滞留云
      }
      if (dist(this.x, this.y, player.x, player.y) < 0.85) {
        player.hurt(this.dmg, this.x);
        this.dead = true;
      }
    }
  }

  /* ============ 龙息滞留云（原版 area effect cloud） ============ */
  class BreathCloud {
    constructor(x, y) {
      this.x = x; this.y = y;
      this.life = 150; this.maxLife = 150;
      this.radius = 1.2;
      this.tick = 0; this.hitCd = 0; this.dead = false;
    }
    step(player) {
      this.tick++;
      this.life--;
      if (this.hitCd > 0) this.hitCd--;
      if (this.life <= 0) { this.dead = true; return false; }
      const t = 1 - this.life / this.maxLife;
      this.radius = (1.2 + Math.min(1, t * 4) * 1.1) *
                    (this.life < 40 ? Math.max(0.2, this.life / 40) : 1);
      if (this.hitCd === 0 &&
          dist(this.x, this.y, player.x, player.y) < this.radius) {
        this.hitCd = 20;
        player.hurt(2, this.x);
        return true;
      }
      return false;
    }
  }

  /* ============ 末影龙：链式 Boss（骨架仿世界吞噬者） ============ */
  // 链式跟随与世界吞噬者同一套写法：p[i] += (p[i-1] - p[i]) / factor
  function followChain(segs, factor) {
    if (!segs || segs.length < 2) return segs;
    const f = factor || 6;
    for (let i = 1; i < segs.length; i++) {
      segs[i].x += (segs[i - 1].x - segs[i].x) / f;
      segs[i].y += (segs[i - 1].y - segs[i].y) / f;
    }
    return segs;
  }
  // 保持段间最小间距，避免身体挤成一团
  function constrainChain(segs, minGap) {
    const g = minGap || 1.35;
    for (let i = 1; i < segs.length; i++) {
      const dx = segs[i].x - segs[i - 1].x, dy = segs[i].y - segs[i - 1].y;
      const d = Math.hypot(dx, dy) || 0.0001;
      if (d < g) {
        const k = (g - d) / d;
        segs[i].x += dx * k; segs[i].y += dy * k;
      }
    }
    return segs;
  }
  // 头部朝目标飞行（加速度 + 阻尼 + 限速），纯函数式作用于 head
  function flyTo(head, tx, ty, acc, maxSp) {
    const dx = tx - head.x, dy = ty - head.y;
    const d = Math.hypot(dx, dy) + 0.001;
    head.vx = (head.vx || 0) + dx / d * acc;
    head.vy = (head.vy || 0) + dy / d * acc;
    const sp = Math.hypot(head.vx, head.vy);
    const m = maxSp || 1.8;
    if (sp > m) { head.vx = head.vx / sp * m; head.vy = head.vy / sp * m; }
    head.vx *= 0.88; head.vy *= 0.88;
    head.x += head.vx; head.y += head.vy;
    head.x = clamp(head.x, 6, C.W - 6);
    head.y = clamp(head.y, 8, C.H - 8);
    return head;
  }

  const DRAGON_HP = 160;
  const SEGMENT_COUNT = 14;   // 头 1 + 体节 12 + 尾 1

  class EnderDragon {
    constructor(world) {
      const s = world.endSpawn;
      const hx = s.x, hy = s.y - 16;
      this.segs = [];
      for (let i = 0; i < SEGMENT_COUNT; i++)
        this.segs.push({ x: hx - i * 1.5, y: hy, vx: 0, vy: 0 });
      this.hp = DRAGON_HP; this.maxHp = DRAGON_HP;
      this.state = 'CRUISE';
      this.t = 0; this.phase = 0; this.cd = 0; this.wing = 0;
      this.target = { x: hx + 40, y: hy - 20 };
      this.alive = true;
      this.landed = false;
      this.deathTimer = 0;
      this.crystalHealTick = 0;
      this.hurtFlash = 0;
      this.pickCruise(world);
    }
    get head() { return this.segs[0]; }
    pickCruise(world) {
      const cx = C.W >> 1;
      const a = (this.phase++ * 2.399963) % (Math.PI * 2);
      const r = 48 + (this.phase % 5) * 12;
      this.target.x = clamp(cx + Math.cos(a) * r, 10, C.W - 10);
      this.target.y = clamp(world.endSpawn.y - 10 - (this.phase % 3) * 4, 12, C.H - 12);
    }
    hurt(n) {
      if (!this.alive) return false;
      this.hp -= n;
      this.hurtFlash = 14;
      if (this.hp <= 0) { this.hp = 0; return true; }
      // 原版：被打中立刻降落栖息，给玩家输出窗口
      if (this.state !== 'PERCH') {
        this.state = 'PERCH'; this.landed = false; this.crystalHealTick = 0; this.t = 0;
      }
      return false;
    }
    die(api, world) {
      this.alive = false;
      this.deathTimer = 90;
      world.dragonDead = true;
      api.onDragonDeath(this.head.x, this.head.y);
    }
    step(world, player, api) {
      if (!this.alive) { if (this.deathTimer > 0) this.deathTimer--; return; }
      if (this.hp <= 0) { this.die(api, world); return; }
      if (this.hurtFlash > 0) this.hurtFlash--;
      if (this.cd > 0) this.cd--;
      this.t++;
      this.wing += 0.22;
      const head = this.head;
      const px = player.x, py = player.y;

      if (this.state === 'CRUISE') {
        flyTo(head, this.target.x, this.target.y, 0.30, 1.8);
        if (this.t % 150 === 0) api.spawnBreath(head.x, head.y, px, py);
        if (this.t > 260 && Math.abs(px - head.x) < 110) {
          const dx = px - head.x, dy = py - head.y;
          const d = Math.hypot(dx, dy) + 0.001;
          // 目标点在玩家身后：龙会穿过去，不贴脸无限压
          this.target.x = clamp(px + dx / d * 30, 8, C.W - 8);
          this.target.y = clamp(py + dy / d * 30, 8, C.H - 8);
          this.state = 'DIVE'; this.t = 0;
        } else if (this.t > 400) { this.t = 0; this.pickCruise(world); }
        if (dist(head.x, head.y, this.target.x, this.target.y) < 6) this.pickCruise(world);
      } else if (this.state === 'DIVE') {
        flyTo(head, this.target.x, this.target.y, 0.34, 2.1);
        if (this.t % 58 === 0) api.spawnBreath(head.x, head.y, px, py);
        if (dist(head.x, head.y, px, py) < 2.6 && this.cd <= 0) {
          player.hurt(6, head.x); this.cd = 90;
        }
        if (this.t > 150 || dist(head.x, head.y, px, py) > 80) {
          this.state = 'PERCH'; this.landed = false;
          this.target.x = px;
          this.target.y = world.groundY(Math.floor(px), py) - 2;
          this.t = 0; this.crystalHealTick = 0;
        }
      } else if (this.state === 'PERCH') {
        if (!this.landed) {
          flyTo(head, this.target.x, this.target.y, 0.45, 1.7);
          if (dist(head.x, head.y, this.target.x, this.target.y) < 3 || this.t > 500) {
            this.landed = true; this.crystalHealTick = 0;
            head.vx = 0; head.vy = 0;
          }
        } else {
          head.vx *= 0.6; head.vy *= 0.6;
          this.crystalHealTick++;
          // 末影水晶存活时栖息回血（原版机制；打掉水晶即可停止）
          if (this.crystalHealTick % 30 === 0 && this.hp < this.maxHp &&
              world.crystalsAlive() > 0) this.hp = Math.min(this.maxHp, this.hp + 1);
          if (this.crystalHealTick > 400 || Math.abs(px - head.x) > 30) {
            this.state = 'CRUISE'; this.t = 0; this.landed = false;
            this.pickCruise(world);
          }
        }
      }
      // 链式身体：与世界吞噬者一致
      followChain(this.segs, 6);
      constrainChain(this.segs, 1.45);
    }
    serialize() {
      return { hp: this.hp, state: this.state, t: this.t, phase: this.phase,
               landed: this.landed, alive: this.alive,
               head: { x: this.head.x, y: this.head.y },
               crystalHealTick: this.crystalHealTick };
    }
    static restore(o, world) {
      const d = new EnderDragon(world);
      if (!o) return d;
      d.hp = o.hp; d.state = o.state || 'CRUISE'; d.t = o.t | 0;
      d.phase = o.phase | 0; d.landed = !!o.landed; d.alive = !!o.alive;
      d.crystalHealTick = o.crystalHealTick | 0;
      if (o.head) { d.head.x = o.head.x; d.head.y = o.head.y; }
      for (let i = 1; i < d.segs.length; i++) {
        d.segs[i].x = d.head.x - i * 1.5; d.segs[i].y = d.head.y;
      }
      return d;
    }
  }

  DMC.Entities = {
    Player, Mob, MOB_DEFS, Projectile, BreathCloud, EnderDragon, Physics,
    followChain, constrainChain, flyTo, DRAGON_HP, SEGMENT_COUNT
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
