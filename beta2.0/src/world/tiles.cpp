// ===================== 方块 / 物品数据表实现 =====================
// DIG / SOLID / LV 三个数组逐值照搬旧版 beta2.0.cpp 的
// blockwj[] / blocktouch[] / blocklv[]（含数组长度不足 150 时补 0 的隐式行为），
// 保证挖掘手感、碰撞与镐等级要求和旧版完全一致。
// VIS 表是新增的：决定图形化渲染时每个 id 长什么样。
#include "tiles.h"

static const i32 DIG[150] = {
    0, 200, 200, 400, 600, 100, 0, 0, 0, 200, -1, 0,
    800, 0, 0, 0, 5, -1, 0, -1, -1, -1, -1, 12,
    -1, 0, 0, 0, 0, 0, 0, -1, -1, -1, 0, 0,
    4000, 0, -1, 150, 400, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 120, 180, 0, 0, 0, 0, 0,
    0, 0, 600, 300, 0, 0, 100, 100, 2000, 1200,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 300,
};

static const u8 SOLID[150] = {
    0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 0, 0, 2, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1,
};

static const i32 LV[150] = {
    0, 2, 1, 2, 3, 1, 0, 0, 0, 2, 114, 0, 4, 0, 0,
    0, 1, 0, 0, 10, 10, 10, 10, 15, -1, 0, 0, 0, 0, 0,
    0, 0, 10, 10, 10, 0, 0, 5, 0, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 4, 2, 0, 0, 1, 1, 5, 5,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 2,
};

// 视觉定义：name 英文名 / cn 中文名 / style 生成样式 / base 主色 / accent 辅色
struct Vis
{
    const char *name;
    const char *cn;
    u8 style;
    Color base;
    Color accent;
};

#define C(r, g, b) col3(r, g, b)

