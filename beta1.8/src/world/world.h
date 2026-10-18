#pragma once
// ===================== 世界数据与生成 =====================
// 从旧版 beta/beta1.8.cpp 迁移。世界尺寸、生成算法、噪声参数全部保持一致，
// 这样用同一个种子生成的地形走向与旧版一致。
//
// 内存：s / s2 / light / ep 共 4 个 10000x1001 的二维数组
//       2+2+2+1 = 7 字节/格 x 10,010,000 格 ≈ 70 MB 静态数据。
//       与旧版相同的量级，旧版能跑这个也能跑。
#include "../core/types.h"

namespace world {

// 世界尺寸（旧版 WORLD_WIDTH / WORLD_HEIGHT）
constexpr i32 W = 10000;
constexpr i32 H = 1001;

// 维度
enum Dim : i32 { DIM_OVERWORLD = 1, DIM_NETHER = 2 };

// 脏列块追踪（旧版增量存档用）
constexpr i32 CHUNK_COLS = 64;
constexpr i32 NCHUNKS = (W + CHUNK_COLS - 1) / CHUNK_COLS;

// 昼夜阶段（旧版 night）
enum Night : i32 { NIGHT_NOON = 0, NIGHT_NIGHT = 1, NIGHT_DUSK = 2, NIGHT_DAWN = 3 };

// ---- 世界数据（对外可见：渲染、实体、存档都要直接读）----
extern i16 s[W][H];      // 主世界方块
extern i16 s2[W][H];     // 下界方块
extern i16 light[W][H];  // 光照 0..19
extern bool ep[W][H];    // 是否已被玩家探索过（旧版 ep）
extern i16 boimes[W + 5];
extern i16 boimes2[W + 5];
extern i16 zs[W + 5];    // 高度场
extern i32 water;

// ---- 世界状态 ----
extern i32 dim;          // 当前维度
extern i32 now_time;     // 0..40000
extern i32 night;
extern i32 will_night, will_light;
extern i32 sun;          // 定格时间开关
extern i32 gamemode;     // 1 生存 / 0 创造
extern i32 ed;           // 刷怪难度基数 8000/6000/4000
extern i32 back_x, back_y;   // 重生点
extern i32 godeater;     // 神明吞噬者模组
extern i32 oi;           // OI 风暴模组
extern i32 tick;
// 光照 / 流体活动范围的中心（= 玩家格坐标），由 game 层每帧写入
extern i32 g_light_cx, g_light_cy;
// 背包里是否有「魔光护符」(id 101)：由 game 层每帧写入，
// set_light() 据此给玩家所在格加 12 级随身光（旧版 setlight 1432）
extern bool g_amulet;

// ---- 随机源 ----
// 旧版用的是全局 srand/rand。这里换成可播种的独立随机源，
// 世界生成用同一个种子就能复现同一张地图。
void rng_seed(u64 seed);
u32  rnd();                  // 等价于 rand()
i32  rnd(i32 n);             // [0,n)
i32  rnd_range(i32 lo, i32 hi);

// ---- 脏列 ----
void mark_dirty_col(i32 c);
void mark_dirty_range(i32 a, i32 b);
void mark_all_dirty();
bool dirty_col(i32 c);
void clear_dirty_col(i32 c);

// ---- 访问（带边界保护，损坏存档不至于崩渲染）----
inline i16 get(i32 x, i32 y) { return (x < 0 || x >= W || y < 0 || y >= H) ? 0 : s[x][y]; }
inline i16 get_nether(i32 x, i32 y) { return (x < 0 || x >= W || y < 0 || y >= H) ? 0 : s2[x][y]; }
inline i16 cur(i32 x, i32 y) { return dim == DIM_NETHER ? get_nether(x, y) : get(x, y); }
inline void set(i32 x, i32 y, i16 v)
{
    if(x < 0 || x >= W || y < 0 || y >= H) return;
    if(dim == DIM_NETHER) s2[x][y] = v;
    else s[x][y] = v;
    mark_dirty_col(x);
}
inline i16 light_at(i32 x, i32 y)
{
    return (x < 0 || x >= W || y < 0 || y >= H) ? 0 : light[x][y];
}
inline bool explored(i32 x, i32 y)
{
    return (x < 0 || x >= W || y < 0 || y >= H) ? false : ep[x][y];
}

// ---- 生成 ----
// 生成整张世界（主世界 + 下界 + 光照 + 宝箱 + 生物）
// progress: 可选回调，用于生成进度条
void generate(void (*progress)(const char *stage, f32 ratio, void *ud), void *ud);

// 只为 from_x..W 的新列补生成地形（旧存档扩展）
void expand_from(i32 from_x);

// ---- 光照 ----
void set_light();
void spread_light(i32 i, i32 j, i32 lt, i32 v);

// ---- 流体 ----
struct NewBlock { i16 v[W][H]; };   // 旧版 short getnew[W][H]
void block_update();                // 旧版同名函数：水/岩浆流动
// 旧版 Clear()：把视野内的空气填成天空（防止洞穴透出天空色）
void clear_around(i32 cx);
void reset_block_update();
i32  block_update_phase();

// ---- 维度切换（旧版 swap_map）----
// 把当前维度切到另一个；ismakedoor=1 时在玩家位置生成下界传送门
void swap_dim(bool make_door = true);

}   // namespace world

