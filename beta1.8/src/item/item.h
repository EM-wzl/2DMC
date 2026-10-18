#pragma once
// ===================== 物品堆栈与耐久度 =====================
// 旧版 beta1.8.cpp 用 beibao[101][2] 存 {id, count}，没有耐久度。
// 新版本扩展成 Stack{id,count,wear,flag}。
//
// wear 语义：已损耗点数，0 = 全新；wear >= max_dur 时物品碎裂消失。
// 只有 block_max_dur(id) > 0 的工具/护甲/弓/盾会用到 wear，
// 可堆叠方块恒为 0（保持与旧版存档二进制兼容）。
#include "../core/types.h"

namespace item {

// 数组容量：与旧版 beibao[101][2] 一致，**存档格式依赖这个长度**（每格 6 字节）。
// 注意它只是数组上限，真正可用的槽位是 1..INV_LAST。
constexpr i32 INV_SLOTS = 101;

// 可用槽位布局：底部快捷栏 1..9，上方背包 3 行 x 9 列 = 27 格（10..36）
constexpr i32 INV_FIRST = 1;
constexpr i32 HOTBAR_FIRST = 1;
constexpr i32 HOTBAR_LAST = 9;
constexpr i32 BACKPACK_COLS = 9;
constexpr i32 BACKPACK_ROWS = 3;
constexpr i32 BACKPACK_FIRST = 10;
constexpr i32 INV_LAST = BACKPACK_FIRST + BACKPACK_COLS * BACKPACK_ROWS - 1;  // 36
constexpr i32 MAX_STACK = 64;

// 护甲槽（与旧版 wear[6] 对齐：1 胸甲 / 2 护腿 / 3 头盔 / 4 靴子）
constexpr i32 ARMOR_SLOTS = 5;

struct Stack
{
    i16 id = 0;
    i16 count = 0;
    u8  wear = 0;    // 已损耗点数**低 8 位**
    u8  flag = 0;    // 已损耗点数**高 8 位**（原来是"保留位"，从未被用过）
                     // 两个字节合起来支持 0..65535；存档里本来就是分开写的，
                     // 所以这样改不改变存档布局。

    bool empty() const { return id <= 0 || count <= 0; }
    i32 max_dur() const;
    i32 wear_total() const { return (i32)wear | ((i32)flag << 8); }
    void set_wear(i32 w)
    {
        if(w < 0) w = 0;
        if(w > 65535) w = 65535;
        wear = (u8)(w & 0xFF);
        flag = (u8)((w >> 8) & 0xFF);
    }
    i32 remain() const;          // 剩余耐久；无耐久物品返回 max_dur（=0 时无意义）
    bool damaged() const { return wear_total() > 0 && max_dur() > 0; }
    // 按比例返回耐久条颜色（绿 -> 黄 -> 橙 -> 红）
    u32 dur_color() const;
    void reset() { id = 0; count = 0; wear = 0; flag = 0; }
};

// 全局背包 / 护甲（旧版 beibao / wear）
extern Stack beibao[INV_SLOTS];
extern Stack armor[ARMOR_SLOTS];

// ---- ★ 网格合成的格子（MC 式）----
// craft[row*3+col] 存摆在格子里的物品；2x2 只用左上 2x2。
// 出口格不是真实槽位（是算出来的），不存东西。
extern Stack craft[9];
extern i32   craft_size;    // 当前是 2x2 还是 3x3（2 或 3）
void craft_clear();         // 关面板时把格子里的物品塞回背包
// 旧版式访问（存档 / 兼容代码用）
i32  inv_id(i32 slot);
i32  inv_count(i32 slot);
void inv_set(i32 slot, i32 id, i32 count, i32 wear = 0);

void inv_clear();
// 把落在不可用槽位（INV_LAST+1 .. INV_SLOTS-1）的物品挪进 1..INV_LAST。
// 旧存档按 101 格序列化，读档后调用一次，避免出现"看不见但占着"的物品。
void inv_compact();
// 找个能放下 type 的格子（旧版 getbeibao）
i32  inv_find(i32 type);
// 找一个已有 type 的格子（旧版 getbeibao_2）
i32  inv_find_existing(i32 type);
// 跨格扣除 nd 个 type，成功返回 1（旧版 getbeibao_3）
i32  inv_consume(i32 type, i32 nd);
// 放入若干物品，返回实际放入数量
i32  inv_add(i32 type, i32 count);
// 扣除指定格子若干
bool inv_take(i32 slot, i32 count);

// ★ 耐久度：对某格的物品造成 dmg 点损耗。
// 返回 true 表示物品因此碎裂并被移除（调用方应给出粒子/提示）。
bool dmg_item(i32 slot, i32 dmg);
// ★ 护甲耐久：对某个护甲槽造成损耗，碎裂时清空该槽
bool dmg_armor(i32 armor_slot, i32 dmg);

// 耐久度上限表查询（内部用 blocks 表）
void init();

}   // namespace item
