// ===================== 合成 / 熔炼 / 升级 配方数据表（实现）=====================
// 数据逐条抄自 beta/beta1.8.cpp，每条后面标注对应旧版行号。
// 旧版 zz/zz2/zz_rl/zz_rl_2 的定义在 7395-7595，调用清单在 7642-7876。
#include "recipes.h"
#include "item.h"
#include "../world/blocks.h"

namespace item
{

// ===================== 配方表 =====================
// 字段顺序：kind, page, need1, type1, need2, type2, out, out_n, desc
// 按页分组，页内顺序与旧版调用顺序一致（旧版顺序 = UI 显示顺序 = 鼠标行号顺序）。
const Recipe RECIPES[] =
{
    // ------------------------------------------------------------------
    // page 1 —— 合成页① 基础工具/装备
    // 旧版文本 craft_update(1) 6763-6801；调用 craft_1 的 page==1 分支 7723-7767
    // ------------------------------------------------------------------
    // 7723-7752：这一条旧版没走 zz2，而是手写在 page==1 里的等价分支
    //（getbeibao_2(2) 找原木 -> 扣 1 -> getbeibao(6) 给 2 个木棍）
    { R_ONE_MAT,   1, 1, B_LOG,          -1, -1, B_STICK,            2, "1原木->2木棍" },
    { R_TWO_MAT,   1, 3, B_LOG,           2, B_STICK, B_WOOD_PICK,   1, "2木棍+3原木->木镐" },      // 7753
    { R_TWO_MAT,   1, 5, B_STONE,         2, B_STICK, B_STONE_PICK,  1, "2木棍+5圆石->石镐" },      // 7754
    { R_TWO_MAT,   1, 20, B_STONE,        5, B_COAL,  B_FURNACE,     1, "20圆石+5煤炭->熔炉" },     // 7755
    { R_TWO_MAT,   1, 3, B_IRON_INGOT,    2, B_STICK, B_IRON_PICK,   1, "2木棍+3铁锭->铁镐" },      // 7756
    { R_ONE_MAT,   1, 10, B_OBSIDIAN,    -1, -1, B_NETHER_PORTAL,    1, "10黑曜石->下界传送门" },   // 7757
    { R_TWO_MAT,   1, 1, B_COAL,          1, B_STICK, B_TORCH,       5, "1木棍+1煤炭->5火把" },     // 7758
    { R_TWO_MAT,   1, 1, B_STICK,         2, B_STONE, B_STONE_SWORD, 1, "1木棍+2圆石->石剑" },      // 7759
    { R_TWO_MAT,   1, 2, B_STICK,         3, B_STONE, B_STONE_AXE,   1, "2木棍+3圆石->石斧" },      // 7760
    { R_ONE_MAT,   1, 3, B_IRON_INGOT,   -1, -1, B_BUCKET,           1, "3铁锭->桶" },              // 7761
    { R_ONE_MAT,   1, 8, B_IRON_INGOT,   -1, -1, B_IRON_CHEST,       1, "8铁锭->铁胸甲" },          // 7762
    { R_ONE_MAT,   1, 7, B_IRON_INGOT,   -1, -1, B_IRON_LEGS,        1, "7铁锭->铁护腿" },          // 7763
    { R_ONE_MAT,   1, 5, B_IRON_INGOT,   -1, -1, B_IRON_HELM,        1, "5铁锭->铁头盔" },          // 7764
    { R_ONE_MAT,   1, 4, B_IRON_INGOT,   -1, -1, B_IRON_BOOTS,       1, "4铁锭->铁靴子" },          // 7765
    { R_ONE_MAT,   1, 9, B_GOLD_NUGGET,  -1, -1, B_GOLD_INGOT,       1, "9金粒->金锭" },            // 7766
    { R_ONE_MAT,   1, 1, B_GOLD_INGOT,   -1, -1, B_GOLD_NUGGET,      9, "金锭->9金粒" },            // 7767

    // ------------------------------------------------------------------
    // page 2 —— 熔炼页（燃料 rl）
    // 旧版文本 craft_update(2) 6802-6824；调用 craft_2 7657-7663
    // ------------------------------------------------------------------
    { R_FUEL,      2, 1, B_COAL,         -1, -1, -1,                  8, "煤炭->8燃烧时长" },        // 7657
    { R_FUEL,      2, 1, B_LOG,          -1, -1, -1,                  2, "原木->2燃烧时长" },        // 7658
    { R_FUEL,      2, 1, B_STICK,        -1, -1, -1,                  1, "木棍->1燃烧时长" },        // 7659
    { R_FUEL_USE,  2, 1, B_RAW_IRON,     -1, -1, B_IRON_INGOT,        1, "粗铁+燃烧->铁锭" },        // 7660
    { R_FUEL_USE,  2, 1, B_RAW_GOLD,     -1, -1, B_GOLD_INGOT,        1, "粗金+燃烧->金锭" },        // 7661
    { R_FUEL_USE,  2, 1, B_LOG,          -1, -1, B_COAL,              1, "原木+燃烧->煤炭" },        // 7662
    { R_FUEL_USE,  2, 1, B_ANCIENT_DEBRIS, -1, -1, B_NETHERITE_SCRAP, 1, "远古残骸+燃烧->下界碎片" }, // 7663

    // ------------------------------------------------------------------
    // page 3 —— 合成页② 武器/弓箭/钻石
    // 旧版文本 craft_update(3) 6825-6863；调用 craft_1 的 page==2 分支 7771-7786
    // ------------------------------------------------------------------
    { R_ONE_MAT,   3, 8, B_LOG,          -1, -1, B_CHEST,            1, "8原木->箱子" },            // 7771
    { R_TWO_MAT,   3, 3, B_STICK,         3, B_STRING, B_BOW,        1, "3木棍+3线->木弓" },        // 7772
    { R_TWO_MAT,   3, 1, B_STICK,         1, B_STONE,  B_WOOD_ARROW, 8, "木棍+圆石->8木箭" },       // 7773
    { R_TWO_MAT,   3, 1, B_STICK,         1, B_IRON_INGOT, B_IRON_ARROW, 8, "木棍+铁锭->8铁箭" },   // 7774
    { R_TWO_MAT,   3, 1, B_STICK,         1, B_GUNPOWDER, B_EXPLOSIVE_ARROW, 8, "木棍+火药->8爆炸箭" }, // 7775
    { R_TWO_MAT,   3, 1, B_STICK,         2, B_IRON_INGOT, B_IRON_SWORD, 1, "木棍+2铁锭->铁剑" },   // 7776
    { R_TWO_MAT,   3, 2, B_STICK,         3, B_IRON_INGOT, B_IRON_AXE,   1, "2木棍+3铁锭->铁斧" },  // 7777
    { R_TWO_MAT,   3, 3, B_DIAMOND,       2, B_STICK, B_DIAMOND_PICK,  1, "2木棍+3钻石->钻石镐" },  // 7778
    { R_TWO_MAT,   3, 1, B_STICK,         2, B_DIAMOND, B_DIAMOND_SWORD, 1, "木棍+2钻石->钻石剑" }, // 7779
    { R_TWO_MAT,   3, 2, B_STICK,         3, B_DIAMOND, B_DIAMOND_AXE,  1, "2木棍+3钻石->钻石斧" }, // 7780
    { R_ONE_MAT,   3, 4, B_IRON_INGOT,   -1, -1, B_OI_CORE,          1, "4铁锭->OI" },              // 7781
    { R_ONE_MAT,   3, 5, B_DIAMOND,      -1, -1, B_DIAMOND_HELM,     1, "5钻石->钻石头盔" },        // 7782
    { R_ONE_MAT,   3, 8, B_DIAMOND,      -1, -1, B_DIAMOND_CHEST,    1, "8钻石->钻石胸甲" },        // 7783
    { R_ONE_MAT,   3, 7, B_DIAMOND,      -1, -1, B_DIAMOND_LEGS,     1, "7钻石->钻石护腿" },        // 7784
    { R_ONE_MAT,   3, 4, B_DIAMOND,      -1, -1, B_DIAMOND_BOOTS,    1, "4钻石->钻石靴子" },        // 7785
    { R_TWO_MAT,   3, 3, B_PLANKS,        3, B_WOOL, B_BED,          1, "3木板+3羊毛->床" },        // 7786

    // ------------------------------------------------------------------
    // page 4 —— 合成页③ 木板/下界锭/暗影装备
    // 旧版文本 craft_update(4) 6864-6902；调用 craft_1 的 page==3 分支 7790-7803
    // 注意：旧版这一页没有 Y = -1 那一行（鼠标行 12），因为「下界锭」的说明
    //       占了两行文本（6875-6877），行号是刻意空出来的，不是漏抄。
    // ------------------------------------------------------------------
    { R_ONE_MAT,   4, 1, B_LOG,          -1, -1, B_PLANKS,           4, "原木->4木板" },            // 7790
    { R_ONE_MAT,   4, 1, B_PLANKS,       -1, -1, B_PLATFORM,         2, "木板->2木平台" },          // 7791
    { R_ONE_MAT,   4, 1, B_LOG,          -1, -1, B_PLATFORM,         8, "原木->8木平台" },          // 7792
    { R_TWO_MAT,   4, 4, B_GOLD_INGOT,    4, B_NETHERITE_SCRAP, B_NETHERITE_INGOT, 1, "4金锭+4下界碎片->下界锭" }, // 7793
    { R_TWO_MAT,   4, 2, B_IRON_INGOT,    4, B_PLANKS, B_SMITHING_TABLE, 1, "4木板+2铁锭->锻造台" }, // 7794
    { R_ONE_MAT,   4, 10, B_ROTTEN_FLESH, -1, -1, B_WORM_FOOD,       1, "10腐肉->蠕虫诱饵" },       // 7795
    { R_ONE_MAT,   4, 1, B_EBONSTONE,    -1, -1, B_EBONWALL,         4, "腐化石块->4腐化石墙" },    // 7796
    { R_ONE_MAT,   4, 5, B_SHADOW_SCALE, -1, -1, B_SHADOW_HELM,      1, "5暗影鳞片->暗影头盔" },    // 7797
    { R_ONE_MAT,   4, 8, B_SHADOW_SCALE, -1, -1, B_SHADOW_CHEST,     1, "8暗影鳞片->暗影胸甲" },    // 7798
    { R_ONE_MAT,   4, 7, B_SHADOW_SCALE, -1, -1, B_SHADOW_LEGS,      1, "7暗影鳞片->暗影护腿" },    // 7799
    { R_ONE_MAT,   4, 4, B_SHADOW_SCALE, -1, -1, B_SHADOW_BOOTS,     1, "4暗影鳞片->暗影靴子" },    // 7800
    { R_ONE_MAT,   4, 10, B_SHADOW_SCALE, -1, -1, B_LIGHTS_BANE,     1, "10暗影鳞片->魔光剑" },     // 7801
    { R_ONE_MAT,   4, 8, B_SHADOW_SCALE, -1, -1, B_DEMON_BOW,        1, "8暗影鳞片->恶魔弓" },      // 7802
    { R_TWO_MAT,   4, 5, B_SHADOW_SCALE,  5, B_GOLD_INGOT, B_LIGHTS_AMULET,     1, "5暗影鳞片+5金锭->魔光护符" }, // 7803

    // ------------------------------------------------------------------
    // page 5 —— 合成页⑤ 金装备
    // 旧版文本 craft_update(5) 6918-6940；调用 craft_1 的 else（page==5）分支 7811-7818
    // ------------------------------------------------------------------
    { R_TWO_MAT,   5, 2, B_STICK,         3, B_GOLD_INGOT, B_GOLD_PICK,  1, "2木棍+3金锭->金镐" },   // 7811
    { R_TWO_MAT,   5, 1, B_STICK,         2, B_GOLD_INGOT, B_GOLD_SWORD, 1, "1木棍+2金锭->金剑" },   // 7812
    { R_TWO_MAT,   5, 2, B_STICK,         3, B_GOLD_INGOT, B_GOLD_AXE,   1, "2木棍+3金锭->金斧" },   // 7813
    { R_ONE_MAT,   5, 5, B_GOLD_INGOT,   -1, -1, B_GOLD_HELM,      1, "5金锭->金头盔" },             // 7814
    { R_ONE_MAT,   5, 8, B_GOLD_INGOT,   -1, -1, B_GOLD_CHEST,     1, "8金锭->金胸甲" },             // 7815
    { R_ONE_MAT,   5, 7, B_GOLD_INGOT,   -1, -1, B_GOLD_LEGS,      1, "7金锭->金护腿" },             // 7816
    { R_ONE_MAT,   5, 4, B_GOLD_INGOT,   -1, -1, B_GOLD_BOOTS,     1, "4金锭->金靴子" },             // 7817
    { R_TWO_MAT,   5, 8, B_GOLD_INGOT,    1, B_APPLE, B_GOLDEN_APPLE, 1, "8金锭+1苹果->金苹果" },    // 7818

    // ------------------------------------------------------------------
    // page 7 —— 下界升级页
    // 旧版文本 craft_update(7) 6941-6973；调用 craft_3 7684-7690
    // 注意：旧版鼠标行号是 -5,-3,-1,1,3,5,7（隔行），因为每条说明都占两行文本。
    // ------------------------------------------------------------------
    { R_TWO_MAT,   7, 1, B_DIAMOND_PICK,  1, B_NETHERITE_INGOT, B_NETHERITE_PICK,  1, "钻石镐+下界锭->下界镐" },     // 7684
    { R_TWO_MAT,   7, 1, B_DIAMOND_SWORD, 1, B_NETHERITE_INGOT, B_NETHERITE_SWORD, 1, "钻石剑+下界锭->下界剑" },     // 7685
    { R_TWO_MAT,   7, 1, B_DIAMOND_AXE,   1, B_NETHERITE_INGOT, B_NETHERITE_AXE,   1, "钻石斧+下界锭->下界斧" },     // 7686
    { R_TWO_MAT,   7, 1, B_DIAMOND_HELM,  1, B_NETHERITE_INGOT, B_NETHERITE_HELM,  1, "钻石头盔+下界锭->下界头盔" }, // 7687
    { R_TWO_MAT,   7, 1, B_DIAMOND_CHEST, 1, B_NETHERITE_INGOT, B_NETHERITE_CHEST, 1, "钻石胸甲+下界锭->下界胸甲" }, // 7688
    { R_TWO_MAT,   7, 1, B_DIAMOND_LEGS,  1, B_NETHERITE_INGOT, B_NETHERITE_LEGS,  1, "钻石护腿+下界锭->下界护腿" }, // 7689
    { R_TWO_MAT,   7, 1, B_DIAMOND_BOOTS, 1, B_NETHERITE_INGOT, B_NETHERITE_BOOTS, 1, "钻石靴子+下界锭->下界靴子" }, // 7690

    // ------------------------------------------------------------------
    // page 6 —— 空页：旧版这一页一条配方都没有
    // craft_1 的 page==4 分支（7805-7808）只有一句 `;`，
    // 但 craft_update(6)（6903-6917）仍在打印两行配方说明。
    // 这两条配方实际写在下界升级页 page 7（7689/7690），
    // 属于旧版「配方被移到升级页、说明文本忘了删」的遗留，这里不重复造配方，
    // 文本原样放在 page_info(6).orphan 里。
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // page 8 —— ★ 耐久度修复（新增，旧版没有这一页）
    // 用材料把磨损的工具/护甲补回来；回复量是耐久上限的百分比。
    // 只在「该物品当前确实有损耗」时才能修（没磨损就不该浪费材料）。
    // ------------------------------------------------------------------
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_WOOD_PICK,      25, "1铁锭 -> 木镐回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_STONE_PICK,     25, "1铁锭 -> 石镐回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_IRON_PICK,      25, "1铁锭 -> 铁镐回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_DIAMOND_PICK,   25, "1铁锭 -> 钻石镐回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_GOLD_PICK,      25, "1铁锭 -> 金镐回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_STONE_SWORD,    25, "1铁锭 -> 石剑回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_IRON_SWORD,     25, "1铁锭 -> 铁剑回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_DIAMOND_SWORD,  25, "1铁锭 -> 钻石剑回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_GOLD_SWORD,     25, "1铁锭 -> 金剑回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_STONE_AXE,      25, "1铁锭 -> 石斧回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_IRON_AXE,       25, "1铁锭 -> 铁斧回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_DIAMOND_AXE,    25, "1铁锭 -> 钻石斧回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_IRON_HELM,      25, "1铁锭 -> 铁头盔回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_IRON_CHEST,     25, "1铁锭 -> 铁胸甲回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_IRON_LEGS,      25, "1铁锭 -> 铁护腿回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_IRON_INGOT,     -1, 0, B_IRON_BOOTS,     25, "1铁锭 -> 铁靴子回复 25% 耐久" },
    // 下界锭：修下界/暗影装备，回复 33%
    { R_REPAIR,    8, 1, B_NETHERITE_INGOT, -1, 0, B_NETHERITE_PICK,  33, "1下界锭 -> 下界镐回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_NETHERITE_INGOT, -1, 0, B_NETHERITE_SWORD, 33, "1下界锭 -> 下界剑回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_NETHERITE_INGOT, -1, 0, B_NETHERITE_AXE,   33, "1下界锭 -> 下界斧回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_NETHERITE_INGOT, -1, 0, B_NETHERITE_HELM,  33, "1下界锭 -> 下界头盔回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_NETHERITE_INGOT, -1, 0, B_NETHERITE_CHEST, 33, "1下界锭 -> 下界胸甲回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_NETHERITE_INGOT, -1, 0, B_NETHERITE_LEGS,  33, "1下界锭 -> 下界护腿回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_NETHERITE_INGOT, -1, 0, B_NETHERITE_BOOTS, 33, "1下界锭 -> 下界靴子回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_SHADOW_SCALE,    -1, 0, B_LIGHTS_BANE,      33, "1暗影鳞片 -> 魔光剑回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_SHADOW_SCALE,    -1, 0, B_DEMON_BOW,        33, "1暗影鳞片 -> 恶魔弓回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_SHADOW_SCALE,    -1, 0, B_SHADOW_HELM,      33, "1暗影鳞片 -> 暗影头盔回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_SHADOW_SCALE,    -1, 0, B_SHADOW_CHEST,     33, "1暗影鳞片 -> 暗影胸甲回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_SHADOW_SCALE,    -1, 0, B_SHADOW_LEGS,      33, "1暗影鳞片 -> 暗影护腿回复 33% 耐久" },
    { R_REPAIR,    8, 1, B_SHADOW_SCALE,    -1, 0, B_SHADOW_BOOTS,     33, "1暗影鳞片 -> 暗影靴子回复 33% 耐久" },
    // 弓与盾
    { R_REPAIR,    8, 2, B_STRING,          -1, 0, B_BOW,              25, "2线 -> 木弓回复 25% 耐久" },
    { R_REPAIR,    8, 2, B_STRING,          -1, 0, B_STORM_BOW,        25, "2线 -> 风暴弓回复 25% 耐久" },
    { R_REPAIR,    8, 2, B_IRON_INGOT,      -1, 0, B_SHIELD,           25, "2铁锭 -> 盾牌回复 25% 耐久" },
    { R_REPAIR,    8, 1, B_OI_CORE,         -1, 0, B_OI_SWORD,         50, "1 OI核心 -> OI剑回复 50% 耐久" },
};

const int RECIPE_COUNT = (int)(sizeof(RECIPES) / sizeof(RECIPES[0]));

// ===================== 页面信息 =====================
// title / note / hint 取自旧版 craft_update 的对应分支；
// orphan 只给 page 6（该页无配方但旧版仍在打印说明）。
static const char *const PAGE6_ORPHAN[] =
{
    "钻石护腿+下界锭->下界护腿",   // 旧版 6908-6910
    "钻石靴子+下界锭->下界靴子",   // 旧版 6912-6914
};

static const PageInfo PAGE_TABLE[9] =
{
    // page 0：占位，页号非法时返回它
    { "", nullptr, nullptr, nullptr, 0 },
    // page 1..7 = 旧版原有页面
    { "合成配方：", nullptr,       "下一页",         nullptr, 0 },   // 6766 / 6800
    { "熔炼配方：", "当前燃烧时长：", nullptr,        nullptr, 0 },   // 6805 / 6807
    { "合成配方：", nullptr,       "下一页   上一页", nullptr, 0 },   // 6828 / 6862
    { "合成配方：", nullptr,       "下一页   上一页", nullptr, 0 },   // 6867 / 6901
    { "合成配方：", nullptr,       "上一页",         nullptr, 0 },   // 6921 / 6939
    { "合成配方：", nullptr,       "下一页   上一页", PAGE6_ORPHAN, 2 }, // 6906 / 6916 / 6908-6914
    { "升级配方：", nullptr,       nullptr,          nullptr, 0 },   // 6944
    // page 8 = ★ 耐久度修复（新增页）
    { "耐久修复：", nullptr,       "上一页",         nullptr, 0 },
};

const char *page_title(int page)
{
    if(page < 1 || page > 8) return "";
    return PAGE_TABLE[page].title;
}

const PageInfo &page_info(int page)
{
    if(page < 1 || page > 8) return PAGE_TABLE[0];
    return PAGE_TABLE[page];
}

void page_range(int page, int &first, int &count)
{
    first = 0;
    count = 0;
    bool in = false;
    for(int i = 0; i < RECIPE_COUNT; i++)
    {
        if(RECIPES[i].page == page)
        {
            if(!in) { first = i; in = true; }
            count++;
        }
        else if(in)
            break;      // 表是按页分组的，离开本页即可结束
    }
}

// ===================== 背包统计（只读，不扣东西）=====================
// 统计背包里某 id 的总数（旧版 getbeibao_3 的“nw 累计”部分，但不做扣除）
static int inv_total(int type)
{
    if(type <= 0) return 0;
    int n = 0;
    for(int i = INV_FIRST; i <= INV_LAST; i++)
        if(beibao[i].count > 0 && beibao[i].id == type)
            n += beibao[i].count;
    return n;
}

// ===================== 燃料点数（旧版全局 rl）=====================
// 旧版：int rl = 0;（beta1.8.cpp:252），zz_rl 里 rl += sl，zz_rl_2 里 rl--
static int g_fuel = 0;

int fuel() { return g_fuel; }

void add_fuel(int n)
{
    if(n <= 0) return;              // 旧版只有 rl += sl（sl 恒为正）
    g_fuel += n;
}

void use_fuel(int n)
{
    if(n <= 0) return;
    g_fuel -= n;
    if(g_fuel < 0) g_fuel = 0;      // 旧版靠 `if(rl)` 保证不会扣成负数
}

// ===================== 合成执行 =====================
// ★ 耐久度修复：在背包里找一件「已损耗」的目标物品
// 返回槽位索引，找不到返回 0
static int find_damaged(int type)
{
    if(type <= 0) return 0;
    for(int i = INV_FIRST; i <= INV_LAST; i++)
        if(beibao[i].count > 0 && beibao[i].id == type && beibao[i].wear_total() > 0)
            return i;
    return 0;
}

bool can_craft(const Recipe &r)
{
    // 产物必须落在物品域内（0..101，含旧版 101 号「魔光护符」）
    if(r.kind != R_FUEL && (r.out <= 0 || r.out >= BLOCK_COUNT))
    {
        if(r.kind != R_REPAIR) return false;
    }
    if(r.out_n <= 0) return false;
    if(r.need1 <= 0 || r.type1 <= 0 || r.type1 >= BLOCK_COUNT) return false;

    // ★ R_REPAIR：材料够 + 背包里确实有一件磨损的目标物品
    if(r.kind == R_REPAIR)
    {
        if(r.out <= 0 || r.out >= BLOCK_COUNT) return false;
        if(block_max_dur(r.out) <= 0) return false;
        return inv_total(r.type1) >= r.need1 && find_damaged(r.out) > 0;
    }

    // 旧版 zz_rl_2 的条件是 `U > 0 && rl`：必须有燃料才能烧
    if(r.kind == R_FUEL_USE && g_fuel <= 0) return false;

    if(r.kind == R_TWO_MAT)
    {
        if(r.need2 <= 0 || r.type2 <= 0 || r.type2 >= BLOCK_COUNT) return false;
        // 旧版没有「两种材料同 id」的配方；真出现时按合计判定才正确
        if(r.type1 == r.type2) return inv_total(r.type1) >= r.need1 + r.need2;
        return inv_total(r.type1) >= r.need1 && inv_total(r.type2) >= r.need2;
    }

    // R_ONE_MAT / R_FUEL / R_FUEL_USE 都只吃一种材料
    return inv_total(r.type1) >= r.need1;
}

bool do_craft(const Recipe &r)
{
    if(!can_craft(r)) return false;

    // ★ 旧版语义：先整体备份背包，任何一步失败就整体回滚
    //   （beta1.8.cpp:7405-7443 / 7460-7497 / 7549-7588）。
    //   新版把 id/count/wear/flag 整个 Stack 一起备份，回滚更彻底。
    Stack backup[INV_SLOTS];
    for(int i = 0; i < INV_SLOTS; i++) backup[i] = beibao[i];
    auto rollback = [&backup]()
    {
        for(int i = 0; i < INV_SLOTS; i++) beibao[i] = backup[i];
    };

    // ---- R_FUEL：旧版 zz_rl（7505-7539）扣材料 -> rl += sl，没有产物 ----
    if(r.kind == R_FUEL)
    {
        if(!inv_consume(r.type1, r.need1)) { rollback(); return false; }
        add_fuel(r.out_n);
        return true;
    }

    // ---- ★ R_REPAIR：扣材料，把磨损回退 out_n% 的耐久上限 ----
    if(r.kind == R_REPAIR)
    {
        int slot = find_damaged(r.out);
        if(slot <= 0) { rollback(); return false; }
        if(!inv_consume(r.type1, r.need1)) { rollback(); return false; }
        Stack &s = beibao[slot];
        int mx = s.max_dur();
        if(mx <= 0) { rollback(); return false; }
        int heal = mx * r.out_n / 100;
        if(heal < 1) heal = 1;
        int w = s.wear_total() - heal;
        if(w < 0) w = 0;
        s.set_wear(w);
        return true;
    }

    // ---- R_ONE_MAT / R_TWO_MAT / R_FUEL_USE ----
    if(!inv_consume(r.type1, r.need1)) { rollback(); return false; }
    if(r.kind == R_TWO_MAT && !inv_consume(r.type2, r.need2)) { rollback(); return false; }

    // 发产物：旧版是 sl 次 getbeibao(get) 逐格 +1，任何一次放不下就整体回滚
    int placed = inv_add(r.out, r.out_n);
    if(placed < r.out_n) { rollback(); return false; }

    // 旧版 zz_rl_2：产物成功放进背包后才 rl--（放不下时 rl 不减）
    if(r.kind == R_FUEL_USE) use_fuel(1);
    return true;
}

// ============================================================================
//                ★ 网格合成（MC 式形状/无序配方）
// ============================================================================
// 图案里 0=空格，其余是物品 id。为了少写、少错，用宏把重复的工具/护甲形状抽出来。
//
//   工具手柄统一是木棍(B_STICK)，头部材料用 M 传入：
//   镐  MMM / .S. / .S.        剑  .M. / .M. / .S.       斧  MM. / MS. / .S.
//   盔  MMM / M.M             甲  M.M / MMM / MMM
//   腿  MMM / M.M / M.M       靴  ... / M.M / M.M
#define ST B_STICK
#define PICK(M)   M,M,M,  0,ST,0,  0,ST,0
#define SWORD(M)  0,M,0,  0,M,0,  0,ST,0
#define AXE(M)    M,M,0,  M,ST,0,  0,ST,0
#define HELM(M)   M,M,M,  M,0,M,   0,0,0
#define CHEST(M)  M,0,M,  M,M,M,   M,M,M
#define LEGS(M)   M,M,M,  M,0,M,   M,0,M
#define BOOTS(M)  0,0,0,  M,0,M,   M,0,M

// 形状配方：G(out, out_n, grid, desc) 前面接 9 格图案
// 两种写法都支持：
//   SHAPED(p0..p8, o,on,g,d)        显式 9 格
//   SHAPED(PICK(M), o,on,g,d)       图案宏整体当第 1 个参数（展开后正好 9 格）
// 宏参数个数在展开前先数好，PICK(M) 只算 1 个参数，所以按个数分派到两个实现。
#define SHAPED_9(p0,p1,p2,p3,p4,p5,p6,p7,p8, o,on,g,d) \
    { {p0,p1,p2,p3,p4,p5,p6,p7,p8}, {0,0,0,0}, {0,0,0,0}, (o),(on),(g),1,(d) }
#define SHAPED_1(pat, o,on,g,d) \
    { {pat}, {0,0,0,0}, {0,0,0,0}, (o),(on),(g),1,(d) }
#define SHAPED_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,NAME,...) NAME
#define SHAPED(...) SHAPED_N(__VA_ARGS__, SHAPED_9, x, x, x, x, x, x, x, SHAPED_1, x, x, x, x)(__VA_ARGS__)
// 无序配方：最多 4 种材料，各需 cnt 个
#define SHAPELESS(i0,c0,i1,c1,i2,c2,i3,c3, o,on,g,d) \
    { {0,0,0,0,0,0,0,0,0}, {(i0),(i1),(i2),(i3)}, {(c0),(c1),(c2),(c3)}, (o),(on),(g),0,(d) }

const GridRecipe GRID_RECIPES[] =
{
    // ---- 2x2 也能做（grid=0）----
    SHAPELESS(B_LOG,1, 0,0, 0,0, 0,0,  B_PLANKS,4, 0, "1原木->4木板"),
    SHAPED(0,B_PLANKS,0, 0,B_PLANKS,0, 0,0,0,      B_STICK,4, 0, "2木板->4木棍"),
    SHAPED(0,B_COAL,0, 0,B_STICK,0, 0,0,0,          B_TORCH,4, 0, "煤+棍->4火把"),
    SHAPELESS(B_GOLD_INGOT,1, 0,0, 0,0, 0,0,        B_GOLD_NUGGET,9, 0, "金锭->9金粒"),
    SHAPED(B_PLANKS,B_PLANKS,0, 0,0,0, 0,0,0,              B_PLATFORM,2, 0, "2木板->2平台"),
    SHAPED(B_PLANKS,B_PLANKS,0, B_PLANKS,B_PLANKS,0, 0,0,0, B_CRAFTING_TABLE,1, 0, "4木板->工作台"),

    // ---- 镐子（3x3）----
    SHAPED(PICK(B_LOG),          B_WOOD_PICK,   1, 3, "3原木+2棍->木镐"),
    SHAPED(PICK(B_STONE),        B_STONE_PICK,  1, 3, "3石头+2棍->石镐"),
    SHAPED(PICK(B_IRON_INGOT),   B_IRON_PICK,   1, 3, "3铁锭+2棍->铁镐"),
    SHAPED(PICK(B_DIAMOND),      B_DIAMOND_PICK,1, 3, "3钻石+2棍->钻石镐"),
    SHAPED(PICK(B_GOLD_INGOT),   B_GOLD_PICK,   1, 3, "3金锭+2棍->金镐"),
    SHAPED(PICK(B_NETHERITE_INGOT), B_NETHERITE_PICK, 1, 3, "3下界锭+2棍->下界镐"),

    // ---- 剑 ----
    SHAPED(SWORD(B_STONE),       B_STONE_SWORD, 1, 3, "2石头+棍->石剑"),
    SHAPED(SWORD(B_IRON_INGOT),  B_IRON_SWORD,  1, 3, "2铁锭+棍->铁剑"),
    SHAPED(SWORD(B_DIAMOND),     B_DIAMOND_SWORD,1, 3, "2钻石+棍->钻石剑"),
    SHAPED(SWORD(B_GOLD_INGOT),  B_GOLD_SWORD,  1, 3, "2金锭+棍->金剑"),
    SHAPED(SWORD(B_NETHERITE_INGOT), B_NETHERITE_SWORD, 1, 3, "2下界锭+棍->下界剑"),
    SHAPED(SWORD(B_OI_CORE),     B_OI_SWORD,    1, 3, "2OI核+棍->OI剑"),

    // ---- 斧 ----
    SHAPED(AXE(B_STONE),         B_STONE_AXE,   1, 3, "3石头+2棍->石斧"),
    SHAPED(AXE(B_IRON_INGOT),    B_IRON_AXE,    1, 3, "3铁锭+2棍->铁斧"),
    SHAPED(AXE(B_DIAMOND),       B_DIAMOND_AXE, 1, 3, "3钻石+2棍->钻石斧"),
    SHAPED(AXE(B_GOLD_INGOT),    B_GOLD_AXE,    1, 3, "3金锭+2棍->金斧"),
    SHAPED(AXE(B_NETHERITE_INGOT), B_NETHERITE_AXE, 1, 3, "3下界锭+2棍->下界斧"),

    // ---- 护甲（铁 / 钻石 / 金 / 下界 / 暗影）----
    SHAPED(HELM(B_IRON_INGOT),   B_IRON_HELM,   1, 3, "5铁锭->铁头盔"),
    SHAPED(CHEST(B_IRON_INGOT),  B_IRON_CHEST,  1, 3, "8铁锭->铁胸甲"),
    SHAPED(LEGS(B_IRON_INGOT),   B_IRON_LEGS,   1, 3, "7铁锭->铁护腿"),
    SHAPED(BOOTS(B_IRON_INGOT),  B_IRON_BOOTS,  1, 3, "4铁锭->铁靴子"),
    SHAPED(HELM(B_DIAMOND),      B_DIAMOND_HELM, 1, 3, "5钻石->钻石头盔"),
    SHAPED(CHEST(B_DIAMOND),     B_DIAMOND_CHEST,1, 3, "8钻石->钻石胸甲"),
    SHAPED(LEGS(B_DIAMOND),      B_DIAMOND_LEGS, 1, 3, "7钻石->钻石护腿"),
    SHAPED(BOOTS(B_DIAMOND),     B_DIAMOND_BOOTS,1, 3, "4钻石->钻石靴子"),
    SHAPED(HELM(B_GOLD_INGOT),   B_GOLD_HELM,   1, 3, "5金锭->金头盔"),
    SHAPED(CHEST(B_GOLD_INGOT),  B_GOLD_CHEST,  1, 3, "8金锭->金胸甲"),
    SHAPED(LEGS(B_GOLD_INGOT),   B_GOLD_LEGS,   1, 3, "7金锭->金护腿"),
    SHAPED(BOOTS(B_GOLD_INGOT),  B_GOLD_BOOTS,  1, 3, "4金锭->金靴子"),
    SHAPED(HELM(B_NETHERITE_INGOT), B_NETHERITE_HELM, 1, 3, "5下界锭->下界头盔"),
    SHAPED(CHEST(B_NETHERITE_INGOT), B_NETHERITE_CHEST, 1, 3, "8下界锭->下界胸甲"),
    SHAPED(LEGS(B_NETHERITE_INGOT), B_NETHERITE_LEGS, 1, 3, "7下界锭->下界护腿"),
    SHAPED(BOOTS(B_NETHERITE_INGOT), B_NETHERITE_BOOTS, 1, 3, "4下界锭->下界靴子"),
    SHAPED(HELM(B_SHADOW_SCALE), B_SHADOW_HELM, 1, 3, "5暗影鳞->暗影头盔"),
    SHAPED(CHEST(B_SHADOW_SCALE),B_SHADOW_CHEST,1, 3, "8暗影鳞->暗影胸甲"),
    SHAPED(LEGS(B_SHADOW_SCALE), B_SHADOW_LEGS, 1, 3, "7暗影鳞->暗影护腿"),
    SHAPED(BOOTS(B_SHADOW_SCALE),B_SHADOW_BOOTS,1, 3, "4暗影鳞->暗影靴子"),

    // ---- 其它方块 / 工具 ----
    SHAPED(B_STONE,B_STONE,B_STONE, B_STONE,0,B_STONE, B_STONE,B_STONE,B_STONE,
           B_FURNACE,1, 3, "8石头->熔炉"),
    SHAPED(B_PLANKS,B_PLANKS,B_PLANKS, B_PLANKS,0,B_PLANKS, B_PLANKS,B_PLANKS,B_PLANKS,
           B_CHEST,1, 3, "8木板->箱子"),
    SHAPED(B_WOOL,B_WOOL,B_WOOL, B_PLANKS,B_PLANKS,B_PLANKS, 0,0,0,
           B_BED,1, 3, "3羊毛+3木板->床"),
    SHAPED(B_IRON_INGOT,0,B_IRON_INGOT, 0,B_IRON_INGOT,0, 0,0,0,
           B_BUCKET,1, 3, "3铁锭->桶"),
    SHAPELESS(B_STRING,3, B_STICK,3, 0,0, 0,0,  B_BOW,1, 3, "3线+3棍->木弓"),
    SHAPELESS(B_STONE,1, B_STICK,1, B_STRING,1, 0,0,  B_WOOD_ARROW,8, 3, "石+棍+线->8木箭"),
    SHAPELESS(B_IRON_INGOT,1, B_STICK,1, B_STRING,1, 0,0,  B_IRON_ARROW,8, 3, "铁+棍+线->8铁箭"),
    SHAPELESS(B_GUNPOWDER,1, B_STICK,1, B_STRING,1, 0,0,  B_EXPLOSIVE_ARROW,8, 3, "火药+棍+线->8爆炸箭"),
    SHAPELESS(B_GOLD_NUGGET,9, 0,0, 0,0, 0,0,  B_GOLD_INGOT,1, 3, "9金粒->金锭"),
    SHAPELESS(B_OBSIDIAN,4, 0,0, 0,0, 0,0,  B_EBONWALL,4, 0, "4黑曜石->4黑石墙"),
    SHAPELESS(B_PLANKS,4, B_IRON_INGOT,1, 0,0, 0,0,  B_SHIELD,1, 3, "4木板+1铁锭->盾牌"),

    // ---- 高阶合成 ----
    SHAPED(B_GOLD_INGOT,B_SHADOW_SCALE,B_GOLD_INGOT,
           B_SHADOW_SCALE,0,B_SHADOW_SCALE,
           B_GOLD_INGOT,B_SHADOW_SCALE,B_GOLD_INGOT,
           B_LIGHTS_AMULET,1, 3, "4金锭+4暗影鳞->魔光护符"),
    SHAPELESS(B_NETHERITE_SCRAP,4, B_GOLD_INGOT,4, 0,0, 0,0,
           B_NETHERITE_INGOT,1, 3, "4下界碎片+4金锭->下界锭"),
    SHAPELESS(B_IRON_INGOT,4, 0,0, 0,0, 0,0,  B_OI_CORE,1, 3, "4铁锭->OI核心"),
    SHAPED(B_GOLD_INGOT,B_GOLD_INGOT,B_GOLD_INGOT,
           B_GOLD_INGOT,B_APPLE,B_GOLD_INGOT,
           B_GOLD_INGOT,B_GOLD_INGOT,B_GOLD_INGOT,
           B_GOLDEN_APPLE,1, 3, "8金锭+苹果->金苹果"),
};

#undef ST
#undef PICK
#undef SWORD
#undef AXE
#undef HELM
#undef CHEST
#undef LEGS
#undef BOOTS
#undef SHAPED
#undef SHAPED_9
#undef SHAPED_1
#undef SHAPED_N
#undef SHAPELESS

const int GRID_RECIPE_COUNT = (int)(sizeof(GRID_RECIPES) / sizeof(GRID_RECIPES[0]));

// 把 size x size 网格里的非空格子平移到左上角，得到归一化后的 3x3 图案。
// 返回 w x h（空网格时 w=h=0）。这样图案摆在网格哪个角落都能匹配上。
static void grid_normalize(const i16 *src, int size, i16 out[9], int &w, int &h)
{
    for(int i = 0; i < 9; i++) out[i] = 0;
    w = h = 0;
    int r0 = size, r1 = -1, c0 = size, c1 = -1;
    for(int r = 0; r < size; r++)
        for(int c = 0; c < size; c++)
            if(src[r * size + c] != 0)
            {
                if(r < r0) r0 = r;
                if(r > r1) r1 = r;
                if(c < c0) c0 = c;
                if(c > c1) c1 = c;
            }
    if(r1 < 0) return;                 // 全空
    w = c1 - c0 + 1;
    h = r1 - r0 + 1;
    if(w > 3) w = 3;
    if(h > 3) h = 3;
    for(int r = 0; r < h; r++)
        for(int c = 0; c < w; c++)
            out[r * 3 + c] = src[(r0 + r) * size + (c0 + c)];
}

// 无序配方匹配：把网格里非空物品按 id 排序后，和配方材料逐个比（含数量）
static bool match_shapeless(const GridRecipe &gr, const i16 *cells, int size)
{
    i16 got[9];
    int n = 0;
    for(int i = 0; i < size * size; i++)
        if(cells[i] != 0 && n < 9) got[n++] = cells[i];
    // 统计网格里每种物品的需求量
    i16 need_id[4], need_cnt[4];
    int kinds = 0;
    for(int k = 0; k < 4; k++)
        if(gr.ing[k] != 0 && gr.ing_cnt[k] > 0)
        {
            need_id[kinds] = gr.ing[k];
            need_cnt[kinds] = gr.ing_cnt[k];
            kinds++;
        }
    // 网格里物品总件数必须等于配方总需求量
    int total = 0;
    for(int k = 0; k < kinds; k++) total += need_cnt[k];
    if(total != n) return false;
    for(int k = 0; k < kinds; k++)
    {
        int have = 0;
        for(int i = 0; i < n; i++) if(got[i] == need_id[k]) have++;
        if(have != need_cnt[k]) return false;
    }
    return true;
}

int match_grid(const i16 *cells, int size)
{
    if(size < 2 || size > 3) return -1;
    i16 gn[9];
    int gw = 0, gh = 0;
    grid_normalize(cells, size, gn, gw, gh);
    if(gw == 0) return -1;             // 空网格

    for(int k = 0; k < GRID_RECIPE_COUNT; k++)
    {
        const GridRecipe &gr = GRID_RECIPES[k];
        if(gr.grid == 2 && size != 2) continue;   // 只能在 2x2 做
        if(gr.grid == 3 && size < 3) continue;    // 需要 3x3

        if(gr.shaped)
        {
            i16 pn[9];
            int pw = 0, ph = 0;
            grid_normalize(gr.pat, 3, pn, pw, ph);
            if(pw != gw || ph != gh) continue;
            bool same = true;
            for(int i = 0; i < 9; i++)
                if(pn[i] != gn[i]) { same = false; break; }
            if(same) return k;
        }
        else
        {
            if(match_shapeless(gr, cells, size)) return k;
        }
    }
    return -1;
}

bool grid_result(const i16 *cells, int size, i16 &out, i16 &out_n)
{
    out = 0;
    out_n = 0;
    int k = match_grid(cells, size);
    if(k < 0) return false;
    out = GRID_RECIPES[k].out;
    out_n = GRID_RECIPES[k].out_n;
    return out > 0;
}

}   // namespace item

// ===================== 抽取时发现的旧版缺陷（不改数据，只记录）=====================
// 1) 7803 行 `zz(9,5,94,5,64,101,1,4)` 产物 id = 101「魔光护符」：
//    已按 Lead 决定补齐 id 101 物品（blocks.h 说明 (3)），本配方可正常合成。
//    随身光源(12) 与 +2 跳跃在 game::amulet_update()/world::set_light() 里实现。
// 2) craft_1 的 page==4（7805-7808）只有 `;`，一条配方都没有，
//    但 craft_update(6) 仍打印「钻石护腿/靴子 + 下界锭 -> 下界护腿/靴子」。
//    这两条配方在升级页 page 7（7689/7690）里已有，属重复文本，不是重复配方。
// 3) 7723-7752「1原木->2木棍」是手写分支，不是 zz2 调用：
//    它用 getbeibao_2(2) 只扣一个格子里的 1 个原木，
//    第二个木棍放不下时也不回滚（材料照扣、只给 1 个木棍）。
//    本表按标准 R_ONE_MAT 处理（1 原木 -> 2 木棍，放不下整体回滚），
//    比旧版更严格，但与其余配方的语义一致。
// 4) 旧版 getbeibao_3（4202-4220）在材料不足时会先把已扫到的格子清零再返回 0，
//    新 item::inv_consume 保留了同样的行为。因此 do_craft 先做 can_craft 纯统计，
//    保证 inv_consume 只会在必然成功时被调用；失败路径另有整体回滚兜底。
