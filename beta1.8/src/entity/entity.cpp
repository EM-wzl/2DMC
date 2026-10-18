// ===================== 实体：物理 / AI / 爆炸 / 刷怪 =====================
// 逐函数移植自旧版 beta/beta1.8.cpp，行号写在每个函数上方，便于对照。
//
// 移植原则：
//   1. 公式、阈值、判定顺序一律照抄，保证同一随机种子下行为一致；
//   2. 旧版裸写 s[i][j] 的地方全部换成 world::get()/world::light_at() 或先判范围，
//      旧版在这里本来就有越界崩的隐患（实体能跑到世界边界外）；
//   3. 所有 rand() 换成 world::rnd()，随机序列由 world 的随机源统一驱动；
//   4. 玩家按键/鼠标：旧版 KEY_DOWN(VK_LBUTTON) -> platform::mouse_pressed(MB_LEFT)；
//   5. 旧版全局变量 -> game::st() 的字段（数值单位不变）。
//
// 旧版对照表（本文件用到的部分）：
//   make_st(1639)      -> spawn()            make_jian(5131) -> spawn_arrow()
//   make_eow(4356)     -> spawn_eow()        try_make_st(5147) -> try_spawn()
//   st_kill(5257)      -> kill_ent()         st_kill_fall(5318) -> kill_ent_drop()
//   dmg_st(5360)       -> damage_ent()       touch_atk(5365) -> touch_attack()
//   thing_fall(5248)   -> inv_add 循环       boom_add(614)/boom_add_ready(628)/
//   boom_loop(636)     -> explode_add()/explode_ready()/explode_tick()
//   st_move(6521)      -> tick_all()
//   ai_ground(5978) ai1_player_click(5424) ai1_despawn_check(5506) ent_physics_ground(5536)
//   ai1_script(5726) ai1_attacks(5811) ai_projectile(6049) ai_flyboss(6153)
//   ai_boss3(6266) ai_passive(6370) boss2_die_split(5407) ent_update_state(6009)
#include "entity.h"
#include "../platform/platform.h"
#include "../world/blocks.h"

#include <cstdio>
#include <cmath>

namespace entity {

// 怪物/动物移动速度倍率。
// 旧版的移动常量（0.1/0.15/0.3、vx*0.02 等）是按字符版一帧一格的手感定的，
// 放到 16px/格、60Hz 的图形版里明显偏快（尤其被动生物乱窜）。
// 这里统一缩放，改这一个数就能整体调快慢。
constexpr f64 ENT_SPEED = 0.55;

// ============================================================================
//                              常量与本地状态
// ============================================================================

Entity ents[MAX_ENT];

namespace {

// 旧版 To_int(a) = int(a + 0.5)
constexpr f64 GRAVITY = 0.6;              // 旧版全局 g

// ---- 旧版全局变量的落点 ----
// 旧版全局 l：链式召唤时记住上一节实体
i32 g_last = 0;
// 旧版 checkhp：本帧是否有 AI==3 的 Boss 存活（决定是否把 boss2hp 清零）
bool g_checkhp = false;
// 旧版全局 dire：追击/游荡的朝向（0=左 1=右），击退方向也用它
i32 g_dire = 0;
// 旧版 boss3hp：世界吞噬者血量合计，每帧由 ai_boss3 累加
i32 g_boss3hp = 0;
// 旧版 fbnq[12]：递归 Boss 各层的血量
const i32 FBNQ[11] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89};

}   // namespace

// ---- 空间哈希（旧版 entity_hash[HASH_SIZE][HASH_SIZE]，HASH_CELL=16）----
namespace {

struct HashCell { i32 idx[16]; i32 n = 0; };
HashCell *g_hash[HASH_W][HASH_H] = {};

inline HashCell *hash_cell(i32 hx, i32 hy, bool create)
{
    if(hx < 0 || hx >= HASH_W || hy < 0 || hy >= HASH_H) return nullptr;
    HashCell *c = g_hash[hx][hy];
    if(!c && create)
    {
        c = new HashCell();
        g_hash[hx][hy] = c;
    }
    return c;
}

// 旧版 hash_coord()：To_int 后除 HASH_CELL 并钳位
inline i32 hash_coord(f64 v)
{
    i32 h = (i32)(v + 0.5) / HASH_CELL;
    if(h < 0) h = 0;
    if(h >= HASH_W) h = HASH_W - 1;
    return h;
}

// 用固定容量数组实现旧版的 vector<int> 桶（不依赖 STL 分配，帧内零碎片）
void bucket_add(f64 x, f64 y, i32 idx)
{
    HashCell *c = hash_cell(hash_coord(x), hash_coord(y), true);
    if(!c) return;
    if(c->n < 16) { c->idx[c->n++] = idx; return; }
    // 桶满：优先复用空槽，否则覆盖最后一个（旧版是无上限 vector，
    // 这里 16 个/16x16 格的容量在 500 实体上限下足够，覆盖只是保险）
    for(i32 i = 0; i < c->n; i++)
        if(ents[c->idx[i]].type <= 0) { c->idx[i] = idx; return; }
    c->idx[c->n - 1] = idx;
}

void bucket_remove(f64 x, f64 y, i32 idx)
{
    HashCell *c = hash_cell(hash_coord(x), hash_coord(y), false);
    if(!c) return;
    for(i32 i = 0; i < c->n; i++)
        if(c->idx[i] == idx)
        {
            for(i32 k = i; k + 1 < c->n; k++) c->idx[k] = c->idx[k + 1];
            c->n--;
            return;
        }
}

}   // namespace

// ============================================================================
//                          空间哈希对外接口
// ============================================================================

void hash_clear()
{
    for(i32 i = 0; i < HASH_W; i++)
        for(i32 j = 0; j < HASH_H; j++)
        {
            delete g_hash[i][j];
            g_hash[i][j] = nullptr;
        }
}

void hash_add(i32 idx, f64 x, f64 y)
{
    if(idx < 0 || idx >= MAX_ENT) return;
    bucket_add(x, y, idx);
}

void hash_remove(i32 idx, f64 x, f64 y)
{
    if(idx < 0 || idx >= MAX_ENT) return;
    bucket_remove(x, y, idx);
}

void hash_update(i32 idx, f64 ox, f64 oy, f64 nx, f64 ny)
{
    if(hash_coord(ox) == hash_coord(nx) && hash_coord(oy) == hash_coord(ny)) return;
    bucket_remove(ox, oy, idx);
    bucket_add(nx, ny, idx);
}

void query_nearby(f64 x, f64 y, f64 range, i32 *out, i32 maxout, i32 &n)
{
    n = 0;
    if(!out || maxout <= 0) return;
    i32 hx = hash_coord(x), hy = hash_coord(y);
    i32 rc = (i32)(range / HASH_CELL) + 1;
    f64 r2 = range * range;
    for(i32 dx = -rc; dx <= rc; dx++)
        for(i32 dy = -rc; dy <= rc; dy++)
        {
            HashCell *c = hash_cell(hx + dx, hy + dy, false);
            if(!c) continue;
            for(i32 k = 0; k < c->n; k++)
            {
                if(n >= maxout) return;
                i32 i = c->idx[k];
                if(i < 0 || i >= MAX_ENT) continue;
                if(ents[i].type <= 0) continue;   // 旧版跳过已死实体
                f64 ddx = ents[i].x - x, ddy = ents[i].y - y;
                if(ddx * ddx + ddy * ddy <= r2)
                {
                    bool dup = false;
                    for(i32 q = 0; q < n; q++)
                        if(out[q] == i) { dup = true; break; }
                    if(!dup) out[n++] = i;
                }
            }
        }
}

// ============================================================================
//                              生命周期
// ============================================================================

// 旧版 make_st 第 1639-1685 行
void spawn(i32 type, f64 hp, f64 hurt, f64 tx, f64 ty, i32 nbt, i32 iseof)
{
    i32 i = 0;
    while(i < MAX_ENT && ents[i].type != 0) i++;
    if(i >= MAX_ENT) return;

    // ---- 世界吞噬者的链式链接（旧版 iseof 1/2/3）----
    if(iseof == 1 && g_last >= 0 && g_last < MAX_ENT && g_last != i)
    {
        ents[i].conl = g_last;
        ents[g_last].conr = i;
    }
    if(iseof == 2 && g_last >= 0 && g_last < MAX_ENT && g_last != i)
    {
        ents[i].conl = i;                     // 头：自己没有前一节
        ents[g_last].conr = i;
    }
    if(iseof == 3 && g_last >= 0 && g_last < MAX_ENT && g_last != i)
    {
        ents[i].conl = g_last;
        ents[g_last].conr = i;
        ents[i].conr = i;                     // 尾：自己没有后一节
    }

    ents[i].hp_max = hp;
    ents[i].hp = hp;
    ents[i].hurt = hurt;
    ents[i].type = type;
    ents[i].x = tx;
    ents[i].y = ty;
    ents[i].vx = 0;
    ents[i].vy = 0;
    ents[i].jp = 0;
    ents[i].cd = 0;
    ents[i].nbt = nbt;
    ents[i].random_tick = 0;
    ents[i].direction = 0;

    if(type == 1) { ents[i].sight = 30; ents[i].ai = 1; }
    else if(type == 2) { ents[i].sight = 18; ents[i].ai = 1; }
    else if(type == 3) { ents[i].sight = 25; ents[i].ai = 1; }
    else if(type == 4) { ents[i].sight = 0; ents[i].ai = 2; ents[i].vx = 10; ents[i].vy = -5; }
    else if(type == 5) { ents[i].sight = 200; ents[i].ai = 1; }
    else if(type == 6) { ents[i].sight = 25; ents[i].ai = 1; }
    else if(type == 7) { ents[i].sight = 35; ents[i].ai = 1; }
    else if(type == 12) { ents[i].sight = 0; ents[i].ai = 5; }
    else if(type == 13) { ents[i].sight = 0; ents[i].ai = 5; }
    else if(type == 8)
    {
        ents[i].direction = ents[i].hp;       // 旧版用 direction 记血量基准
        if(nbt == 10) game::st().boss2hp = 689;
        ents[i].sight = 200;
        ents[i].ai = 3;
    }
    else if(type >= 9) { ents[i].sight = 300; ents[i].ai = 4; }

    hash_add(i, tx, ty);
    g_last = i;
}

