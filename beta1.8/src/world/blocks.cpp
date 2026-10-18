// ===================== 102 项方块/物品数据表实现 =====================
#include "blocks.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

BlockDef g_blocks[BLOCK_COUNT];

// ---------------- 色板 ----------------
namespace {
constexpr Color C(u8 r, u8 g, u8 b, u8 a = 255) { return rgba(r, g, b, a); }

// 材质主色（工具/护甲/锭共用，改这里即整套换色）
constexpr Color MW = C(150, 108,  62);   // 木
constexpr Color MS = C(130, 130, 134);   // 石
constexpr Color MI = C(206, 206, 212);   // 铁
constexpr Color MG = C(252, 214,  86);   // 金
constexpr Color MD = C( 94, 232, 222);   // 钻石
constexpr Color MN = C( 92,  80,  96);   // 下界
constexpr Color MH = C(122,  82, 178);   // 暗影
constexpr Color MX = C(120, 200, 255);   // 特殊

// 工具耐久（MC 风格）
// 工具耐久。比 MC 原版高一些：本作矿物硬度（钻石矿 800）远高于 MC，
// 按原版数值会修不过来、也换得太勤。
constexpr f32 DUR_WOOD = 120, DUR_STONE = 240, DUR_GOLD = 70, DUR_IRON = 480,
              DUR_DIA = 1600, DUR_NETH = 2200, DUR_SHADOW = 2600;
// 护甲耐久：索引 0=头盔 1=胸甲 2=护腿 3=靴
constexpr f32 AD_GOLD[4] = {150, 220, 205, 175};
constexpr f32 AD_IRON[4] = {330, 480, 450, 390};
constexpr f32 AD_DIA[4]  = {720, 1050, 990, 860};
constexpr f32 AD_NETH[4] = {810, 1180, 1110, 960};
constexpr f32 AD_SHA[4]  = {920, 1320, 1240, 1080};

// 护甲 id -> AD_* 表的索引
constexpr i32 AI_HELM = 0, AI_CHEST = 1, AI_LEGS = 2, AI_BOOT = 3;
}   // namespace

