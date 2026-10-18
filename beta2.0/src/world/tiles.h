#pragma once
// ===================== 方块 / 物品数据表 =====================
// 从旧版 beta2.0.cpp 的 name[]/blockwj[]/blocktouch[]/blocklv[]/blockprint[]/blockcolor[]
// 迁移而来，并补充了图形化渲染需要的视觉样式字段。
// 注意：id 编号必须与旧版一致，否则 v3 存档里的方块 id 会全部错位。
#include "../core/types.h"

// 贴图生成样式：决定程序化纹理怎么画
enum TileStyle : u8
{
    TS_EMPTY = 0,     // 透明（空气）
    TS_ROCK,          // 碎石斑驳
    TS_DIRT,          // 泥土颗粒
    TS_BRICK,         // 砖墙（地牢墙）
    TS_LOG,           // 原木（竖纹树皮）
    TS_PLANK,         // 木板（横纹）
    TS_LEAVES,        // 树叶（团簇）
    TS_ORE,           // 矿石（石底 + 矿点）
    TS_GEM_BLOCK,     // 宝石块（晶簇）
    TS_WATER,         // 水（波纹）
    TS_LAVA,          // 岩浆（熔流）
    TS_ICE,           // 冰（高光斜纹）
    TS_SNOW,          // 雪（白噪点）
    TS_GLASS,         // 玻璃/透明
    TS_FURNACE,       // 熔炉（带炉口）
    TS_CHEST,         // 箱子
    TS_TORCH,         // 火把
    TS_DOOR,          // 门
    TS_PORTAL,        // 传送门
    TS_PLATFORM,      // 平台
    TS_MACHINE,       // 扭蛋机/机器
    TS_BED,           // 床
    TS_WOOL,          // 羊毛
    TS_TABLE,         // 锻造台
    TS_NETHERRACK,    // 下界岩
    // ---- 物品图标（非方块，仅作图标绘制）----
    TS_I_ROD, TS_I_PICK, TS_I_SWORD, TS_I_AXE, TS_I_ARMOR, TS_I_HELM,
    TS_I_LEGS, TS_I_BOOT, TS_I_BOW, TS_I_GUN, TS_I_POTION, TS_I_INGOT,
    TS_I_GEM, TS_I_FOOD, TS_I_COIN, TS_I_ARROW, TS_I_BALL, TS_I_SHIELD,
    TS_I_BUCKET, TS_I_SCALE, TS_I_AMULET, TS_I_SCRAP, TS_I_EGG,
};

struct TileDef
{
    const char *name;      // 英文名（存档/调试用）
    const char *cn;        // 中文名（HUD 显示）
    bool  solid;           // 是否阻挡移动
    i32   dig;             // 挖掘硬度；-1 = 不可破坏；0 = 秒破
    i32   level;           // 需要的最低镐等级（0=徒手可挖）
    u8    style;
    Color base;            // 主色
    Color accent;          // 辅色
};

extern TileDef g_tiles[150];
extern const char *g_ent_cn[24];   // 实体中文名（stname[] 迁移）

// 启动时调用一次，按 DIG/SOLID/LV/VIS 表填满 g_tiles
void tiles_init();

// 便捷查询（带边界保护，防止损坏存档的越界 id 崩渲染）
inline const TileDef &tile_of(i32 id)
{
    static const TileDef dummy = {"invalid", "未知", true, -1, 0, TS_ROCK,
                                  col3(255, 0, 220), col3(0, 0, 0)};
    if(id < 0 || id >= 150) return dummy;
    return g_tiles[id];
}
inline bool tile_solid(i32 id) { return tile_of(id).solid; }