// 旧版 make_jian 第 5131 行
void spawn_arrow(f64 tx, f64 ty, f64 vx, f64 vy, i32 nbt, f64 dmg)
{
    i32 i = 0;
    while(i < MAX_ENT && ents[i].type != 0) i++;
    if(i >= MAX_ENT) return;

    hash_add(i, tx, ty);
    ents[i].hp = 0;
    ents[i].hurt = dmg;
    ents[i].type = 4;
    ents[i].x = tx;
    ents[i].y = ty;
    ents[i].nbt = nbt;
    ents[i].vx = vx;
    ents[i].vy = vy;
    ents[i].jp = 0;
    ents[i].cd = 0;
    ents[i].random_tick = 0;
    ents[i].direction = 0;
    ents[i].sight = 0;
    ents[i].ai = 2;
}

// 旧版 make_eow 第 4356 行：头(iseof=2) -> 一串体节(iseof=1) -> 尾(iseof=3)
void spawn_eow(f64 px, f64 py)
{
    i32 spawn_x = (i32)px + world::rnd(31) - 15;
    spawn(T_WORM_HEAD, 20, 12, spawn_x, py + 30, 0, 2);
    for(i32 k = 1; k <= 38 + world::godeater * 30; k++)
        spawn(T_WORM_BODY, 20, 8, spawn_x, py + 30, 0, 1);
    spawn(T_WORM_TAIL, 20, 5, spawn_x, py + 30, 0, 3);
}

void clear_all()
{
    for(i32 i = 0; i < MAX_ENT; i++) ents[i].reset();
    hash_clear();
    g_last = 0;
    g_checkhp = false;
}

i32 live_count()
{
    i32 c = 0;
    for(i32 i = 0; i < MAX_ENT; i++)
        if(ents[i].type > 0) c++;
    return c;
}

// ============================================================================
//                       内部工具（旧版若干 static inline 助手）
// ============================================================================
namespace {

// 旧版 To_int
inline i32 to_int(f64 a) { return (i32)(a + 0.5); }
// 旧版 jl(abs(a), abs(b))：对两个绝对值求欧氏距离
inline f64 jl(f64 a, f64 b) { return std::sqrt(a * a + b * b); }
inline f64 jl_abs(f64 a, f64 b)
{
    if(a < 0) a = -a;
    if(b < 0) b = -b;
    return std::sqrt(a * a + b * b);
}
// 旧版 ent_oob
inline bool ent_oob(i32 ex, i32 ey)
{
    return ex < 0 || ey < 0 || ex > world::W - 1 || ey > world::H - 1;
}
// 旧版 fluid_block_id：19..22 水 / 32..34 岩浆
inline bool fluid_block_id(i16 b) { return is_liquid(b); }
// 当前维度取方块（带边界保护）
inline i16 ent_blk(i32 x, i32 y)
{
    if(x < 0 || y < 0 || x >= world::W || y >= world::H) return B_AIR;
    return world::cur(x, y);
}
// 旧版 blocktouch[...] == 1（固体）
inline bool solid_at(i32 x, i32 y) { return block_of(ent_blk(x, y)).touch == 1; }
inline bool touch_is(i32 x, i32 y, i32 v) { return block_of(ent_blk(x, y)).touch == v; }
// 旧版 ent_block：刷新实体所在方块
inline i16 ent_block(i32 l, i32 &ex, i32 &ey)
{
    ex = to_int(ents[l].x);
    ey = to_int(ents[l].y);
    return ent_blk(ex, ey);
}
// 旧版 out_of_active_area 第 5401 行：离玩家太远或出世界
inline bool out_of_active_area(f64 gx, f64 gy)
{
    f64 px = game::st().pl.x, py = game::st().pl.y;
    return gx < (i32)px - 120 || gy < (i32)py - 80 ||
           gx > (i32)px + 120 || gy > (i32)py + 80 ||
           gx < 0 || gy < 0 || gx > world::W - 1 || gy > world::H - 1;
}
// 旧版 KEY_DOWN(VK_LBUTTON)
inline bool lbutton()
{
    return platform::mouse_pressed(platform::MB_LEFT);
}
// 旧版 thing_fall 第 5248 行：掉落物品进背包（旧版是 getbeibao + 计数，这里等价）
void thing_fall(i32 many_, i32 get)
{
    for(i32 i = 0; i < many_; i++)
    {
        i32 u = item::inv_find(get);
        if(u != 0)
        {
            item::beibao[u].count++;
            item::beibao[u].id = (i16)get;
        }
    }
}

}   // namespace

// ============================================================================
//                        玩家受击（旧版 damage 第 5339 行）
// ============================================================================
namespace {

// 旧版 damage()。新版本 game 层没有暴露这个函数，这里就地实现。
// 注意：wudi 的递减由 game::tick_once() 统一负责（旧版在 game_loop 里），
//       本模块只负责"置无敌帧"。
void player_hurt(f64 dmg, f64 fromx)
{
    game::Player &p = game::st().pl;
    if(p.wudi != 0) return;
    p.wudi = 40;
    i32 ed = game::st().difficulty;
    if(ed == 8000) dmg *= 0.75;      // 简单
    if(ed == 4000) dmg *= 1.5;       // 困难
    p.life -= dmg * p.jianshang;
    p.spx = ((fromx < p.x) * 2 - 1) * 6;
    p.spy = -8;
    if(p.life <= 0)
    {
        // 旧版在此直接 Start() 并复活；新版本统一交给 game::tick_once()
        // （life<=0 -> start_respawn()），这里只把速度清掉。
        p.spx = 0;
        p.spy = 0;
        p.jump = 0;
    }
}

// ★ 耐久度接入（旧版没有）：受击时给一件已装备护甲扣耐久
void armor_take_hit(f64 dmg)
{
    if(!game::st().durability_on) return;
    i32 pick = world::rnd(4) + 1;                  // 1 胸甲 / 2 护腿 / 3 头盔 / 4 靴子
    for(i32 k = 0; k < 4; k++)
    {
        i32 s = (pick - 1 + k) % 4 + 1;
        const item::Stack &a = item::armor[s];
        if(a.id <= 0 || a.count <= 0 || a.max_dur() <= 0) continue;
        i32 id = a.id;
        i32 amt = (i32)(dmg / 4);
        if(amt < 1) amt = 1;
        if(item::dmg_armor(s, amt))
            game::toast("你的%s已损坏", block_of(id).cn);
        break;
    }
}

}   // namespace

// ============================================================================
//                              爆炸冲击波
// ============================================================================

// 旧版 boomthings bz[1000] / zc[1000]，这里收进匿名命名空间
namespace {

struct BoomThing { i32 x = 0, y = 0, color = 0; };
constexpr i32 BOOM_MAX = 299;       // 旧版遍历上限（数组本身 1000，保留即可）
BoomThing bz[1000];
BoomThing zc[1000];

// 旧版 Clear()：视野内空气填天空（旧版是全局函数，只按玩家 x 更新）。
// world::clear_around(cx) 已存在，但它没有出现在 world.h 里，从 entity.cpp 直接
// 声明会与 game.h 的 game::clear_view() 撞符号，所以这里用文件内 static 版本。
static void clear_around(f64 cx)
{
    if(world::dim == world::DIM_NETHER) return;
    i32 c = to_int(cx);
    if(c < 0) c = 0;
    if(c >= world::W) c = world::W - 1;
    for(i32 i = c - 99; i <= c + 100; i++)
    {
        if(i < 0 || i >= world::W) continue;
        for(i32 j = 0; j <= 180; j++)
            if(world::s[i][j] == B_AIR) world::s[i][j] = B_SKY;
    }
}

// 旧版 ischanging 的那段：爆炸范围内的实体受伤
void boom_hurt_ents(i32 bx, i32 by, f64 scale)
{
    for(i32 k = 0; k < MAX_ENT; k++)
    {
        if(ents[k].type <= 0) continue;
        f64 d = jl(std::fabs(ents[k].x - bx), std::fabs(ents[k].y - by));
        if(d <= 8) ents[k].hp -= (8 - d) * scale;
    }
}

}   // namespace

// 旧版 boom_add 第 614 行：入队一个爆炸特效（同时炸掉所在方块）
void explode_add(i32 i, i32 j, i32 col)
{
    if(i < 0 || i >= world::W || j < 0 || j >= world::H) return;
    i32 k = 0;
    while(bz[k].color != 0 && k < BOOM_MAX) k++;
    if(k >= BOOM_MAX) return;
    bz[k].color = col;
    bz[k].x = i;
    bz[k].y = j;
    if(block_of(world::get(i, j)).dig != -1 && bz[k].color != 1)
    {
        world::set(i, j, B_AIR);          // 旧版 s[i][j]=0 + mark_dirty_col
    }
    // 爆炸会伤到附近的实体（旧版在 boom_add 里没有，但箭矢自爆与脚本爆炸
    // 各自处理；统一放在 explode_tick 的 -4 阶段，见下）
}

// 旧版 boom_add_ready 第 628 行
void explode_ready(i32 i, i32 j, i32 col)
{
    if(i < 0 || i >= world::W || j < 0 || j >= world::H) return;
    i32 k = 0;
    while(zc[k].color != 0 && k < 99) k++;
    if(k >= 99) return;
    zc[k].color = col;
    zc[k].x = i;
    zc[k].y = j;
}