// ---------------- 数据表 ----------------
// 顺序 = 旧版 test[] 的索引，绝不可重排。
// dig / touch 为旧版 blockwj[] / blocktouch[] 的**实测值**（见 blocks.h 说明），
// level 由旧版 blocklv[] 换算（1=木 2=石 3=铁 4=钻石 5=下界 6=金）。
static const BlockDef TBL[BLOCK_COUNT] = {
//  英文名              中文名        dig  tch lv  样式           材质    主色                    辅色                    耐久        工具 武器 护甲 部位     可放置 发光
  { "air",            "空气",          0, 0,  0, TS_EMPTY,      MAT_NONE, C(  0,  0,  0,  0), C(  0,  0,  0,  0),    0, 0,0,0, AS_NONE,  0, 0 },
  { "stone",          "圆石",        200, 1,  2, TS_ROCK,       MAT_NONE, C(124,128,136),    C( 86, 90, 99),        0, 0,0,0, AS_NONE,  1, 0 },
  { "wood",           "原木",        200, 0,  1, TS_LOG,        MAT_NONE, C(124, 88, 50),    C( 86, 60, 34),        0, 0,0,0, AS_NONE,  1, 0 },
  { "coal",           "煤炭",        400, 1,  2, TS_ORE,        MAT_NONE, C( 31, 31, 36),    C( 16, 16, 20),        0, 0,0,0, AS_NONE,  1, 0 },
  { "raw_iron",       "粗铁",        600, 1,  3, TS_ORE,        MAT_NONE, C(191,144,118),    C(140, 98, 76),        0, 0,0,0, AS_NONE,  1, 0 },
  { "dirt",           "泥土",        100, 1,  1, TS_DIRT,       MAT_NONE, C(120, 84, 54),    C( 92, 62, 38),        0, 0,0,0, AS_NONE,  1, 0 },
  { "stick",          "木棍",          0, 0,  0, TS_I_STICK,    MAT_WOOD, MW,                C(104, 72, 40),        0, 0,0,0, AS_NONE,  1, 0 },
  { "wood_pickaxe",   "木镐",          0, 0,  0, TS_I_PICK,     MAT_WOOD, MW,                C(104, 72, 40), DUR_WOOD,  1,0,0, AS_NONE,  1, 0 },
  { "stone_pickaxe",  "石镐",          0, 0,  0, TS_I_PICK,     MAT_STONE,MS,                C( 92, 92, 96), DUR_STONE, 1,0,0, AS_NONE,  1, 0 },
  { "furnace",        "熔炉",        200, 0,  2, TS_FURNACE,    MAT_NONE, C(122,122,126),    C( 58, 58, 62),        0, 0,0,0, AS_NONE,  1, 0 },
  { "bedrock",        "基岩",         -1, 1,  0, TS_ROCK,       MAT_NONE, C( 54, 54, 58),    C( 30, 30, 34),        0, 0,0,0, AS_NONE,  1, 0 },
  { "sky",            "天空",          0, 0,  0, TS_EMPTY,      MAT_NONE, C(  0,  0,  0,  0), C(  0,  0,  0,  0),    0, 0,0,0, AS_NONE,  0, 0 },
  { "diamond",        "钻石",        800, 1,  4, TS_GEM,        MAT_NONE, C( 94,232,222),    C(255,255,255),        0, 0,0,0, AS_NONE,  1, 0 },
  { "iron_ingot",     "铁锭",          0, 0,  0, TS_I_INGOT,    MAT_IRON, MI,                C(140,140,146),        0, 0,0,0, AS_NONE,  1, 0 },
  { "iron_pickaxe",   "铁镐",          0, 0,  0, TS_I_PICK,     MAT_IRON, MI,                C(146,146,152), DUR_IRON,  1,0,0, AS_NONE,  1, 0 },
  { "diamond_pickaxe","钻石镐",        0, 0,  0, TS_I_PICK,     MAT_DIAMOND,MD,              C( 40,160,154), DUR_DIA,   1,0,0, AS_NONE,  1, 0 },
  { "torch",          "火把",          5, 0,  1, TS_TORCH,      MAT_NONE, C(255,196, 84),    C(120, 78, 40),        0, 0,0,0, AS_NONE,  1, 0 },
  { "stone_sword",    "石剑",         -1, 0,  0, TS_I_SWORD,    MAT_STONE,MS,                C( 92, 92, 96), DUR_STONE, 0,1,0, AS_NONE,  0, 0 },
  { "stone_axe",      "石斧",          0, 1,  0, TS_I_AXE,      MAT_STONE,MS,                C( 92, 92, 96), DUR_STONE, 1,0,0, AS_NONE,  1, 0 },
  { "water",          "静态水",       -1, 0, 10, TS_WATER,      MAT_NONE, C( 56,110,212,190),C(150,200,255),        0, 0,0,0, AS_NONE,  0, 0 },
  { "flowing_water_3","流动水3/4",    -1, 0, 10, TS_FLUID_PART, MAT_NONE, C( 56,110,212,190),C(150,200,255),        0, 0,0,0, AS_NONE,  0, 0 },
  { "flowing_water_2","流动水2/4",    -1, 0, 10, TS_FLUID_PART, MAT_NONE, C( 56,110,212,190),C(150,200,255),        0, 0,0,0, AS_NONE,  0, 0 },
  { "flowing_water_1","流动水1/4",    -1, 0, 10, TS_FLUID_PART, MAT_NONE, C( 56,110,212,190),C(150,200,255),        0, 0,0,0, AS_NONE,  0, 0 },
  { "leaf",           "树叶",         12, 0, 15, TS_LEAVES,     MAT_NONE, C( 62,132, 50),    C( 38, 92, 34),        0, 0,0,0, AS_NONE,  1, 0 },
  { "health_potion",  "回血药水",     -1, 0, -1, TS_POTION,     MAT_NONE, C(232, 72, 92),    C(255,190,200),        0, 0,0,0, AS_NONE,  0, 0 },
  { "bucket",         "桶",            0, 0,  0, TS_I_BUCKET,   MAT_IRON, MI,                C(120,120,126),        0, 0,0,0, AS_NONE,  0, 0 },
  { "water_bucket",   "水桶",          0, 0,  0, TS_I_BUCKET,   MAT_IRON, MI,                C( 56,110,212),        0, 0,0,0, AS_NONE,  0, 0 },
  { "iron_chestplate","铁胸甲",        0, 0,  0, TS_I_CHEST,    MAT_IRON, MI,                C(146,146,152), AD_IRON[AI_CHEST],0,0,1, AS_CHEST, 0, 0 },
  { "iron_leggings",  "铁护腿",        0, 0,  0, TS_I_LEGS,     MAT_IRON, MI,                C(146,146,152), AD_IRON[AI_LEGS], 0,0,1, AS_LEGS,  0, 0 },
  { "iron_helmet",    "铁头盔",        0, 0,  0, TS_I_HELM,     MAT_IRON, MI,                C(146,146,152), AD_IRON[AI_HELM], 0,0,1, AS_HELM,  0, 0 },
  { "iron_boots",     "铁靴子",        0, 0,  0, TS_I_BOOT,     MAT_IRON, MI,                C(146,146,152), AD_IRON[AI_BOOT], 0,0,1, AS_BOOT,  0, 0 },
  { "shield",         "盾牌",         -1, 0,  0, TS_I_SHIELD,   MAT_IRON, C(148,104, 62),    C(196,196,202),     600, 0,0,0, AS_NONE,  0, 0 },
  { "lava",           "岩浆",         -1, 0, 10, TS_LAVA,       MAT_NONE, C(214, 78, 18),    C(255,206, 90),        0, 0,0,0, AS_NONE,  0, 0 },
  { "flowing_lava_2", "2/3岩浆",      -1, 0, 10, TS_FLUID_PART, MAT_NONE, C(214, 78, 18),    C(255,206, 90),        0, 0,0,0, AS_NONE,  0, 0 },
  { "flowing_lava_1", "1/3岩浆",       0, 0, 10, TS_FLUID_PART, MAT_NONE, C(214, 78, 18),    C(255,206, 90),        0, 0,0,0, AS_NONE,  0, 0 },
  { "lava_bucket",    "岩浆桶",        0, 0,  0, TS_I_BUCKET,   MAT_IRON, MI,                C(214, 78, 18),        0, 0,0,0, AS_NONE,  0, 0 },
  { "obsidian",       "黑曜石",     4000, 1,  0, TS_ROCK,       MAT_NONE, C( 32, 26, 44),    C( 84, 60,120),        0, 0,0,0, AS_NONE,  1, 0 },
  { "rotten_flesh",   "腐肉",          0, 0,  5, TS_FOOD,       MAT_NONE, C(124,104, 62),    C( 88, 72, 42),        0, 0,0,0, AS_NONE,  1, 0 },
  { "nether_portal",  "下界传送门",   -1, 0,  0, TS_PORTAL,     MAT_NONE, C(120, 42,168,215),C(220,140,255),        0, 0,0,0, AS_NONE,  0, 0 },
  { "netherrack",     "下界岩",      150, 1,  2, TS_NETHERRACK, MAT_NONE, C(122, 42, 44),    C( 78, 24, 26),        0, 0,0,0, AS_NONE,  1, 0 },
  { "chest",          "箱子",        400, 1,  0, TS_CHEST,      MAT_NONE, C(150,108, 58),    C(196,196,202),        0, 0,0,0, AS_NONE,  1, 0 },
  { "bow",            "木弓",          0, 0,  0, TS_I_BOW,      MAT_WOOD, MW,                C(220,220,220),      700, 0,1,0, AS_NONE,  1, 0 },
  { "string",         "线",            0, 0,  0, TS_I_STRING,   MAT_NONE, C(228,228,228),    C(178,178,178),        0, 0,0,0, AS_NONE,  1, 0 },
  { "gunpowder",      "火药",          0, 0,  0, TS_I_POWDER,   MAT_NONE, C(104,104,110),    C( 62, 62, 68),        0, 0,0,0, AS_NONE,  1, 0 },
  { "wood_arrow",     "木箭",          0, 0,  0, TS_I_ARROW,    MAT_WOOD, MW,                C(196,196,202),        0, 0,0,0, AS_NONE,  0, 0 },
  { "iron_arrow",     "铁箭",          0, 0,  0, TS_I_ARROW,    MAT_IRON, MI,                C(140,140,146),        0, 0,0,0, AS_NONE,  0, 0 },
  { "explosive_arrow","爆炸箭",        0, 0,  0, TS_I_ARROW,    MAT_SPECIAL,C(214, 78, 18), C(255,206, 90),        0, 0,0,0, AS_NONE,  0, 0 },
  { "iron_sword",     "铁剑",          0, 0,  0, TS_I_SWORD,    MAT_IRON, MI,                C(146,146,152), DUR_IRON,  0,1,0, AS_NONE,  0, 0 },
  { "iron_axe",       "铁斧",          0, 0,  0, TS_I_AXE,      MAT_IRON, MI,                C(146,146,152), DUR_IRON,  1,0,0, AS_NONE,  0, 0 },
  { "diamond_sword",  "钻石剑",        0, 0,  0, TS_I_SWORD,    MAT_DIAMOND,MD,              C( 40,160,154), DUR_DIA,   0,1,0, AS_NONE,  0, 0 },
  { "diamond_axe",    "钻石斧",        0, 0,  0, TS_I_AXE,      MAT_DIAMOND,MD,              C( 40,160,154), DUR_DIA,   1,0,0, AS_NONE,  0, 0 },
  { "oi_core",        "OI核心",        0, 0,  0, TS_MISC,       MAT_SPECIAL,MX,              C(255,255,255),        0, 0,0,0, AS_NONE,  1, 0 },
  { "recursive",      "递归",          0, 0,  0, TS_MISC,       MAT_SPECIAL,C(180, 90,220),  C(255,220,255),        0, 0,0,0, AS_NONE,  1, 0 },
  { "snow",           "雪",          120, 1,  0, TS_GRASS,      MAT_NONE, C(238,244,252),    C(120, 84, 54),        0, 0,0,0, AS_NONE,  1, 0 },
  { "ice",            "冰",          180, 1,  1, TS_ICE,        MAT_NONE, C(168,214,240),    C(232,250,255),        0, 0,0,0, AS_NONE,  1, 0 },
  { "apple",          "苹果",          0, 0,  1, TS_FOOD,       MAT_NONE, C(206, 52, 48),    C( 62,132, 50),        0, 0,0,0, AS_NONE,  1, 0 },
  { "diamond_helmet", "钻石头盔",      0, 0,  0, TS_I_HELM,     MAT_DIAMOND,MD,              C( 40,160,154), AD_DIA[AI_HELM], 0,0,1, AS_HELM,  0, 0 },
  { "diamond_chestplate","钻石胸甲",   0, 0,  0, TS_I_CHEST,    MAT_DIAMOND,MD,              C( 40,160,154), AD_DIA[AI_CHEST],0,0,1, AS_CHEST, 0, 0 },
  { "diamond_leggings","钻石护腿",     0, 0,  0, TS_I_LEGS,     MAT_DIAMOND,MD,              C( 40,160,154), AD_DIA[AI_LEGS], 0,0,1, AS_LEGS,  0, 0 },
  { "diamond_boots",  "钻石靴子",      0, 0,  0, TS_I_BOOT,     MAT_DIAMOND,MD,              C( 40,160,154), AD_DIA[AI_BOOT], 0,0,1, AS_BOOT,  0, 0 },
  { "oi_sword",       "OI剑",          0, 0,  0, TS_I_SWORD,    MAT_SPECIAL,MX,              C(255,255,255),     2000, 0,1,0, AS_NONE,  0, 0 },
  { "storm_bow",      "风暴弓",        0, 0,  0, TS_I_BOW,      MAT_SPECIAL,MX,              C(255,255,255),      900, 0,1,0, AS_NONE,  0, 0 },
  { "raw_gold",       "粗金",        600, 1,  4, TS_ORE,        MAT_NONE, C(232,196, 92),    C(166,130, 44),        0, 0,0,0, AS_NONE,  1, 0 },
  { "nether_gold_ore","下界金矿",    300, 1,  2, TS_ORE,        MAT_NONE, C(174,132, 56),    C(232,196, 92),        0, 0,0,0, AS_NONE,  1, 0 },
  { "gold_ingot",     "金锭",          0, 0,  0, TS_I_INGOT,    MAT_GOLD, MG,                C(184,144, 40),        0, 0,0,0, AS_NONE,  1, 0 },
  { "gold_nugget",    "金粒",          0, 0,  0, TS_I_NUGGET,   MAT_GOLD, MG,                C(184,144, 40),        0, 0,0,0, AS_NONE,  1, 0 },
  { "platforms",      "木平台",        0, 2,  1, TS_PLANK,      MAT_NONE, C(150,108, 62,205),C(104, 72, 40),        0, 0,0,0, AS_NONE,  1, 0 },
  { "planks",         "木板",        100, 1,  1, TS_PLANK,      MAT_NONE, C(150,108, 62),    C(104, 72, 40),        0, 0,0,0, AS_NONE,  1, 0 },
  { "ebonstone",      "腐化石块",   2000, 1,  5, TS_EBON,       MAT_NONE, C( 64, 44, 86),    C(126, 82,168),        0, 0,0,0, AS_NONE,  1, 0 },
  { "ancient_debris", "远古残骸",   1200, 1,  5, TS_DEBRIS,     MAT_NONE, C( 96, 66, 56),    C(150,104, 74),        0, 0,0,0, AS_NONE,  1, 0 },
  { "netherite_scrap","下界碎片",      0, 0,  0, TS_I_INGOT,    MAT_NETHERITE,C(120, 96, 92),C( 78, 62, 60),        0, 0,0,0, AS_NONE,  1, 0 },
  { "netherite_ingot","下界锭",        0, 0,  0, TS_I_INGOT,    MAT_NETHERITE,MN,            C(196,170,150),        0, 0,0,0, AS_NONE,  1, 0 },
  { "netherite_pickaxe","下界镐",      0, 0,  0, TS_I_PICK,     MAT_NETHERITE,MN,            C(196,170,150), DUR_NETH,  1,0,0, AS_NONE,  1, 0 },
  { "netherite_sword","下界剑",        0, 0,  0, TS_I_SWORD,    MAT_NETHERITE,MN,            C(196,170,150), DUR_NETH,  0,1,0, AS_NONE,  1, 0 },
  { "netherite_axe",  "下界斧",        0, 0,  0, TS_I_AXE,      MAT_NETHERITE,MN,            C(196,170,150), DUR_NETH,  1,0,0, AS_NONE,  1, 0 },
  { "worm_food",      "蠕虫诱饵",      0, 0,  0, TS_FOOD,       MAT_NONE, C(186, 92, 76),    C(120, 56, 48),        0, 0,0,0, AS_NONE,  1, 0 },
  { "gold_pickaxe",   "金镐",          0, 0,  0, TS_I_PICK,     MAT_GOLD, MG,                C(184,144, 40), DUR_GOLD,  1,0,0, AS_NONE,  1, 0 },
  { "gold_sword",     "金剑",          0, 0,  0, TS_I_SWORD,    MAT_GOLD, MG,                C(184,144, 40), DUR_GOLD,  0,1,0, AS_NONE,  1, 0 },
  { "gold_axe",       "金斧",          0, 0,  0, TS_I_AXE,      MAT_GOLD, MG,                C(184,144, 40), DUR_GOLD,  1,0,0, AS_NONE,  1, 0 },
  { "gold_helmet",    "金头盔",        0, 0,  0, TS_I_HELM,     MAT_GOLD, MG,                C(184,144, 40), AD_GOLD[AI_HELM], 0,0,1, AS_HELM,  0, 0 },
  { "gold_chestplate","金胸甲",        0, 0,  0, TS_I_CHEST,    MAT_GOLD, MG,                C(184,144, 40), AD_GOLD[AI_CHEST],0,0,1, AS_CHEST, 0, 0 },
  { "gold_leggings",  "金护腿",        0, 0,  0, TS_I_LEGS,     MAT_GOLD, MG,                C(184,144, 40), AD_GOLD[AI_LEGS], 0,0,1, AS_LEGS,  0, 0 },
  { "gold_boots",     "金靴子",        0, 0,  0, TS_I_BOOT,     MAT_GOLD, MG,                C(184,144, 40), AD_GOLD[AI_BOOT], 0,0,1, AS_BOOT,  0, 0 },
  { "netherite_helmet","下界头盔",     0, 0,  1, TS_I_HELM,     MAT_NETHERITE,MN,            C(196,170,150), AD_NETH[AI_HELM], 0,0,1, AS_HELM,  0, 0 },
  { "netherite_chestplate","下界胸甲", 0, 0,  2, TS_I_CHEST,    MAT_NETHERITE,MN,            C(196,170,150), AD_NETH[AI_CHEST],0,0,1, AS_CHEST, 0, 0 },
  { "netherite_leggings","下界护腿",   0, 0,  0, TS_I_LEGS,     MAT_NETHERITE,MN,            C(196,170,150), AD_NETH[AI_LEGS], 0,0,1, AS_LEGS,  0, 0 },
  { "netherite_boots","下界靴子",      0, 0,  0, TS_I_BOOT,     MAT_NETHERITE,MN,            C(196,170,150), AD_NETH[AI_BOOT], 0,0,1, AS_BOOT,  0, 0 },
  { "wool",           "羊毛",          0, 0,  0, TS_WOOL,       MAT_NONE, C(232,232,232),    C(200,200,200),        0, 0,0,0, AS_NONE,  1, 0 },
  { "mutton",         "羊肉",          0, 0,  0, TS_FOOD,       MAT_NONE, C(196, 96, 92),    C(240,190,186),        0, 0,0,0, AS_NONE,  1, 0 },
  { "pork",           "猪肉",          0, 0,  0, TS_FOOD,       MAT_NONE, C(226,150,146),    C(250,206,202),        0, 0,0,0, AS_NONE,  1, 0 },
  { "bed",            "床",           30, 1,  0, TS_BED,        MAT_NONE, C(186, 52, 58),    C(238,238,238),        0, 0,0,0, AS_NONE,  1, 0 },
  { "golden_apple",   "金苹果",        0, 0,  0, TS_FOOD,       MAT_GOLD, MG,                C(255,255,255),        0, 0,0,0, AS_NONE,  1, 0 },
  { "ebonwall",       "腐化石墙",      0, 0,  0, TS_EBON,       MAT_NONE, C( 52, 34, 72),    C(108, 68,148),        0, 0,0,0, AS_NONE,  1, 7 },
  { "smithing_table", "锻造台",        0, 0,  0, TS_TABLE,      MAT_NONE, C( 96, 92,104),    C(148,104, 62),        0, 0,0,0, AS_NONE,  1, 0 },
  { "shadow_scale",   "暗影鳞片",      0, 0,  0, TS_I_SCALE,    MAT_SHADOW,MH,               C( 62, 38, 96),        0, 0,0,0, AS_NONE,  1, 0 },
  { "shadow_helmet",  "暗影头盔",      0, 0,  0, TS_I_HELM,     MAT_SHADOW,MH,               C( 62, 38, 96), AD_SHA[AI_HELM], 0,0,1, AS_HELM,  0, 0 },
  { "shadow_chestplate","暗影胸甲",    0, 0,  0, TS_I_CHEST,    MAT_SHADOW,MH,               C( 62, 38, 96), AD_SHA[AI_CHEST],0,0,1, AS_CHEST, 0, 0 },
  { "shadow_leggings","暗影护腿",      0, 0,  0, TS_I_LEGS,     MAT_SHADOW,MH,               C( 62, 38, 96), AD_SHA[AI_LEGS], 0,0,1, AS_LEGS,  0, 0 },
  { "shadow_boots",   "暗影靴子",      0, 0,  0, TS_I_BOOT,     MAT_SHADOW,MH,               C( 62, 38, 96), AD_SHA[AI_BOOT], 0,0,1, AS_BOOT,  0, 0 },
  { "lights_bane",    "魔光剑",        0, 0,  0, TS_I_SWORD,    MAT_SHADOW,MH,               C(255,255,255),    2600, 0,1,0, AS_NONE,  1, 0 },
  { "demon_bow",      "恶魔弓",        0, 0,  0, TS_I_BOW,      MAT_SHADOW,C(255,120, 80),   C(255,255,255),     900, 0,1,0, AS_NONE,  1, 0 },
  // id 101：旧版 test[101]，随身光源(12级) + 跳跃 +2；无耐久、可堆叠
  { "magiluminescence","魔光护符",      0, 0,  0, TS_I_AMULET,   MAT_SPECIAL,MX,              C(255,255,255),       0, 0,0,0, AS_NONE,  1, 0 },
  // id 102：新增工作台（4 木板合成，右键打开 3x3 合成）
  { "crafting_table", "工作台",        100, 1,  0, TS_TABLE,      MAT_WOOD, C(150,108, 62),    C(104, 72, 40),        0, 0,0,0, AS_NONE,  1, 0 },
};

