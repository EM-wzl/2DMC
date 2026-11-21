/* 2DMC Web — 方块/物品定义、配方表、原版合成匹配（纯逻辑） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const { B, I, C } = DMC;

  // tex 决定程序化纹理；hardness 是挖掘需要的累计力度；tier 是掉落所需工具等级
  const D = {};
  function def(id, name, cn, color, opt) {
    D[id] = Object.assign({ id, name, cn, color, color2: color, tex: 'solid',
                            hardness: 1, solid: true, light: 0, tier: 0,
                            drop: id, opaque: true }, opt || {});
  }

  /* ---------------- 方块 ---------------- */
  def(B.AIR, 'air', '空气', '#00000000', { solid: false, opaque: false, hardness: 0, tex: 'air' });
  def(B.STONE, 'stone', '石头', '#7c7c7c', { hardness: 12, tier: 1, drop: B.COBBLE, tex: 'stone' });
  def(B.GRASS, 'grass_block', '草方块', '#5b9b45', { color2: '#79583a', hardness: 6, tex: 'grass' });
  def(B.DIRT, 'dirt', '泥土', '#79583a', { hardness: 5, tex: 'dirt' });
  def(B.COBBLE, 'cobblestone', '圆石', '#6f6f6f', { hardness: 12, tier: 1, tex: 'stone' });
  def(B.BEDROCK, 'bedrock', '基岩', '#2b2b2b', { hardness: -1, tex: 'stone' });
  def(B.SAND, 'sand', '沙子', '#dbcf8f', { hardness: 4, tex: 'sand' });
  def(B.SANDSTONE, 'sandstone', '砂岩', '#d8c77d', { hardness: 8, tier: 1 });
  def(B.LOG, 'oak_log', '橡木原木', '#8a6238', { color2: '#a37b46', hardness: 8, tex: 'wood' });
  def(B.LEAVES, 'oak_leaves', '橡树树叶', '#3f7a35', { hardness: 2, solid: false, opaque: false, tex: 'leaves' });
  def(B.PLANK, 'oak_planks', '橡木木板', '#b58a51', { hardness: 7, tex: 'plank' });
  def(B.WATER, 'water', '水', '#2f6fd0', { solid: false, opaque: false, hardness: -1, tex: 'liquid' });
  def(B.LAVA, 'lava', '岩浆', '#e2631c', { solid: false, opaque: false, hardness: -1, light: 12, tex: 'liquid' });
  const ore = (id, name, cn, color, hardness, tier, drop, color2) =>
    def(id, name, cn, color, { hardness, tier, drop: drop || id, tex: 'ore', color2: color2 || '#d9d9d9' });
  ore(B.COAL_ORE, 'coal_ore', '煤矿石', '#5a5a5a', 14, 1, I.COAL, '#232323');
  ore(B.IRON_ORE, 'iron_ore', '铁矿石', '#8d7d6b', 16, 2, I.RAW_IRON, '#d8af8b');
  ore(B.GOLD_ORE, 'gold_ore', '金矿石', '#8d7d6b', 18, 3, I.RAW_GOLD, '#f6d33c');
  ore(B.DIAMOND_ORE, 'diamond_ore', '钻石矿石', '#8d8d8d', 20, 3, I.DIAMOND, '#5de5df');
  ore(B.LAPIS_ORE, 'lapis_ore', '青金石矿石', '#5a5a5a', 16, 2, I.LAPIS, '#2b49b8');
  ore(B.REDSTONE_ORE, 'redstone_ore', '红石矿石', '#5a5a5a', 16, 2, I.REDSTONE_DUST, '#e02b2b');
  ore(B.EMERALD_ORE, 'emerald_ore', '绿宝石矿石', '#5a5a5a', 20, 3, I.EMERALD, '#31d17a');
  ore(B.COPPER_ORE, 'copper_ore', '铜矿石', '#8d7d6b', 14, 2, I.RAW_COPPER, '#e08b58');
  ore(B.IRON_ORE_DEEP, 'deepslate_iron_ore', '深层铁矿石', '#3d3f47', 20, 2, I.RAW_IRON, '#d8af8b');
  ore(B.DIAMOND_ORE_DEEP, 'deepslate_diamond_ore', '深层钻石矿石', '#3d3f47', 24, 3, I.DIAMOND, '#5de5df');
  ore(B.REDSTONE_ORE_DEEP, 'deepslate_redstone_ore', '深层红石矿石', '#3d3f47', 20, 2, I.REDSTONE_DUST, '#e02b2b');
  def(B.DEEPSLATE, 'deepslate', '深板岩', '#484a52', { hardness: 18, tier: 1, tex: 'stone' });
  def(B.AMETHYST, 'amethyst_block', '紫水晶块', '#9a6bc4', { hardness: 10, tier: 1, tex: 'crystal' });
  def(B.SCULK, 'sculk', '幽匿块', '#16333a', { hardness: 8, tex: 'crystal' });
  def(B.TORCH, 'torch', '火把', '#f0b53c', { solid: false, opaque: false, hardness: 1, light: 12, tex: 'plant' });
  def(B.CRAFTING_TABLE, 'crafting_table', '工作台', '#a2743f', { hardness: 8, tex: 'machine' });
  def(B.FURNACE, 'furnace', '熔炉', '#6d6d6d', { hardness: 14, tier: 1, tex: 'machine' });
  def(B.CHEST, 'chest', '箱子', '#a97a3c', { hardness: 8, tex: 'machine' });
  def(B.GLASS, 'glass', '玻璃', '#b9e3ea', { solid: true, opaque: false, hardness: 3, tex: 'glass' });
  def(B.OBSIDIAN, 'obsidian', '黑曜石', '#241a38', { hardness: 40, tier: 4, tex: 'crystal' });
  def(B.NETHER_PORTAL, 'nether_portal', '下界传送门', '#7d3bd6', { solid: false, opaque: false, hardness: -1, light: 8, tex: 'portal' });
  def(B.NETHERRACK, 'netherrack', '下界岩', '#6d3030', { hardness: 6, tier: 1, tex: 'stone' });
  def(B.GLOWSTONE, 'glowstone', '萤石', '#e8c45f', { hardness: 3, light: 14, tex: 'crystal' });
  def(B.SOUL_SAND, 'soul_sand', '灵魂沙', '#4a3a30', { hardness: 5 });
  ore(B.NETHER_GOLD, 'nether_gold_ore', '下界金矿石', '#6d3030', 12, 2, I.RAW_GOLD, '#f6d33c');
  ore(B.ANCIENT_DEBRIS, 'ancient_debris', '远古残骸', '#6d3030', 36, 4, I.NETHERITE_SCRAP, '#c98d5c');
  def(B.NETHER_BRICKS, 'nether_bricks', '下界砖块', '#4a2029', { hardness: 14, tier: 1, tex: 'brick' });
  def(B.MAGMA, 'magma_block', '岩浆块', '#a53b12', { hardness: 10, tier: 1, light: 6 });
  def(B.SULFUR, 'sulfur_block', '硫磺块', '#d8c542', { hardness: 8, tex: 'ore', color2: '#f1e27a' });
  def(B.CINNABAR, 'cinnabar_block', '朱砂块', '#b6402f', { hardness: 8, tex: 'ore', color2: '#ef8a63' });
  def(B.POTENT_SULFUR, 'potent_sulfur', '强效硫磺块', '#e8a72b', { hardness: 10, light: 5, tex: 'crystal' });
  def(B.SULFUR_SPIKE, 'sulfur_spike', '硫磺尖刺', '#e0cc55', { hardness: 6, tex: 'plant' });
  def(B.GEYSER, 'geyser', '间歇泉', '#e6d26a', { hardness: 8, light: 3, tex: 'plant' });
  def(B.SULFUR_POOL, 'sulfur_pool', '硫磺池', '#c9b23a', { solid: false, opaque: false, hardness: -1, tex: 'liquid' });
  def(B.CHERRY_LOG, 'cherry_log', '樱花原木', '#c98b8f', { hardness: 8, tex: 'wood' });
  def(B.CHERRY_LEAVES, 'cherry_leaves', '樱花树叶', '#f2a9c4', { hardness: 2, solid: false, opaque: false, tex: 'leaves' });
  def(B.MANGROVE_LOG, 'mangrove_log', '红树原木', '#8c4a3b', { hardness: 8, tex: 'wood' });
  def(B.MANGROVE_LEAVES, 'mangrove_leaves', '红树树叶', '#387a43', { hardness: 2, solid: false, opaque: false, tex: 'leaves' });
  def(B.PALE_LOG, 'pale_oak_log', '苍白橡木原木', '#a9a89a', { hardness: 8, tex: 'wood' });
  def(B.PALE_LEAVES, 'pale_oak_leaves', '苍白橡木树叶', '#8d9c86', { hardness: 2, solid: false, opaque: false, tex: 'leaves' });
  def(B.MOSS, 'moss_block', '苔藓块', '#4d7a3a', { hardness: 4 });
  def(B.AZALEA, 'azalea_leaves', '杜鹃树叶', '#4e7a46', { hardness: 2, solid: false, opaque: false, tex: 'leaves' });
  def(B.CACTUS, 'cactus', '仙人掌', '#3f8a4a', { hardness: 3, tex: 'plant' });
  def(B.SUGAR_CANE, 'sugar_cane', '甘蔗', '#8fbf58', { hardness: 2, solid: false, opaque: false, tex: 'plant' });
  def(B.SNOW, 'snow_block', '雪块', '#e9f2f7', { hardness: 3 });
  def(B.ICE, 'ice', '冰', '#8fc9e8', { hardness: 4, opaque: false, tex: 'glass' });
  def(B.GRAVEL, 'gravel', '砂砾', '#8a8479', { hardness: 5, tex: 'sand' });
  def(B.CLAY, 'clay', '黏土', '#a0a8b3', { hardness: 5 });
  def(B.PURPUR, 'purpur_block', '紫珀块', '#c38fd0', { hardness: 12, tier: 1, tex: 'brick' });
  def(B.PURPUR_PILLAR, 'purpur_pillar', '紫珀柱', '#b078c4', { hardness: 12, tier: 1, tex: 'pillar' });
  def(B.CHORUS_PLANT, 'chorus_plant', '紫颂植物', '#8a5fa0', { hardness: 4, tex: 'plant' });
  def(B.CHORUS_FLOWER, 'chorus_flower', '紫颂花', '#d6a6e2', { hardness: 3, tex: 'plant' });
  def(B.DRAGON_EGG, 'dragon_egg', '龙蛋', '#1b1226', { hardness: 30, tier: 4, tex: 'crystal', color2: '#8b5bd6' });
  def(B.END_PORTAL, 'end_portal', '末地传送门', '#2b1046', { solid: false, opaque: false, hardness: -1, light: 9, tex: 'portal', color2: '#c46bff' });
  def(B.END_PORTAL_FRAME, 'end_portal_frame', '末地传送门框架', '#3c6b63', { hardness: 24, tier: 3, tex: 'machine', color2: '#c8a4ff' });
  def(B.END_CRYSTAL, 'end_crystal', '末影水晶', '#b478f0', { hardness: 20, tier: 3, light: 10, tex: 'crystal' });
  def(B.OBSIDIAN_PILLAR, 'obsidian_pillar', '黑曜石柱', '#241a38', { hardness: 30, tier: 4, tex: 'pillar' });
  def(B.END_ROD, 'end_rod', '末地烛', '#f5e6c8', { hardness: 2, solid: false, opaque: false, light: 12, tex: 'plant' });
  def(B.END_STONE, 'end_stone', '末地石', '#d8d2ae', { hardness: 16, tier: 1, tex: 'stone' });
  def(B.END_STONE_BRICKS, 'end_stone_bricks', '末地石砖', '#ded7b5', { hardness: 16, tier: 1, tex: 'brick' });
  def(B.GOLD_DANDELION, 'gold_dandelion', '金蒲公英', '#f2c53d', { hardness: 1, solid: false, opaque: false, tex: 'plant' });
  def(B.BARREL, 'barrel', '木桶', '#9a713f', { hardness: 8, tex: 'machine' });
  def(B.SMITHING, 'smithing_table', '锻造台', '#5b5b66', { hardness: 12, tier: 1, tex: 'machine' });
  def(B.CAMPFIRE, 'campfire', '营火', '#d07a2c', { hardness: 4, solid: false, opaque: false, light: 12, tex: 'plant' });
  def(B.WOOL, 'wool', '羊毛', '#e8e8e8', { hardness: 3 });
  def(B.HAY, 'hay_block', '干草捆', '#d3ad3c', { hardness: 4, tex: 'plank' });
  def(B.BONE_BLOCK, 'bone_block', '骨块', '#dedbc4', { hardness: 8, tier: 1 });
  def(B.IRON_BLOCK, 'iron_block', '铁块', '#dedede', { hardness: 18, tier: 2 });
  def(B.GOLD_BLOCK, 'gold_block', '金块', '#f6d33c', { hardness: 14, tier: 2 });
  def(B.DIAMOND_BLOCK, 'diamond_block', '钻石块', '#5de5df', { hardness: 20, tier: 3 });
  def(B.LAPIS_BLOCK, 'lapis_block', '青金石块', '#2b49b8', { hardness: 12, tier: 2 });
  def(B.EMERALD_BLOCK, 'emerald_block', '绿宝石块', '#31d17a', { hardness: 12, tier: 2 });
  def(B.STONE_BRICKS, 'stone_bricks', '石砖', '#7a7a7a', { hardness: 14, tier: 1, tex: 'brick' });
  def(B.BRICKS, 'bricks', '砖块', '#a85443', { hardness: 14, tier: 1, tex: 'brick' });
  def(B.BOOKSHELF, 'bookshelf', '书架', '#8a6238', { hardness: 8, tex: 'machine', color2: '#c8453a' });
  def(B.ENCHANTING, 'enchanting_table', '附魔台', '#3a2a4a', { hardness: 18, tier: 2, tex: 'machine', color2: '#c46bff' });
  def(B.BREWING, 'brewing_stand', '酿造台', '#7a6a55', { hardness: 10, tier: 1, tex: 'machine' });
  def(B.SEA_LANTERN, 'sea_lantern', '海晶灯', '#a9dedb', { hardness: 6, light: 14, tex: 'crystal' });
  def(B.PRISMARINE, 'prismarine', '海晶石', '#5c9a90', { hardness: 12, tier: 1 });
  def(B.SPONGE, 'sponge', '海绵', '#d9cf55', { hardness: 3 });
  def(B.TERRACOTTA, 'terracotta', '陶瓦', '#a3654d', { hardness: 10, tier: 1 });
  def(B.LANTERN, 'lantern', '灯笼', '#e0b95c', { hardness: 4, solid: false, opaque: false, light: 12, tex: 'plant' });
  def(B.SCAFFOLDING, 'scaffolding', '脚手架', '#c2a56a', { hardness: 2, solid: false, opaque: false, tex: 'plank' });
  def(B.BAMBOO, 'bamboo', '竹子', '#6fa348', { hardness: 2, solid: false, opaque: false, tex: 'plant' });
  def(B.HONEY, 'honey_block', '蜂蜜块', '#e5a72d', { hardness: 3 });
  def(B.NETHER_WART, 'nether_wart', '下界疣', '#8d2c3d', { hardness: 2, solid: false, opaque: false, tex: 'plant' });
  def(B.SHROOMLIGHT, 'shroomlight', '菌光体', '#f0a94b', { hardness: 4, light: 13, tex: 'crystal' });
  def(B.CRIMSON_STEM, 'crimson_stem', '绯红菌柄', '#7b3a50', { hardness: 8, tex: 'wood' });
  def(B.WARPED_STEM, 'warped_stem', '诡异菌柄', '#3a8a86', { hardness: 8, tex: 'wood' });
  def(B.NETHER_SPROUTS, 'nether_sprouts', '下界苗', '#4fae86', { hardness: 1, solid: false, opaque: false, tex: 'plant' });
  def(B.BASALT, 'basalt', '玄武岩', '#4a4a52', { hardness: 14, tier: 1 });
  def(B.BLACKSTONE, 'blackstone', '黑石', '#2d2a30', { hardness: 14, tier: 1 });
  def(B.TUFF, 'tuff', '凝灰岩', '#6d7166', { hardness: 12, tier: 1 });
  def(B.CALCITE, 'calcite', '方解石', '#e2e0d5', { hardness: 10, tier: 1 });
  def(B.COPPER_BLOCK, 'copper_block', '铜块', '#d1784b', { hardness: 12, tier: 2 });
  def(B.CANDLE, 'candle', '蜡烛', '#f0e2b8', { hardness: 1, solid: false, opaque: false, light: 8, tex: 'plant' });
  def(B.AMETHYST_CLUSTER, 'amethyst_cluster', '紫水晶簇', '#b485dd', { hardness: 4, solid: false, opaque: false, tex: 'crystal' });
  def(B.CHERRY_PLANK, 'cherry_planks', '樱花木板', '#e0a58f', { hardness: 7, tex: 'plank' });
  def(B.MUD, 'mud', '泥巴', '#8a7d6d', { hardness: 4 });
  def(B.SUSPICIOUS_SAND, 'suspicious_sand', '可疑的沙子', '#c9bb84', { hardness: 5, tex: 'sand' });

  /* ---------------- 物品 ---------------- */
  function item(id, name, cn, color, opt) {
    D[id] = Object.assign({ id, name, cn, color, color2: color, tex: 'item',
                            hardness: 0, solid: false, opaque: false, tier: 0,
                            drop: id }, opt || {});
  }
  item(I.STICK, 'stick', '木棍', '#a97c48');
  item(I.COAL, 'coal', '煤炭', '#282828', { fuel: 80 });
  item(I.RAW_IRON, 'raw_iron', '粗铁', '#d8af8b');
  item(I.RAW_GOLD, 'raw_gold', '粗金', '#f6d33c');
  item(I.RAW_COPPER, 'raw_copper', '粗铜', '#e08b58');
  item(I.IRON_INGOT, 'iron_ingot', '铁锭', '#e0e0e0');
  item(I.GOLD_INGOT, 'gold_ingot', '金锭', '#f6d33c');
  item(I.COPPER_INGOT, 'copper_ingot', '铜锭', '#e08b58');
  item(I.DIAMOND, 'diamond', '钻石', '#5de5df');
  item(I.LAPIS, 'lapis_lazuli', '青金石', '#2b49b8');
  item(I.REDSTONE_DUST, 'redstone', '红石粉', '#e02b2b');
  item(I.EMERALD, 'emerald', '绿宝石', '#31d17a');
  item(I.NETHERITE_SCRAP, 'netherite_scrap', '下界碎片', '#9c6b50');
  item(I.NETHERITE_INGOT, 'netherite_ingot', '下界合金锭', '#6b5a58');
  item(I.FLINT, 'flint', '燧石', '#555555');
  item(I.BONE, 'bone', '骨头', '#dedbc4');
  item(I.STRING, 'string', '线', '#e8e8e8');
  item(I.GUNPOWDER, 'gunpowder', '火药', '#8a8a8a');
  item(I.LEATHER, 'leather', '皮革', '#a06a3c');
  item(I.APPLE, 'apple', '苹果', '#e2483e', { food: 4 });
  item(I.BREAD, 'bread', '面包', '#d9a44f', { food: 5 });
  const tool = (id, name, cn, color, tier, dmg) =>
    item(id, name, cn, color, { tier, dmg, tool: true, tex: 'tool' });
  tool(I.WOOD_PICK, 'wooden_pickaxe', '木镐', '#b58a51', 1, 2);
  tool(I.WOOD_AXE, 'wooden_axe', '木斧', '#b58a51', 1, 3);
  tool(I.WOOD_SHOVEL, 'wooden_shovel', '木锹', '#b58a51', 1, 2);
  tool(I.WOOD_SWORD, 'wooden_sword', '木剑', '#b58a51', 1, 4);
  tool(I.STONE_PICK, 'stone_pickaxe', '石镐', '#8a8a8a', 2, 3);
  tool(I.STONE_AXE, 'stone_axe', '石斧', '#8a8a8a', 2, 4);
  tool(I.STONE_SHOVEL, 'stone_shovel', '石锹', '#8a8a8a', 2, 3);
  tool(I.STONE_SWORD, 'stone_sword', '石剑', '#8a8a8a', 2, 5);
  tool(I.IRON_PICK, 'iron_pickaxe', '铁镐', '#e0e0e0', 3, 4);
  tool(I.IRON_AXE, 'iron_axe', '铁斧', '#e0e0e0', 3, 5);
  tool(I.IRON_SHOVEL, 'iron_shovel', '铁锹', '#e0e0e0', 3, 4);
  tool(I.IRON_SWORD, 'iron_sword', '铁剑', '#e0e0e0', 3, 6);
  tool(I.DIAMOND_PICK, 'diamond_pickaxe', '钻石镐', '#5de5df', 4, 5);
  tool(I.DIAMOND_AXE, 'diamond_axe', '钻石斧', '#5de5df', 4, 6);
  tool(I.DIAMOND_SHOVEL, 'diamond_shovel', '钻石锹', '#5de5df', 4, 5);
  tool(I.DIAMOND_SWORD, 'diamond_sword', '钻石剑', '#5de5df', 4, 7);
  tool(I.NETHERITE_SWORD, 'netherite_sword', '下界合金剑', '#6b5a58', 5, 8);
  item(I.SHEARS, 'shears', '剪刀', '#c8c8c8', { tex: 'tool' });
  item(I.BUNDLE, 'bundle', '收纳袋', '#a06a3c');
  item(I.NAME_TAG, 'name_tag', '命名牌', '#dedbc4');
  item(I.ENDER_PEARL, 'ender_pearl', '末影珍珠', '#3f9e8e');
  item(I.BLAZE_ROD, 'blaze_rod', '烈焰棒', '#f0b53c', { fuel: 120 });
  item(I.GHAST_TEAR, 'ghast_tear', '恶魂之泪', '#e6e2d8');
  item(I.CHORUS_FRUIT, 'chorus_fruit', '紫颂果', '#a95fae', { food: 4 });
  item(I.POPPED_CHORUS, 'popped_chorus_fruit', '爆裂紫颂果', '#c98fd0');
  item(I.ELYTRA, 'elytra', '鞘翅', '#b9a6d8');
  item(I.TOTEM, 'totem_of_undying', '不死图腾', '#d8b45c');
  item(I.SHULKER_SHELL, 'shulker_shell', '潜影壳', '#a98fc0');
  item(I.BRICK, 'brick', '红砖', '#a85443');
  item(I.PAPER, 'paper', '纸', '#eeeeee');
  item(I.BOOK, 'book', '书', '#c8453a');
  item(I.BUCKET, 'bucket', '桶', '#a0a0a0');
  item(I.WATER_BUCKET, 'water_bucket', '水桶', '#3f7fe0');
  item(I.LAVA_BUCKET, 'lava_bucket', '岩浆桶', '#e2631c');
  item(I.FLINT_STEEL, 'flint_and_steel', '打火石', '#9a9a9a');
  item(I.COMPASS, 'compass', '指南针', '#c8453a');
  item(I.CLOCK, 'clock', '时钟', '#f0c65a');
  item(I.GOLDEN_APPLE, 'golden_apple', '金苹果', '#f6d33c', { food: 8 });
  item(I.POTION, 'potion', '治疗药水', '#e0526a', { potion: 'heal' });
  item(I.POTION_HEAL, 'potion_heal', '强效治疗药水', '#f06a7e', { potion: 'heal2' });
  item(I.POTION_SPEED, 'potion_speed', '迅捷药水', '#5ad1ff', { potion: 'speed' });
  item(I.POTION_JUMP, 'potion_jump', '跳跃药水', '#7ee787', { potion: 'jump' });
  item(I.POTION_NIGHT, 'potion_night', '夜视药水', '#8f7bff', { potion: 'night' });
  item(I.POTION_FIRE, 'potion_fire', '抗火药水', '#ff9f43', { potion: 'fire' });
  item(I.POTION_REGEN, 'potion_regen', '再生药水', '#ff8fab', { potion: 'regen' });
  item(I.POTION_POISON, 'potion_poison', '剧毒药水', '#9ccc65', { potion: 'poison' });
  item(I.POTION_SLOW, 'potion_slow', '迟缓药水', '#9aa5b1', { potion: 'slow' });
  item(I.HONEYCOMB, 'honeycomb', '蜜脾', '#e5a72d');
  item(I.SWEET_BERRIES, 'sweet_berries', '甜浆果', '#c8324a', { food: 2 });
  item(I.GLOW_BERRIES, 'glow_berries', '发光浆果', '#f0b45c', { food: 2 });
  item(I.SULFUR_CHUNK, 'sulfur_chunk', '硫磺块粒', '#e0cc55');
  item(I.AMETHYST_SHARD, 'amethyst_shard', '紫水晶碎片', '#b485dd');
  item(I.ECHO_SHARD, 'echo_shard', '回响碎片', '#7fd6cf');
  item(I.GOLD_DANDELION_ITEM, 'gold_dandelion_item', '金蒲公英', '#f2c53d');
  item(I.NETHER_WART_ITEM, 'nether_wart_item', '下界疣', '#8d2c3d');
  item(I.CANDLE_ITEM, 'candle_item', '蜡烛', '#f0e2b8');
  item(I.RAW_SULFUR, 'raw_sulfur', '生硫磺', '#d8c542');
  item(I.CINNABAR_DUST, 'cinnabar_dust', '朱砂粉', '#b6402f');

  /* ---------------- 配方 ---------------- */
  // shaped 的 pattern 必须正好 9 字符（3 行各 3 字符），与 C++ 主线一致
  const RECIPES = [];
  function shaped(out, count, a, b, c, key) {
    const pat = (a + b + c);
    if (pat.length !== 9) throw new Error('shaped pattern must be 9 chars: ' + out + ' ' + pat);
    RECIPES.push({ type: 'shaped', out, count, pattern: [a, b, c], key });
  }
  function shapeless(out, count, need) {
    RECIPES.push({ type: 'shapeless', out, count, need });
  }
  shaped(B.PLANK, 4, 'L  ', '   ', '   ', { L: B.LOG });
  shaped(B.CHERRY_PLANK, 4, 'L  ', '   ', '   ', { L: B.CHERRY_LOG });
  shaped(I.STICK, 4, 'P  ', 'P  ', '   ', { P: B.PLANK });
  shaped(B.CRAFTING_TABLE, 1, 'PP ', 'PP ', '   ', { P: B.PLANK });
  shaped(B.FURNACE, 1, 'SSS', 'S S', 'SSS', { S: B.COBBLE });
  shaped(B.CHEST, 1, 'PPP', 'P P', 'PPP', { P: B.PLANK });
  shaped(B.TORCH, 4, 'C  ', 'S  ', '   ', { C: I.COAL, S: I.STICK });
  // 工具：三个材料 + 两根木棍
  function toolRecipe(out, m) {
    shaped(out, 1, 'MMM', ' S ', ' S ', { M: m, S: I.STICK });
  }
  function axeRecipe(out, m) {
    shaped(out, 1, 'MM ', 'MS ', ' S ', { M: m, S: I.STICK });
  }
  function shovelRecipe(out, m) {
    shaped(out, 1, 'M  ', 'S  ', 'S  ', { M: m, S: I.STICK });
  }
  function swordRecipe(out, m) {
    shaped(out, 1, 'M  ', 'M  ', 'S  ', { M: m, S: I.STICK });
  }
  [B.PLANK, B.COBBLE, I.IRON_INGOT, I.DIAMOND].forEach((m, i) => {
    const picks = [I.WOOD_PICK, I.STONE_PICK, I.IRON_PICK, I.DIAMOND_PICK];
    const axes = [I.WOOD_AXE, I.STONE_AXE, I.IRON_AXE, I.DIAMOND_AXE];
    const shovels = [I.WOOD_SHOVEL, I.STONE_SHOVEL, I.IRON_SHOVEL, I.DIAMOND_SHOVEL];
    const swords = [I.WOOD_SWORD, I.STONE_SWORD, I.IRON_SWORD, I.DIAMOND_SWORD];
    toolRecipe(picks[i], m); axeRecipe(axes[i], m);
    shovelRecipe(shovels[i], m); swordRecipe(swords[i], m);
  });
  shaped(I.NETHERITE_SWORD, 1, 'N  ', 'N  ', 'S  ', { N: I.NETHERITE_INGOT, S: I.STICK });
  shaped(I.SHEARS, 1, 'I  ', ' I ', '   ', { I: I.IRON_INGOT });
  shaped(I.BUNDLE, 1, ' L ', 'S S', ' S ', { L: I.LEATHER, S: I.STRING });
  shaped(I.NAME_TAG, 1, '  S', ' P ', 'S  ', { S: I.STRING, P: I.PAPER });
  // 压缩方块 / 拆解
  function block9(out, m) { shaped(out, 1, 'MMM', 'MMM', 'MMM', { M: m }); }
  function unblock(out, b) { shaped(out, 9, 'B  ', '   ', '   ', { B: b }); }
  block9(B.IRON_BLOCK, I.IRON_INGOT); unblock(I.IRON_INGOT, B.IRON_BLOCK);
  block9(B.GOLD_BLOCK, I.GOLD_INGOT); unblock(I.GOLD_INGOT, B.GOLD_BLOCK);
  block9(B.DIAMOND_BLOCK, I.DIAMOND); unblock(I.DIAMOND, B.DIAMOND_BLOCK);
  block9(B.LAPIS_BLOCK, I.LAPIS); unblock(I.LAPIS, B.LAPIS_BLOCK);
  block9(B.EMERALD_BLOCK, I.EMERALD); unblock(I.EMERALD, B.EMERALD_BLOCK);
  shaped(B.STONE_BRICKS, 4, 'SS ', 'SS ', '   ', { S: B.STONE });
  shaped(B.END_STONE_BRICKS, 4, 'EE ', 'EE ', '   ', { E: B.END_STONE });
  shaped(B.PURPUR, 4, 'FF ', 'FF ', '   ', { F: I.POPPED_CHORUS });
  shaped(B.PURPUR_PILLAR, 2, 'P  ', 'P  ', '   ', { P: B.PURPUR });
  shaped(B.END_ROD, 4, 'B  ', 'S  ', '   ', { B: I.BLAZE_ROD, S: I.POPPED_CHORUS });
  shaped(B.END_CRYSTAL, 1, 'GGG', 'GDG', 'GTG', { G: B.GLASS, D: I.DIAMOND, T: I.GHAST_TEAR });
  shaped(B.END_PORTAL_FRAME, 1, 'OOO', 'OEO', 'OOO', { O: B.OBSIDIAN, E: I.ENDER_PEARL });
  shaped(B.OBSIDIAN_PILLAR, 1, 'E  ', 'O  ', '   ', { E: B.END_STONE, O: B.OBSIDIAN });
  shaped(B.CHORUS_PLANT, 1, 'F  ', 'E  ', '   ', { F: I.CHORUS_FRUIT, E: B.END_STONE });
  shaped(B.CHORUS_FLOWER, 1, 'F  ', 'P  ', '   ', { F: I.CHORUS_FRUIT, P: B.CHORUS_PLANT });
  shaped(B.BOOKSHELF, 1, 'PPP', 'BBB', 'PPP', { P: B.PLANK, B: I.BOOK });
  shaped(B.ENCHANTING, 1, ' B ', 'DOD', 'OOO', { B: I.BOOK, D: I.DIAMOND, O: B.OBSIDIAN });
  shaped(B.BREWING, 1, ' S ', 'CCC', '   ', { S: I.BLAZE_ROD, C: B.COBBLE });
  shaped(B.SMITHING, 1, 'II ', 'PP ', 'PP ', { I: I.IRON_INGOT, P: B.PLANK });
  shaped(B.BARREL, 1, 'PSP', 'P P', 'PSP', { P: B.PLANK, S: I.STICK });
  shaped(B.CAMPFIRE, 1, ' S ', 'SLS', 'PPP', { S: I.STICK, L: I.COAL, P: B.PLANK });
  shaped(B.LANTERN, 1, 'SSS', 'SLS', 'SSS', { S: I.IRON_INGOT, L: B.TORCH });
  shaped(B.SCAFFOLDING, 6, 'B B', 'SSS', 'B B', { B: B.BAMBOO, S: I.STRING });
  shaped(B.CANDLE, 1, 'S  ', 'H  ', '   ', { S: I.STRING, H: I.HONEYCOMB });
  shaped(B.BRICKS, 1, 'BB ', 'BB ', '   ', { B: I.BRICK });
  shaped(B.SULFUR, 1, 'SS ', 'SS ', '   ', { S: I.RAW_SULFUR });
  shaped(B.CINNABAR, 1, 'CC ', 'CC ', '   ', { C: I.CINNABAR_DUST });
  shaped(B.POTENT_SULFUR, 1, 'SSS', 'SCS', 'SSS', { S: I.RAW_SULFUR, C: I.CINNABAR_DUST });
  shaped(B.GOLD_DANDELION, 1, ' G ', ' S ', '   ', { G: I.GOLD_DANDELION_ITEM, S: I.STICK });
  shaped(I.BRICK, 1, 'C  ', '   ', '   ', { C: B.CLAY });
  shapeless(I.BOOK, 1, [[I.PAPER, 3], [I.LEATHER, 1]]);
  shapeless(I.PAPER, 3, [[B.SUGAR_CANE, 3]]);
  shapeless(I.POTION, 1, [[I.GLOW_BERRIES, 1], [I.AMETHYST_SHARD, 1]]);
  // 药水：发光浆果 + 对应材料（无序）
  shapeless(I.POTION_HEAL, 1, [[I.GLOW_BERRIES, 1], [I.GOLDEN_APPLE, 1]]);
  shapeless(I.POTION_SPEED, 1, [[I.GLOW_BERRIES, 1], [B.SUGAR_CANE, 1]]);
  shapeless(I.POTION_JUMP, 1, [[I.GLOW_BERRIES, 1], [B.HAY, 1]]);
  shapeless(I.POTION_NIGHT, 1, [[I.GLOW_BERRIES, 1], [B.GLOWSTONE, 1]]);
  shapeless(I.POTION_FIRE, 1, [[I.GLOW_BERRIES, 1], [B.MAGMA, 1]]);
  shapeless(I.POTION_REGEN, 1, [[I.GLOW_BERRIES, 1], [I.GHAST_TEAR, 1]]);
  shapeless(I.POTION_POISON, 1, [[I.GLOW_BERRIES, 1], [B.SULFUR, 1]]);
  shapeless(I.POTION_SLOW, 1, [[I.GLOW_BERRIES, 1], [B.SOUL_SAND, 1]]);
  shapeless(I.POPPED_CHORUS, 1, [[I.CHORUS_FRUIT, 1], [I.COAL, 1]]);
  shapeless(I.NETHERITE_INGOT, 1, [[I.NETHERITE_SCRAP, 4], [I.GOLD_INGOT, 4]]);
  shapeless(I.BREAD, 1, [[B.HAY, 1], [I.COAL, 1]]);
  shapeless(B.MUD, 1, [[B.DIRT, 1], [I.WATER_BUCKET, 1]]);
  shapeless(B.CHERRY_PLANK, 4, [[B.CHERRY_LOG, 1]]);
  shapeless(I.GOLD_DANDELION_ITEM, 1, [[B.GOLD_DANDELION, 1]]);

  // 熔炉：输入 -> 输出（燃料另算）
  const SMELT = {};
  SMELT[B.LOG] = [I.COAL, 1];
  SMELT[B.COBBLE] = [B.STONE, 1];
  SMELT[I.RAW_IRON] = [I.IRON_INGOT, 1];
  SMELT[I.RAW_GOLD] = [I.GOLD_INGOT, 1];
  SMELT[I.RAW_COPPER] = [I.COPPER_INGOT, 1];
  SMELT[B.SAND] = [B.GLASS, 1];
  SMELT[B.CLAY] = [I.BRICK, 1];
  SMELT[B.NETHERRACK] = [I.BRICK, 1];
  SMELT[B.CHORUS_FRUIT] = [I.POPPED_CHORUS, 1];
  SMELT[B.SULFUR] = [I.RAW_SULFUR, 2];
  SMELT[B.CINNABAR] = [I.CINNABAR_DUST, 2];
  SMELT[B.ANCIENT_DEBRIS] = [I.NETHERITE_SCRAP, 1];
  SMELT[B.PALE_LOG] = [I.COAL, 1];
  SMELT[B.CHERRY_LOG] = [I.COAL, 1];

  /* ---------------- 合成匹配（纯函数） ---------------- */
  // grid: 长度 size*size 的数组，元素为 id（0 表示空）
  function craftMatch(grid, size) {
    const n = size * size;
    if (!grid || grid.length !== n) return null;
    const cells = Array.from(grid, v => v | 0);
    for (const r of RECIPES) {
      if (r.type === 'shaped') {
        const pat = r.pattern.join('');
        const map = {};
        for (let p = 0; p < 9; p++) {
          const ch = pat[p];
          if (ch !== ' ' && !(ch in map)) map[ch] = r.key[ch];
        }
        // 配方包围盒
        let minX = 3, maxX = -1, minY = 3, maxY = -1;
        for (let p = 0; p < 9; p++) {
          if (pat[p] === ' ') continue;
          const px = p % 3, py = (p / 3) | 0;
          if (px < minX) minX = px; if (px > maxX) maxX = px;
          if (py < minY) minY = py; if (py > maxY) maxY = py;
        }
        const pw = maxX - minX + 1, ph = maxY - minY + 1;
        if (pw > size || ph > size) continue;
        for (let oy = 0; oy <= size - ph; oy++) {
          for (let ox = 0; ox <= size - pw; ox++) {
            let ok = true;
            for (let y = 0; y < size && ok; y++) {
              for (let x = 0; x < size && ok; x++) {
                const needChar = (x - ox + minX >= 0 && x - ox + minX < 3 &&
                                  y - oy + minY >= 0 && y - oy + minY < 3)
                  ? pat[(y - oy + minY) * 3 + (x - ox + minX)] : ' ';
                const want = needChar === ' ' ? 0 : (map[needChar] | 0);
                if ((cells[y * size + x] | 0) !== want) ok = false;
              }
            }
            if (ok) return { out: r.out, count: r.count, recipe: r };
          }
        }
      } else {
        // 无序：材料多重集合必须完全一致
        const have = {};
        let cnt = 0;
        for (const v of cells) if (v) { have[v] = (have[v] || 0) + 1; cnt++; }
        const need = {};
        let needCnt = 0;
        for (const [id, c] of r.need) { need[id] = (need[id] || 0) + c; needCnt += c; }
        if (cnt !== needCnt) continue;
        let ok = true;
        for (const k in need) if ((have[k] || 0) !== need[k]) { ok = false; break; }
        for (const k in have) if (!(k in need)) { ok = false; break; }
        if (ok) return { out: r.out, count: r.count, recipe: r };
      }
    }
    return null;
  }

  // 2x2 界面只允许能放进 2x2 的配方
  function craftMatch2(grid) { return craftMatch(grid, 2); }
  function craftMatch3(grid) { return craftMatch(grid, 3); }

  /* ---------------- 配方书支持（纯函数） ---------------- */
  // 有序配方包围盒；无序配方返回需要占用的格子数
  function recipeShape(r) {
    if (r.type === 'shapeless') {
      let n = 0;
      for (const [, c] of r.need) n += c;
      return { cells: n, w: Math.min(3, Math.ceil(Math.sqrt(n))), h: 1 };
    }
    const pat = r.pattern.join('');
    let minX = 3, maxX = -1, minY = 3, maxY = -1;
    for (let p = 0; p < 9; p++) {
      if (pat[p] === ' ') continue;
      const px = p % 3, py = (p / 3) | 0;
      if (px < minX) minX = px; if (px > maxX) maxX = px;
      if (py < minY) minY = py; if (py > maxY) maxY = py;
    }
    return { w: maxX - minX + 1, h: maxY - minY + 1,
             minX, minY, pat, cells: 0 };
  }
  function fitsSize(r, size) {
    const s = recipeShape(r);
    if (r.type === 'shapeless') return s.cells <= size * size;
    return s.w <= size && s.h <= size;
  }
  // 配方需要的材料多重集合
  function recipeNeeds(r) {
    const need = {};
    if (r.type === 'shapeless') {
      for (const [id, c] of r.need) need[id] = (need[id] || 0) + c;
    } else {
      const pat = r.pattern.join('');
      for (let p = 0; p < 9; p++) {
        const ch = pat[p];
        if (ch === ' ') continue;
        const id = r.key[ch] | 0;
        if (id) need[id] = (need[id] || 0) + 1;
      }
    }
    return need;
  }
  function canCraft(r, inv, size) {
    if (!fitsSize(r, size)) return false;
    const need = recipeNeeds(r);
    for (const k in need) if (inv.countOf(Number(k)) < need[k]) return false;
    return true;
  }
  // 把配方图案摆进网格（左上对齐，其余留空）
  function applyRecipe(r, size) {
    const cells = new Array(size * size).fill(0);
    if (r.type === 'shapeless') {
      let i = 0;
      for (const [id, c] of r.need)
        for (let k = 0; k < c && i < cells.length; k++, i++) cells[i] = id;
    } else {
      const s = recipeShape(r);
      for (let y = 0; y < s.h && y < size; y++)
        for (let x = 0; x < s.w && x < size; x++) {
          const ch = s.pat[(y + s.minY) * 3 + (x + s.minX)];
          if (ch && ch !== ' ') cells[y * size + x] = r.key[ch] | 0;
        }
    }
    return cells;
  }

  const isBlock = id => id > 0 && id < 200;
  const defOf = id => D[id] || D[B.AIR];
  const nameOf = id => (D[id] ? D[id].cn : '未知');
  const stackable = id => id !== 0;

  DMC.DEF = D;
  DMC.Items = { RECIPES, SMELT, craftMatch, craftMatch2, craftMatch3,
                recipeShape, fitsSize, recipeNeeds, canCraft, applyRecipe,
                defOf, nameOf, isBlock, stackable, shaped, shapeless };
})(typeof globalThis !== 'undefined' ? globalThis : this);
