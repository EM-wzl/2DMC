// ===================== 箱子物品容器实现 =====================
// 从旧版 beta/beta1.8.cpp 的 box / boxlen / findbox / alloc_box /
// add_box_item / fill_box_loot 迁移。
//
// 打包格式与旧版一致：things[i] = 物品id + 数量*1000，0 表示空。
// v4 存档会在这个基础上把耐久度塞进更高的位（见 save.cpp）。
#include "chest.h"
#include "../world/blocks.h"
#include "../world/world.h"

#include <cstring>
#include <vector>

namespace item {

namespace {
std::vector<Box> g_box;
}

void box_reset()
{
    g_box.clear();
}
i32 box_count() { return (i32)g_box.size(); }
Box &box_at(i32 idx) { return g_box[idx]; }

i32 find_box(i32 xxx, i32 yyy)
{
    for(i32 i = 0; i < (i32)g_box.size(); i++)
        if(g_box[i].bx == (u16)xxx && g_box[i].by == (u16)yyy) return i;
    return -1;
}

i32 alloc_box(i32 xxx, i32 yyy)
{
    for(i32 i = 0; i < (i32)g_box.size(); i++)
        if(g_box[i].bx == BOX_FREE_X)
        {
            g_box[i].bx = (u16)xxx;
            g_box[i].by = (u16)yyy;
            memset(g_box[i].things, 0, sizeof(g_box[i].things));
            return i;
        }
    Box b;
    memset(&b, 0, sizeof(b));
    b.bx = (u16)xxx;
    b.by = (u16)yyy;
    g_box.push_back(b);
    return (i32)g_box.size() - 1;
}

void add_box_item(i32 idx, i32 item, i32 count)
{
    if(idx < 0 || idx >= (i32)g_box.size() || item <= 0 || count <= 0) return;
    if(item >= BLOCK_COUNT) return;          // 防止损坏存档/错误 id 污染
    if(count > 64) count = 64;
    Box &b = g_box[idx];
    for(i32 i = 0; i < BOX_USABLE; i++)
    {
        if(b.things[i] % 1000 == item && b.things[i] / 1000 < 64)
        {
            i32 cur = b.things[i] / 1000;
            i32 add = count;
            if(cur + add > 64) add = 64 - cur;
            if(add > 0)
            {
                b.things[i] = (u16)(item + (cur + add) * 1000);
                count -= add;
            }
            if(count <= 0) return;
        }
    }
    for(i32 i = 0; i < BOX_USABLE; i++)
        if(b.things[i] == 0)
        {
            b.things[i] = (u16)(item + count * 1000);
            return;
        }
}

// 旧版战利品表，逐条照抄
void fill_box_loot(i32 idx)
{
    if(idx < 0 || idx >= (i32)g_box.size()) return;
    i32 rolls = 2 + world::rnd(3);
    for(i32 r = 0; r < rolls; r++)
    {
        i32 roll = world::rnd(100);
        if(roll < 30)      add_box_item(idx, B_COAL, 3 + world::rnd(8));
        else if(roll < 55) add_box_item(idx, B_RAW_IRON, 2 + world::rnd(6));
        else if(roll < 70) add_box_item(idx, B_TORCH, 4 + world::rnd(10));
        else if(roll < 78) add_box_item(idx, B_WOOD_PICK, 1);
        else if(roll < 86) add_box_item(idx, B_STONE_PICK, 1);
        else if(roll < 92) add_box_item(idx, B_IRON_INGOT, 1 + world::rnd(3));
        else if(roll < 95) add_box_item(idx, B_HEALTH_POTION, 1);
        else if(roll < 97) add_box_item(idx, B_APPLE, 1 + world::rnd(2));
        else if(roll < 99)
        {
            add_box_item(idx, B_BOW, 1);
            add_box_item(idx, B_WOOD_ARROW, 6 + world::rnd(10));
        }
        else add_box_item(idx, B_DIAMOND, 1);
    }
    if(world::rnd(4) == 0) add_box_item(idx, B_STRING, 1 + world::rnd(3));
    if(world::rnd(8) == 0) add_box_item(idx, B_GUNPOWDER, 1 + world::rnd(2));
    // 金苹果：稀有箱子战利品
    if(world::rnd(16) == 0) add_box_item(idx, B_GOLDEN_APPLE, 1);
}

}   // namespace item