// 旧版 boom_loop 第 636 行：推进一轮爆炸动画并结算下一批
void explode_tick()
{
    for(i32 i = 0; i < BOOM_MAX; i++)
    {
        if(bz[i].color == 0) continue;
        if(bz[i].color == -4)
        {
            i32 nx = bz[i].x, ny = bz[i].y;
            bz[i].color = 0; bz[i].y = 0; bz[i].x = 0;
            // 中心 3x3 -> 火花，外圈 -> 方块清除环
            for(i32 dx = -1; dx <= 1; dx++)
                for(i32 dy = -1; dy <= 1; dy++)
                    explode_ready(nx + dx, ny + dy, -3);
            explode_ready(nx + 2, ny - 1, 1);
            explode_ready(nx + 2, ny, 1);
            explode_ready(nx + 2, ny + 1, 1);
            explode_ready(nx - 2, ny - 1, 1);
            explode_ready(nx - 2, ny, 1);
            explode_ready(nx - 2, ny + 1, 1);
            explode_ready(nx + 1, ny + 2, 1);
            explode_ready(nx - 1, ny + 2, 1);
            explode_ready(nx, ny + 2, 1);
            explode_ready(nx + 1, ny - 2, 1);
            explode_ready(nx - 1, ny - 2, 1);
            explode_ready(nx, ny - 2, 1);
        }
        if(bz[i].color == -3)
        {
            i32 nx = bz[i].x, ny = bz[i].y;
            bz[i].color = 0; bz[i].y = 0; bz[i].x = 0;
            explode_ready(nx, ny, -5);
        }
        if(bz[i].color == 1)
        {
            i32 nx = bz[i].x, ny = bz[i].y;
            bz[i].color = 0; bz[i].y = 0; bz[i].x = 0;
            explode_ready(nx, ny, -5);
        }
        if(bz[i].color == -5)
        {
            bz[i].color = 0; bz[i].y = 0; bz[i].x = 0;
        }
    }
    for(i32 i = 0; i < 99; i++)
    {
        if(zc[i].color == 0) continue;
        if(zc[i].color == -4)
        {
            // 旧版：-4 是"真正的爆炸"，入队时炸方块；这里补一次实体伤害
            boom_hurt_ents(zc[i].x, zc[i].y, 0.5);
        }
        explode_add(zc[i].x, zc[i].y, zc[i].color);
        zc[i].x = 0; zc[i].y = 0; zc[i].color = 0;
    }
    clear_around(game::st().pl.x);
}

void explode_reset()
{
    for(i32 i = 0; i < 1000; i++) { bz[i] = BoomThing(); zc[i] = BoomThing(); }
}

// ============================================================================
//                              伤害与死亡
// ============================================================================

// 旧版 dmg_st 第 5360 行
void damage_ent(i32 idx, f64 dmg)
{
    if(idx < 0 || idx >= MAX_ENT) return;
    ents[idx].hp -= dmg;
    ents[idx].wd = 20;
}

// 旧版 st_kill 第 5257 行
// ★ 关键：AI==4（世界吞噬者）的链式死亡——头尾变体与 conl/conr 重连
void kill_ent(i32 l)
{
    if(l < 0 || l >= MAX_ENT) return;
    if(ents[l].type == 0) return;

    // 爆炸箭（nbt 1 木箭其实不带爆炸，旧版照抄：nbt==1 || nbt==3 都会炸）
    if(ents[l].type == T_ARROW && (ents[l].nbt == 1 || ents[l].nbt == 3))
    {
        explode_add((i32)(ents[l].x + 0.5), (i32)(ents[l].y + 0.5), -4);
        game::Player &p = game::st().pl;
        if(p.wudi == 0)
        {
            f64 d = jl(std::fabs(p.x - ents[l].x), std::fabs(p.y - ents[l].y));
            if(d <= 8)
            {
                p.life -= (8 - d) * 0.5 * p.jianshang;
                p.wudi = 45;
            }
        }
    }

    if(ents[l].ai == 4)
    {
        thing_fall(world::rnd(2) + 1, B_SHADOW_SCALE);
        i32 cl = ents[l].conl, cr = ents[l].conr;
        bool has_l = (cl >= 0 && cl < MAX_ENT && cl != l);
        bool has_r = (cr >= 0 && cr < MAX_ENT && cr != l);
        if(has_l && l == cl)
        {
            // 自己就是头：后一节要么删掉（本来就是尾），要么升格成新头
            if(has_r)
            {
                if(ents[cr].type == T_WORM_TAIL)
                {
                    hash_remove(cr, ents[cr].x, ents[cr].y);
                    ents[cr].reset();
                }
                else
                {
                    ents[cr].type = T_WORM_HEAD;
                    ents[cr].conl = cr;
                }
            }
        }
        else if(has_r && l == cr)
        {
            // 自己就是尾：前一节要么删掉（本来就是头），要么降格成新尾
            if(has_l)
            {
                if(ents[cl].type == T_WORM_HEAD)
                {
                    hash_remove(cl, ents[cl].x, ents[cl].y);
                    ents[cl].reset();
                }
                else
                {
                    ents[cl].type = T_WORM_TAIL;
                    ents[cl].conr = cl;
                }
            }
        }
        else
        {
            // 体节：两边的邻居各自补位
            if(has_l)
            {
                if(ents[cl].type == T_WORM_HEAD)
                {
                    hash_remove(cl, ents[cl].x, ents[cl].y);
                    ents[cl].reset();
                }
                else
                {
                    ents[cl].type = T_WORM_TAIL;
                    ents[cl].conr = cl;
                }
            }
            if(has_r)
            {
                if(ents[cr].type == T_WORM_TAIL)
                {
                    hash_remove(cr, ents[cr].x, ents[cr].y);
                    ents[cr].reset();
                }
                else
                {
                    ents[cr].type = T_WORM_HEAD;
                    ents[cr].conl = cr;
                }
            }
        }
    }

    hash_remove(l, ents[l].x, ents[l].y);
    ents[l].reset();
}

// 旧版 st_kill_fall 第 5318 行：先掉落再删除
void kill_ent_drop(i32 l)
{
    if(l < 0 || l >= MAX_ENT) return;
    i32 t = ents[l].type;
    if(t == T_ZOMBIE) thing_fall(world::rnd(2) + 1, B_ROTTEN_FLESH);
    if(t == T_CREEPER) thing_fall(world::rnd(2) + 1, B_GUNPOWDER);
    if(t == T_SKELETON)
    {
        thing_fall(world::rnd(2) + 1, B_WOOD_ARROW);
        if(world::rnd(10) == 0) thing_fall(1, B_BOW);
    }
    if(t == T_SPIDER) thing_fall(world::rnd(2) + 1, B_STRING);
    if(t == T_PIGLIN)
    {
        thing_fall(world::rnd(2) + 1, B_ROTTEN_FLESH);
        thing_fall(world::rnd(2) + 1, B_GOLD_NUGGET);
    }
    if(t == T_OI)
    {
        if(world::rnd(2) == 0) thing_fall(1, B_OI_SWORD);
        else thing_fall(1, B_STORM_BOW);
    }
    if(t == T_SHEEP)
    {
        thing_fall(1, B_WOOL);
        thing_fall(world::rnd(2) + 1, B_MUTTON);
    }
    if(t == T_PIG) thing_fall(world::rnd(3) + 1, B_PORK);
    kill_ent(l);
}

// 旧版 touch_atk 第 5365 行：接触攻击 + ★ 护甲耐久
void touch_attack(i32 idx, f64 dmg, f64 dist)
{
    if(idx < 0 || idx >= MAX_ENT) return;
    game::Player &p = game::st().pl;
    if(std::fabs(p.x - ents[idx].x) < dist && std::fabs(p.y - ents[idx].y) < dist &&
            ents[idx].cd <= 0)
    {
        ents[idx].cd = 90;
        if(p.wudi == 0) armor_take_hit(dmg);   // ★ 只有真正吃到伤害才扣护甲
        player_hurt(dmg, ents[idx].x);
    }
}

// ============================================================================
//                               AI 状态机
// ============================================================================

// 旧版 ent_update_state（定义在 AI 分支之后，这里前置声明）
void ent_update_state(i32 l);

