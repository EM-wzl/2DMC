/* 2DMC Web — 自检（浏览器 /selftest 与 Node test/selftest.cjs 共用） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, Core, DEF, Items, Inv, World, Entities } = DMC;

  function run() {
    const lines = [];
    let pass = 0, fail = 0;
    const ck = (ok, name) => {
      lines.push((ok ? 'PASS ' : 'FAIL ') + name);
      ok ? pass++ : fail++;
    };
    const eq = (a, b, name) => ck(a === b, name + ' (got ' + a + ', want ' + b + ')');

    /* ---- 1. RLE ---- */
    {
      const arr = new Uint16Array(5000);
      for (let i = 1200; i < 1300; i++) arr[i] = B.STONE;
      arr[4000] = B.DIAMOND_ORE;
      const enc = Core.rleEncode(arr);
      const dec = Core.rleDecode(enc, arr.length);
      let same = true;
      for (let i = 0; i < arr.length; i++) if (dec[i] !== arr[i]) { same = false; break; }
      ck(same, 'RLE 往返无损');
      ck(enc.length < 500, 'RLE 压缩稀疏平面（' + enc.length + ' 字符）');
    }

    /* ---- 2. 合成匹配（原版有序 / 无序） ---- */
    {
      const z3 = () => new Array(9).fill(0);
      let g = new Array(4).fill(0); g[0] = B.LOG;
      let r = Items.craftMatch2(g);
      ck(r && r.out === B.PLANK && r.count === 4, '2x2 原木 → 木板 ×4');

      g = z3(); g[0] = B.PLANK; g[1] = B.PLANK; g[3] = B.PLANK; g[4] = B.PLANK;
      r = Items.craftMatch3(g);
      ck(r && r.out === B.CRAFTING_TABLE, '3x3 四木板 → 工作台');
      const g2 = [B.PLANK, B.PLANK, B.PLANK, B.PLANK];
      const r2 = Items.craftMatch2(g2);
      ck(r2 && r2.out === B.CRAFTING_TABLE, '2x2 四木板也能合成工作台（bbox ≤ 2×2）');

      g = z3(); g[0] = B.PLANK; g[3] = B.PLANK;
      r = Items.craftMatch3(g);
      ck(r && r.out === I.STICK && r.count === 4, '竖排两木板 → 木棍 ×4');

      g = new Array(9).fill(I.IRON_INGOT);
      r = Items.craftMatch3(g);
      ck(r && r.out === B.IRON_BLOCK, '3x3 九铁锭 → 铁块');

      g = z3(); g[0] = B.SUGAR_CANE; g[1] = B.SUGAR_CANE; g[2] = B.SUGAR_CANE;
      r = Items.craftMatch3(g);
      ck(r && r.out === I.PAPER && r.count === 3, '无序配方：三甘蔗 → 纸 ×3');

      g = z3();
      g[0] = B.COBBLE; g[1] = B.COBBLE; g[2] = B.COBBLE;
      g[3] = B.COBBLE; g[5] = B.COBBLE;
      g[6] = B.COBBLE; g[7] = B.COBBLE; g[8] = B.COBBLE;
      r = Items.craftMatch3(g);
      ck(r && r.out === B.FURNACE, '有序配方：熔炉（中空 3x3）');
      ck(!Items.craftMatch2(new Array(4).fill(B.COBBLE)), '2x2 无法合成 3x3 熔炉');

      g = z3();
      g[0] = B.GLASS; g[1] = B.GLASS; g[2] = B.GLASS;
      g[3] = B.GLASS; g[4] = I.DIAMOND; g[5] = B.GLASS;
      g[6] = B.GLASS; g[7] = I.GHAST_TEAR; g[8] = B.GLASS;
      r = Items.craftMatch3(g);
      ck(r && r.out === B.END_CRYSTAL, '原版配方：末影水晶');

      g = z3(); g[0] = B.PURPUR; g[3] = B.PURPUR;
      r = Items.craftMatch3(g);
      ck(r && r.out === B.PURPUR_PILLAR, '竖排紫珀块 → 紫珀柱（9 字符图案）');

      g = z3();
      g[0] = B.OBSIDIAN; g[1] = B.OBSIDIAN; g[2] = B.OBSIDIAN;
      g[3] = B.OBSIDIAN; g[4] = I.ENDER_PEARL; g[5] = B.OBSIDIAN;
      g[6] = B.OBSIDIAN; g[7] = B.OBSIDIAN; g[8] = B.OBSIDIAN;
      r = Items.craftMatch3(g);
      ck(r && r.out === B.END_PORTAL_FRAME, '原版配方：末地传送门框架');

      // 图案平移（2x2 配方在 3x3 右下角也应能匹配）
      g = z3(); g[4] = B.LOG;
      r = Items.craftMatch3(g);
      ck(r && r.out === B.PLANK, '有序配方支持在 3x3 网格内平移');
    }

    /* ---- 3. 原版光标拿取 ---- */
    {
      let cur = Inv.slot(0, 0), s = Inv.slot(I.DIAMOND, 5);
      Inv.cursorLeft(cur, s);
      ck(cur.id === I.DIAMOND && cur.count === 5 && s.id === 0, '左键拿起整堆');
      const e = Inv.slot(0, 0);
      Inv.cursorLeft(cur, e);
      ck(e.id === I.DIAMOND && e.count === 5 && cur.id === 0, '左键放下整堆');
      cur = Inv.slot(I.DIAMOND, 3); s = Inv.slot(I.DIAMOND, 10);
      Inv.cursorLeft(cur, s);
      ck(s.count === 13 && cur.id === 0, '左键同类合并');
      cur = Inv.slot(I.IRON_INGOT, 2); s = Inv.slot(B.STONE, 7);
      Inv.cursorLeft(cur, s);
      ck(cur.id === B.STONE && cur.count === 7 && s.id === I.IRON_INGOT && s.count === 2, '左键异类交换');

      cur = Inv.slot(0, 0); s = Inv.slot(I.COAL, 5);
      Inv.cursorRight(cur, s);
      ck(cur.count === 3 && s.count === 2, '右键拿一半（向上取整）');
      const e2 = Inv.slot(0, 0);
      Inv.cursorRight(cur, e2);
      ck(e2.id === I.COAL && e2.count === 1 && cur.count === 2, '右键放一个');
      const other = Inv.slot(B.STONE, 4);
      const before = cur.count;
      Inv.cursorRight(cur, other);
      eq(cur.count, before, '右键对异类槽位无操作');
    }
    {
      const inv = new Inv.Inventory();
      eq(inv.add(I.COAL, 70), 0, '背包堆叠跨槽');
      eq(inv.countOf(I.COAL), 70, '背包计数');
      ck(inv.remove(I.COAL, 30) && inv.countOf(I.COAL) === 40, '背包移除');
      inv.cursor = Inv.slot(I.DIAMOND, 3);
      inv.dropCursor();
      ck(inv.cursor.id === 0 && inv.countOf(I.DIAMOND) === 3, '关闭界面光标物品退回背包');
    }

    /* ---- 4. 世界生成 / 存档 ---- */
    let world;
    {
      world = new World(20261114);
      world.generateAll();
      let grass = 0, ore = 0, bedrock = 0;
      for (let x = 0; x < C.W; x += 7) {
        if (world.getAt(0, x, world.heights[x]) === B.GRASS ||
            world.getAt(0, x, world.heights[x]) === B.SAND ||
            world.getAt(0, x, world.heights[x]) === B.SNOW) grass++;
        if (world.getAt(0, x, C.H - 2) === B.BEDROCK) bedrock++;
        for (let y = 120; y < 230; y += 3) {
          const v = world.getAt(0, x, y);
          if ([B.COAL_ORE, B.IRON_ORE, B.DIAMOND_ORE, B.GOLD_ORE, B.COPPER_ORE].includes(v)) ore++;
        }
      }
      ck(grass > 50, '主世界生成地表（' + grass + ' 采样列）');
      ck(ore > 20, '主世界生成矿脉（' + ore + ' 处）');
      eq(bedrock, Math.ceil(C.W / 7), '世界底部基岩');
      ck(world.stronghold.x > 0 && world.stronghold.y > 0, '要塞 / 末地传送门框架位置已记录');
    }
    {
      let stone = 0, crystal = 0, portal = 0, egg = 0, pillar = 0;
      for (let x = 0; x < C.W; x += 1) for (let y = 70; y < 165; y += 1) {
        const v = world.getAt(2, x, y);
        if (v === B.END_STONE || v === B.END_STONE_BRICKS) stone++;
        else if (v === B.END_CRYSTAL) crystal++;
        else if (v === B.END_PORTAL) portal++;
        else if (v === B.DRAGON_EGG) egg++;
        else if (v === B.OBSIDIAN_PILLAR) pillar++;
      }
      ck(stone > 50, '末地中央岛生成（' + stone + ' 采样格）');
      ck(crystal > 0 && world.crystalsAlive() === 4, '四枚末影水晶 + 存活计数');
      ck(portal > 0 && egg > 0, '退出传送门与龙蛋');
      ck(pillar > 0, '黑曜石柱');
      const cx = C.W >> 1;
      let voidBelow = 0;
      for (let y = 190; y < C.H - 2; y += 5) if (world.getAt(2, cx, y) === 0) voidBelow++;
      ck(voidBelow > 5, '中央岛下方是虚空');
    }
    {
      const snap = world.serialize();
      const w2 = World.deserialize(snap);
      ck(!!w2, '存档反序列化成功');
      let same = true;
      for (let d = 0; d < 3 && same; d++)
        for (let x = 0; x < C.W && same; x += 29)
          for (let y = 0; y < C.H; y += 29)
            if (w2.getAt(d, x, y) !== world.getAt(d, x, y)) { same = false; break; }
      ck(same, '三维度 RLE 存档往返逐格一致');
      eq(w2.seed, world.seed, '种子往返');
    }

    /* ---- 5. 末影龙（链式 Boss） ---- */
    {
      const d = new Entities.EnderDragon(world);
      eq(d.segs.length, Entities.SEGMENT_COUNT, '龙链长度（头 + 12 体节 + 尾）');
      eq(d.hp, Entities.DRAGON_HP, '末影龙初始满血');
      const segs = [{ x: 10, y: 10 }, { x: 0, y: 0 }, { x: -10, y: -10 }];
      const before = Math.hypot(segs[1].x - segs[0].x, segs[1].y - segs[0].y);
      Entities.followChain(segs, 6);
      const after = Math.hypot(segs[1].x - segs[0].x, segs[1].y - segs[0].y);
      ck(after < before, '链式跟随收敛（世界吞噬者同款公式）');
      const c = [{ x: 0, y: 0 }, { x: 0.2, y: 0 }];
      Entities.constrainChain(c, 1.4);
      ck(Math.hypot(c[1].x - c[0].x, c[1].y - c[0].y) >= 1.39, '体节最小间距约束');
      const h = { x: 100, y: 100, vx: 0, vy: 0 };
      for (let i = 0; i < 40; i++) Entities.flyTo(h, 0, 100, 0.5, 2);
      ck(h.x < 100 && Math.abs(h.vx) <= 2.0001, '头部飞行朝目标移动且限速生效');

      // 状态机循环
      const player = new Entities.Player(world.endSpawn.x, world.endSpawn.y - 6);
      player.creative = true;   // 避免被打死干扰状态机
      const api = {
        spawnBreath() { breaths++; },
        onDragonDeath() { deaths++; }
      };
      let breaths = 0, deaths = 0;
      const seen = new Set();
      for (let i = 0; i < 1400; i++) { d.step(world, player, api); seen.add(d.state); }
      ck(seen.has('CRUISE') && seen.has('DIVE') && seen.has('PERCH'),
         '状态机循环：巡航 / 俯冲 / 栖息（' + Array.from(seen).join(',') + '）');
      ck(breaths > 0, '龙息发射');
      ck(d.head.x > 0 && d.head.x < C.W && d.head.y > 0 && d.head.y < C.H, '龙不会飞出世界');

      // 被打 → 立刻栖息
      d.state = 'CRUISE'; d.hp = 100;
      d.hurt(5);
      eq(d.state, 'PERCH', '被近战命中后立刻降落栖息');

      // 水晶回血
      d.state = 'PERCH'; d.landed = true; d.crystalHealTick = 0; d.hp = 50;
      for (let i = 0; i < 62; i++) d.step(world, player, api);
      ck(d.hp > 50, '末影水晶存活时栖息回血（hp ' + d.hp + '）');
      // 打掉所有水晶
      for (const c2 of world.crystals) {
        c2.alive = false;
        world.set(c2.x, c2.y, B.AIR, 2);
      }
      eq(world.crystalsAlive(), 0, '水晶可被打掉');
      d.hp = 50; d.state = 'PERCH'; d.landed = true; d.crystalHealTick = 0;
      for (let i = 0; i < 62; i++) d.step(world, player, api);
      eq(d.hp, 50, '水晶全毁后不再回血');
      // 死亡
      d.hp = 1; d.alive = true;
      d.hurt(5);
      d.die(api, world);
      ck(deaths > 0 && !d.alive && world.dragonDead, '死亡回调 + 清状态');
    }

    /* ---- 6. 玩家物理 / 跳跃不是飞行 ---- */
    {
      const w = new World(7); w.generateOverworld();
      const p = new Entities.Player(C.W / 2, 40);
      const zero = { left: 0, right: 0, jump: 0, up: 0, down: 0 };
      const y0 = p.y;
      for (let i = 0; i < 40; i++) p.update(w, zero);
      ck(p.y > y0, '重力生效');
      for (let i = 0; i < 300; i++) p.update(w, zero);
      ck(p.y < C.H - 2, '落地不会穿出世界');
      ck(p.onGround, '落地后 onGround 为真');

      // 单次跳跃：高度必须是「格」级而不是「飞」
      const groundY = p.y;
      p.update(w, Object.assign({}, zero, { jump: 1 }));
      let top = p.y;
      let airborne = 0;
      for (let i = 0; i < 150; i++) {
        p.update(w, zero);
        top = Math.min(top, p.y);
        if (!p.onGround) airborne++;
      }
      const rise = groundY - top;
      ck(rise > 0.6 && rise < 2.6,
         '单次跳跃高度 ' + rise.toFixed(2) + ' 格（0.6~2.6，不能像飞行）');
      ck(airborne > 8 && airborne < 60, '滞空 ' + airborne + ' 帧（不是长时间漂浮）');
      ck(Math.abs(p.y - groundY) < 0.6, '跳跃后回到地面');

      // 行走速度也是格/帧量级
      const walkStart = p.x;
      for (let i = 0; i < 60; i++) p.update(w, Object.assign({}, zero, { right: 1 }));
      const walked = p.x - walkStart;
      ck(walked > 0.5 && walked < 10, '行走 60 帧位移 ' + walked.toFixed(2) + ' 格');
    }

    /* ---- 7. 生物不卡方块 ---- */
    {
      const w = new World(31); w.generateOverworld();
      const p0 = new Entities.Player(C.W / 2, 40);
      const zero = { left: 0, right: 0, jump: 0, up: 0, down: 0 };
      for (let i = 0; i < 300; i++) p0.update(w, zero);
      const player = new Entities.Player(p0.x, p0.y);
      player.creative = true;
      const api = { explode() {} };
      const types = ['zombie', 'skeleton', 'creeper', 'spider', 'enderman', 'cube', 'sheep'];
      const mobs = [];
      for (let i = 0; i < 12; i++) {
        const t = types[i % types.length];
        const d = Entities.MOB_DEFS[t];
        const x = Math.floor(p0.x) - 30 + i * 5 + 0.5;
        const y = Entities.Physics.findSpawn(w, x, p0.y, d.w, d.h);
        if (y !== null) mobs.push(new Entities.Mob(t, x, y));
      }
      ck(mobs.length >= 8, '生物生成点（头顶够高 + 脚下有支撑）' + mobs.length + '/12');
      let stuck = 0, oob = 0, moved = 0;
      const startX = mobs.map(m => m.x);
      for (let f = 0; f < 400; f++) {
        for (const m of mobs) {
          m.update(w, player, api);
          if (Entities.Physics.hitsSolid(w, Entities.Physics.boxOf(m))) stuck++;
          if (m.y < 1 || m.y > C.H - 1 || m.x < 1 || m.x > C.W - 1) oob++;
        }
      }
      eq(stuck, 0, '12 只生物跑动 400 帧，0 帧出现卡在方块里');
      eq(oob, 0, '生物不会掉出世界');
      mobs.forEach((m, i) => { if (Math.abs(m.x - startX[i]) > 0.5) moved++; });
      ck(moved >= 4, '生物确实会走动（' + moved + '/12）');

      // 故意把生物埋进方块，验证 unstick 自救
      const buried = new Entities.Mob('zombie', Math.floor(p0.x) + 0.5, p0.y);
      w.set(Math.floor(buried.x), Math.floor(buried.y), B.STONE);
      const ok = Entities.Physics.unstick(buried, w);
      ck(ok && !Entities.Physics.hitsSolid(w, Entities.Physics.boxOf(buried)),
         '被方块埋住的生物能自动脱困');
    }

    /* ---- 8. 龙息滞留云 / 配方书纯函数 ---- */
    {
      const p = new Entities.Player(C.W / 2, 60);
      const cloud = new Entities.BreathCloud(p.x, p.y);
      const life0 = p.life;
      let hits = 0;
      for (let i = 0; i < 60; i++) if (cloud.step(p)) hits++;
      ck(hits > 0 && p.life < life0, '龙息云对范围内玩家持续造成伤害（' + hits + ' 次判定）');
      const far = new Entities.Player(p.x + 20, p.y);
      const c2 = new Entities.BreathCloud(p.x + 20, p.y + 20);
      const lf = far.life;
      for (let i = 0; i < 40; i++) c2.step(far);
      eq(far.life, lf, '云范围外不扣血');
      const c3 = new Entities.BreathCloud(0, 0);
      for (let i = 0; i < 160; i++) c3.step(new Entities.Player(100, 100));
      ck(c3.dead, '龙息云到期消散');

      const inv = new Inv.Inventory();
      inv.add(B.LOG, 2);
      const rPlank = Items.RECIPES.find(r => r.out === B.PLANK);
      ck(Items.canCraft(rPlank, inv, 2), '配方书：有原木 → 可做木板');
      const grid = Items.applyRecipe(rPlank, 2);
      const m = Items.craftMatch2(grid);
      ck(m && m.out === B.PLANK, '一键摆料后 2x2 网格能匹配出木板');
      const rFurnace = Items.RECIPES.find(r => r.out === B.FURNACE);
      ck(!Items.fitsSize(rFurnace, 2), '2x2 界面过滤掉 3x3 熔炉配方');
      inv.add(B.COBBLE, 8);
      ck(Items.canCraft(rFurnace, inv, 3), '配方书：圆石够了 → 可做熔炉');
      const grid3 = Items.applyRecipe(rFurnace, 3);
      const m3 = Items.craftMatch3(grid3);
      ck(m3 && m3.out === B.FURNACE, '熔炉一键摆料后 3x3 网格能匹配');
      const needs = Items.recipeNeeds(rFurnace);
      eq(needs[B.COBBLE], 8, '熔炉材料统计正确（8 圆石）');
    }

    /* ---- 9. 饥饿 / 食物 ---- */
    {
      const w = new World(9); w.generateOverworld();
      const p = new Entities.Player(C.W / 2, 60);
      eq(p.hunger, 20, '初始饱食度 20');
      eq(DEF[I.BREAD].food, 5, '面包提供 5 点饱食');
      ck(!p.eat(5), '饱食时不浪费食物');
      p.hunger = 10; p.saturation = 0;
      ck(p.eat(5) && p.hunger === 15, '吃面包后饱食度 +5');
      ck(p.saturation > 0, '吃东西同时补充隐藏饱和度');
      p.hunger = 20; p.saturation = 0; p.exhaustion = 3.9;
      p.exhaust(0.2); p.metabolism(0);
      eq(p.hunger, 19, '消耗度满 4 且无饱和度时扣 1 点饥饿');
      p.creative = true; p.hunger = 20;
      p.exhaust(100); p.metabolism(100);
      eq(p.hunger, 20, '创造模式不消耗饥饿');
      p.creative = false; p.hunger = 20; p.saturation = 5; p.life = 10;
      p.exhaustion = 0; p.regenTick = 0;
      for (let i = 0; i < 80; i++) p.metabolism(0);
      ck(p.life > 10, '饱食时自然回血（' + p.life + '）');
      p.hunger = 0; p.saturation = 0; p.life = 5; p.starveTick = 0;
      for (let i = 0; i < 420; i++) p.metabolism(0);
      eq(p.life, 1, '饿空会掉血但最低保留 1 点');
      p.respawn(w);
      eq(p.hunger, 20, '重生恢复满饱食');
    }

    /* ---- 10. 天气状态机 ---- */
    {
      const Wea = DMC.Weather;
      ck(!!Wea, '天气模块存在');
      const w = new Wea(12345);
      ck(w.set('rain', 800) && w.raining && !w.snowing, '可设为下雨');
      w.intensity = 1;
      ck(w.skyDim > 0.1, '雨天削弱天空光照（' + w.skyDim.toFixed(2) + '）');
      ck(w.set('snow', 800) && w.snowing, '可设为下雪');
      ck(!w.set('volcano', 100), '拒绝未知天气类型');
      let desertOk = true, snowOk = true;
      for (let i = 0; i < 60; i++) {
        const a = new Wea(700 + i); a.pick(3); if (a.type !== 'clear') desertOk = false;
        const b = new Wea(900 + i); b.pick(1);
        if (b.type === 'rain' || b.type === 'thunder') snowOk = false;
      }
      ck(desertOk, '沙漠群系不会下雨');
      ck(snowOk, '雪原群系下雨自动转为降雪');
      const t = new Wea(5); t.set('thunder', 5000); t.intensity = 1; t.thunderCd = 1;
      eq(t.step(0, 0), 'thunder', '雷暴会触发闪电事件');
      ck(t.flash > 0.5, '闪电产生画面白闪');
      const n = new Wea(6); n.set('rain', 900); n.intensity = 1;
      eq(n.step(1, 0), 'stop', '下界自动停雨');
      eq(n.type, 'clear', '非主世界天气为晴');
      const s = n.serialize();
      const r = Wea.restore(JSON.parse(JSON.stringify(s)), 6);
      ck(r.type === n.type && r.ticks === n.ticks &&
         Math.abs(r.intensity - n.intensity) < 1e-9, '天气存档往返无损');
      const c = new Wea(8); c.set('clear', 3000);
      eq(c.skyDim, 0, '晴天不削弱光照');
      // 联机客户端：只做渐变与闪电衰减，不自行切换天气
      const cli = new Wea(77); cli.set('rain', 3);
      const cev = [];
      for (let i = 0; i < 10; i++) cev.push(cli.step(0, 0, true));
      ck(cli.type === 'rain' && cli.ticks === 3 && cev.every(e => e === null),
         '联机客户端不自行切换天气、不触发雷暴事件');
      ck(cli.intensity > 0, '客户端仍平滑渐变天气强度');
    }

    /* ---- 11. 音效模块 ---- */
    {
      ck(!!DMC.Sound, '音效模块存在');
      const s = new DMC.Sound();
      let threw = false;
      try {
        s.play('break'); s.play('eat'); s.setAmbient('rain'); s.setAmbient('none');
      } catch (e) { threw = true; }
      ck(!threw, '无音频设备时音效调用安全降级');
      s.setEnabled(false);
      ck(!s.enabled && !s.unlock(), '静音时不解锁音频上下文');
      ck(s.toggle() === true && s.enabled, '静音开关可重新开启');
    }

    /* ---- 12. 成就与统计 ---- */
    {
      const A = DMC.Achievements;
      ck(!!A && DMC.ACHIEVEMENTS.length >= 10, '成就模块与成就表存在');
      const a = new A();
      ck(a.count === 0 && a.total === DMC.ACHIEVEMENTS.length, '初始未解锁任何成就');
      a.note('mine', B.LOG);
      let got = a.check(null);
      ck(got.length === 1 && got[0].id === 'first_mine', '挖第一格解锁开局一镐');
      ck(a.check(null).length === 0, '成就不会重复解锁');
      for (let i = 0; i < 7; i++) a.note('mine', B.LOG);
      ck(a.check(null).some(x => x.id === 'lumberjack'), '挖满 8 原木解锁伐木工');
      a.note('mine', B.IRON_ORE);
      a.note('mine', B.DIAMOND_ORE);
      a.note('craft'); a.note('eat');
      a.note('dim', 1); a.note('dim', 2); a.note('dragon'); a.note('storm');
      got = a.check(null);
      const ids = got.map(x => x.id);
      ck(['iron_age', 'diamond', 'crafter', 'foodie', 'nether', 'end', 'dragon', 'storm']
           .every(k => ids.indexOf(k) >= 0), '矿石/合成/进食/维度/屠龙/雷暴成就全部解锁');
      const s = a.serialize();
      const b = A.restore(JSON.parse(JSON.stringify(s)));
      ck(b.count === a.count && b.stats.mined === a.stats.mined &&
         b.stats.dims[2] === true && b.stats.dragonDead === true, '成就存档往返无损');
      for (let i = 0; i < 6000; i++) a.note('tick');
      ck(a.check(null).some(x => x.id === 'survivor'), '累计 6000 帧解锁老玩家');
      ck(a.progress().length === a.total, '进度列表长度与成就总数一致');
    }

    /* ---- 13. 状态效果 / 药水 ---- */
    {
      const E = DMC.Effects;
      ck(!!E && DMC.EFFECT_IDS.length >= 8, '状态效果模块与效果表存在');
      const e = new E();
      ck(!e.has('speed') && e.speedMul === 1, '无效果时速度倍率为 1');
      ck(e.apply('speed', 1200, 2) && e.level('speed') === 2, '施加二级速度');
      ck(e.speedMul > 1.4, '速度倍率生效');
      e.apply('speed', 600, 1);
      ck(e.level('speed') === 2 && e.active.speed.ticks === 1200,
         '重复饮用取较高等级与较长时间');
      e.apply('slow', 600, 1);
      ck(e.speedMul < 1 + 0.28 * 2, '缓慢抵消部分速度');
      ck(e.apply('night', 100, 1) && e.nightVision, '夜视标记生效');
      ck(!e.apply('bogus', 10, 1), '拒绝未知效果');
      for (let i = 0; i < 101; i++) e.tick();
      ck(!e.has('night'), '效果到期消失');
      ck(e.jumpMul === 1 && e.mineMul >= 0.4, '未施加时跳跃/挖掘倍率正常');
      const r = new E(); r.apply('regen', 500, 1);
      const pl = { life: 5, heal(n) { this.life = Math.min(20, this.life + n); } };
      for (let i = 0; i < 80; i++) r.potionTick(pl);
      ck(pl.life > 5, '再生效果持续回血');
      const ps = new E(); ps.apply('poison', 1000, 1);
      const pl2 = { life: 8 };
      for (let i = 0; i < 60; i++) ps.potionTick(pl2);
      eq(pl2.life, 7, '中毒每 60 帧掉 1 血');
      for (let i = 0; i < 700; i++) ps.potionTick(pl2);
      eq(pl2.life, 1, '中毒不会直接毒死');
      const e2 = E.restore(JSON.parse(JSON.stringify(r.serialize())));
      ck(e2.has('regen') && e2.active.regen.ticks === 500, '状态效果存档往返无损');
      ck(DEF[I.POTION_SPEED].potion === 'speed' &&
         DEF[I.POTION_NIGHT].potion === 'night', '药水物品带效果标记');
      const inv = new Inv.Inventory();
      inv.add(I.GLOW_BERRIES, 1); inv.add(B.SUGAR_CANE, 1);
      const rp = Items.RECIPES.find(x => x.out === I.POTION_SPEED);
      ck(Items.canCraft(rp, inv, 2), '发光浆果 + 甘蔗可合成迅捷药水');
      const a = new DMC.Achievements();
      a.note('potion');
      ck(a.check(null).some(x => x.id === 'alchemist'), '喝药水解锁炼金术士成就');
    }

    return { passed: pass, failed: fail, lines, ok: fail === 0 };
  }

  DMC.SelfTest = { run };
})(typeof globalThis !== 'undefined' ? globalThis : this);
