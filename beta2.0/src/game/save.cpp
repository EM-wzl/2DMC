// ===================== 存档（局内进度）=====================
// 设计取舍：关卡网格不再整体落盘。
// 关卡几何由 (runSeed, 关卡号) 确定性重算，所以存档只需要记玩家状态 + 进度，
// 一个几百字节的小文件就够了，彻底摆脱旧版几十 MB 的整世界快照。
// v4：新增 用时 elapsed 与 无伤房间 noHitRooms；v3/v2 旧档仍可读（缺失字段按 0）。
// v3：新增背包（30 格 id+count）与选中快捷栏槽；手持武器读档后由选中槽重新推导。
// v2：无背包的旧档仍可读——武器放进快捷栏第 1 格，背包其余为空。
#include "game.h"
#include "../platform/platform.h"

#include <cstdio>
#include <cstring>

namespace game {

namespace {

const char *SAVE_PATH = "run.sav";
const char MAGIC[8] = {'2', 'D', 'M', 'C', 'G', 'S', 'V', 0};
const u32 SAVE_VER = 4;

#pragma pack(push, 1)
struct SaveBlob        // v4（348 字节）
{
    char magic[8];
    u32 version;
    u32 runSeed;
    i32 currentLevel;
    f32 hp, hpMax;
    f32 armor, armorMax;
    f32 energy, energyMax;
    i32 coins;
    i32 weapon;
    i32 kills, roomsCleared, damageTaken;
    i32 unlockedLevel;
    i32 talentCount;                 // v2 起：已选天赋次数
    i32 roomCount;                    // v2 起：本关房间数（校验布局一致性）
    u8  roomState[16];                // v2 起：每个房间的清空状态（读档续玩用）
    i32 hotSel;                       // v3：选中的快捷栏槽
    i32 invId[INV_SLOTS];             // v3：背包物品 id
    i32 invCount[INV_SLOTS];          // v3：背包物品数量
    f32 elapsed;                      // v4：本局用时（秒）
    i32 noHitRooms;                   // v4：无伤清空的战斗房数
    u32 checksum;      // 所有字段简单异或，防止半截文件被当成有效档
};

struct SaveBlobV3      // v3（340 字节）：旧档兼容读取
{
    char magic[8];
    u32 version;
    u32 runSeed;
    i32 currentLevel;
    f32 hp, hpMax;
    f32 armor, armorMax;
    f32 energy, energyMax;
    i32 coins;
    i32 weapon;
    i32 kills, roomsCleared, damageTaken;
    i32 unlockedLevel;
    i32 talentCount;
    i32 roomCount;
    u8  roomState[16];
    i32 hotSel;
    i32 invId[INV_SLOTS];
    i32 invCount[INV_SLOTS];
    u32 checksum;
};

struct SaveBlobV2      // v2（96 字节）：旧档兼容读取
{
    char magic[8];
    u32 version;
    u32 runSeed;
    i32 currentLevel;
    f32 hp, hpMax;
    f32 armor, armorMax;
    f32 energy, energyMax;
    i32 coins;
    i32 weapon;
    i32 kills, roomsCleared, damageTaken;
    i32 unlockedLevel;
    i32 talentCount;
    i32 roomCount;
    u8  roomState[16];
    u32 checksum;
};
#pragma pack(pop)

u32 blob_checksum(const void *data, size_t len)
{
    const u8 *p = (const u8 *)data;
    u32 h = 2166136261u;
    for(size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

}   // namespace

bool save_run()
{
    SaveBlob b = {};
    memcpy(b.magic, MAGIC, sizeof(MAGIC));
    b.version = SAVE_VER;
    b.runSeed = st().runSeed;
    b.currentLevel = st().currentLevel;
    const Player &p = st().pl;
    b.hp = p.hp;
    b.hpMax = p.hpMax;
    b.armor = p.armor;
    b.armorMax = p.armorMax;
    b.energy = p.energy;
    b.energyMax = p.energyMax;
    b.coins = p.coins;
    b.weapon = p.weapon;
    b.kills = st().stats.kills;
    b.roomsCleared = st().stats.roomsCleared;
    b.damageTaken = st().stats.damageTaken;
    b.unlockedLevel = st().currentLevel;
    b.talentCount = st().talentCount;
    b.roomCount = (i32)st().level.rooms.size();
    for(size_t i = 0; i < 16; i++)
        b.roomState[i] = i < st().level.rooms.size() ? st().level.rooms[i].state : (u8)0;
    b.hotSel = st().hotSel;
    for(i32 i = 0; i < INV_SLOTS; i++)
    {
        b.invId[i] = st().inv[i].id;
        b.invCount[i] = st().inv[i].count;
    }
    b.elapsed = st().stats.elapsed;
    b.noHitRooms = st().stats.noHitRooms;
    b.checksum = blob_checksum(&b, sizeof(b) - sizeof(u32));

    FILE *f = fopen(SAVE_PATH, "wb");
    if(!f)
    {
        show_msg("存档失败：无法写入 %s", SAVE_PATH);
        return false;
    }
    size_t wrote = fwrite(&b, 1, sizeof(b), f);
    fclose(f);
    if(wrote != sizeof(b))
    {
        show_msg("存档失败：只写入 %d 字节", (int)wrote);
        return false;
    }
    show_msg("已保存：第 %d 关 · %d 金币 · %s", b.currentLevel, b.coins,
             weapon_of(p.weapon)->cn);
    return true;
}

bool has_save()
{
    FILE *f = fopen(SAVE_PATH, "rb");
    if(!f) return false;
    u8 buf[sizeof(SaveBlob)];
    size_t got = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    if(got == sizeof(SaveBlob))
    {
        SaveBlob b;
        memcpy(&b, buf, sizeof(b));
        if(memcmp(b.magic, MAGIC, sizeof(MAGIC)) == 0 && b.version == SAVE_VER
                && b.checksum == blob_checksum(&b, sizeof(b) - sizeof(u32)))
            return true;
    }
    if(got == sizeof(SaveBlobV3))
    {
        SaveBlobV3 b;
        memcpy(&b, buf, sizeof(b));
        if(memcmp(b.magic, MAGIC, sizeof(MAGIC)) == 0 && b.version == 3
                && b.checksum == blob_checksum(&b, sizeof(b) - sizeof(u32)))
            return true;
    }
    if(got == sizeof(SaveBlobV2))
    {
        SaveBlobV2 b;
        memcpy(&b, buf, sizeof(b));
        if(memcmp(b.magic, MAGIC, sizeof(MAGIC)) == 0 && b.version == 2
                && b.checksum == blob_checksum(&b, sizeof(b) - sizeof(u32)))
            return true;
    }
    return false;
}

// 按版本恢复公共字段（各版本前段布局完全一致，v2/v3 提升时新字段补 0）
static void apply_common(const SaveBlob &b)
{
    st().runSeed = b.runSeed ? b.runSeed : 1u;
    Player &p = st().pl;
    p = Player();
    st().stats = RunStats();
    p.hpMax = b.hpMax > 1 ? b.hpMax : p.hpMax;
    p.armorMax = b.armorMax > 1 ? b.armorMax : p.armorMax;
    p.energyMax = b.energyMax > 1 ? b.energyMax : p.energyMax;
    p.coins = b.coins;
    p.weapon = b.weapon;
    st().stats.kills = b.kills;
    st().stats.roomsCleared = b.roomsCleared;
    st().stats.damageTaken = b.damageTaken;
    st().stats.elapsed = b.elapsed;
    st().stats.noHitRooms = b.noHitRooms;
    st().talentCount = b.talentCount;
    st().held = {0, 0};
    st().holding = false;
    st().invOpen = false;
}

bool load_run()
{
    FILE *f = fopen(SAVE_PATH, "rb");
    if(!f)
    {
        show_msg("没有找到存档");
        return false;
    }
    u8 buf[sizeof(SaveBlob)];
    size_t got = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    bool ok = false;
    SaveBlob b = {};
    if(got == sizeof(SaveBlob))
    {
        memcpy(&b, buf, sizeof(b));
        if(memcmp(b.magic, MAGIC, sizeof(MAGIC)) == 0 && b.version == SAVE_VER
                && b.checksum == blob_checksum(&b, sizeof(b) - sizeof(u32)))
            ok = true;
    }
    if(!ok && got == sizeof(SaveBlobV3))
    {
        SaveBlobV3 b3;
        memcpy(&b3, buf, sizeof(b3));
        if(memcmp(b3.magic, MAGIC, sizeof(MAGIC)) == 0 && b3.version == 3
                && b3.checksum == blob_checksum(&b3, sizeof(b3) - sizeof(u32)))
        {
            // v3 提升到 v4 布局（新增 elapsed/noHitRooms 补 0）
            memcpy(b.magic, b3.magic, 8);
            b.version = b3.version;
            b.runSeed = b3.runSeed;
            b.currentLevel = b3.currentLevel;
            b.hp = b3.hp;  b.hpMax = b3.hpMax;
            b.armor = b3.armor;  b.armorMax = b3.armorMax;
            b.energy = b3.energy;  b.energyMax = b3.energyMax;
            b.coins = b3.coins;
            b.weapon = b3.weapon;
            b.kills = b3.kills;  b.roomsCleared = b3.roomsCleared;
            b.damageTaken = b3.damageTaken;
            b.unlockedLevel = b3.unlockedLevel;
            b.talentCount = b3.talentCount;
            b.roomCount = b3.roomCount;
            memcpy(b.roomState, b3.roomState, sizeof(b.roomState));
            b.hotSel = b3.hotSel;
            for(i32 i = 0; i < INV_SLOTS; i++)
            {
                b.invId[i] = b3.invId[i];
                b.invCount[i] = b3.invCount[i];
            }
            b.elapsed = 0;
            b.noHitRooms = 0;
            ok = true;
        }
    }
    if(!ok && got == sizeof(SaveBlobV2))
    {
        SaveBlobV2 b2;
        memcpy(&b2, buf, sizeof(b2));
        if(memcmp(b2.magic, MAGIC, sizeof(MAGIC)) == 0 && b2.version == 2
                && b2.checksum == blob_checksum(&b2, sizeof(b2) - sizeof(u32)))
        {
            // v2 提升到 v4 布局再走统一路径
            memcpy(b.magic, b2.magic, 8);
            b.version = b2.version;
            b.runSeed = b2.runSeed;
            b.currentLevel = b2.currentLevel;
            b.hp = b2.hp;  b.hpMax = b2.hpMax;
            b.armor = b2.armor;  b.armorMax = b2.armorMax;
            b.energy = b2.energy;  b.energyMax = b2.energyMax;
            b.coins = b2.coins;
            b.weapon = b2.weapon;
            b.kills = b2.kills;  b.roomsCleared = b2.roomsCleared;
            b.damageTaken = b2.damageTaken;
            b.unlockedLevel = b2.unlockedLevel;
            b.talentCount = b2.talentCount;
            b.roomCount = b2.roomCount;
            memcpy(b.roomState, b2.roomState, sizeof(b.roomState));
            b.hotSel = 0;
            // v2 没有背包：存档里的武器放进快捷栏第 1 格
            for(i32 i = 0; i < INV_SLOTS; i++) { b.invId[i] = 0; b.invCount[i] = 0; }
            if(is_weapon(b.weapon)) { b.invId[0] = b.weapon; b.invCount[0] = 1; }
            b.elapsed = 0;
            b.noHitRooms = 0;
            ok = true;
        }
    }
    if(!ok)
    {
        show_msg("存档已损坏，无法读取");
        return false;
    }

    apply_common(b);

    // ---- 背包（数量按叠加上限清洗，武器恒为 1）----
    for(i32 i = 0; i < INV_SLOTS; i++)
    {
        i32 id = b.invId[i], n = b.invCount[i];
        if(id <= 0 || n <= 0) { st().inv[i] = {0, 0}; continue; }
        if(is_weapon(id)) n = 1;
        else if(n > 64) n = 64;
        st().inv[i] = {id, n};
    }
    i32 hs = b.hotSel;
    if(hs < 0) hs = 0;
    if(hs >= HOT_SLOTS) hs = HOT_SLOTS - 1;
    st().hotSel = hs;

    // 先按种子重建关卡，再套用玩家状态（start_level 会重置位置）
    i32 lv = b.currentLevel;
    if(lv < 1) lv = 1;
    start_level(lv, false);      // 读档不弹天赋（天赋已在最大值里）
    // 同一 (种子, 关卡号) 布局是确定的：按存档恢复每个房间的清空状态
    if(b.roomCount == (i32)st().level.rooms.size())
    {
        for(size_t i = 0; i < st().level.rooms.size(); i++)
        {
            u8 s = b.roomState[i];
            if(s == world::ROOM_CLEAR || s == world::ROOM_SAFE)
                st().level.rooms[i].state = s;
        }
    }
    // start_level 之后才恢复血量，避免被初始值覆盖
    Player &p = st().pl;
    p.hp = b.hp > 0 ? b.hp : p.hpMax;
    p.armor = b.armor;
    p.energy = b.energy;
    inv_refresh_weapon();        // 手持武器由选中快捷栏槽重新推导
    st().inGame = true;
    st().paused = false;
    st().screen = 1;
    show_msg("已读取：第 %d 关 · %d 金币 · %s", lv, p.coins,
             weapon_of(p.weapon)->cn);
    return true;
}

}   // namespace game