namespace {

// 旧版 boss2_die_split 第 5407 行
void boss2_die_split(i32 l, f64 old_x, f64 old_y)
{
    game::st().boss2hp -= (i32)ents[l].direction;
    i32 xxxx = (i32)ents[l].x, yyyy = (i32)ents[l].y, nbtt = ents[l].nbt - 1;
    hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
    kill_ent(l);
    if(nbtt >= 1)
    {
        if(nbtt >= 0 && nbtt < 11)
            spawn(T_RECURSIVE, (f64)FBNQ[nbtt], 1 + (nbtt / 2.0),
                  xxxx + world::rnd(5) - 2, yyyy + world::rnd(5) - 2, nbtt);
        if(nbtt - 1 >= 0)
            spawn(T_RECURSIVE, (f64)FBNQ[nbtt - 1], 1 + (nbtt / 2.0),
                  xxxx + world::rnd(5) - 2, yyyy + world::rnd(5) - 2, nbtt - 1);
    }
}

// 旧版 ai1_player_click 第 5424 行；返回 false 表示实体已死并被移除
bool ai1_player_click(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    if(!(lbutton() && (std::fabs(p.mpx - ents[l].x) < 2 && std::fabs(p.mpy - ents[l].y) < 2) &&
            p.playerCD <= 0))
        return true;
    if(!(std::fabs(p.x - ents[l].x) < 4.5 && std::fabs(p.y - ents[l].y) < 4.5))
        return true;

    if(ents[l].type == T_OI && ents[l].islife2 == 1)
        ;                                    // 二阶段 OI 免疫近战
    else
    {
        if(p.spy > 2)
        {
            damage_ent(l, p.gongji * 1.5);
            if(ents[l].type != T_OI) { ents[l].vy = -8; p.gj = 2; }
        }
        else
        {
            damage_ent(l, p.gongji);
            if(ents[l].type != T_OI) { ents[l].vy = -6; p.gj = 1; }
            if(p.issword)
            {
                // 剑的横扫：打到鼠标格附近的其它实体
                for(i32 ll = 0; ll < MAX_ENT; ll++)
                {
                    if(ents[ll].type <= 0 || ll == l) continue;
                    if(std::fabs(ents[ll].x - p.mpx) <= 0.75 &&
                            std::fabs(ents[ll].y - p.mpy) <= 1.5)
                    {
                        damage_ent(ll, 1);
                        if(ents[ll].type != T_OI)
                        {
                            ents[ll].vy = -6;
                            p.gj = 1;
                            ents[ll].vx += ((p.x < ents[l].x) * 2 - 1) * 0.4;
                        }
                        if(ents[ll].hp <= 0)
                        {
                            if(ents[ll].type == T_OI && ents[ll].islife2 == 0)
                            {
                                ents[ll].islife2 = true;
                                ents[ll].hp = 1;
                                ents[ll].attack_tick = 0;
                            }
                            else
                            {
                                kill_ent_drop(ll);
                            }
                        }
                    }
                }
            }
        }
    }
    p.playerCD = p.gongjicd;
    ents[l].chv = 900;
    ents[l].issd = true;
    if(ents[l].type == T_PIGLIN)
    {
        // 猪灵被激怒会喊附近的同类
        ents[l].ch = true;
        static i32 nearby[512];
        i32 n = 0;
        query_nearby(p.x, p.y, ents[l].sight, nearby, 512, n);
        for(i32 k = 0; k < n; k++)
        {
            i32 i = nearby[k];
            if(ents[i].type == T_PIGLIN &&
                    jl_abs(ents[i].x - p.x, ents[i].y - p.y) < ents[l].sight)
                ents[i].ch = true;
        }
    }
    if(ents[l].hp <= 0)
    {
        if(ents[l].type == T_OI && ents[l].islife2 == 0)
        {
            ents[l].islife2 = true;
            ents[l].hp = 1;
            ents[l].attack_tick = 0;
        }
        else
        {
            hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
            kill_ent_drop(l);
            return false;
        }
    }
    return true;
}

// 旧版 ai1_despawn_check 第 5506 行；返回 false 表示实体已移除
bool ai1_despawn_check(i32 l, f64 old_x, f64 old_y)
{
    if(ents[l].type <= 4 || ents[l].type >= 6)
    {
        if(ents[l].hp <= 0 || out_of_active_area(ents[l].x, ents[l].y))
        {
            hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
            kill_ent(l);
            return false;
        }
    }
    else if(ents[l].type == T_OI && ents[l].hp <= 0)
    {
        if(ents[l].islife2 == 0)
        {
            ents[l].islife2 = true;
            ents[l].hp = 1;
            ents[l].attack_tick = 0;
        }
        else
        {
            hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
            kill_ent_drop(l);
            return false;
        }
    }
    return true;
}

// 旧版 ent_physics_ground 第 5536 行；返回是否着地
bool ent_physics_ground(i32 l)
{
    game::Player &p = game::st().pl;
    i32 ex = 0, ey = 0;
    i16 eb = ent_block(l, ex, ey);
    bool grounded = false;

    if(fluid_block_id(eb))
    {
        // 液体里：岩浆会烧（OI 与猪灵免疫）
        if(eb >= B_LAVA && eb <= B_LAVA1 && ents[l].type != T_OI && ents[l].type != T_PIGLIN)
            damage_ent(l, 0.25);
        ents[l].vy += GRAVITY;
        if(ents[l].type <= 3) ents[l].vx *= 0.5;
        ents[l].vy *= 0.75;
        if(ents[l].type == T_CREEPER) ents[l].vy = -3;
        if(ents[l].type == T_OI)
        {
            // 凋灵在水里也维持悬浮
            if(ents[l].y < p.y) ents[l].vy = 8;
            else ents[l].vy = -8;
        }
        ents[l].y += ents[l].vy * 0.02;
    }
    else
    {
        ents[l].vy += GRAVITY;
        ents[l].y += ents[l].vy * 0.06 * ENT_SPEED;
        ents[l].vy *= 0.98;
        if(ents[l].type == T_OI && ents[l].hp <= 250)
        {
            // 二阶段凋灵：贴着玩家上方 5~6 格悬停，沿途啃掉方块
            if(ents[l].y > p.y - 6 && ents[l].y < p.y - 5) ents[l].vy = 0;
            else if(ents[l].y > p.y - 5) ents[l].vy = -6;
            else ents[l].vy = 6;
            eb = ent_block(l, ex, ey);
            if(block_of(eb).touch == 1)
            {
                world::set(ex, ey, B_AIR);
                clear_around(p.x);
            }
        }
    }

    eb = ent_block(l, ex, ey);
    if(block_of(eb).touch == 1)
    {
        if(ents[l].vy >= 0)
        {
            i32 guard = 0;
            while(solid_at(to_int(ents[l].x), to_int(ents[l].y)) && guard++ < 400)
                ents[l].y -= 0.1;
            ents[l].jp = 0;
            ents[l].vy = 0.01;
            grounded = true;
        }
        if(ents[l].vy < 0)
        {
            i32 guard = 0;
            while(solid_at(to_int(ents[l].x), to_int(ents[l].y)) && guard++ < 400)
                ents[l].y += 0.1;
            ents[l].vy = 2;
        }
    }

    // 僵尸/OI 在玩家正下方时踩头上跳（只在**已经锁定玩家**时，避免漫无目的地乱跳）
    bool isjump = true, isdown = false;
    if(ents[l].chv > 0 && std::fabs(p.x - ents[l].x) < 1.5 &&
            (ents[l].type == T_ZOMBIE || ents[l].type == T_OI))
    {
        if(p.y < ents[l].y)
        {
            for(i32 i = -3; i <= -1; i++)
                if(touch_is(to_int(ents[l].x), to_int(ents[l].y + i), 1)) isjump = false;
            if(isjump && ents[l].jp == 0) ents[l].jp = 1;
        }
    }
    if(p.y > ents[l].y) isdown = true;

    // 单向平台（touch==2）
    eb = ent_block(l, ex, ey);
    if(block_of(eb).touch == 2 && ents[l].y - (i32)ents[l].y > 0.5)
    {
        if(ents[l].vy >= 0 && !isdown)
        {
            i32 guard = 0;
            while(touch_is(to_int(ents[l].x), to_int(ents[l].y), 2) &&
                    ents[l].y - (i32)ents[l].y > 0.5 && guard++ < 400)
                ents[l].y -= 0.05;
            ents[l].jp = 0;
            ents[l].vy = 0.01;
            grounded = true;
        }
    }

    // 视线检测（旧版每 5 tick 一次）
    if(world::tick % 5 == 0 && !ents[l].issd)
    {
        f64 total = 25, px, py;
        for(f64 ddx = 0, ddy = total; ddx <= total && !ents[l].issd; ddx++)
        {
            ddy = total - ddx;
            for(i32 i = 1; i <= 4 && !ents[l].issd; i++)
            {
                px = ents[l].x;
                py = ents[l].y;
                for(f64 ll = 0; ll <= ents[l].sight; ll += 0.25)
                {
                    if(i == 1) { px += ddx / 100; py += ddy / 100; }
                    else if(i == 2) { px += -ddx / 100; py += ddy / 100; }
                    else if(i == 3) { px += ddx / 100; py += -ddy / 100; }
                    else { px += -ddx / 100; py += -ddy / 100; }
                    if(ent_oob(to_int(px), to_int(py))) break;      // 越界保护
                    if(solid_at(to_int(px), to_int(py))) break;
                    if(std::fabs(px - p.x) < 0.5 && std::fabs(py - p.y) < 0.5)
                    {
                        ents[l].chv = 600;
                        ents[l].issd = true;
                        break;
                    }
                }
            }
        }
    }

    if(ents[l].chv > 0) ents[l].chv--;
    else ents[l].issd = false;
    if(ents[l].type == T_OI) { ents[l].issd = true; ents[l].chv = 300; }

    if(ents[l].chv > 0 && (ents[l].type != T_PIGLIN || ents[l].ch))
    {
        // 追击：朝玩家加速
        if(ents[l].x > p.x) { ents[l].vx -= 0.007; g_dire = 1; }
        else { ents[l].vx += 0.007; g_dire = 0; }
        ents[l].direction = 1;
    }
    else
    {
        // 游荡
        ents[l].vx += 0.004 * ents[l].direction;
        if(ents[l].direction == 0) ents[l].random_tick += 2;
        ents[l].random_tick++;
        if(ents[l].random_tick > 300)
        {
            ents[l].random_tick = 0;
            if(ents[l].direction != 0) ents[l].direction = 0;
            else ents[l].direction = (world::rnd(2)) * 2 - 1;
        }
        if(ents[l].direction == -1) g_dire = 0;
        else g_dire = 1;
    }
    if(p.gj == 1) ents[l].vx += ((g_dire) * 2 - 1) * 0.4;
    else if(p.gj == 2) ents[l].vx += ((g_dire) * 2 - 1) * 0.6;

    const f64 prev_x = ents[l].x;
    eb = ent_block(l, ex, ey);
    if(eb >= B_WATER && eb <= B_FLOW1) ents[l].x += ents[l].vx * 0.5 * ENT_SPEED;
    else ents[l].x += ents[l].vx * ENT_SPEED;
    // 水平碰撞：坐标一旦落进固体格就退回原位（旧版有 x_touch，这里之前漏了）。
    // 没有它怪物会走进墙里；卡进方块后视线检测第一步就被墙 break 掉，
    // 于是怪物既追不到玩家，骷髅也永远射不出箭。
    if(solid_at(to_int(ents[l].x), to_int(ents[l].y)))
    {
        ents[l].x = prev_x;
        ents[l].vx = 0;
        if(ents[l].direction == -1) ents[l].direction = 1;    // 撞墙掉头
        else if(ents[l].direction == 1) ents[l].direction = -1;
    }
    // 陆地怪避水：前方落点或脚下是液体就不进入该方向（同撞水一样退回原位，不走进水里）。
    // 凋灵 OI、世界吞噬者等飞行 Boss 不受此限制。已经在液体里的不拦，让它能走出来。
    if(ents[l].type == T_ZOMBIE || ents[l].type == T_SKELETON ||
            ents[l].type == T_CREEPER || ents[l].type == T_SPIDER)
    {
        i32 nx = to_int(ents[l].x), ny = to_int(ents[l].y);
        if(!fluid_block_id(ent_blk(to_int(prev_x), ny)) &&
                (fluid_block_id(ent_blk(nx, ny)) || fluid_block_id(ent_blk(nx, ny + 1))))
        {
            ents[l].x = prev_x;
            ents[l].vx = 0;
        }
    }
    ents[l].vx *= 0.9;

    if(ents[l].type == T_OI && ents[l].hp <= 250)
    {
        eb = ent_block(l, ex, ey);
        if(block_of(eb).touch == 1)
        {
            world::set(ex, ey, B_AIR);
            clear_around(p.x);
        }
    }

    // 前方是断崖就跳（只在追击玩家时才跳；没锁定玩家就老实游荡，不蹦跶）
    isjump = true;
    for(i32 i = 1; i <= 3; i++)
        if(touch_is(to_int(ents[l].x - ((g_dire) * 2 - 1)), to_int(ents[l].y + i), 1))
            isjump = false;
    if(isjump && ents[l].chv > 0 && ents[l].jp == 0) ents[l].jp = 1;

    // 卡墙：左右试探找出可站的位置
    eb = ent_block(l, ex, ey);
    if(block_of(eb).touch == 1)
    {
        i32 step = 1;
        while(step < 60)
        {
            ents[l].x -= 0.03 * step;
            if(!solid_at(to_int(ents[l].x), to_int(ents[l].y))) break;
            ents[l].x += 0.06 * step;
            if(!solid_at(to_int(ents[l].x), to_int(ents[l].y))) break;
            ents[l].x -= 0.03 * step;
            step++;
        }
        if(ents[l].jp == 0) ents[l].jp = 1;
        ents[l].vx = 0;
    }
    else if(ents[l].type == T_SPIDER)
    {
        ents[l].jp = 0;
    }
    return grounded;
}

// 旧版 ai1_script 第 5726 行：Boss 脚本阶段
void ai1_script(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    if(ents[l].islife2 == 1)
    {
        // ---- 二阶段：地面砸击 -> 转移 -> 冲刺 ----
        if(ents[l].attack_tick <= 180)
        {
            if((i32)ents[l].attack_tick % 30 == 0)
                explode_add((i32)(ents[l].x + world::rnd(3) - 1 + 0.5),
                            (i32)(ents[l].y + world::rnd(3) - 1 + 0.5), -4);
            ents[l].target_x = p.x;
            ents[l].target_y = p.y - 8;
        }
        else if(ents[l].attack_tick <= 360)
        {
            ents[l].x += (ents[l].target_x - ents[l].x) / 12.0;
            ents[l].y += (ents[l].target_y - ents[l].y) / 12.0;
        }
        else
        {
            if((i32)ents[l].attack_tick % 30 == 0)
            {
                f64 ttx = std::fabs(p.x - ents[l].x);
                f64 tty = std::fabs(p.y - ents[l].y);
                f64 total = ttx + tty + 0.001;
                f64 px = ttx / total, py = tty / total;
                if(p.x - ents[l].x < 0) px *= -1;
                if(p.y - ents[l].y < 0) py *= -1;
                ents[l].vx = 90 * px;
                ents[l].vy = 90 * py * 1.5;
            }
            if((i32)ents[l].attack_tick % 30 == 29)
                explode_add((i32)(ents[l].x + 0.5), (i32)(ents[l].y + 0.5), -4);
            ents[l].x += ents[l].vx * 0.02 * ENT_SPEED;
            ents[l].y += ents[l].vy * 0.02;
            ents[l].vy *= 0.92;
            ents[l].vx *= 0.92;
        }
        if(ents[l].attack_tick >= 900)
        {
            ents[l].hp = 0;
            explode_add((i32)(ents[l].x + 0.5), (i32)(ents[l].y + 0.5), -4);
        }
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
    }
    else
    {
        // ---- 一阶段：俯冲攻击 ----
        if(ents[l].attack_tick <= 810)
        {
            ents[l].x += (p.x - ents[l].x) / 6.0;
            ents[l].y += (p.y - ents[l].y - 15) / 6.0;
            if(ents[l].attack_tick == 810)
            {
                ents[l].x += (p.x - ents[l].x) / 2.0;
                ents[l].target_y = p.y;
                ents[l].vy = 15;
            }
        }
        else if(ents[l].attack_tick >= 820 && ents[l].hp > 250)
        {
            ents[l].vy += GRAVITY * 2;
            ents[l].y += ents[l].vy * 0.04;
            if(ents[l].y >= ents[l].target_y && solid_at(to_int(ents[l].x), to_int(ents[l].y)))
            {
                ents[l].isnotouch = false;
                ents[l].y -= ents[l].vy * 0.02;
                explode_add((i32)(ents[l].x + 0.5), (i32)(ents[l].y + 0.5), -4);
            }
        }
        else if(ents[l].attack_tick >= 820 && ents[l].hp <= 250)
        {
            if((i32)ents[l].attack_tick % (60 - (ents[l].hp <= 125) * 20) == 0)
            {
                f64 ttx = std::fabs(p.x - ents[l].x);
                f64 tty = std::fabs(p.y - ents[l].y);
                f64 total = ttx + tty + 0.001;
                f64 px = ttx / total, py = tty / total;
                if(p.x - ents[l].x < 0) px *= -1;
                if(p.y - ents[l].y < 0) py *= -1;
                f64 sp = 60 + (ents[l].hp <= 75) * 20;
                ents[l].vx = sp * px;
                ents[l].vy = sp * py * 1.5;
            }
            ents[l].x += ents[l].vx * 0.02 * ENT_SPEED;
            ents[l].y += ents[l].vy * 0.02;
            ents[l].vy *= 0.92;
            ents[l].vx *= 0.92;
        }
    }
}

// 旧版 ai1_attacks 第 5811 行；返回 false 表示实体已死并被移除
bool ai1_attacks(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    if(ents[l].type == T_ZOMBIE || ents[l].type == T_SPIDER ||
            (ents[l].type == T_PIGLIN && ents[l].ch))
    {
        touch_attack(l, ents[l].hurt, 0.75);
        return true;
    }
    if(ents[l].type == T_CREEPER)
    {
        // 引信：CD<0 起爆倒计时
        if(std::fabs(p.x - ents[l].x) < 8 && std::fabs(p.y - ents[l].y) < 8 && ents[l].chv > 0)
        {
            if(ents[l].cd < 0) { ents[l].cd--; ents[l].vx = 0; }
            if(ents[l].cd < -100)
            {
                if(p.wudi == 0)
                {
                    f64 d = jl_abs(p.x - ents[l].x, p.y - ents[l].y);
                    if(d <= 6) player_hurt((6 - d) * 5, ents[l].x);
                }
                explode_add(to_int(ents[l].x), to_int(ents[l].y), -4);
                clear_around(p.x);
                for(i32 ll = 0; ll < MAX_ENT; ll++)
                {
                    if(ents[ll].type <= 0) continue;
                    f64 d = jl_abs(ents[l].x - ents[ll].x, ents[l].y - ents[ll].y);
                    if(d <= 8) ents[ll].hp -= (8 - d) * 0.1 * ents[l].hurt;
                }
                hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
                kill_ent(l);
                return false;
            }
            if(std::fabs(p.x - ents[l].x) < 4 && std::fabs(p.y - ents[l].y) < 4)
                if(ents[l].cd == 0) ents[l].cd--;
        }
        else
        {
            ents[l].cd = 0;
        }
        return true;
    }
    if(ents[l].type == T_SKELETON)
    {
        f64 d = jl_abs(ents[l].x - p.x, ents[l].y - p.y);
        if(d < ents[l].sight * 1.25 && ents[l].chv > 0)
        {
            ents[l].cd--;
            if(d <= 8) ents[l].vx = ((p.x < ents[l].x) * 2 - 1) * 0.05;
            else if(d <= 10) ents[l].vx = 0;
            if(ents[l].cd <= -120 + (ents[l].sight * 2.5 - d * 3))
            {
                ents[l].cd = 0;
                f64 ttx = std::fabs(p.x - ents[l].x);
                f64 tty = std::fabs(p.y - ents[l].y);
                f64 total = ttx + tty + 0.001;
                f64 px = ttx / total, py = tty / total;
                if(p.x - ents[l].x < 0) px *= -1;
                if(p.y - ents[l].y < 0) py *= -1;
                spawn_arrow(ents[l].x, ents[l].y - 0.75, 30 * px, 50 * py, 0, 3);
            }
        }
        return true;
    }
    if(ents[l].type == T_OI)
    {
        // 凋灵：弹幕时间线 gongjitick 0..1020
        f64 dd = jl_abs(ents[l].x - p.x, ents[l].y - p.y);
        if(dd < ents[l].sight)
        {
            game::st().bosshp = (i32)ents[l].hp;
            ents[l].attack_tick++;
            touch_attack(l, ents[l].hurt, 0.75);
            if(ents[l].attack_tick < 240) ents[l].attack_tick++;
            if(ents[l].islife2 == 1)
            {
                if(ents[l].attack_tick == 2)
                {
                    ents[l].isnotouch = true;
                    ents[l].target_x = p.x;
                    ents[l].target_y = p.y;
                }
                return true;
            }
            if(ents[l].attack_tick >= 240 && ents[l].attack_tick <= 480 && ents[l].cd <= 0)
            {
                ents[l].cd = (i32)(50 - (500 - ents[l].hp) / 20);
                f64 ttx = std::fabs(p.x - ents[l].x);
                f64 tty = std::fabs(p.y - ents[l].y);
                f64 total = ttx + tty + 0.001;
                f64 px = ttx / total, py = tty / total;
                if(p.x - ents[l].x < 0) px *= -1;
                if(p.y - ents[l].y < 0) py *= -1;
                if(ents[l].hp > 200)
                    spawn_arrow(ents[l].x, ents[l].y,
                                ((500 - ents[l].hp) / 25 + 25) * px,
                                ((500 - ents[l].hp) / 25 + 25) * py * 1.5, 4, 3);
                else
                {
                    spawn_arrow(ents[l].x, ents[l].y, -40, 0, 4, 3);
                    spawn_arrow(ents[l].x, ents[l].y, 40, 0, 4, 3);
                    spawn_arrow(ents[l].x, ents[l].y, 0, -60, 4, 3);
                    spawn_arrow(ents[l].x, ents[l].y, 0, 60, 4, 3);
                    spawn_arrow(ents[l].x, ents[l].y, -20, 30, 4, 3);
                    spawn_arrow(ents[l].x, ents[l].y, 20, -30, 4, 3);
                    spawn_arrow(ents[l].x, ents[l].y, -20, -30, 4, 3);
                    spawn_arrow(ents[l].x, ents[l].y, 20, 30, 4, 3);
                }
            }
            if(ents[l].attack_tick >= 480 && ents[l].attack_tick <= 720 && ents[l].cd <= 0)
            {
                ents[l].cd = (i32)(100 - (500 - ents[l].hp) / 10);
                if(ents[l].hp > 150 || (ents[l].hp > 75 && (world::rnd(2) == 0)))
                {
                    // 天降箭雨
                    for(i32 vx = -30; vx <= 30; vx += 5)
                        spawn_arrow(p.x + vx, p.y - 25, 0, (500 - ents[l].hp) / 50, 5, 3);
                }
                else
                {
                    if(ents[l].hp <= 75)
                        for(i32 vx = -30; vx <= 30; vx += 5)
                            spawn_arrow(p.x + vx, p.y - 25, 0, (500 - ents[l].hp) / 50, 5, 3);
                    i32 ran__ = world::rnd(2) * 2 - 1;
                    for(i32 vy = -30; vy <= 30; vy += 6)
                        spawn_arrow(p.x + ran__ * 45, p.y + vy,
                                    ((500 - ents[l].hp) / 50 + 15) * -ran__, 0, 4, 3);
                }
            }
            if(ents[l].attack_tick >= 800 &&
                    ents[l].attack_tick <= 980 + (ents[l].hp <= 250) * 120 +
                    (ents[l].hp <= 125) * 120)
            {
                if(ents[l].attack_tick == 800)
                {
                    ents[l].vy = 0;
                    ents[l].vx = 0;
                    ents[l].cd = 120;
                    ents[l].isnotouch = true;
                    ents[l].target_y = p.y;
                }
            }
            if(ents[l].attack_tick > 980 + (ents[l].hp <= 250) * 120 +
                    (ents[l].hp <= 125) * 120)
            {
                ents[l].vy = 0;
                ents[l].vx = 0;
                ents[l].attack_tick = 0;
                ents[l].isnotouch = false;
            }
        }
        return true;
    }
    return true;
}

// 旧版 ai_ground 第 5978 行；返回 false 表示实体已移除
bool ai_ground(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    p.gj = 0;
    if(!ai1_player_click(l, old_x, old_y)) return false;
    if(!ai1_despawn_check(l, old_x, old_y)) return false;
    if(ents[l].cd > 0) ents[l].cd--;
    if(ents[l].jp == 1)
    {
        if(ents[l].type == T_OI) ents[l].vy = -10;
        else if(ents[l].type == T_SPIDER) ents[l].vy = -4;
        else ents[l].vy = -10;
        if(ents[l].type != T_SPIDER) ents[l].jp = 2;
    }
    ent_update_state(l);
    if(ents[l].state == ST_SCRIPT)
    {
        ai1_script(l, old_x, old_y);
        return ai1_attacks(l, old_x, old_y);
    }
    ent_physics_ground(l);
    return ai1_attacks(l, old_x, old_y);
}

// 旧版 ai_projectile 第 6049 行；返回 false 表示已移除
bool ai_projectile(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    if(out_of_active_area(ents[l].x, ents[l].y))
    {
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
        kill_ent(l);
        return false;
    }
    ents[l].random_tick++;
    if(ents[l].random_tick > 1200)
    {
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
        kill_ent(l);
        return false;
    }
    if(ents[l].nbt <= 3)
    {
        ents[l].vx *= 0.995;
        ents[l].x += ents[l].vx * 0.03 * ENT_SPEED;
    }
    else
    {
        ents[l].x += ents[l].vx * 0.03 * ENT_SPEED;
    }
    i32 ex = 0, ey = 0;
    i16 eb = ent_block(l, ex, ey);
    if(block_of(eb).touch == 1 && ents[l].nbt != 4 && ents[l].nbt != 5)
    {
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
        kill_ent(l);
        return false;
    }
    if(std::fabs(p.x - ents[l].x) < 0.75 && std::fabs(p.y - ents[l].y) < 0.75 &&
            ents[l].nbt != 2 && ents[l].nbt != 3)
    {
        player_hurt(3, ents[l].x);
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
        kill_ent(l);
        return false;
    }
    if(ents[l].nbt == 2)
    {
        // 玩家射出的箭：命中第一个非弹道实体
        for(i32 ll = 0; ll < MAX_ENT; ll++)
        {
            if(ents[ll].type <= 0 || ll == l) continue;
            if(std::fabs(ents[ll].x - ents[l].x) < 1.5 &&
                    std::fabs(ents[ll].y - ents[l].y) < 1.5 && ents[ll].ai != 2)
            {
                if(ents[ll].type != T_OI && ents[ll].type < 9)
                {
                    ents[ll].vy = -6;
                    ents[ll].vx = ((ents[ll].x - p.x > 0) * 2 - 1) * 0.4;
                }
                if(!(ents[ll].type == T_OI && ents[ll].islife2 == 1))
                    damage_ent(ll, ents[l].hurt);
                if(ents[ll].hp <= 0)
                {
                    if(ents[ll].type == T_OI && ents[ll].islife2 == 0)
                    {
                        ents[ll].islife2 = true;
                        ents[ll].hp = 1;
                        ents[ll].attack_tick = 0;
                    }
                    else
                    {
                        kill_ent_drop(ll);
                    }
                }
                hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
                kill_ent(l);
                return false;
            }
        }
    }
    if(ents[l].nbt <= 3)
    {
        ents[l].vy += GRAVITY;
        ents[l].y += ents[l].vy * 0.02;
        ents[l].vy *= 0.98;
    }
    else
    {
        if(ents[l].nbt == 5) ents[l].y += GRAVITY;
        ents[l].y += ents[l].vy * 0.02;
    }
    eb = ent_block(l, ex, ey);
    if(block_of(eb).touch == 1 && ents[l].nbt != 4 && ents[l].nbt != 5)
    {
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
        kill_ent(l);
        return false;
    }
    if(std::fabs(p.x - ents[l].x) < 0.75 && std::fabs(p.y - ents[l].y) < 0.75 &&
            ents[l].nbt != 2 && ents[l].nbt != 3)
    {
        player_hurt(3, ents[l].x);
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
        kill_ent(l);
        return false;
    }
    return true;
}

// 旧版 ai_flyboss 第 6153 行（递归 Boss，AI==3）；返回 false 表示已移除
bool ai_flyboss(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    g_checkhp = true;
    if(ents[l].hp <= 0)
    {
        boss2_die_split(l, old_x, old_y);
        return false;
    }
    if(ents[l].hp != ents[l].direction)
        game::st().boss2hp -= (i32)(ents[l].direction - ents[l].hp);
    ents[l].direction = ents[l].hp;
    p.gj = 0;

    if(lbutton() && (std::fabs(p.mpx - ents[l].x) < 2 && std::fabs(p.mpy - ents[l].y) < 2) &&
            p.playerCD <= 0)
    {
        if(std::fabs(p.x - ents[l].x) < 4.5 && std::fabs(p.y - ents[l].y) < 4.5)
        {
            if(p.spy > 2)
            {
                damage_ent(l, p.gongji * 1.5);
                ents[l].vy = -8;
                p.gj = 2;
                p.playerCD = p.gongjicd;
            }
            else
            {
                damage_ent(l, p.gongji);
                ents[l].vy = -6;
                p.gj = 1;
                p.playerCD = p.gongjicd;
            }
            // 冲刺相位打到会额外受伤
            if(ents[l].attack_tick > 600 && ents[l].attack_tick < 900)
                damage_ent(l, p.gongji * 10);
            if(ents[l].hp <= 0)
            {
                game::st().boss2hp -= (i32)ents[l].direction;
                if(ents[l].attack_tick > 600 && ents[l].attack_tick < 900)
                {
                    explode_add((i32)(ents[l].x + 0.5), (i32)(ents[l].y + 0.5), -4);
                    boom_hurt_ents((i32)ents[l].x, (i32)ents[l].y, 2);
                }
                i32 xxxx = (i32)ents[l].x, yyyy = (i32)ents[l].y, nbtt = ents[l].nbt - 1;
                hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
                kill_ent(l);
                if(nbtt >= 1)
                {
                    if(nbtt >= 0 && nbtt < 11)
                        spawn(T_RECURSIVE, (f64)FBNQ[nbtt], 1 + (nbtt / 2.0),
                              xxxx + world::rnd(5) - 2, yyyy + world::rnd(5) - 2, nbtt);
                    if(nbtt - 1 >= 0)
                        spawn(T_RECURSIVE, (f64)FBNQ[nbtt - 1], 1 + (nbtt / 2.0),
                              xxxx + world::rnd(5) - 2, yyyy + world::rnd(5) - 2, nbtt - 1);
                }
                return false;
            }
        }
    }

    if(p.gj == 1 && ents[l].attack_tick < 900) { ents[l].vy *= -3; ents[l].vx *= -3; }
    if(p.gj == 2 && ents[l].attack_tick < 900) { ents[l].vy *= -4; ents[l].vx *= -4; }

    ents[l].attack_tick++;
    if(ents[l].attack_tick < 300)
        ents[l].vx += ((p.x - ents[l].x) * 0.05 +
                       (((p.x - ents[l].x) > 0) * 2 - 1) * 0.4) *
                      (ents[l].nbt / 10.0 + 0.1);
    else if(ents[l].attack_tick <= 600)
        ents[l].vx += ((p.x - ents[l].x) * 0.02 +
                       (((p.x - ents[l].x) > 0) * 2 - 1) * 0.2) *
                      (ents[l].nbt / 10.0 + 0.1);
    if(ents[l].vx < -24 && ents[l].attack_tick < 900) ents[l].vx = -24;
    if(ents[l].vx > 24 && ents[l].attack_tick < 900) ents[l].vx = 24;
    ents[l].x += ents[l].vx * 0.01 * ENT_SPEED;

    if(ents[l].attack_tick < 300)
        ents[l].vy -= ((p.y - ents[l].y) * 0.05 +
                       (((p.y - ents[l].y) > 0) * 2 - 1) * 0.6) *
                      (ents[l].nbt / 10.0 + 0.1);
    else if(ents[l].attack_tick <= 600)
        ents[l].vy -= ((p.y - ents[l].y - 3) * 0.02 +
                       (((p.y - ents[l].y - 3) > 0) * 2 - 1) * 0.2) *
                      (ents[l].nbt / 10.0 + 0.1);
    if(ents[l].vy < -24 && ents[l].attack_tick < 900) ents[l].vy = -24;
    if(ents[l].vy > 24 && ents[l].attack_tick < 900) ents[l].vy = 24;
    ents[l].y -= ents[l].vy * 0.01;

    if(ents[l].cd > 0) ents[l].cd--;
    if(ents[l].attack_tick > 300 && ents[l].attack_tick <= 600 &&
            ents[l].cd <= 0 && ents[l].nbt > 5)
    {
        ents[l].cd = (20 - ents[l].nbt) * 12;
        spawn_arrow(ents[l].x, ents[l].y, (p.x - ents[l].x),
                    (p.y - ents[l].y) * 5 - 1, 0, 3);
    }
    if(ents[l].attack_tick == 900)
    {
        ents[l].vx = (p.x - ents[l].x) * (ents[l].nbt / 2.0);
        ents[l].vy = -((p.y - ents[l].y) * (ents[l].nbt / 2.0));
    }
    if(ents[l].attack_tick > 1020) ents[l].attack_tick = 0;
    ents[l].vx *= 0.98;
    ents[l].vy *= 0.98;
    if(std::fabs(p.x - ents[l].x) < 0.5 && std::fabs(p.y - ents[l].y) < 0.5 && p.wudi <= 0)
    {
        p.life -= ents[l].hurt * p.jianshang;
        p.wudi = 45;
    }
    return true;
}

// 旧版 ai_boss3 第 6266 行（世界吞噬者，AI==4）；返回 false 表示已移除
bool ai_boss3(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    if(lbutton() && (std::fabs(p.mpx - ents[l].x) < 2 && std::fabs(p.mpy - ents[l].y) < 2) &&
            p.playerCD <= 0)
    {
        if(std::fabs(p.x - ents[l].x) < 5.5 && std::fabs(p.y - ents[l].y) < 5.5)
        {
            if(p.spy > 2) damage_ent(l, p.gongji * 1.5);
            else damage_ent(l, p.gongji);
            p.playerCD = p.gongjicd;
            if(ents[l].hp < 0) kill_ent(l);
        }
    }
    g_boss3hp += (i32)ents[l].hp;
    if(ents[l].cd > 0) ents[l].cd--;
    touch_attack(l, ents[l].hurt, 1);

    // 头部随机喷吐
    if(ents[l].chv < 480) ents[l].chv++;
    else
    {
        if(world::rnd(2) == 0)
        {
            ents[l].chv = 0;
            f64 ttx = std::fabs(p.x - ents[l].x);
            f64 tty = std::fabs(p.y - ents[l].y);
            f64 total = ttx + tty + 0.001;
            f64 px = ttx / total, py = tty / total;
            if(p.x - ents[l].x < 0) px *= -1;
            if(p.y - ents[l].y < 0) py *= -1;
            spawn_arrow(ents[l].x, ents[l].y, 10 * px, 15 * py, 4, 3);
        }
        else
        {
            ents[l].chv = 0;
        }
    }

    if(ents[l].type == T_WORM_HEAD)
    {
        // 头：自主游动
        bool ischangespeed = true;
        f64 speed = 0.1 * ENT_SPEED;
        ents[l].attack_tick++;
        if(ents[l].attack_tick == 1) { ents[l].target_x = p.x; ents[l].target_y = p.y; }
        if(ents[l].attack_tick <= 300 && !(ents[l].random_tick > 60))
        {
            f64 d = jl_abs(ents[l].x - p.x, ents[l].y - p.y);
            if(d < 10 || ents[l].issd)
            {
                ischangespeed = false;
                ents[l].target_x = p.x;
                ents[l].target_y = p.y;
                ents[l].direction = ents[l].vx * 25;
                speed = 0.15 * ENT_SPEED;
                ents[l].issd = true;
                ents[l].random_tick++;
            }
            else
            {
                ents[l].target_x = p.x;
                ents[l].target_y = p.y;
                speed = 0.12 * ENT_SPEED;
                if(d > 30) speed = 0.3 * ENT_SPEED;
            }
        }
        else
        {
            ents[l].target_x += ents[l].direction * 0.1;
            if(ents[l].random_tick > 180)
                ents[l].direction += ((p.x > ents[l].x) * 2 - 1) * 0.25;
            ents[l].direction *= 0.95;
            ents[l].target_y += 0.25;
            speed = 0.1 * ENT_SPEED;
            ents[l].random_tick++;
        }
        if(ischangespeed)
        {
            f64 ttx = std::fabs(ents[l].x - ents[l].target_x);
            f64 tty = std::fabs(ents[l].y - ents[l].target_y);
            f64 total = ttx + tty + 0.001;
            f64 px = ttx / total, py = tty / total;
            if(ents[l].target_x - ents[l].x < 0) px *= -1;
            if(ents[l].target_y - ents[l].y < 0) py *= -1;
            if(world::godeater) speed *= 2.5;
            ents[l].vx += px * speed;
            ents[l].vy += py * speed;
            ents[l].vx *= 0.85;
            ents[l].vy *= 0.85;
        }
        ents[l].x += ents[l].vx;
        ents[l].y += ents[l].vy;
        if(ents[l].attack_tick >= 600 || ents[l].random_tick > 300)
        {
            ents[l].attack_tick = 1;
            ents[l].issd = false;
            ents[l].random_tick = 0;
        }
    }
    else
    {
        // 体节 / 尾：跟随前一节
        i32 cl = ents[l].conl;
        if(cl >= 0 && cl < MAX_ENT && cl != l && ents[cl].type > 0)
        {
            f64 k = (f64)(7 - world::godeater * 4);
            if(k < 1) k = 1;
            ents[l].x += (ents[cl].x - ents[l].x) / k;
            ents[l].y += (ents[cl].y - ents[l].y) / k;
        }
    }
    return true;
}

// 旧版 ai_passive 第 6370 行（羊 / 猪）
void ai_passive(i32 l, f64 old_x, f64 old_y)
{
    game::Player &p = game::st().pl;
    p.gj = 0;
    if(lbutton() && (std::fabs(p.mpx - ents[l].x) < 2 && std::fabs(p.mpy - ents[l].y) < 2) &&
            p.playerCD <= 0)
    {
        if(std::fabs(p.x - ents[l].x) < 4.5 && std::fabs(p.y - ents[l].y) < 4.5)
        {
            if(p.spy > 2) { damage_ent(l, p.gongji * 1.5); p.gj = 2; }
            else { damage_ent(l, p.gongji); p.gj = 1; }
            p.playerCD = p.gongjicd;
            ents[l].chv = 240;
            ents[l].issd = true;
            ents[l].vy = -6;
            ents[l].vx += ((ents[l].x > p.x) * 2 - 1) * 0.6;
            if(ents[l].hp <= 0)
            {
                hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
                kill_ent_drop(l);
                return;
            }
        }
    }
    if(ents[l].hp <= 0 || out_of_active_area(ents[l].x, ents[l].y))
    {
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
        kill_ent(l);
        return;
    }
    if(ents[l].cd > 0) ents[l].cd--;
    if(ents[l].jp == 1)
    {
        ents[l].vy = -8;
        ents[l].jp = 2;
    }
    ent_update_state(l);

    i32 ex = 0, ey = 0;
    i16 fluid_block = ent_block(l, ex, ey);
    if(fluid_block_id(fluid_block))
    {
        ents[l].vy += GRAVITY;
        ents[l].vx *= 0.5;
        ents[l].vy *= 0.75;
        if(fluid_block >= B_WATER && fluid_block <= B_FLOW1 && ents[l].vy > 1.5)
            ents[l].vy = 1.5;                 // 水里上浮慢
        ents[l].y += ents[l].vy * 0.02;
    }
    else
    {
        ents[l].vy += GRAVITY;
        ents[l].y += ents[l].vy * 0.06 * ENT_SPEED;
        ents[l].vy *= 0.98;
    }
    i16 eb = ent_block(l, ex, ey);
    if(block_of(eb).touch == 1)
    {
        if(ents[l].vy >= 0)
        {
            i32 guard = 0;
            while(solid_at(to_int(ents[l].x), to_int(ents[l].y)) && guard++ < 400)
                ents[l].y -= 0.1;
            ents[l].jp = 0;
            ents[l].vy = 0.01;
        }
        if(ents[l].vy < 0)
        {
            i32 guard = 0;
            while(solid_at(to_int(ents[l].x), to_int(ents[l].y)) && guard++ < 400)
                ents[l].y += 0.1;
            ents[l].vy = 2;
        }
    }

    if(ents[l].state == ST_FLEE)
    {
        ents[l].chv--;
        if(ents[l].x > p.x) ents[l].vx += 0.012;
        else ents[l].vx -= 0.012;
    }
    else
    {
        ents[l].issd = false;
        ents[l].random_tick++;
        if(ents[l].random_tick > 80)
        {
            ents[l].random_tick = 0;
            if(ents[l].direction != 0) ents[l].direction = 0;
            else ents[l].direction = (world::rnd(2)) * 2 - 1;
        }
        ents[l].vx += 0.015 * ents[l].direction;
    }

    // 避水：前方有水就掉头
    i32 cx = to_int(ents[l].x), cy = to_int(ents[l].y);
    bool in_water_now = (cx >= 0 && cx < world::W && cy >= 0 && cy < world::H &&
                         ent_blk(cx, cy) >= B_WATER && ent_blk(cx, cy) <= B_FLOW1);
    if(!in_water_now && ents[l].vx != 0)
    {
        i32 ahead_x = (ents[l].vx > 0) ? cx + 1 : cx - 1;
        if(ahead_x >= 0 && ahead_x < world::W)
        {
            bool water_ahead = false;
            for(i32 ddy = -1; ddy <= 1; ddy++)
            {
                i32 yy = cy + ddy;
                if(yy >= 0 && yy < world::H)
                {
                    i16 b = ent_blk(ahead_x, yy);
                    if(b >= B_WATER && b <= B_FLOW1) { water_ahead = true; break; }
                }
            }
            if(water_ahead)
            {
                ents[l].vx = 0;
                ents[l].direction = -ents[l].direction;
                ents[l].random_tick = 0;
            }
        }
    }

    eb = ent_block(l, ex, ey);
    if(eb >= B_WATER && eb <= B_FLOW1) ents[l].x += ents[l].vx * 0.5 * ENT_SPEED;
    else ents[l].x += ents[l].vx * ENT_SPEED;
    ents[l].vx *= 0.9;

    eb = ent_block(l, ex, ey);
    if(block_of(eb).touch == 1)
    {
        i32 step = 1;
        while(step < 60)
        {
            ents[l].x -= 0.03 * step;
            if(!solid_at(to_int(ents[l].x), to_int(ents[l].y))) break;
            ents[l].x += 0.06 * step;
            if(!solid_at(to_int(ents[l].x), to_int(ents[l].y))) break;
            ents[l].x -= 0.03 * step;
            step++;
        }
        if(ents[l].jp == 0) ents[l].jp = 1;
        ents[l].vx = 0;
    }
}

}   // namespace

