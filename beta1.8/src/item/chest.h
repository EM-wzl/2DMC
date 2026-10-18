#pragma once
// ===================== 箱子物品容器 =====================
// 旧版 beta1.8.cpp 用全局 vector<Box_> box + boxlen 管理所有箱子。
// 世界生成（要塞/腐化空腔/随机宝箱）和玩家交互（开挖/存取）都要用它，
// 放在独立模块里，避免 world 依赖 item。
#include "../core/types.h"

namespace item {

// 每个箱子 25 个格（旧版循环写的是 0..24，但结构体数组是 things[27]，
// 这里保持 27 长度、只用前 25 格，与旧版行为一致）
constexpr i32 BOX_SLOTS = 27;
constexpr i32 BOX_USABLE = 25;
// 空的箱子坐标哨兵（旧版用 5005 表示"已删除，可复用"）
constexpr u16 BOX_FREE_X = 5005;

struct Box
{
    u16 things[BOX_SLOTS];   // 打包：物品 id + 数量*1000 (+ 耐久见 v4)
    u16 bx, by;
};

void box_reset();
i32  box_count();
Box &box_at(i32 idx);

// 旧版 findbox：按坐标找，找不到返回 -1
i32  find_box(i32 xxx, i32 yyy);
// 旧版 alloc_box：优先复用已删除的槽位，否则追加
i32  alloc_box(i32 xxx, i32 yyy);
// 旧版 add_box_item
void add_box_item(i32 idx, i32 item, i32 count);
// 旧版 fill_box_loot：随机战利品表
void fill_box_loot(i32 idx);

}   // namespace item