// ---------------- 接口 ----------------
namespace {
BlockDef g_invalid = { "invalid", "未知", -1, 1, 0, TS_ROCK, MAT_NONE,
                       rgba(255, 0, 220), rgba(0, 0, 0), 0, false, false,
                       false, AS_NONE, false, 0 };
}

bool block_by_name(const char *name, i32 &out_id)
{
    if(!name || !name[0]) return false;
    for(i32 i = 0; i < BLOCK_COUNT; i++)
        if(strcmp(g_blocks[i].name, name) == 0) { out_id = i; return true; }
    // 允许直接用数字 id
    if(name[0] >= '0' && name[0] <= '9')
    {
        i32 v = atoi(name);
        if(v >= 0 && v < BLOCK_COUNT) { out_id = v; return true; }
    }
    // 允许用中文名
    for(i32 i = 0; i < BLOCK_COUNT; i++)
        if(strcmp(g_blocks[i].cn, name) == 0) { out_id = i; return true; }
    return false;
}

const BlockDef &block_of(i32 id)
{
    if(id < 0 || id >= BLOCK_COUNT) return g_invalid;
    return g_blocks[id];
}

const char *material_cn(u8 mat)
{
    switch(mat)
    {
    case MAT_WOOD:      return "木";
    case MAT_STONE:     return "石";
    case MAT_IRON:      return "铁";
    case MAT_GOLD:      return "金";
    case MAT_DIAMOND:   return "钻石";
    case MAT_NETHERITE: return "下界";
    case MAT_SHADOW:    return "暗影";
    case MAT_SPECIAL:   return "特殊";
    default:            return "";
    }
}

