#pragma once
// ===================== 方块 / 物品数据表（102 项，id 0..101）=====================
// 数据来源：beta/beta1.8.cpp 的 name[] / test[] / blockwj[] / blocktouch[] /
// blocklv[] / blockcolor[] / blockprint[]。
//
// !!! id 编号必须与旧版完全一致，否则 v1/v2/v3 存档里的方块 id 会全部错位 !!!
//
// 迁移时确认并修正了旧版的三处数据缺陷（已用探针程序直接量取旧版数组实测确认）：
//
//  (1) name[] 第 93 项缺逗号：
//          "smithing_table"   "shadow_scale"     -> 被 C++ 拼成一项
//      于是 name[93] = "smithing_tableshadow_scale"，且从 id 94 起
//      name[] 相对 test[] 整体错位一位：name[94]="shadow_helmet" 而
//      test[94]="暗影鳞片"。旧版的 /give shadow_helmet 实际发的是暗影鳞片。
//      本表按 test[]（中文名）为准重新对齐，id 编号不变，只修正名字。
//
//  (2) blockwj / blocktouch / blocklv / blockcolor 声明为 [151] 但初始化列表
//      分别只有 86 / 85 / 85 / 94 个值，id 86 之后全部是 0。因此旧版
//      id 86..100（下界靴子、羊毛、床、锻造台、全部暗影装备……）的挖掘硬度
//      实际为 0（秒破）。本表直接沿用旧版的**实测值 0** 以保持行为一致，
//      但在 blockdig() 里统一钳到 1，避免出现负数硬度导致
//      wajue() 的 blocklv[lv<0] 越界读（旧版潜在崩溃点）。
//
//  (3) 旧版 blocknum = 101，而 name[]/test[] 表长 151：id 102..150 只有名字、
//      没有 test[]/blockwj[] 数据，属于废弃残留。但 id 101「魔光护符」是例外——
//      test[101] 有中文名，且旧版确实在用它：setlight(1432) 给玩家随身 12 级光，
//      hujia_update(1547) 给 +2 跳跃，配方 7803 可用 5 暗影鳞片 + 5 金锭合成。
//      因此新版物品域为 0..101，把 id 101 补齐实现。
#include "../core/types.h"

// 方块/物品总数（旧版 blocknum=101 只覆盖 id 0..100，id 101 魔光护符为补齐实现，
// id 102 工作台为新增，追加在末尾不影响旧存档 id 对应关系）
constexpr i32 BLOCK_COUNT = 103;
constexpr i32 BLOCK_MAX_ID = BLOCK_COUNT - 1;

// 贴图生成样式：决定程序化纹理怎么画
enum TileStyle : u8
{
    TS_EMPTY = 0,     // 透明（空气 / 天空）
    TS_ROCK,          // 碎石斑驳（圆石 / 基岩 / 黑曜石）
    TS_DIRT,          // 泥土颗粒
    TS_GRASS,         // 带草/雪表层的泥土
    TS_LOG,           // 原木（竖纹树皮 + 年轮端面）
    TS_PLANK,         // 木板 / 木平台（横纹）
    TS_LEAVES,        // 树叶（团簇 + 镂空）
    TS_ORE,           // 矿石（石底 + 矿脉）
    TS_GEM,           // 宝石/金属块
    TS_WATER,         // 水（波纹 + 半透明）
    TS_LAVA,          // 岩浆（熔流 + 自发光）
    TS_ICE,           // 冰
    TS_SNOW,          // 雪块
    TS_FURNACE,       // 熔炉
    TS_CHEST,         // 箱子
    TS_TORCH,         // 火把
    TS_PORTAL,        // 下界传送门
    TS_BED,           // 床
    TS_WOOL,          // 羊毛
    TS_TABLE,         // 锻造台
    TS_NETHERRACK,    // 下界岩
    TS_EBON,          // 腐化石/墙
    TS_DEBRIS,        // 远古残骸
    TS_FLUID_PART,    // 流动液体（部分高度）
    TS_POTION,        // 药水
    TS_FOOD,          // 食物
    TS_MISC,          // 杂项（线 / 火药 / 桶 / 核心）
    // ---- 物品图标 ----
    TS_I_ROD, TS_I_PICK, TS_I_SWORD, TS_I_AXE,
    TS_I_HELM, TS_I_CHEST, TS_I_LEGS, TS_I_BOOT,
    TS_I_BOW, TS_I_ARROW, TS_I_SHIELD, TS_I_BUCKET,
    TS_I_INGOT, TS_I_NUGGET, TS_I_GEM, TS_I_STICK,
    TS_I_STRING, TS_I_POWDER, TS_I_SCALE, TS_I_AMULET,
};