// 旧版 ent_update_state 第 6009 行：状态迁移（纯判定，无副作用）
void ent_update_state(i32 l)
{
    if(ents[l].ai == 1)
    {
        if(ents[l].isnotouch == 1) { ents[l].state = ST_SCRIPT; return; }
        if(ents[l].type == T_CREEPER && ents[l].cd < 0) ents[l].state = ST_FUSE;
        else if(ents[l].issd || ents[l].chv > 0) ents[l].state = ST_CHASE;
        else ents[l].state = ST_WANDER;
        return;
    }
    if(ents[l].ai == 2) { ents[l].state = ST_PROJECTILE; return; }
    if(ents[l].ai == 3)
    {
        ents[l].state = (ents[l].attack_tick > 900) ? ST_PHASE : ST_FLY;
        return;
    }
    if(ents[l].ai == 4)
    {
        ents[l].state = (ents[l].type == T_WORM_HEAD) ? ST_FLY : ST_FOLLOW_CHAIN;
        return;
    }
    if(ents[l].ai == 5)
    {
        ents[l].state = (ents[l].chv > 0) ? ST_FLEE : ST_WANDER;
        return;
    }
    ents[l].state = ST_IDLE;
}

// ============================================================================
//                        每帧主循环（旧版 st_move 第 6521 行）
// ============================================================================
void tick_all()
{
    g_checkhp = false;
    g_boss3hp = 0;
    // 说明：旧版在此递减全局 playerCD，新版本由 game::tick_once() 统一递减。
    for(i32 l = 0; l < MAX_ENT; l++)
    {
        if(ents[l].type <= 0) continue;
        f64 old_x = ents[l].x;
        f64 old_y = ents[l].y;
        if(ents[l].wd > 0) ents[l].wd--;

        if(ents[l].ai == 1)
        {
            if(!ai_ground(l, old_x, old_y)) continue;
        }
        else if(ents[l].ai == 2)
        {
            if(!ai_projectile(l, old_x, old_y)) continue;
        }
        else if(ents[l].ai == 3)
        {
            if(!ai_flyboss(l, old_x, old_y)) continue;
        }
        else if(ents[l].ai == 4)
        {
            if(!ai_boss3(l, old_x, old_y)) continue;
        }
        else if(ents[l].ai == 5)
        {
            ai_passive(l, old_x, old_y);
            if(ents[l].type == 0) continue;
        }
        hash_update(l, old_x, old_y, ents[l].x, ents[l].y);
    }
    // 本帧没有递归 Boss 存活：清掉 Boss 血条（旧版 checkhp 为 bool，故直接归零）
    if(!g_checkhp) game::st().boss2hp = 0;
    // 旧版 Print() 里：boss3hp 更大就用它当 Boss 血条
    if(g_boss3hp > game::st().bosshp) game::st().bosshp = g_boss3hp;
}