// ---------------- 启动自检 ----------------
namespace {
void blog(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    // 只写 stderr：GUI 构建（-mwindows）下是无效句柄，此时由 main 捕获重定向到 debug.log
    fprintf(stderr, "%s\n", buf);
}

// 与旧版块数据逐项对照，任何不一致都报出来
struct Ref { i32 id; const char *name; const char *cn; i32 dig; u8 touch; };
const Ref REFS[] = {
    {  0, "air", "空气", 0, 0},        {  1, "stone", "圆石", 200, 1},
    {  2, "wood", "原木", 200, 0},     {  3, "coal", "煤炭", 400, 1},
    {  4, "raw_iron", "粗铁", 600, 1}, {  5, "dirt", "泥土", 100, 1},
    {  9, "furnace", "熔炉", 200, 0},  { 10, "bedrock", "基岩", -1, 1},
    { 12, "diamond", "钻石", 800, 1},  { 16, "torch", "火把", 5, 0},
    { 17, "stone_sword", "石剑", -1, 0},{18, "stone_axe", "石斧", 0, 1},
    { 19, "water", "静态水", -1, 0},   { 23, "leaf", "树叶", 12, 0},
    { 24, "health_potion", "回血药水", -1, 0},
    { 31, "shield", "盾牌", -1, 0},    { 32, "lava", "岩浆", -1, 0},
    { 34, "flowing_lava_1", "1/3岩浆", 0, 0},
    { 36, "obsidian", "黑曜石", 4000, 1},
    { 38, "nether_portal", "下界传送门", -1, 0},
    { 39, "netherrack", "下界岩", 150, 1},
    { 40, "chest", "箱子", 400, 1},    { 53, "snow", "雪", 120, 1},
    { 54, "ice", "冰", 180, 1},        { 62, "raw_gold", "粗金", 600, 1},
    { 63, "nether_gold_ore", "下界金矿", 300, 1},
    { 66, "platforms", "木平台", 0, 2},{ 67, "planks", "木板", 100, 1},
    { 68, "ebonstone", "腐化石块", 2000, 1},
    { 69, "ancient_debris", "远古残骸", 1200, 1},
    { 90, "bed", "床", 30, 1},         { 93, "smithing_table", "锻造台", 0, 0},
    { 94, "shadow_scale", "暗影鳞片", 0, 0},
    {100, "demon_bow", "恶魔弓", 0, 0},
    {101, "magiluminescence", "魔光护符", 0, 0},
};
}   // namespace