// 地牢里真正会出现的方块 id 常量（渲染/生成复用）
enum BlockId : i32
{
    B_AIR = 0, B_STONE = 1, B_LOG = 2, B_COAL = 3, B_RAW_IRON = 4, B_DIRT = 5,
    B_STICK = 6, B_WOOD_PICK = 7, B_STONE_PICK = 8, B_FURNACE = 9,
    B_BEDROCK = 10, B_SKY = 11, B_DIAMOND = 12, B_IRON_INGOT = 13,
    B_IRON_PICK = 14, B_DIAMOND_PICK = 15, B_TORCH = 16, B_STONE_SWORD = 17,
    B_STONE_AXE = 18, B_WATER = 19, B_FLOW3 = 20, B_FLOW2 = 21, B_FLOW1 = 22,
    B_LEAF = 23, B_HEALTH_POTION = 24, B_BUCKET = 25, B_WATER_BUCKET = 26,
    B_IRON_CHEST = 27, B_IRON_LEGS = 28, B_IRON_HELM = 29, B_IRON_BOOTS = 30,
    B_SHIELD = 31, B_LAVA = 32, B_LAVA2 = 33, B_LAVA1 = 34, B_LAVA_BUCKET = 35,
    B_OBSIDIAN = 36, B_ROTTEN_FLESH = 37, B_NETHER_PORTAL = 38,
    B_NETHERRACK = 39, B_CHEST = 40, B_BOW = 41, B_STRING = 42, B_GUNPOWDER = 43,
    B_WOOD_ARROW = 44, B_IRON_ARROW = 45, B_EXPLOSIVE_ARROW = 46,
    B_IRON_SWORD = 47, B_IRON_AXE = 48, B_DIAMOND_SWORD = 49, B_DIAMOND_AXE = 50,
    B_OI_CORE = 51, B_RECURSIVE = 52, B_SNOW = 53, B_ICE = 54, B_APPLE = 55,
    B_DIAMOND_HELM = 56, B_DIAMOND_CHEST = 57, B_DIAMOND_LEGS = 58,
    B_DIAMOND_BOOTS = 59, B_OI_SWORD = 60, B_STORM_BOW = 61, B_RAW_GOLD = 62,
    B_NETHER_GOLD = 63, B_GOLD_INGOT = 64, B_GOLD_NUGGET = 65, B_PLATFORM = 66,
    B_PLANKS = 67, B_EBONSTONE = 68, B_ANCIENT_DEBRIS = 69, B_NETHERITE_SCRAP = 70,
    B_NETHERITE_INGOT = 71, B_NETHERITE_PICK = 72, B_NETHERITE_SWORD = 73,
    B_NETHERITE_AXE = 74, B_WORM_FOOD = 75, B_GOLD_PICK = 76, B_GOLD_SWORD = 77,
    B_GOLD_AXE = 78, B_GOLD_HELM = 79, B_GOLD_CHEST = 80, B_GOLD_LEGS = 81,
    B_GOLD_BOOTS = 82, B_NETHERITE_HELM = 83, B_NETHERITE_CHEST = 84,
    B_NETHERITE_LEGS = 85, B_NETHERITE_BOOTS = 86, B_WOOL = 87, B_MUTTON = 88,
    B_PORK = 89, B_BED = 90, B_GOLDEN_APPLE = 91, B_EBONWALL = 92,
    B_LEVEL_PORTAL = 93, B_ENERGY_POTION = 94, B_PISTOL = 95, B_SMG = 96,
    B_SHOTGUN = 97, B_SNIPER = 98, B_BULLET = 99, B_BIG_HEALTH = 100,
    B_COIN = 101, B_GACHA = 102, B_LEVEL_GOLD_ORE = 103, B_SMITHING_TABLE = 104,
    B_SHADOW_SCALE = 105, B_SHADOW_HELM = 106, B_SHADOW_CHEST = 107,
    B_SHADOW_LEGS = 108, B_SHADOW_BOOTS = 109, B_LIGHTS_BANE = 110,
    B_DEMON_BOW = 111, B_MAGILUM = 112, B_ARMOR_POTION = 113,
    // 地牢墙体：旧版用 id 10(基岩) 当墙
    B_WALL = 10,
};