/* 2DMC Web — 自检（浏览器 /selftest 与 Node test/selftest.cjs 共用） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C, Core, Items, Inv, World, Entities } = DMC;

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

    /* ---- 6. 玩家物理 ---- */
    {
      const w = new World(7); w.generateOverworld();
      const p = new Entities.Player(C.W / 2, 40);
      const zero = { left: 0, right: 0, jump: 0, up: 0, down: 0 };
      let y0 = p.y;
      for (let i = 0; i < 40; i++) p.update(w, zero);
      ck(p.y > y0, '重力生效');
      for (let i = 0; i < 300; i++) p.update(w, zero);
      ck(p.y < C.H - 2, '落地不会穿出世界');
    }

    return { passed: pass, failed: fail, lines, ok: fail === 0 };
  }

  DMC.SelfTest = { run };
})(typeof globalThis !== 'undefined' ? globalThis : this);