static const Vis VIS[150] = {
    /*  0 air            */ {"air", "空气", TS_EMPTY, C(0, 0, 0), C(0, 0, 0)},
    /*  1 stone          */ {"stone", "圆石", TS_ROCK, C(132, 132, 136), C(98, 98, 104)},
    /*  2 wood           */ {"wood", "原木", TS_LOG, C(122, 88, 50), C(84, 58, 32)},
    /*  3 coal           */ {"coal", "煤炭", TS_ORE, C(128, 128, 132), C(40, 40, 44)},
    /*  4 raw_iron       */ {"raw_iron", "粗铁", TS_ORE, C(128, 128, 132), C(198, 152, 118)},
    /*  5 dirt           */ {"dirt", "泥土", TS_DIRT, C(124, 90, 58), C(94, 66, 40)},
    /*  6 stick          */ {"stick", "木棍", TS_I_ROD, C(150, 110, 60), C(96, 68, 34)},
    /*  7 wood_pickaxe   */ {"wood_pickaxe", "木镐", TS_I_PICK, C(168, 124, 66), C(120, 86, 44)},
    /*  8 stone_pickaxe  */ {"stone_pickaxe", "石镐", TS_I_PICK, C(150, 150, 154), C(104, 104, 110)},
    /*  9 furnace        */ {"furnace", "熔炉", TS_FURNACE, C(120, 120, 124), C(60, 56, 56)},
    /* 10 bedrock/wall   */ {"bedrock", "基岩", TS_BRICK, C(64, 64, 74), C(42, 42, 52)},
    /* 11 sky            */ {"sky", "天空", TS_GLASS, C(140, 200, 245), C(180, 225, 255)},
    /* 12 diamond        */ {"diamond", "钻石", TS_I_GEM, C(120, 235, 235), C(70, 180, 190)},
    /* 13 iron_ingot     */ {"iron_ingot", "铁锭", TS_I_INGOT, C(216, 216, 220), C(150, 150, 158)},
    /* 14 iron_pickaxe   */ {"iron_pickaxe", "铁镐", TS_I_PICK, C(220, 220, 226), C(150, 150, 158)},
    /* 15 diamond_pickaxe*/ {"diamond_pickaxe", "钻石镐", TS_I_PICK, C(120, 235, 235), C(70, 180, 190)},
    /* 16 torch          */ {"torch", "火把", TS_TORCH, C(255, 196, 92), C(128, 84, 34)},
    /* 17 stone_sword    */ {"stone_sword", "石剑", TS_I_SWORD, C(160, 160, 164), C(104, 104, 110)},
    /* 18 stone_axe      */ {"stone_axe", "石斧", TS_I_AXE, C(160, 160, 164), C(104, 104, 110)},
    /* 19 water          */ {"water", "静态水", TS_WATER, C(58, 108, 200), C(96, 156, 238)},
    /* 20 flowing_water_3*/ {"flowing_water_3", "流动水3", TS_WATER, C(62, 116, 206), C(100, 162, 240)},
    /* 21 flowing_water_2*/ {"flowing_water_2", "流动水2", TS_WATER, C(66, 124, 212), C(108, 170, 242)},
    /* 22 flowing_water_1*/ {"flowing_water_1", "流动水1", TS_WATER, C(70, 132, 218), C(116, 178, 245)},
    /* 23 leaf           */ {"leaf", "树叶", TS_LEAVES, C(58, 132, 52), C(96, 176, 78)},
    /* 24 health_potion  */ {"health_potion", "回血药水", TS_I_POTION, C(228, 70, 82), C(255, 210, 214)},
    /* 25 bucket         */ {"bucket", "桶", TS_I_BUCKET, C(190, 190, 198), C(130, 130, 140)},
    /* 26 water_bucket   */ {"water_bucket", "水桶", TS_I_BUCKET, C(190, 190, 198), C(72, 130, 220)},
    /* 27 iron_chestplate*/ {"iron_chestplate", "铁胸甲", TS_I_ARMOR, C(214, 214, 220), C(146, 146, 154)},
    /* 28 iron_leggings  */ {"iron_leggings", "铁护腿", TS_I_LEGS, C(214, 214, 220), C(146, 146, 154)},
    /* 29 iron_helmet    */ {"iron_helmet", "铁头盔", TS_I_HELM, C(214, 214, 220), C(146, 146, 154)},
    /* 30 iron_boots     */ {"iron_boots", "铁靴子", TS_I_BOOT, C(214, 214, 220), C(146, 146, 154)},
    /* 31 shield         */ {"shield", "盾牌", TS_I_SHIELD, C(160, 118, 62), C(206, 206, 214)},
    /* 32 lava           */ {"lava", "岩浆", TS_LAVA, C(226, 106, 26), C(255, 196, 72)},
    /* 33 flowing_lava_2 */ {"flowing_lava_2", "流动岩浆2", TS_LAVA, C(216, 96, 22), C(250, 176, 62)},
    /* 34 flowing_lava_1 */ {"flowing_lava_1", "流动岩浆1", TS_LAVA, C(206, 86, 20), C(244, 158, 54)},
    /* 35 lava_bucket    */ {"lava_bucket", "岩浆桶", TS_I_BUCKET, C(190, 190, 198), C(226, 106, 26)},
    /* 36 obsidian       */ {"obsidian", "黑曜石", TS_ROCK, C(40, 32, 62), C(76, 58, 118)},
    /* 37 rotten_flesh   */ {"rotten_flesh", "腐肉", TS_I_FOOD, C(126, 92, 74), C(88, 62, 52)},
    /* 38 nether_portal  */ {"nether_portal", "下界传送门", TS_PORTAL, C(126, 46, 190), C(232, 176, 255)},
    /* 39 netherrack     */ {"netherrack", "下界岩", TS_NETHERRACK, C(114, 42, 42), C(78, 26, 28)},
    /* 40 chest          */ {"chest", "箱子", TS_CHEST, C(170, 116, 56), C(96, 62, 26)},
    /* 41 bow            */ {"bow", "木弓", TS_I_BOW, C(150, 110, 60), C(226, 226, 226)},
    /* 42 string         */ {"string", "线", TS_I_ROD, C(226, 226, 226), C(170, 170, 176)},
    /* 43 gunpowder      */ {"gunpowder", "火药", TS_I_BALL, C(72, 72, 78), C(140, 140, 148)},
    /* 44 wood_arrow     */ {"wood_arrow", "木箭", TS_I_ARROW, C(150, 110, 60), C(214, 214, 220)},
    /* 45 iron_arrow     */ {"iron_arrow", "铁箭", TS_I_ARROW, C(206, 206, 212), C(150, 110, 60)},
    /* 46 explosive_arrow*/ {"explosive_arrow", "爆炸箭", TS_I_ARROW, C(226, 96, 40), C(255, 210, 120)},
    /* 47 iron_sword     */ {"iron_sword", "铁剑", TS_I_SWORD, C(220, 220, 226), C(146, 146, 154)},
    /* 48 iron_axe       */ {"iron_axe", "铁斧", TS_I_AXE, C(220, 220, 226), C(146, 146, 154)},
    /* 49 diamond_sword  */ {"diamond_sword", "钻石剑", TS_I_SWORD, C(120, 235, 235), C(70, 180, 190)},
    /* 50 diamond_axe    */ {"diamond_axe", "钻石斧", TS_I_AXE, C(120, 235, 235), C(70, 180, 190)},
    /* 51 oi_core        */ {"oi_core", "OI核心", TS_I_BALL, C(226, 72, 226), C(255, 190, 255)},
    /* 52 recursive      */ {"recursive", "递归", TS_I_BALL, C(96, 226, 160), C(220, 255, 236)},
    /* 53 snow           */ {"snow", "雪", TS_SNOW, C(238, 244, 250), C(206, 218, 234)},
    /* 54 ice            */ {"ice", "冰", TS_ICE, C(152, 206, 240), C(214, 240, 255)},
    /* 55 apple          */ {"apple", "苹果", TS_I_FOOD, C(206, 54, 54), C(96, 168, 66)},
    /* 56 diamond_helmet */ {"diamond_helmet", "钻石头盔", TS_I_HELM, C(120, 235, 235), C(70, 180, 190)},
    /* 57 diamond_chestplate*/{"diamond_chestplate", "钻石胸甲", TS_I_ARMOR, C(120, 235, 235), C(70, 180, 190)},
    /* 58 diamond_leggings*/{"diamond_leggings", "钻石护腿", TS_I_LEGS, C(120, 235, 235), C(70, 180, 190)},
    /* 59 diamond_boots  */ {"diamond_boots", "钻石靴子", TS_I_BOOT, C(120, 235, 235), C(70, 180, 190)},
    /* 60 oi_sword       */ {"oi_sword", "OI剑", TS_I_SWORD, C(226, 72, 226), C(255, 200, 255)},
    /* 61 storm_bow      */ {"storm_bow", "风暴弓", TS_I_BOW, C(96, 152, 226), C(220, 240, 255)},
    /* 62 raw_gold       */ {"raw_gold", "粗金", TS_ORE, C(128, 128, 132), C(226, 186, 74)},
    /* 63 nether_gold_ore*/ {"nether_gold_ore", "下界金矿", TS_ORE, C(114, 42, 42), C(238, 196, 84)},
    /* 64 gold_ingot     */ {"gold_ingot", "金锭", TS_I_INGOT, C(240, 204, 84), C(186, 148, 42)},
    /* 65 gold_nugget    */ {"gold_nugget", "金粒", TS_I_GEM, C(240, 204, 84), C(186, 148, 42)},
    /* 66 platforms      */ {"platforms", "木平台", TS_PLATFORM, C(160, 116, 58), C(110, 78, 36)},
    /* 67 planks         */ {"planks", "木板", TS_PLANK, C(166, 122, 62), C(116, 84, 40)},
    /* 68 ebonstone      */ {"ebonstone", "腐化石块", TS_ROCK, C(96, 74, 108), C(64, 48, 76)},
    /* 69 ancient_debris */ {"ancient_debris", "远古残骸", TS_ROCK, C(94, 66, 54), C(150, 106, 74)},
    /* 70 netherite_scrap*/ {"netherite_scrap", "下界碎片", TS_I_SCRAP, C(96, 66, 58), C(56, 38, 34)},
    /* 71 netherite_ingot*/ {"netherite_ingot", "下界锭", TS_I_INGOT, C(88, 70, 74), C(52, 42, 46)},
    /* 72 netherite_pick */ {"netherite_pickaxe", "下界镐", TS_I_PICK, C(88, 70, 74), C(52, 42, 46)},
    /* 73 netherite_sword*/ {"netherite_sword", "下界剑", TS_I_SWORD, C(88, 70, 74), C(52, 42, 46)},
    /* 74 netherite_axe  */ {"netherite_axe", "下界斧", TS_I_AXE, C(88, 70, 74), C(52, 42, 46)},
    /* 75 worm_food      */ {"worm_food", "蠕虫诱饵", TS_I_EGG, C(150, 96, 160), C(228, 200, 240)},
    /* 76 gold_pickaxe   */ {"gold_pickaxe", "金镐", TS_I_PICK, C(240, 204, 84), C(186, 148, 42)},
    /* 77 gold_sword     */ {"gold_sword", "金剑", TS_I_SWORD, C(240, 204, 84), C(186, 148, 42)},
    /* 78 gold_axe       */ {"gold_axe", "金斧", TS_I_AXE, C(240, 204, 84), C(186, 148, 42)},
    /* 79 gold_helmet    */ {"gold_helmet", "金头盔", TS_I_HELM, C(240, 204, 84), C(186, 148, 42)},
    /* 80 gold_chestplate*/ {"gold_chestplate", "金胸甲", TS_I_ARMOR, C(240, 204, 84), C(186, 148, 42)},
    /* 81 gold_leggings  */ {"gold_leggings", "金护腿", TS_I_LEGS, C(240, 204, 84), C(186, 148, 42)},
    /* 82 gold_boots     */ {"gold_boots", "金靴子", TS_I_BOOT, C(240, 204, 84), C(186, 148, 42)},
    /* 83 netherite_helmet*/{"netherite_helmet", "下界头盔", TS_I_HELM, C(88, 70, 74), C(52, 42, 46)},
    /* 84 netherite_chestplate*/{"netherite_chestplate", "下界胸甲", TS_I_ARMOR, C(88, 70, 74), C(52, 42, 46)},
    /* 85 netherite_leggings*/{"netherite_leggings", "下界护腿", TS_I_LEGS, C(88, 70, 74), C(52, 42, 46)},
    /* 86 netherite_boots*/ {"netherite_boots", "下界靴子", TS_I_BOOT, C(88, 70, 74), C(52, 42, 46)},
    /* 87 wool           */ {"wool", "羊毛", TS_WOOL, C(232, 232, 228), C(200, 200, 196)},
    /* 88 mutton         */ {"mutton", "羊肉", TS_I_FOOD, C(196, 108, 100), C(140, 70, 66)},
    /* 89 pork           */ {"pork", "猪肉", TS_I_FOOD, C(226, 146, 138), C(160, 92, 86)},
    /* 90 bed            */ {"bed", "床", TS_BED, C(190, 54, 54), C(232, 232, 228)},
    /* 91 golden_apple   */ {"golden_apple", "金苹果", TS_I_FOOD, C(240, 204, 84), C(96, 168, 66)},
    /* 92 ebonwall       */ {"ebonwall", "腐化瘀墙", TS_BRICK, C(78, 58, 88), C(52, 38, 60)},
    /* 93 level_portal   */ {"level_portal", "关卡传送门", TS_PORTAL, C(170, 80, 230), C(244, 204, 255)},
    /* 94 energy_potion  */ {"energy_potion", "能量药水", TS_I_POTION, C(70, 140, 232), C(206, 232, 255)},
    /* 95 pistol         */ {"pistol", "手枪", TS_I_GUN, C(120, 124, 132), C(72, 74, 80)},
    /* 96 smg            */ {"smg", "冲锋枪", TS_I_GUN, C(108, 112, 120), C(64, 66, 72)},
    /* 97 shotgun        */ {"shotgun", "霰弹枪", TS_I_GUN, C(136, 116, 88), C(80, 66, 48)},
    /* 98 sniper         */ {"sniper", "狙击枪", TS_I_GUN, C(96, 104, 92), C(54, 60, 52)},
    /* 99 bullet         */ {"bullet", "子弹", TS_I_BALL, C(226, 186, 84), C(140, 112, 44)},
    /*100 big_health_potion*/{"big_health_potion", "大回血药", TS_I_POTION, C(232, 54, 68), C(255, 206, 210)},
    /*101 coin           */ {"coin", "金币", TS_I_COIN, C(244, 208, 84), C(186, 146, 40)},
    /*102 gacha_machine  */ {"gacha_machine", "扭蛋机", TS_MACHINE, C(196, 72, 140), C(240, 220, 120)},
    /*103 level_gold_ore */ {"level_gold_ore", "关卡金矿", TS_ORE, C(128, 128, 132), C(248, 214, 96)},
    /*104 smithing_table */ {"smithing_table", "锻造台", TS_TABLE, C(112, 78, 50), C(66, 46, 28)},
    /*105 shadow_scale   */ {"shadow_scale", "暗影鳞片", TS_I_SCALE, C(72, 62, 92), C(140, 122, 176)},
    /*106 shadow_helmet  */ {"shadow_helmet", "暗影头盔", TS_I_HELM, C(84, 72, 110), C(146, 126, 186)},
    /*107 shadow_chestplate*/{"shadow_chestplate", "暗影胸甲", TS_I_ARMOR, C(84, 72, 110), C(146, 126, 186)},
    /*108 shadow_leggings*/ {"shadow_leggings", "暗影护腿", TS_I_LEGS, C(84, 72, 110), C(146, 126, 186)},
    /*109 shadow_boots   */ {"shadow_boots", "暗影靴子", TS_I_BOOT, C(84, 72, 110), C(146, 126, 186)},
    /*110 Light's_Bane   */ {"Light's_Bane", "魔光剑", TS_I_SWORD, C(150, 226, 236), C(76, 176, 196)},
    /*111 demon_bow      */ {"demon_bow", "恶魔弓", TS_I_BOW, C(96, 44, 60), C(226, 96, 84)},
    /*112 magiluminescence*/{"magiluminescence", "魔光护符", TS_I_AMULET, C(236, 208, 96), C(150, 220, 236)},
    /*113 armor_potion   */ {"armor_potion", "护甲药水", TS_I_POTION, C(96, 200, 214), C(220, 248, 252)},
};

#undef C

const char *g_ent_cn[24] = {
    "僵尸", "箭", "骷髅", "苦力怕", "OI", "蜘蛛", "僵尸猪灵", "递归",
    "世界吞噬者-头部", "世界吞噬者-体节", "世界吞噬者-尾部", "羊", "猪", "",
    "商人", "工匠", "占星师", "雇佣兵", "牧师", "地牢商人", "被关押NPC",
    "奸商", "小偷猫", "传送门",
};

// 组装最终表：启动时显式填满 150 项，避免依赖隐式补 0
TileDef g_tiles[150];

void tiles_init()
{
    for(int i = 0; i < 150; i++)
    {
        TileDef &d = g_tiles[i];
        d.name = VIS[i].name;
        d.cn = VIS[i].cn;
        d.solid = SOLID[i] != 0;
        d.dig = DIG[i];
        d.level = LV[i];
        d.style = VIS[i].style;
        d.base = VIS[i].base;
        d.accent = VIS[i].accent;
    }
}