// 材质色板
enum Material : u8
{
    MAT_NONE = 0, MAT_WOOD, MAT_STONE, MAT_IRON,
    MAT_GOLD, MAT_DIAMOND, MAT_NETHERITE, MAT_SHADOW, MAT_SPECIAL,
};

struct BlockDef
{
    const char *name;      // 英文名（/give 用；已修正错位）
    const char *cn;        // 中文名（旧版 test[]，只保留文字）
    i32   dig;             // 挖掘硬度 = 旧版 blockwj[] 实测值，-1 = 不可破坏
    u8    touch;           // 旧版 blocktouch[] 实测值：0 可穿 / 1 固体 / 2 平台
    i32   level;           // 需要的最低工具等级（由 blocklv[] 换算）
    u8    style;           // TileStyle
    u8    mat;             // 材质色板
    Color base;            // 主色
    Color accent;          // 辅色
    f32   max_dur;         // ★ 耐久度上限，0 = 无耐久（可堆叠）
    bool  is_tool;         // 可挖掘工具（产生挖掘损耗）
    bool  is_weapon;       // 武器
    bool  is_armor;        // 护甲
    u8    armor_slot;      // 1 胸甲 / 2 护腿 / 3 头盔 / 4 靴子（与旧版 wear[] 对齐）
    bool  can_place;       // 旧版 block_() 判定：是否可作为方块放置
    i32   light;           // 放置时自身发光等级（旧版 92 魔光=7，其余 0）
};

// 护甲部位（对外编号，与旧版 wear[] 一致）
enum ArmorSlot : u8 { AS_NONE = 0, AS_CHEST = 1, AS_LEGS = 2, AS_HELM = 3, AS_BOOT = 4 };

extern BlockDef g_blocks[BLOCK_COUNT];

// 启动时调用一次
void blocks_init();

// 英文名 / 中文名 / 数字 -> id
bool block_by_name(const char *name, i32 &out_id);

// 带边界保护的查询
const BlockDef &block_of(i32 id);

// 旧版 blocktouch[] == 1：阻挡移动
inline bool block_solid(i32 id) { return block_of(id).touch == 1; }
// 旧版 blocktouch[] >= 1：可以站上去
inline bool block_standable(i32 id) { return block_of(id).touch >= 1; }
// 旧版 blockwj[] > 0：可挖且不是瞬破；钳过的硬度，-1 表示不可破坏
inline i32 block_dig(i32 id) { return block_of(id).dig; }

// 液体判定（旧版到处在写 19..22 / 32..34 两个区间）
inline bool is_water(i32 id) { return id >= 19 && id <= 22; }
inline bool is_lava(i32 id) { return id >= 32 && id <= 34; }
inline bool is_liquid(i32 id) { return is_water(id) || is_lava(id); }
// 旧版反复出现的「可穿过 / 可放置」判定
inline bool is_passable(i32 id) { return id == 0 || id == 11 || is_liquid(id) || id == 92; }

inline f32 block_max_dur(i32 id) { return block_of(id).max_dur; }
inline bool block_has_dur(i32 id) { return block_of(id).max_dur > 0; }

const char *material_cn(u8 mat);

// ---- 方块 id 常量（与旧版数字一一对应）----
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
    B_PLANKS = 67, B_EBONSTONE = 68, B_ANCIENT_DEBRIS = 69,
    B_NETHERITE_SCRAP = 70, B_NETHERITE_INGOT = 71, B_NETHERITE_PICK = 72,
    B_NETHERITE_SWORD = 73, B_NETHERITE_AXE = 74, B_WORM_FOOD = 75,
    B_GOLD_PICK = 76, B_GOLD_SWORD = 77, B_GOLD_AXE = 78, B_GOLD_HELM = 79,
    B_GOLD_CHEST = 80, B_GOLD_LEGS = 81, B_GOLD_BOOTS = 82,
    B_NETHERITE_HELM = 83, B_NETHERITE_CHEST = 84, B_NETHERITE_LEGS = 85,
    B_NETHERITE_BOOTS = 86, B_WOOL = 87, B_MUTTON = 88, B_PORK = 89, B_BED = 90,
    B_GOLDEN_APPLE = 91, B_EBONWALL = 92, B_SMITHING_TABLE = 93,
    B_SHADOW_SCALE = 94, B_SHADOW_HELM = 95, B_SHADOW_CHEST = 96,
    B_SHADOW_LEGS = 97, B_SHADOW_BOOTS = 98, B_LIGHTS_BANE = 99,
    B_DEMON_BOW = 100, B_LIGHTS_AMULET = 101, B_CRAFTING_TABLE = 102,
};
