// ===================== 物品堆栈与耐久度实现 =====================
#include "item.h"
#include "chest.h"
#include "../world/blocks.h"

#include <cstring>

namespace item {

Stack beibao[INV_SLOTS];
Stack armor[ARMOR_SLOTS];

// ★ 网格合成：格子内容 + 当前网格大小
Stack craft[9];
i32 craft_size = 2;

void craft_clear()
{
    for(i32 i = 0; i < 9; i++)
    {
        if(craft[i].empty()) continue;
        i32 back = inv_add(craft[i].id, craft[i].count);
        craft[i].count -= (i16)back;
        if(craft[i].count <= 0) craft[i].reset();
    }
}

void init()
{
    inv_clear();
}

void inv_clear()
{
    for(i32 i = 0; i < INV_SLOTS; i++) beibao[i].reset();
    for(i32 i = 0; i < ARMOR_SLOTS; i++) armor[i].reset();
}

i32 Stack::max_dur() const
{
    if(id <= 0) return 0;
    return (i32)block_of(id).max_dur;
}
i32 Stack::remain() const
{
    i32 m = max_dur();
    if(m <= 0) return 0;
    i32 r = m - wear_total();
    return r > 0 ? r : 0;
}
u32 Stack::dur_color() const
{
    i32 m = max_dur();
    if(m <= 0) return rgba(255, 255, 255);
    f32 f = (f32)remain() / (f32)m;
    if(f > 0.60f) return rgba(96, 214, 96);      // 绿
    if(f > 0.35f) return rgba(232, 214, 72);     // 黄
    if(f > 0.15f) return rgba(236, 150, 48);     // 橙
    return rgba(228, 68, 60);                    // 红
}

i32 inv_id(i32 slot) { return (slot >= 0 && slot < INV_SLOTS) ? beibao[slot].id : 0; }
i32 inv_count(i32 slot) { return (slot >= 0 && slot < INV_SLOTS) ? beibao[slot].count : 0; }

void inv_set(i32 slot, i32 id, i32 count, i32 wear)
{
    if(slot < 0 || slot >= INV_SLOTS) return;
    if(id < 0 || id >= BLOCK_COUNT) { beibao[slot].reset(); return; }
    beibao[slot].id = (i16)id;
    beibao[slot].count = (i16)count;
    beibao[slot].set_wear(wear);
}

i32 inv_find(i32 type)
{
    // 先找同类且没满的格子（带损耗的工具不能叠，所以要求 wear==0）
    for(i32 i = INV_FIRST; i <= INV_LAST; i++)
        if(beibao[i].id == type && beibao[i].count < MAX_STACK &&
                !(block_of(type).max_dur > 0 && beibao[i].wear_total() > 0))
            return i;
    // 再找空格
    for(i32 i = INV_FIRST; i <= INV_LAST; i++)
        if(beibao[i].count <= 0) return i;
    return 0;
}

// 旧存档按 101 格读入，把落在 37..100 的堆栈搬回 1..36（搬不动就留在原处，
// 不销毁玩家物品）。
void inv_compact()
{
    for(i32 i = INV_LAST + 1; i < INV_SLOTS; i++)
    {
        if(beibao[i].count <= 0) continue;
        i32 id = beibao[i].id, n = beibao[i].count, wear = beibao[i].wear_total();
        beibao[i].reset();
        // 先并到已有同类（仅可堆叠物），再找空位
        for(i32 k = INV_FIRST; k <= INV_LAST && n > 0; k++)
        {
            if(beibao[k].count <= 0) continue;
            if(beibao[k].id != id) continue;
            if(block_of(id).max_dur > 0) continue;
            i32 add = MAX_STACK - beibao[k].count;
            if(add > n) add = n;
            if(add > 0) { beibao[k].count += (i16)add; n -= add; }
        }
        for(i32 k = INV_FIRST; k <= INV_LAST && n > 0; k++)
            if(beibao[k].count <= 0)
            {
                i32 add = (block_of(id).max_dur > 0) ? 1 : (n > MAX_STACK ? MAX_STACK : n);
                beibao[k].id = (i16)id;
                beibao[k].count = (i16)add;
                beibao[k].set_wear(wear);
                n -= add;
            }
        if(n > 0)   // 实在放不下：留在原位，避免丢东西
        {
            beibao[i].id = (i16)id;
            beibao[i].count = (i16)n;
            beibao[i].set_wear(wear);
        }
    }
}

i32 inv_find_existing(i32 type)
{
    for(i32 i = INV_FIRST; i <= INV_LAST; i++)
        if(beibao[i].count > 0 && beibao[i].id == type) return i;
    return 0;
}

i32 inv_consume(i32 type, i32 nd)
{
    if(nd <= 0) return 1;
    i32 nw = 0, m = nd;
    for(i32 i = INV_FIRST; i <= INV_LAST; i++)
    {
        if(beibao[i].count > 0 && beibao[i].id == type)
        {
            nw += beibao[i].count;
            if(nw >= nd)
            {
                beibao[i].count -= (i16)m;
                if(beibao[i].count <= 0) beibao[i].reset();
                return 1;
            }
            m -= beibao[i].count;
            beibao[i].reset();
        }
    }
    return 0;
}

i32 inv_add(i32 type, i32 count)
{
    if(type <= 0 || type >= BLOCK_COUNT || count <= 0) return 0;
    i32 left = count;
    // 可堆叠：先并到已有格
    if(block_of(type).max_dur <= 0)
    {
        for(i32 i = INV_FIRST; i <= INV_LAST && left > 0; i++)
            if(beibao[i].id == type && beibao[i].count > 0 && beibao[i].count < MAX_STACK)
            {
                i32 add = MAX_STACK - beibao[i].count;
                if(add > left) add = left;
                beibao[i].count += (i16)add;
                left -= add;
            }
    }
    // 再开新格
    for(i32 i = INV_FIRST; i <= INV_LAST && left > 0; i++)
        if(beibao[i].count <= 0)
        {
            i32 add = (block_of(type).max_dur > 0) ? 1 : (left > MAX_STACK ? MAX_STACK : left);
            beibao[i].id = (i16)type;
            beibao[i].count = (i16)add;
            beibao[i].wear = 0;
            beibao[i].flag = 0;
            left -= add;
        }
    return count - left;
}

bool inv_take(i32 slot, i32 count)
{
    if(slot < 0 || slot >= INV_SLOTS || count <= 0) return false;
    if(beibao[slot].count < count) return false;
    beibao[slot].count -= (i16)count;
    if(beibao[slot].count <= 0) beibao[slot].reset();
    return true;
}

// ---------------- ★ 耐久度 ----------------
bool dmg_item(i32 slot, i32 dmg)
{
    if(slot < 0 || slot >= INV_SLOTS || dmg <= 0) return false;
    Stack &st = beibao[slot];
    i32 m = st.max_dur();
    if(m <= 0) return false;               // 无耐久物品不受影响
    i32 w = st.wear_total() + dmg;
    if(w >= m)
    {
        st.reset();                        // 碎裂
        return true;
    }
    st.set_wear(w);
    return false;
}

bool dmg_armor(i32 aslot, i32 dmg)
{
    if(aslot < 1 || aslot >= ARMOR_SLOTS || dmg <= 0) return false;
    Stack &st = armor[aslot];
    i32 m = st.max_dur();
    if(m <= 0) return false;
    i32 w = st.wear_total() + dmg;
    if(w >= m)
    {
        st.reset();
        return true;
    }
    st.set_wear(w);
    return false;
}

}   // namespace item
