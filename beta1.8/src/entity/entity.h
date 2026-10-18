#pragma once
// ===================== 实体（怪物 / 箭矢 / Boss）=====================
// 旧版 beta1.8.cpp 用 guaiwu shiti[500] + 空间哈希 entity_hash[][]。
// 字段一一对应，只是改成有名字的结构体（旧版单字母字段太容易写错）。
//
// 旧版字段对照：
//   HP/hurt/maxHP  -> hp/hurt/hp_max      gx/gy -> x/y
//   spgx/spgy      -> vx/vy               randomtick -> random_tick
//   direction      -> direction           sd -> sight          gongjitick -> attack_tick
//   tox/toy        -> target_x/target_y   type/jp/CD/AI/nbt -> 同名语义
//   chv/conl/conr/wd -> same             ch/isnotouch/islife2/issd -> same
//   state          -> state
#include "../core/types.h"
#include "../game/game.h"

namespace entity {

constexpr i32 MAX_ENT = 500;
constexpr i32 HASH_CELL = 16;
constexpr i32 HASH_W = (world::W / HASH_CELL) + 2;
constexpr i32 HASH_H = (world::H / HASH_CELL) + 2;

// 实体类型（与旧版 make_st 的 typ 一一对应）
enum Type : i32
{
    T_NONE = 0,
    T_ZOMBIE = 1,      // 僵尸
    T_CREEPER = 2,     // 苦力怕
    T_SKELETON = 3,    // 骷髅
    T_ARROW = 4,       // 箭
    T_OI = 5,          // 凋灵 / OI
    T_SPIDER = 6,      // 蜘蛛
    T_PIGLIN = 7,      // 僵尸猪灵
    T_RECURSIVE = 8,   // 递归（血量分裂）
    T_WORM_HEAD = 9,   // 世界吞噬者-头
    T_WORM_BODY = 10,  // 世界吞噬者-体节
    T_WORM_TAIL = 11,  // 世界吞噬者-尾
    T_SHEEP = 12,      // 羊
    T_PIG = 13,        // 猪
};

// AI 状态机（旧版 EntState）
enum State : u8
{
    ST_IDLE = 0, ST_WANDER, ST_CHASE, ST_ATTACK, ST_FLEE,
    ST_FUSE, ST_SCRIPT, ST_FLY, ST_PHASE, ST_FOLLOW_CHAIN, ST_PROJECTILE
};

struct Entity
{
    f64 hp = 0;             // HP
    f64 hp_max = 0;         // maxHP
    f64 hurt = 0;           // hurt：接触伤害
    f64 x = 0, y = 0;       // gx / gy
    f64 vx = 0, vy = 0;     // spgx / spgy
    f64 random_tick = 0;    // randomtick
    f64 direction = 0;      // direction（递归 Boss 用作血量基准）
    f64 sight = 0;          // sd：视野 / 攻击距离
    f64 attack_tick = 0;    // gongjitick
    f64 target_x = 0, target_y = 0;   // tox / toy
    f64 anim = 0;           // 动画相位（贴图摆动）
    f64 fuse = 0;           // 苦力怕引信剩余（0..1，1=快炸）
    f64 flash = 0;          // 受击闪白剩余帧
    i32 type = 0;           // type
    i32 jp = 0;             // jp：跳跃状态
    i32 cd = 0;             // CD
    i32 ai = 0;             // AI：1 地面近战 / 2 弹道 / 3 Boss脚本 / 4 蠕虫 / 5 被动
    i32 nbt = 0;            // nbt：变体/等级
    i32 chv = 0;            // chv：追击计时
    i32 conl = 0;           // conl：链上前一节
    i32 conr = 0;           // conr：链下一节
    i32 wd = 0;             // wd：受击闪红剩余
    bool ch = false;        // ch：被激怒
    bool isnotouch = false; // isnotouch：免接触伤害
    bool islife2 = false;   // islife2：二阶段
    bool issd = false;      // issd：闪烁标记
    u8 state = ST_IDLE;     // state：AI 状态机

    bool alive() const { return type > 0 && hp > 0; }
    void reset() { *this = Entity(); }
};

extern Entity ents[MAX_ENT];

// ---- 生命周期 ----
void clear_all();
// 旧版 make_st
void spawn(i32 type, f64 hp, f64 hurt, f64 tx, f64 ty, i32 nbt = 0, i32 iseof = 0);
// 旧版 make_jian
void spawn_arrow(f64 tx, f64 ty, f64 vx, f64 vy, i32 nbt, f64 dmg);
// 旧版 make_eow：召唤世界吞噬者
void spawn_eow(f64 px, f64 py);

// ---- 空间哈希（旧版 entity_hash）----
void hash_clear();
void hash_add(i32 idx, f64 x, f64 y);
void hash_remove(i32 idx, f64 x, f64 y);
void hash_update(i32 idx, f64 ox, f64 oy, f64 nx, f64 ny);
void query_nearby(f64 x, f64 y, f64 range, i32 *out, i32 maxout, i32 &n);

// ---- 每帧 ----
// 旧版 st_move()：遍历全部实体跑物理 + AI；旧版 try_make_st()：自然刷怪
void tick_all();
void try_spawn();
// 旧版 boom_loop()：爆炸冲击波（放在 entity 因为它和实体伤害耦合）
void explode_add(i32 i, i32 j, i32 col);
void explode_ready(i32 i, i32 j, i32 col);
void explode_tick();
// 清空爆炸队列（换世界/读档时）
void explode_reset();

// ---- 伤害 ----
// 旧版 dmg_st：对实体造成伤害并标记闪白
void damage_ent(i32 idx, f64 dmg);
// 旧版 st_kill / st_kill_fall
void kill_ent(i32 idx);
void kill_ent_drop(i32 idx);
// 旧版 touch_atk：接触攻击
void touch_attack(i32 idx, f64 dmg, f64 dist);

// ---- 查询 ----
i32  live_count();

}   // namespace entity
