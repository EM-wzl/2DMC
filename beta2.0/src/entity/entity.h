#pragma once
// ===================== 实体层 =====================
// 关键设计（针对「怪物重叠导致看不出有几只」的问题）：
//  1) 每只怪都有占位半径 radius，移动求解后强制做「硬分离」——
//     重叠的一对会被沿连线推开到刚好相切，完全重合时按确定性方向弹开；
//  2) 分离用 32 格空间哈希找邻居，不做 O(n^2) 全表比较；
//  3) 分离后再做一次「贴墙与房间边界修正」，避免被推进墙里。
#include "../core/types.h"
#include "../world/world.h"

namespace ent {

enum Type : i32
{
    NONE = 0,
    ZOMBIE,     // 近战追击，慢
    SKELETON,   // 远程放箭
    CREEPER,    // 贴脸自爆（有引信预警）
    SPIDER,     // 快、跳扑
    PIGLIN,     // 中速、抗性高
    BAT,        // 乱飞骚扰
    MAGE,       // 远程追踪弹
    BOSS,       // 多阶段
    MERC,       // 雇佣兵随从（友方：跟玩家、打怪、可被爆炸误伤）
    NPC,        // 商人等（渲染位次：必须 < ARROW，否则会被分流到弹丸分支看不到）
    ARROW,      // 箭
    BULLET,     // 子弹
    GRENADE,    // 爆炸物
    PICKUP,     // 掉落物/金币
    TYPE_COUNT
};

// 每种怪的基础属性（数值表集中在这里，方便平衡）
struct MonDef
{
    const char *cn;
    i32 atlasSlot;
    f32 hp;
    f32 speed;       // 格/秒
    f32 radius;      // 占位半径
    f32 dmg;
    i32 atkCd;       // 攻击间隔（帧）
    i32 ranged;      // 1=远程
    f32 scale;       // 渲染缩放
};

const MonDef *mon_def(i32 type);

constexpr i32 MAX_ENTITIES = 512;

struct Entity
{
    bool alive = false;
    i32 type = NONE;
    Vec2 pos, vel;
    f32 radius = 0.34f;
    f32 hp = 0, hpMax = 0;
    f32 speed = 0;
    f32 dmg = 0;
    f32 rot = 0;            // 朝向（渲染/弹道）
    f32 anim = 0;           // 动画计时
    f32 flash = 0;          // 受击白闪，0..1
    f32 cd = 0;             // 攻击冷却（帧@60 基准；f32 计时防高刷截断）
    f32 meleeCd = 0;        // 近战冷却（Boss 用：与弹幕 cd 分离，避免互相抢占）
    f32 ttl = 0;            // 生存时间（帧@60；0=永久，用于弹丸寿命）
    i32 variant = 0;        // 拾取物 id / 弹丸伤害 / NPC 种类
    i32 target = -1;        // 金币价值（拾取物）/ 追踪目标下标
    i32 pierceLeft = 0;     // 剩余穿透次数
    f32 knock = 0;          // 命中击退强度
    u8  state = 0;          // AI 状态（弹丸用 9=玩家发射，2=纯特效）
    f32 stateT = 0;         // 状态计时（帧@60）
    f32 fuse = 0;           // 苦力怕引信 / 预警计时（0..1 表示进度）
    bool bossPhase2 = false;
};

// ---- 生命周期 ----
void clear();
Entity *spawn(i32 type, f32 x, f32 y);
void kill(i32 index);
i32 count();
i32 alive_count_of_type(i32 type);
i32 alive_monster_count();          // 排除弹丸/拾取/NPC

// ---- 空间哈希 ----
void rebuild_hash(const world::Level &lv);
// 收集中心 (x,y) 半径 r 内的实体下标
void query(f32 x, f32 y, f32 r, std::vector<i32> &out);

// ---- 每帧求解 ----
// 硬分离：消除所有怪物之间的重叠（不含弹丸/拾取/NPC）
void separate(const world::Level &lv);
// 玩家与怪物的硬分离：玩家不在实体池里，单独处理。
// 双向推开——怪被推得多（玩家有"重量感"），并把玩家限制在可行走区域。
void separate_player(const world::Level &lv, Vec2 &ppos, f32 pradius);

// 直接访问实体池（下标即 ID；alive==false 表示空槽）
Entity *pool();

}   // namespace ent