void blocks_init()
{
    memcpy(g_blocks, TBL, sizeof(TBL));
    static_assert(sizeof(TBL) / sizeof(TBL[0]) == BLOCK_COUNT,
                  "方块表长度必须等于 BLOCK_COUNT");

    // 1) 逐项核对 id -> 名字/中文名/硬度/阻挡
    i32 bad = 0;
    for(const Ref &r : REFS)
    {
        const BlockDef &b = g_blocks[r.id];
        if(strcmp(b.name, r.name) != 0)
            blog("[blocks] id %d 英文名不符: 期望 '%s' 实际 '%s'", r.id, r.name, b.name), bad++;
        if(strcmp(b.cn, r.cn) != 0)
            blog("[blocks] id %d 中文名不符: 期望 '%s' 实际 '%s'", r.id, r.cn, b.cn), bad++;
        if(b.dig != r.dig)
            blog("[blocks] id %d (%s) 硬度不符: 期望 %d 实际 %d", r.id, r.name, r.dig, b.dig), bad++;
        if(b.touch != r.touch)
            blog("[blocks] id %d (%s) 阻挡不符: 期望 %d 实际 %d", r.id, r.name, (int)r.touch, (int)b.touch), bad++;
    }

    // 2) 名字唯一性：旧版的错位 bug 会让两个 id 抢同一个名字
    for(i32 i = 0; i < BLOCK_COUNT; i++)
        for(i32 j = i + 1; j < BLOCK_COUNT; j++)
            if(strcmp(g_blocks[i].name, g_blocks[j].name) == 0)
                blog("[blocks] 名字重复: id %d 与 id %d 都叫 '%s'", i, j,
                     g_blocks[i].name), bad++;

    // 3) 护甲标记与部位一致性
    i32 tool_cnt = 0, armor_cnt = 0, dur_cnt = 0;
    for(i32 i = 0; i < BLOCK_COUNT; i++)
    {
        const BlockDef &b = g_blocks[i];
        if(b.is_armor && b.armor_slot == AS_NONE)
            blog("[blocks] id %d (%s) 是护甲但未指定部位", i, b.name), bad++;
        if(!b.is_armor && b.armor_slot != AS_NONE)
            blog("[blocks] id %d (%s) 不是护甲却指定了部位", i, b.name), bad++;
        if(b.max_dur > 0 && !b.is_tool && !b.is_weapon && !b.is_armor &&
                i != B_SHIELD)
            blog("[blocks] id %d (%s) 有耐久 %.0f 却不是工具/武器/护甲/盾", i,
                 b.name, (double)b.max_dur), bad++;
        if(b.is_tool) tool_cnt++;
        if(b.is_armor) armor_cnt++;
        if(b.max_dur > 0) dur_cnt++;
    }

    blog("[blocks] 初始化完成: %d 项，工具 %d，护甲 %d，有耐久 %d，问题 %d",
         BLOCK_COUNT, tool_cnt, armor_cnt, dur_cnt, bad);
}



