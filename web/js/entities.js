/* 2DMC Web — 玩家物理、生物、投射物、末影龙链式 Boss（纯逻辑） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, Core, DEF } = DMC;
  const { clamp, dist, makeRNG } = Core;

  /* ============ 玩家 ============ */
  class Player {
    constructor(x, y) {
      this.x = x; this.y = y;
      this.vx = 0; this.vy = 0;
      this.w = 0.62; this.h = 1.78;
      this.onGround = false;
      this.facing = 1;
      this.life = 20; this.maxLife = 20;
      this.air = 0;
      this.invuln = 0;
      this.creative = false;
      this.fly = false;
      this.dead = false;
      this.attackCd = 0;
    }
    get cx() { return this.x; }
    get cy() { return this.y; }
    box() {
      return { x0: this.x - this.w / 2, x1: this.x + this.w / 2,
               y0: this.y - this.h / 2, y1: this.y + this.h / 2 };
    }
    static hitsSolid(world, b) {
      const x0 = Math.floor(b.x0 + 0.001), x1 = Math.floor(b.x1 - 0.001);
      const y0 = Math.floor(b.y0 + 0.001), y1 = Math.floor(b.y1 - 0.001);
      for (let y = y0; y <= y1; y++)
        for (let x = x0; x <= x1; x++)
          if (world.solid(x, y)) return true;
      return false;
    }
    move(world, dx, dy) {
      if (this.fly && this.creative) { this.x += dx; this.y += dy; return; }
      // 分轴推进，逐格消解
      const stepX = (d) => {
        const n = Math.ceil(Math.abs(d) / 0.25);
        const s = d / Math.max(1, n);
        for (let i = 0; i < n; i++) {
          const nb = this.box(); nb.x0 += s; nb.x1 += s;
          if (!Player.hitsSolid(world, nb)) this.x += s;
          else { this.vx = 0; return; }
        }
      };
      const stepY = (d) => {
        const n = Math.ceil(Math.abs(d) / 0.25);
        const s = d / Math.max(1, n);
        for (let i = 0; i < n; i++) {
          const nb = this.box(); nb.y0 += s; nb.y1 += s;
          if (!Player.hitsSolid(world, nb)) this.y += s;
          else {
            if (s > 0) this.onGround = true;
            this.vy = 0; return;
          }
        }
      };
      stepX(dx);
      stepY(dy);
    }
    update(world, input) {
      if (this.invuln > 0) this.invuln--;
      if (this.attackCd > 0) this.attackCd--;
      if (this.dead) return;
      const inWater = world.get(Math.floor(this.x), Math.floor(this.y)) === B.WATER;
      if (this.creative && this.fly) {
        const sp = 0.24;
        this.vx = (input.right - input.left) * sp;
        this.vy = (input.down - input.jump) * sp;
        this.facing = this.vx < 0 ? -1 : this.vx > 0 ? 1 : this.facing;
        this.move(world, this.vx, this.vy);
        this.onGround = false;
      } else {
        const accel = this.onGround ? 0.09 : 0.045;
        const target = (input.right - input.left) * (inWater ? 0.10 : 0.19);
        this.vx += clamp(target - this.vx, -accel, accel);
        this.vx *= this.onGround ? 0.80 : 0.94;
        if (input.left) this.facing = -1;
        if (input.right) this.facing = 1;
        this.vy = Math.min(C.MAX_FALL, this.vy + (inWater ? C.GRAVITY * 0.35 : C.GRAVITY));
        if (inWater) this.vy *= 0.86;
        if (input.jump && this.onGround) { this.vy = -8.1; this.onGround = false; }
        if (input.jump && inWater) this.vy = -3.0;
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
    }
    hurt(dmg, fromX) {
      if (this.invuln > 0 || this.creative) return false;
      this.life -= dmg;
      this.invuln = 40;
      this.vx = (this.x < fromX ? -1 : 1) * 5;
      this.vy = -5;
      if (this.life <= 0) { this.life = 0; this.dead = true; }
      return true;
    }
    heal(n) { this.life = Math.min(this.maxLife, this.life + n); }
    attackDamage(inv) {
      const h = inv.held();
      const d = h.id ? DEF[h.id] : null;
      return d && d.dmg ? d.dmg : 1;
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
    constructor(type, x, y) {
      const d = MOB_DEFS[type] || MOB_DEFS.zombie;
      this.type = type; this.def = d;
      this.x = x; this.y = y; this.vx = 0; this.vy = 0;
      this.hp = d.hp; this.maxHp = d.hp;
      this.cd = 0; this.fuse = 0; this.wander = Math.random() * 200;
      this.facing = 1; this.held = 0; this.arche = Math.floor(Math.random() * 6);
      this.hurtFlash = 0; this.dead = false;
    }
    get w() { return this.def.w; }
    get h() { return this.def.h; }
    update(world, player, api) {
      if (this.dead) return;
      if (this.cd > 0) this.cd--;
      if (this.hurtFlash > 0) this.hurtFlash--;
      const b = { x0: this.x - this.w / 2, x1: this.x + this.w / 2,
                  y0: this.y - this.h / 2, y1: this.y + this.h / 2 };
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
      this.vx = dir * this.def.speed;
      this.facing = dir < 0 ? -1 : 1;
      // 重力 + 碰撞（简化：脚下有方块就停）
      this.vy = Math.min(11, this.vy + 0.4);
      const footY = Math.floor(this.y + this.h / 2 + 0.05);
      if (world.solid(Math.floor(this.x), footY) ||
          world.solid(Math.floor(this.x - this.w / 2), footY) ||
          world.solid(Math.floor(this.x + this.w / 2), footY)) {
        this.vy = 0;
        this.y = footY - this.h / 2 - 0.001;
        if (dir !== 0 && world.solid(Math.floor(this.x + dir * 0.6), footY - 1) &&
            !world.solid(Math.floor(this.x + dir * 0.6), footY - 2)) this.vy = -5.5;
      }
      let nx = this.x + this.vx;
      if (world.solid(Math.floor(nx + (this.vx > 0 ? this.w / 2 : -this.w / 2)),
                      Math.floor(this.y))) { this.vx = 0; nx = this.x; this.facing *= -1; }
      this.x = clamp(nx, 2, C.W - 2);
      this.y = clamp(this.y + this.vy, 2, C.H - 2);
      // 苦力怕引信
      if (this.type === 'creeper' && distP < 2.2) {
        this.fuse++;
        if (this.fuse > 60) { api.explode(this.x, this.y, 3); this.dead = true; }
      } else if (this.type !== 'creeper') this.fuse = 0;
      // 接触伤害
      if (hostile && distP < 1.1 && this.cd <= 0) {
        player.hurt(this.def.dmg, this.x);
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
      if (this.type === 'arrow') this.vy += 0.035;
      if (this.life <= 0 || !Core.inWorld(this.x, this.y)) { this.dead = true; return; }
      if (world.solid(Math.floor(this.x), Math.floor(this.y)) &&
          this.type !== 'breath') { this.dead = true; return; }
      if (dist(this.x, this.y, player.x, player.y) < 0.85) {
        player.hurt(this.dmg, this.x);
        this.dead = true;
      }
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
    Player, Mob, MOB_DEFS, Projectile, EnderDragon,
    followChain, constrainChain, flyTo, DRAGON_HP, SEGMENT_COUNT
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