// ============================================================================
//                        自然刷怪（旧版 try_make_st 第 5147 行）
// ============================================================================
void try_spawn()
{
    game::Player &p = game::st().pl;
    i32 px = to_int(p.x), py = to_int(p.y);
    i32 ed = game::st().difficulty;
    i32 div = (i32)(ed / 1.2);
    if(div < 1) div = 1;                     // 防 rand()%0

    for(i32 i = px - 120; i <= px + 120; i++)
    {
        for(i32 j = py - 80; j <= py + 80; j++)
        {
            if(i <= 0 || j <= 0 || i >= world::W - 1 || j >= world::H - 1) continue;
            // 玩家身边 26x16 不刷
            if(i >= px - 26 && j >= py - 16 && i <= px + 26 && j <= py + 16) continue;
            i16 b = world::cur(i, j);
            // 生成点或落脚点是液体（水/岩浆）就不刷：怪物不生成在水里，也不生成在水面上
            if(is_liquid(world::cur(i, j)) || is_liquid(world::cur(i, j + 1))) continue;
            if(b == B_AIR || b == B_SKY)
            {
                if(world::dim == world::DIM_OVERWORLD)
                {
                    if(world::light_at(i, j) <= 7 &&
                            block_of(world::cur(i, j + 1)).touch >= 1 &&
                            world::rnd(div) == 0)
                    {
                        i32 q = world::rnd(4);
                        if(q == 0) spawn(T_ZOMBIE, 20, 4, i, j);
                        else if(q == 1) spawn(T_SKELETON, 20, 3, i, j);
                        else if(q == 2) spawn(T_CREEPER, 20, 50, i, j);
                        else spawn(T_SPIDER, 16, 3, i, j);
                    }
                }
                else
                {
                    // 下界：只刷僵尸猪灵
                    if(block_of(world::cur(i, j + 1)).touch >= 1 && world::rnd(div) == 0)
                        spawn(T_PIGLIN, 20, 6, i, j);
                }
            }
        }
    }

    // 白天在地表自然生成羊和猪（基于列定位地表）
    if(world::dim == world::DIM_OVERWORLD && world::night == world::NIGHT_NOON)
    {
        i32 div2 = ed / 2;
        if(div2 < 1) div2 = 1;
        for(i32 i = px - 120; i <= px + 120; i += 2)
        {
            if(i <= 0 || i >= world::W - 1) continue;
            if(std::abs(i - px) < 30) continue;
            // 从上往下找地表（第一个实心方块上方的空气）
            i32 ground = -1;
            for(i32 j = 1; j < world::H - 1; j++)
            {
                if(block_of(world::cur(i, j)).touch >= 1 &&
                        (world::cur(i, j - 1) == B_AIR || world::cur(i, j - 1) == B_SKY))
                {
                    ground = j - 1;
                    break;
                }
            }
            if(ground > 0 && world::light_at(i, ground) > 7)
            {
                // 检查该格上方是否完全无方块（露天地表）
                bool open_sky = true;
                for(i32 jj = 0; jj < ground; jj++)
                {
                    i16 t = world::cur(i, jj);
                    if(t != B_AIR && t != B_SKY) { open_sky = false; break; }
                }
                if((open_sky || ground > 840) && world::rnd(div2) == 0)
                {
                    i32 sx = i + (world::rnd(3) - 1);
                    bool water_here = false;
                    if(sx > 0 && sx < world::W - 1)
                    {
                        if(world::cur(sx, ground) >= B_WATER && world::cur(sx, ground) <= B_FLOW1)
                            water_here = true;
                        if(ground + 1 < world::H &&
                                world::cur(sx, ground + 1) >= B_WATER &&
                                world::cur(sx, ground + 1) <= B_FLOW1)
                            water_here = true;
                    }
                    if(sx > 0 && sx < world::W - 1 && !water_here &&
                            (world::cur(sx, ground) == B_AIR || world::cur(sx, ground) == B_SKY))
                    {
                        if(world::rnd(2) == 0) spawn(T_SHEEP, 8, 0, sx, ground);
                        else spawn(T_PIG, 8, 0, sx, ground);
                    }
                }
            }
        }
    }
}

}   // namespace entity

