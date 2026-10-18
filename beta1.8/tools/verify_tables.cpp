// ===================== 数据表回归校验 =====================
// 用途：每次改动 blocks.cpp 后跑一次，确认 102 项数据仍与旧版 beta1.8.cpp
//       实测值一一对应。id 错位会让 v1/v2/v3 存档里的方块全部变形，所以这个
//       校验必须常驻。
//
// 构建： g++ tools/verify_tables.cpp src/world/blocks.cpp -o verify_tables.exe ^
//            -std=c++17 -Isrc -finput-charset=UTF-8 -fexec-charset=UTF-8
// 运行： verify_tables.exe      退出码 0 = 全部通过，1 = 有偏差
#include "../src/world/blocks.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;

static void expect(bool ok, const char *what)
{
    if(!ok)
    {
        printf("  [FAIL] %s\n", what);
        g_fail++;
    }
}

int verify_tables_main()
{
    blocks_init();

    printf("== 表长 ==\n");
    expect(BLOCK_COUNT == 102,
           "BLOCK_COUNT 必须为 102（旧版 blocknum 0..100 + id 101 魔光护符）");

    printf("== id -> 英文名（旧版 name[] 修正缺逗号错位后的结果）==\n");
    static const struct { i32 id; const char *name; } NAMES[] = {
        {  0, "air"}, {  1, "stone"}, {  2, "wood"}, {  3, "coal"},
        {  4, "raw_iron"}, {  5, "dirt"}, {  9, "furnace"}, { 10, "bedrock"},
        { 12, "diamond"}, { 16, "torch"}, { 19, "water"}, { 23, "leaf"},
        { 24, "health_potion"}, { 31, "shield"}, { 32, "lava"},
        { 36, "obsidian"}, { 38, "nether_portal"}, { 39, "netherrack"},
        { 40, "chest"}, { 53, "snow"}, { 54, "ice"}, { 66, "platforms"},
        { 67, "planks"}, { 68, "ebonstone"}, { 69, "ancient_debris"},
        { 90, "bed"}, { 93, "smithing_table"}, { 94, "shadow_scale"},
        { 95, "shadow_helmet"}, { 96, "shadow_chestplate"},
        { 97, "shadow_leggings"}, { 98, "shadow_boots"}, { 99, "lights_bane"},
        {100, "demon_bow"}, {101, "magiluminescence"},
    };
    for(const auto &n : NAMES)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "id %d 应为 '%s'，实际 '%s'", n.id, n.name,
                 block_of(n.id).name);
        expect(strcmp(block_of(n.id).name, n.name) == 0, buf);
    }

    printf("== id -> 中文名 ==\n");
    static const struct { i32 id; const char *cn; } CNS[] = {
        { 0, "空气"}, { 1, "圆石"}, { 2, "原木"}, { 9, "熔炉"},
        { 12, "钻石"}, { 19, "静态水"}, { 24, "回血药水"}, { 31, "盾牌"},
        { 32, "岩浆"}, { 40, "箱子"}, { 68, "腐化石块"}, { 69, "远古残骸"},
        { 87, "羊毛"}, { 90, "床"}, { 93, "锻造台"}, { 94, "暗影鳞片"},
        { 95, "暗影头盔"}, {100, "恶魔弓"}, {101, "魔光护符"},
    };
    for(const auto &n : CNS)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "id %d 中文名应为 '%s'，实际 '%s'", n.id, n.cn,
                 block_of(n.id).cn);
        expect(strcmp(block_of(n.id).cn, n.cn) == 0, buf);
    }

    printf("== 挖掘硬度 / 阻挡（旧版 blockwj[] / blocktouch[] 实测值）==\n");
    static const struct { i32 id; i32 dig; u8 touch; } DIGS[] = {
        {  1,  200, 1}, {  2,  200, 0}, {  3,  400, 1}, {  4,  600, 1},
        {  5,  100, 1}, {  9,  200, 0}, { 10,   -1, 1}, { 12,  800, 1},
        { 16,    5, 0}, { 17,   -1, 0}, { 18,    0, 1}, { 19,   -1, 0},
        { 23,   12, 0}, { 24,   -1, 0}, { 31,   -1, 0}, { 32,   -1, 0},
        { 36, 4000, 1}, { 38,   -1, 0}, { 39,  150, 1}, { 40,  400, 1},
        { 53,  120, 1}, { 54,  180, 1}, { 66,    0, 2}, { 67,  100, 1},
        { 68, 2000, 1}, { 69, 1200, 1}, { 90,   30, 1}, { 93,    0, 0},
        { 94,    0, 0}, {100,    0, 0}, {101,    0, 0},
    };
    for(const auto &d : DIGS)
    {
        char buf[160];
        snprintf(buf, sizeof(buf), "id %d (%s) 硬度 %d/%d  阻挡 %d/%d", d.id,
                 block_of(d.id).name, block_of(d.id).dig, d.dig,
                 (int)block_of(d.id).touch, (int)d.touch);
        expect(block_of(d.id).dig == d.dig && block_of(d.id).touch == d.touch, buf);
    }

    printf("== 耐久度表 ==\n");
    // 数值随手感调过一轮：本作矿物硬度（钻石矿 800）远高于 MC，
    // 按 MC 原版数值会修不过来、也换得太勤，所以整体上调。
    static const struct { i32 id; f32 dur; } DURS[] = {
        {  7,  120}, {  8,  240}, { 14,  480}, { 15, 1600}, { 72, 2200},
        { 76,   70}, { 27,  480}, { 28,  450}, { 29,  330}, { 30,  390},
        { 57, 1050}, { 58,  990}, { 56,  720}, { 59,  860},
        { 84, 1180}, { 85, 1110}, { 83,  810}, { 86,  960},
        { 80,  220}, { 81,  205}, { 79,  150}, { 82,  175},
        { 96, 1320}, { 97, 1240}, { 95,  920}, { 98, 1080},
        { 41,  700}, { 61,  900}, {100,  900}, { 31,  600},
        { 99, 2600}, { 60, 2000},
    };
    for(const auto &d : DURS)
    {
        char buf[160];
        snprintf(buf, sizeof(buf), "id %d (%s) 耐久应为 %.0f，实际 %.0f", d.id,
                 block_of(d.id).name, (double)d.dur,
                 (double)block_of(d.id).max_dur);
        expect(block_of(d.id).max_dur == d.dur, buf);
    }

    printf("== 无耐久物品不应带耐久 ==\n");
    static const i32 NO_DUR[] = {
        B_AIR, B_STONE, B_LOG, B_COAL, B_RAW_IRON, B_DIRT, B_STICK, B_FURNACE,
        B_BEDROCK, B_DIAMOND, B_IRON_INGOT, B_WATER, B_TORCH, B_LEAF,
        B_HEALTH_POTION, B_BUCKET, B_ROTTEN_FLESH, B_NETHER_PORTAL,
        B_NETHERRACK, B_CHEST, B_STRING, B_GUNPOWDER, B_WOOD_ARROW,
        B_IRON_ARROW, B_EXPLOSIVE_ARROW, B_OI_CORE, B_RECURSIVE, B_SNOW,
        B_ICE, B_APPLE, B_RAW_GOLD, B_NETHER_GOLD, B_GOLD_INGOT,
        B_GOLD_NUGGET, B_PLATFORM, B_PLANKS, B_EBONSTONE, B_ANCIENT_DEBRIS,
        B_NETHERITE_SCRAP, B_NETHERITE_INGOT, B_WORM_FOOD, B_WOOL, B_MUTTON,
        B_PORK, B_BED, B_GOLDEN_APPLE, B_EBONWALL, B_SMITHING_TABLE,
        B_SHADOW_SCALE, B_LIGHTS_AMULET,
    };
    for(i32 id : NO_DUR)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "id %d (%s) 不应有耐久，实际 %.0f", id,
                 block_of(id).name, (double)block_of(id).max_dur);
        expect(block_of(id).max_dur == 0, buf);
    }

    printf("== 护甲部位 ==\n");
    expect(block_of(B_IRON_CHEST).armor_slot == AS_CHEST, "铁胸甲 = 胸甲位");
    expect(block_of(B_IRON_LEGS).armor_slot == AS_LEGS, "铁护腿 = 护腿位");
    expect(block_of(B_IRON_HELM).armor_slot == AS_HELM, "铁头盔 = 头盔位");
    expect(block_of(B_IRON_BOOTS).armor_slot == AS_BOOT, "铁靴子 = 靴子位");
    expect(block_of(B_DIAMOND_CHEST).armor_slot == AS_CHEST, "钻石胸甲 = 胸甲位");
    expect(block_of(B_GOLD_CHEST).armor_slot == AS_CHEST, "金胸甲 = 胸甲位");
    expect(block_of(B_NETHERITE_CHEST).armor_slot == AS_CHEST, "下界胸甲 = 胸甲位");
    expect(block_of(B_SHADOW_CHEST).armor_slot == AS_CHEST, "暗影胸甲 = 胸甲位");
    expect(block_of(B_STONE_SWORD).is_armor == false, "石剑不是护甲");

    printf("== 名字唯一（旧版缺逗号 bug 会导致两个 id 同名）==\n");
    for(i32 i = 0; i < BLOCK_COUNT; i++)
        for(i32 j = i + 1; j < BLOCK_COUNT; j++)
            if(strcmp(block_of(i).name, block_of(j).name) == 0)
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "id %d 与 id %d 同名 '%s'", i, j,
                         block_of(i).name);
                expect(false, buf);
            }

    printf("== 名字查询 ==\n");
    i32 id = -1;
    expect(block_by_name("iron_pickaxe", id) && id == 14, "iron_pickaxe -> 14");
    expect(block_by_name("shadow_helmet", id) && id == 95,
           "shadow_helmet -> 95（旧版会错给 94 暗影鳞片）");
    expect(block_by_name("shadow_scale", id) && id == 94, "shadow_scale -> 94");
    expect(block_by_name("smithing_table", id) && id == 93, "smithing_table -> 93");
    expect(block_by_name("demon_bow", id) && id == 100, "demon_bow -> 100");
    expect(block_by_name("magiluminescence", id) && id == 101,
           "magiluminescence -> 101");
    expect(block_by_name("魔光护符", id) && id == 101, "中文名 魔光护符 -> 101");
    expect(block_by_name("铁镐", id) && id == 14, "中文名 铁镐 -> 14");
    expect(block_by_name("64", id) && id == 64, "数字 64 -> 64");

    printf("== 统计 ==\n");
    i32 tools = 0, armors = 0, durs = 0, placeable = 0;
    for(i32 i = 0; i < BLOCK_COUNT; i++)
    {
        if(block_of(i).is_tool) tools++;
        if(block_of(i).is_armor) armors++;
        if(block_of(i).max_dur > 0) durs++;
        if(block_of(i).can_place) placeable++;
    }
    printf("  工具 %d / 护甲 %d / 有耐久 %d / 可放置 %d\n", tools, armors, durs,
           placeable);

    if(g_fail == 0)
        printf("\n*** 全部通过 (0 项偏差) ***\n");
    else
        printf("\n*** 失败：%d 项偏差 ***\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
