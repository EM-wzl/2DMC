// ===================== 世界数据与生成实现 =====================
// 本文件是旧版 beta/beta1.8.cpp 的定点移植：
//   MAP()            -> world::generate()
//   MAP_nether()     -> gen_nether()
//   perlin/perlin_2  -> 同名保留
//   setlight/dfs     -> set_light()/spread_light()
//   block_update()   -> block_update()
//   swap_map         -> swap_dim()
//   Clear_formap / expand_world / make_lake / make_mineshaft / spawn_ebon_cave ...
//
// 随机数：旧版用全局 rand()。这里换成可播种的 world::rnd()，
// 同一个种子生成同一张地图（旧版每次开局地形都不同）。
#include "world.h"
#include "blocks.h"
#include "../item/chest.h"

#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace world {

// ---------------- 数据 ----------------
i16 s[W][H];
i16 s2[W][H];
i16 light[W][H];
bool ep[W][H];
i16 boimes[W + 5];
i16 boimes2[W + 5];
i16 zs[W + 5];
i32 water = 0;

i32 dim = DIM_OVERWORLD;
i32 now_time = 0;
i32 night = NIGHT_NOON;
i32 will_night = 0, will_light = 0;
i32 sun = 1;
i32 gamemode = 1;
i32 ed = 6000;
i32 back_x = 10, back_y = 145;
i32 godeater = 0;
i32 oi = 0;
i32 tick = 0;

// 光照 / 流体活动范围的中心（= 玩家格坐标），由 game 层每帧写入
i32 g_light_cx = 0, g_light_cy = 0;
bool g_amulet = false;

namespace {

bool g_dirty[NCHUNKS];
bool g_dirty_meta = true;

// 列 -> 列块索引（避免渲染热路径里做除法）
i32 g_col_chunk[W + 1];

// 旧版到处直接用 blocktouch[id] 下标，越界在旧版是未定义行为。
// 新代码统一走带边界保护的 touch1()。
inline bool touch1(i32 id) { return block_of(id).touch == 1; }

// 旧版 world 里到处用到的 rand()%n。n<=0 时旧版是除零/未定义，这里返回 0。
Rng g_rng;

// 生成期的进度回调
void (*g_progress)(const char *, f32, void *) = nullptr;
void *g_progress_ud = nullptr;
void report(const char *stage, f32 ratio)
{
    if(g_progress) g_progress(stage, ratio, g_progress_ud);
}

}   // namespace

void rng_seed(u64 seed) { g_rng.seed(seed); }
u32  rnd() { return g_rng.next(); }
i32  rnd(i32 n) { return n > 0 ? (i32)(rnd() % (u32)n) : 0; }
i32  rnd_range(i32 lo, i32 hi) { return lo + rnd(hi - lo + 1); }

void mark_dirty_col(i32 c)
{
    if(c < 0) c = 0;
    if(c >= W) c = W - 1;
    g_dirty[g_col_chunk[c]] = true;
    g_dirty_meta = true;
}
void mark_dirty_range(i32 a, i32 b)
{
    if(a < 0) a = 0;
    if(b >= W) b = W - 1;
    if(a > b) return;
    for(i32 c = g_col_chunk[a]; c <= g_col_chunk[b]; c++) g_dirty[c] = true;
    g_dirty_meta = true;
}
void mark_all_dirty()
{
    memset(g_dirty, 1, sizeof(g_dirty));
    g_dirty_meta = true;
}
bool dirty_col(i32 c)
{
    if(c < 0 || c >= W) return false;
    return g_dirty[g_col_chunk[c]];
}
void clear_dirty_col(i32 c)
{
    if(c < 0 || c >= W) return;
    g_dirty[g_col_chunk[c]] = false;
}

// ============================================================================
//                                噪声
// ============================================================================
namespace {

struct Grad { f64 nx, ny; };
constexpr i32 NOISE_GX = W / 10 + 10;
Grad noise_g[NOISE_GX][201];
i16 noise_result[W][H];
const Grad GRAD8[8] = {{1,1},{1,0},{0,1},{-1,-1},{-1,0},{-1,-1},{0,-1},{1,-1}};

f64 fade5(f64 td)
{
    return 6 * td * td * td * td * td - 15 * td * td * td * td + 10 * td * td * td;
}

void spawn_dots(i16 steplen, i16 eff)
{
    // 与旧版一致：这里重新播种会让整张噪声图每层都不同
    for(i32 i = 1; i <= (W - 1) / steplen; i++)
        for(i32 j = 100 / steplen; j <= (H - 1) / steplen; j++)
        {
            noise_g[i][j] = GRAD8[rnd() % 8];
            noise_g[i][j].nx *= eff;
            noise_g[i][j].ny *= eff;
        }
}

void fill_noise(i16 steplen)
{
    for(i32 i = 1; i <= W - 1; i++)
        for(i32 j = 100; j <= H - 1; j++)
        {
            f64 st = steplen;
            i32 x0, y0, x1, y1;
            f64 ax = i / st, ay = j / st;
            x0 = i / steplen, x1 = x0 + 1;
            y0 = j / steplen, y1 = y0 + 1;
            Grad dots[4];
            dots[0] = {ax - x0, ay - y0};
            dots[1] = {ax - x0, ay - y1};
            dots[2] = {ax - x1, ay - y0};
            dots[3] = {ax - x1, ay - y1};
            f64 c00 = noise_g[x0][y0].nx * dots[0].nx + noise_g[x0][y0].ny * dots[0].ny;
            f64 c01 = noise_g[x0][y1].nx * dots[1].nx + noise_g[x0][y1].ny * dots[1].ny;
            f64 c10 = noise_g[x1][y0].nx * dots[2].nx + noise_g[x1][y0].ny * dots[2].ny;
            f64 c11 = noise_g[x1][y1].nx * dots[3].nx + noise_g[x1][y1].ny * dots[3].ny;
            f64 fdx = fade5(ax - x0);
            f64 fdy = fade5(ay - y0);
            noise_result[i][j] += (i16)((c00 * (1 - fdx) * (1 - fdy) +
                                         c10 * fdx * (1 - fdy) +
                                         c01 * (1 - fdx) * fdy +
                                         c11 * fdx * fdy) * 50);
        }
}

void spawn_noise_nether()
{
    memset(noise_g, 0, sizeof(noise_g));
    memset(noise_result, 0, sizeof(noise_result));
    spawn_dots(80, 4); fill_noise(80);
    spawn_dots(80, 4); fill_noise(80);
    spawn_dots(40, 3); fill_noise(40);
    spawn_dots(20, 2); fill_noise(20);
    spawn_dots(10, 1); fill_noise(10);
}

void spawn_noise_cave()
{
    memset(noise_g, 0, sizeof(noise_g));
    memset(noise_result, 0, sizeof(noise_result));
    spawn_dots(50, 4); fill_noise(50);
    spawn_dots(30, 3); fill_noise(40);
    spawn_dots(20, 2); fill_noise(20);
    spawn_dots(10, 1); fill_noise(10);
    for(i32 i = 1; i <= W - 1; i++)
        for(i32 j = 100; j <= H - 1; j++)
            noise_result[i][j] = (i16)(noise_result[i][j] * 0.5);
}

f64 zs_ls[W + 5];

void perlin(const i32 pl, const i32 rad)
{
    const i32 muc = (W - 1) / pl + 3;
    static std::vector<i16> cz;
    cz.assign(muc, 0);
    for(i32 i = 0; i < muc; i++) cz[i] = (i16)rnd(rad);
    for(i32 i = 0; i <= W - 1; i++)
    {
        i32 dpl = i / pl;
        f64 di = i * 1.0 / pl;
        f64 c0 = cz[dpl] * (di - dpl);
        f64 c1 = cz[dpl + 1] * (di - (dpl + 1));
        f64 fdx = fade5(di - dpl);
        zs_ls[i] += (c0 * (1 - fdx) + c1 * fdx) * 2 + rad / 2;
    }
}

f32 cosine_interp(f32 a, f32 b, f32 x)
{
    f32 ft = x * 3.1415927f;
    f32 f = (1 - std::cos(ft)) * 0.5f;
    return a * (1 - f) + b * f;
}

void perlin_2(const f64 pl, const i32 rad, const i32 startt, const i32 endd,
              const i32 plu = 0)
{
    const i32 muc = (i32)((W - 1) / pl) + 2;
    static std::vector<i16> cz;
    cz.assign(muc, 0);
    for(i32 i = 0; i < muc; i++)
    {
        cz[i] = (i16)(rnd(rad) + plu);
        if(i == startt / ((i32)pl) - 1) cz[i] = 0;
        if(i == endd / ((i32)pl)) cz[i] = 0;
    }
    for(i32 i = startt / ((i32)pl) * ((i32)pl) - (i32)pl;
            i <= endd / ((i32)pl) * ((i32)pl); i++)
    {
        if(i < 0 || i > W - 1) continue;
        zs_ls[i] += cosine_interp(cz[i / ((i32)pl)], cz[i / ((i32)pl) + 1],
                                  (f32)((i - i / ((i32)pl) * ((i32)pl)) / pl)) + 0.5;
    }
}

void result_heights()
{
    for(i32 i = 0; i <= W - 1; i++)
    {
        i32 v = (i32)(zs_ls[i] + 0.5);
        if(v < 0) v = 0;
        if(v > H - 50) v = H - 50;
        zs[i] = (i16)v;
    }
}

// ============================================================================
//                            地形细节生成
// ============================================================================

void stt(i32 i, i32 j, i32 step)
{
    if(i < 0 || j < 0 || i >= W || j >= H || step > 8) return;
    // 只替换石头并向石头内扩散，避免雪块飘进空气/洞穴形成浮空雪
    if(s[i][j] != B_STONE) return;
    s[i][j] = B_SNOW;
    if(rnd(4) == 0) stt(i + 1, j, step + 1);
    if(rnd(4) == 0) stt(i - 1, j, step + 1);
    if(rnd(4) == 0) stt(i, j + 1, step + 1);
    if(rnd(4) == 0) stt(i, j - 1, step + 1);
    if(rnd(4) == 0) stt(i + 1, j + 1, step + 1);
    if(rnd(4) == 0) stt(i + 1, j - 1, step + 1);
    if(rnd(4) == 0) stt(i - 1, j + 1, step + 1);
    if(rnd(4) == 0) stt(i - 1, j - 1, step + 1);
}

void sweep_floating_snow(i32 from_x)
{
    if(from_x < 1) from_x = 1;
    if(from_x > W - 1) return;
    for(i32 i = from_x; i < W; i++)
        for(i32 j = H - 2; j >= 1; j--)
            if(s[i][j] == B_SNOW && s[i][j + 1] == B_AIR) s[i][j] = B_AIR;
}

void make_water(i16 i, i16 j, i16 step)
{
    if(i < 0 || j < 0 || i >= W || j >= H || step > 110 || s[i][j] != B_AIR) return;
    if(step > 105)
    {
        if(rnd(3)) { s[i][j] = B_STONE; return; }
    }
    s[i][j] = B_WATER;
    if(s[i][j + 1] == B_AIR) make_water(i, j + 1, step + 1);
    if(s[i + 1][j] == B_AIR) make_water(i + 1, j, step + 1);
    if(s[i - 1][j] == B_AIR) make_water(i - 1, j, step + 1);
}

void make_lava(i16 i, i16 j, i16 step)
{
    if(i < 0 || j < 0 || i >= W || j >= H || step > 70 || s[i][j] != B_AIR) return;
    if(step > 65)
    {
        if(rnd(3)) { s[i][j] = B_STONE; return; }
    }
    s[i][j] = B_LAVA;
    if(s[i][j + 1] == B_AIR) make_lava(i, j + 1, step + 1);
    if(s[i + 1][j] == B_AIR) make_lava(i + 1, j, step + 1);
    if(s[i - 1][j] == B_AIR) make_lava(i - 1, j, step + 1);
}

void noodle_cave(i32 i, i32 j)
{
    i32 move_ = 0;
    if(rnd(2) == 0)
    {
        i32 deep_ = rnd(60) + 30;
        i32 wei_ = rnd(5) + 3;
        bool dir = rnd(2) != 0;
        for(i32 uu = 0; uu <= deep_; uu++)
        {
            if(uu == 0 || uu == deep_)
            {
                for(i32 kk = wei_ / 4; kk <= wei_ * 3 / 4; kk++)
                {
                    if(i + kk + move_ < 0 || j + uu < 0 || i + kk + move_ >= W ||
                            j + uu > H - 2) return;
                    s[i + kk + move_][j + uu] = B_AIR;
                }
            }
            else
            {
                for(i32 kk = 0; kk <= wei_; kk++)
                {
                    if(i + kk + move_ < 0 || j + uu < 0 || i + kk + move_ >= W ||
                            j + uu > H - 2) return;
                    s[i + kk + move_][j + uu] = B_AIR;
                }
            }
            if(dir == 0 && rnd(8) == 0) move_ += 2;
            else if(rnd(2 + dir * 10) == 0) move_++;
            if(dir == 1 && rnd(8) == 0) move_ -= 2;
            else if(rnd(2 + (1 - dir) * 10) == 0) move_--;
            if(rnd(20) == 0) dir = !dir;
            if(rnd(15) == 0) wei_++;
            if(rnd(15) == 0) wei_--;
        }
    }
    else
    {
        i32 len_ = rnd(60) + 20;
        i32 wei_ = rnd(4) + 1;
        for(i32 kk = 1; kk <= wei_ / 2; kk++)
        {
            if(i - 1 < 0 || j + kk < 0 || i - 1 >= W || j + kk + move_ > H - 2) return;
            s[i - 1][j + kk] = B_AIR;
        }
        for(i32 uu = 0; uu <= len_; uu++)
        {
            for(i32 kk = 0; kk <= wei_; kk++)
            {
                if(i + uu < 0 || j + kk + move_ < 0 || i + uu >= W ||
                        j + kk + move_ > H - 2) return;
                s[i + uu][j + kk + move_] = B_AIR;
            }
            if(rnd(6) == 0) move_++;
            if(rnd(6) == 0) move_--;
            if(rnd(12) == 0) wei_++;
            if(rnd(12) == 0) wei_--;
        }
        for(i32 kk = 1; kk <= wei_ / 2; kk++)
        {
            if(i - 1 < 0 || j + kk < 0 || i - 1 >= W || j + kk + move_ > H - 2) return;
            s[i - 1][j + kk + move_] = B_AIR;
        }
    }
}

void make_deep(i32 i)
{
    i32 j;
    if(rnd(2) == 0)
    {
        j = 100;
        while(s[i][j] == B_AIR && j < 300) j++;
        if((s[i][j] >= B_WATER && s[i][j] <= B_FLOW1) || s[i][j] == B_LOG ||
                s[i][j] == B_HEALTH_POTION)
            return;
    }
    else
        j = rnd(940) + 50;
    i32 move_ = 0;
    i32 deep_ = rnd(100) + 20;
    i32 wei_ = rnd(6) + 3;
    for(i32 uu = 0; uu <= deep_ + 1; uu++)
    {
        if(uu == deep_ + 1)
        {
            for(i32 kk = (i32)(wei_ / 4.0 + 0.5); kk <= (i32)(wei_ / 4.0 * 3.0 + 0.5); kk++)
            {
                if(i + kk + move_ < 0 || j + uu < 0 || i + kk + move_ >= W ||
                        j + uu > H - 2) return;
                s[i + kk + move_][j + uu] = B_AIR;
            }
        }
        else
        {
            for(i32 kk = 0; kk <= wei_; kk++)
            {
                if(i + kk + move_ < 0 || j + uu < 0 || i + kk + move_ >= W ||
                        j + uu > H - 2) return;
                s[i + kk + move_][j + uu] = B_AIR;
            }
        }
        if(rnd(6) == 0) move_++;
        if(rnd(6) == 0) move_--;
        if(rnd(15) == 0) wei_++;
        if(rnd(15) == 0) wei_--;
    }
}

void make_skycave(i32 i)
{
    i32 j = 100;
    while(s[i][j] == B_AIR && j < 300) j++;
    if((s[i][j] >= B_WATER && s[i][j] <= B_FLOW1) || s[i][j] == B_LOG ||
            s[i][j] == B_HEALTH_POTION)
        return;
    i32 move_ = 0;
    i32 deep_ = rnd(40) + 35;
    i32 wei_ = rnd(5) + 4;
    bool dir = rnd(2) != 0;
    for(i32 uu = 0; uu <= deep_; uu++)
    {
        if(uu == deep_)
        {
            for(i32 kk = wei_ / 4; kk <= wei_ * 3 / 4; kk++)
            {
                if(i + uu < 0 || j + kk + move_ < 0 || i + uu >= W ||
                        j + kk + move_ >= H) return;
                s[i + kk + move_][j + uu] = B_AIR;
            }
        }
        else
        {
            for(i32 kk = 0; kk <= wei_; kk++)
            {
                if(i + uu < 0 || j + kk + move_ < 0 || i + uu >= W ||
                        j + kk + move_ >= H) return;
                s[i + kk + move_][j + uu] = B_AIR;
            }
        }
        if(dir == 0 && rnd(8) == 0) move_ += 2;
        else if(rnd(2 + dir * 10) == 0) move_++;
        if(dir == 1 && rnd(8) == 0) move_ -= 2;
        else if(rnd(2 + (1 - dir) * 10) == 0) move_--;
        if(rnd(15) == 0) dir = !dir;
        if(rnd(15) == 0) wei_++;
        if(rnd(15) == 0) wei_--;
    }
}

// ---- 废弃矿井 ----
constexpr i32 STRU_GX = W / 5 + 20;
i16 stru[STRU_GX][201];
i16 maxlenth = 10;
const char struc[10][10][10] =
{
    {   {0,0,0,0,0},
        {-1,-1,67,-1,-1},
        {-1,-1,2,-1,-1},
        {-1,-1,2,-1,-1},
        {-2,-2,-2,-2,-2},
    },
    {   {0,0,0,0,0},
        {-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1},
        {-2,-2,-2,-2,-2},
    },
    {   {0,0,0,0,0},
        {0,0,0,-1,-1},
        {0,0,-1,-1,-1},
        {0,-1,-1,-1,-1},
        {0,66,66,66,66},
    },
    {   {-1,-1,-1,-1,-2},
        {-1,-1,-1,-2,0},
        {-1,-1,-2,0,0},
        {-1,-2,0,0,0},
        {-2,0,0,0,0},
    },
    {   {0,0,0,0,0},
        {-1,-1,0,0,0},
        {-1,-1,-1,0,0},
        {-1,-1,-1,-1,0},
        {66,66,66,66,0},
    },
    {   {-2,-1,-1,-1,-1},
        {0,-2,-1,-1,-1},
        {0,0,-2,-1,-1},
        {0,0,0,-2,-1},
        {0,0,0,0,-2},
    },
    {   {0,0,0,0,0},
        {-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1},
        {66,66,66,66,66},
    },
    {   {-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1},
        {-2,-2,-2,-2,-2},
    },
};

void make_up_down(i16 isl, i16 leng, i16 i, i16 j, bool isu, i16 tlr);

void make_straight(bool isl, i16 leng, i16 i, i16 j)
{
    if(leng > maxlenth) return;
    if(stru[i][j] != 0) return;
    stru[i][j] = 1;
    if(!isl)
    {
        if(rnd(3) == 0)
        {
            if(rnd(2) == 0) make_up_down(rnd(3), leng + 1, i + 1, j + 1, 1, isl);
            else make_up_down(rnd(3), leng + 1, i + 1, j - 1, 0, isl);
        }
        else make_straight(isl, leng + 1, i + 1, j);
    }
    else
    {
        if(rnd(3) == 0)
        {
            if(rnd(2) == 0) make_up_down(rnd(3), leng + 1, i - 1, j + 1, 1, isl);
            else make_up_down(rnd(3), leng + 1, i - 1, j - 1, 0, isl);
        }
        else make_straight(isl, leng + 1, i - 1, j);
    }
}

void make_up_down(i16 isl, i16 leng, i16 i, i16 j, bool isu, i16 tlr)
{
    if(leng > maxlenth) return;
    if(stru[i][j] != 0) return;
    if(tlr == 1)
    {
        if(isu)
        {
            stru[i][j] = isl ? 4 : 6;
            if(stru[i][j - 1] == 0) stru[i][j - 1] = 3;
        }
        else
        {
            stru[i][j] = isl ? 5 : 3;
            if(stru[i][j + 1] == 0) stru[i][j + 1] = 6;
        }
    }
    else
    {
        if(isu)
        {
            stru[i][j] = isl ? 4 : 6;
            if(stru[i][j - 1] == 0) stru[i][j - 1] = 5;
        }
        else
        {
            stru[i][j] = isl ? 5 : 3;
            if(stru[i][j + 1] == 0) stru[i][j + 1] = 4;
        }
    }
    if(isl == 2) stru[i][j] = (i16)(7 + isu);
    if(isl == 0)
    {
        if(rnd(3) == 0)
        {
            if(rnd(2) == 0) make_up_down(rnd(3), leng + 1, i + 1, j + 1, 1, isl);
            else make_up_down(rnd(3), leng + 1, i + 1, j - 1, 0, isl);
        }
        make_straight(isl, leng + 1, i + 1, j);
    }
    if(isl == 1)
    {
        if(rnd(3) == 0)
        {
            if(rnd(2) == 0) make_up_down(rnd(3), leng + 1, i - 1, j + 1, 1, isl);
            else make_up_down(rnd(3), leng + 1, i - 1, j - 1, 0, isl);
        }
        make_straight(isl, leng + 1, i - 1, j);
    }
    if(isl == 2)
    {
        make_straight(isl, leng + 1, i - 1, j);
        make_straight(isl, leng + 1, i + 1, j);
    }
}

void make_mineshaft()
{
    memset(stru, 0, sizeof(stru));
    for(i32 i = 1; i <= 300; i++)
    {
        i32 ddx = rnd(W / 5 - 50) + 25, ddy = rnd(150) + 40;
        maxlenth = (i16)(20 + rnd(16));
        make_straight(1, 0, (i16)ddx, (i16)ddy);
        make_straight(0, 0, (i16)(ddx + 1), (i16)ddy);
    }
    for(i32 jj = 1; jj <= (H - 1) / 5 - 10; jj++)
        for(i32 ii = 10; ii <= (W - 1) / 5 - 2; ii++)
        {
            if(stru[ii][jj] == 1)
            {
                if(rnd(5) < 2)
                {
                    if(touch1(s[ii * 5 + 2][jj * 5])) { /* 已是固体，跳过 */ }
                    else
                    {
                        i32 ddy = 5;
                        stru[ii][jj] = 2;
                        while(!touch1(s[ii * 5 + 2][jj * 5 + ddy]))
                            s[ii * 5 + 2][jj * 5 + ddy] = B_LOG, ddy++;
                    }
                }
                else stru[ii][jj] = 2;
            }
            for(i32 j = 0; j <= 4; j++)
                for(i32 i = 0; i <= 4; i++)
                {
                    i32 ddx = ii * 5 + j, ddy = jj * 5 + i;
                    if(stru[ii][jj] <= 0) continue;
                    if(struc[stru[ii][jj] - 1][i][j] == 0) continue;
                    if(struc[stru[ii][jj] - 1][i][j] == -2)
                    {
                        if(!touch1(s[ddx][ddy])) s[ddx][ddy] = B_PLANKS;
                        continue;
                    }
                    if(struc[stru[ii][jj] - 1][i][j] == -1)
                    {
                        if(touch1(s[ddx][ddy]) || is_water(s[ddx][ddy]) ||
                                is_lava(s[ddx][ddy]))
                            s[ddx][ddy] = B_AIR;
                        continue;
                    }
                    s[ddx][ddy] = struc[stru[ii][jj] - 1][i][j];
                }
        }
}

// ---- 腐化空腔（黑檀石 + 宝箱）----
void spawn_ebon_cave(i32 spawn_x, i32 spawn_y)
{
    const i32 s_out = 60 + rnd(9), s_mid = s_out - 8 - rnd(3), s_in = rnd(3) + 5;
    i32 s_up, s_down;
    for(i32 i = spawn_y - s_out; i <= spawn_y + s_out; i++)
        for(i32 j = spawn_x - s_out; j <= spawn_x + s_out; j++)
        {
            f32 d = (f32)std::sqrt((f32)((i - spawn_y) * (i - spawn_y) +
                                         (j - spawn_x) * (j - spawn_x)));
            s_up = spawn_y - (i32)(s_out / 2.5) + std::abs(j - spawn_x) / 4 - 4;
            s_down = spawn_y + (i32)(s_out / 2.5) - std::abs(j - spawn_x) / 4 + 2;
            if(d < s_in - 1) s[j][i] = B_EBONSTONE;
            else if(d < s_in && rnd(2) == 0) s[j][i] = B_EBONSTONE;
            else if(d < s_mid - 2)
            {
                if(i < s_up) s[j][i] = B_EBONSTONE;
                else if(i < s_up + rnd(3) && rnd(2) == 0) s[j][i] = B_EBONSTONE;
                else if(i > s_down) s[j][i] = B_EBONSTONE;
                else if(i > s_down - rnd(3) && rnd(2) == 0) s[j][i] = B_EBONSTONE;
                else s[j][i] = B_EBONWALL;
            }
            else if(d < s_mid && rnd(2) == 0 && i <= s_up && i >= s_down) s[j][i] = B_EBONWALL;
            else if(d < s_out - 2 && std::abs(i - spawn_y) > 3 + rnd(3)) s[j][i] = B_EBONSTONE;
            else if(d < s_out && rnd(2) == 0 && std::abs(i - spawn_y) > 3 + rnd(3))
                s[j][i] = B_EBONSTONE;
            else if(d < s_out - 2) s[j][i] = B_EBONWALL;
        }
    s[spawn_x][spawn_y] = B_CHEST;
    i32 idx = item::alloc_box(spawn_x, spawn_y);
    item::add_box_item(idx, B_ROTTEN_FLESH, rnd(10) + 10);
}

void try_spawn_ebon()
{
    for(i32 i = 1; i <= W / 250; i++)
        spawn_ebon_cave(i * 250 - 180 + rnd(110), 300 + rnd(630));
}

// ---- 湖 ----
void make_lake(i32 i, bool isunder)
{
    i32 j = 50;
    bool islava;
    if(rnd(2) == 0) islava = true;
    else islava = false;
    i32 wei_ = rnd(13) + 5;
    i32 dep = rnd(3) + 2;
    i32 now_high = 1, low = 0;
    if(!isunder)
    {
        for(i32 uu = i - 7; uu <= i + wei_ - 7; uu++)
        {
            i32 kk = 50;
            while(kk < H - 2)
            {
                kk++;
                if(is_water(s[uu][kk])) return;
                if(s[uu][kk] != B_AIR)
                {
                    low = std::max(low, kk);
                    break;
                }
            }
        }
        if(low == 0) return;
        j = low;
    }
    else j = rnd(700) + 200;
    for(i32 uu = i - 7; uu <= i + wei_ - 7; uu++)
    {
        for(i32 kk = j - 2; kk <= j + 1; kk++) s[uu][kk] = B_AIR;
        for(i32 kk = j - 8; kk <= j - 3; kk++)
            if(s[uu][kk] == B_LEAF || s[uu][kk] == B_LOG) s[uu][kk] = B_AIR;
    }
    for(i32 uu = i - 8; uu <= i + wei_ - 6; uu++)
    {
        bool inx = (uu >= i - 7 && uu <= i + wei_ - 7);
        if(inx)
            for(i32 kk = j; kk <= j + now_high; kk++)
                s[uu][kk] = islava ? B_LAVA : B_WATER;
        for(i32 kk = j + now_high + 1 - (!inx) * 2; kk <= j + now_high + 2; kk++)
            s[uu][kk] = (islava || j >= 200) ? B_STONE : B_DIRT;
        if(inx)
        {
            if(uu <= i - 7 + wei_ / 2 && now_high < dep) { if(rnd(5) != 0) now_high++; }
            if(uu > i - 7 + wei_ / 2 && now_high > 0) { if(rnd(5) != 0) now_high--; }
        }
    }
}

// ---- 树 ----
void fill_blocks(const i32 block_id, const i32 stx, const i32 sty, const i32 enx,
                 const i32 eny)
{
    for(i32 i = stx; i <= enx; i++)
        for(i32 j = sty; j <= eny; j++)
        {
            if(i < 0 || i >= W || j < 0 || j >= H) continue;
            s[i][j] = (i16)block_id;
        }
}

void make_tree(i32 i, i32 tall)
{
    if(boimes2[i] == 0)
    {
        s[i][tall] = B_LOG; tall--;
        s[i][tall] = B_LOG; tall--;
        s[i][tall] = B_LOG;
        while(rnd(2) == 0) { tall--; s[i][tall] = B_LOG; }
        if(rnd(2) == 0)
        {
            fill_blocks(B_LEAF, i - 1, tall - 2, i + 1, tall);
            s[i][tall] = B_LOG;
        }
        else
        {
            fill_blocks(B_LEAF, i - 2, tall - 2, i + 2, tall);
            fill_blocks(B_LEAF, i - 1, tall - 3, i + 1, tall - 3);
            s[i][tall] = B_LOG;
        }
    }
    else
    {
        s[i][tall] = B_LOG; tall--;
        s[i][tall] = B_LOG; tall--;
        s[i][tall] = B_LOG;
        if(rnd(2) == 0) { tall--; s[i][tall] = B_LOG; }
        fill_blocks(B_LEAF, i - 2, tall - 1, i + 2, tall);
        fill_blocks(B_LEAF, i - 1, tall - 2, i + 1, tall - 2);
        s[i][tall] = B_LOG;
        tall -= 3;
        s[i][tall] = B_LOG; tall--;
        s[i][tall] = B_LOG;
        while(rnd(2) == 0) { tall--; s[i][tall] = B_LOG; }
        fill_blocks(B_LEAF, i - 1, tall - 1, i + 1, tall - 1);
        s[i - 1][tall] = B_LEAF;
        s[i + 1][tall] = B_LEAF;
        s[i][tall - 2] = B_LEAF;
    }
}

// ---- 矿脉散布 ----
void st(i32 type, i32 x, i32 y, i32 ra2)
{
    if(x < 1 || x >= W - 1 || y < 1 || y >= H - 1 || ra2 <= 0) return;
    if(s[x][y] == B_STONE || s[x][y] == B_ICE) s[x][y] = (i16)type;
    static const i32 DX[8] = {1, -1, 1, -1, 1, -1, 0, 0};
    static const i32 DY[8] = {0, 0, 1, -1, -1, 1, 1, -1};
    for(i32 k = 0; k < 8; k++)
    {
        i32 nx = x + DX[k], ny = y + DY[k];
        if(nx < 1 || nx >= W - 1 || ny < 1 || ny >= H - 1) continue;
        if((s[nx][ny] == B_STONE || s[nx][ny] == B_ICE) && rnd(ra2) == 0)
            s[nx][ny] = (i16)type;
    }
}

void st_nether(i32 type, i32 x, i32 y, i32 ra2)
{
    if(x < 1 || x >= W - 1 || y < 1 || y >= H - 1 || ra2 <= 0) return;
    if(s2[x][y] == B_NETHERRACK) s2[x][y] = (i16)type;
    static const i32 DX[8] = {1, -1, 1, -1, 1, -1, 0, 0};
    static const i32 DY[8] = {0, 0, 1, -1, -1, 1, 1, -1};
    for(i32 k = 0; k < 8; k++)
    {
        i32 nx = x + DX[k], ny = y + DY[k];
        if(nx < 1 || nx >= W - 1 || ny < 1 || ny >= H - 1) continue;
        if(s2[nx][ny] == B_NETHERRACK && rnd(ra2) == 0) s2[nx][ny] = (i16)type;
    }
}

// ---- 宝箱 ----
bool can_place_chest(i32 xxx, i32 yyy)
{
    if(xxx <= 2 || xxx >= W - 2 || yyy <= 5 || yyy >= H - 6) return false;
    if(s[xxx][yyy] != B_AIR) return false;
    if(!(s[xxx][yyy - 1] == B_AIR || s[xxx][yyy - 1] == B_SKY)) return false;
    i32 below = s[xxx][yyy + 1];
    if(below == B_AIR || below == B_SKY) return false;
    if(is_liquid(below)) return false;
    if(s[xxx][yyy] == B_CHEST || s[xxx - 1][yyy] == B_CHEST ||
            s[xxx + 1][yyy] == B_CHEST || s[xxx][yyy - 1] == B_CHEST ||
            s[xxx][yyy + 1] == B_CHEST) return false;
    return true;
}

void spawn_chest(i32 xxx, i32 yyy)
{
    if(!can_place_chest(xxx, yyy)) return;
    s[xxx][yyy] = B_CHEST;
    i32 idx = item::alloc_box(xxx, yyy);
    item::fill_box_loot(idx);
}

// ---- 天空填充 / 全图清理 ----
void clear_formap()
{
    for(i32 i = 0; i < W; i++)
    {
        for(i32 j = 0; j <= 180; j++)
            if(s[i][j] == B_AIR) s[i][j] = B_SKY;
        for(i32 j = 950; j <= 999; j++)
            if(s[i][j] == B_AIR || s[i][j] == B_WATER) s[i][j] = B_LAVA;
    }
}

// 视野内把空气换成天空（旧版 Clear()）
void clear_view(i32 cx)
{
    if(dim == DIM_NETHER) return;
    for(i32 i = cx - 99; i <= cx + 100; i++)
    {
        if(i < 0 || i >= W) continue;
        for(i32 j = 0; j <= 180; j++)
            if(s[i][j] == B_AIR) s[i][j] = B_SKY;
    }
}

}   // namespace

// 供外部调用的 Clear()（旧版同名）
void clear_around(i32 cx)
{
    clear_view(cx);
}

// ============================================================================
//                                下界生成
// ============================================================================
namespace {

void gen_nether()
{
    i32 tall = 900;
    water = 0;
    i32 wmax = 0, wh = 0;
    for(i32 i = 0; i < W; i++)
    {
        if(rnd(15) == 0) tall++;
        if(rnd(15) == 1) tall--;
        if(water <= 0)
        {
            if(rnd(120) == 0) water = 30 + rnd(30), wmax = water, wh = tall + 1;
        }
        else if(water > wmax / 2)
        {
            tall += rnd(2) + 1;
            water--;
        }
        else
        {
            tall -= rnd(2) + 1;
            if(!(tall + 1 > wh && water == 1)) water--;
        }
        if(tall < 0) tall = 0;
        for(i32 j = H - 1; j >= tall + 1; j--) s2[i][j] = B_NETHERRACK;
        if(water > 0)
            for(i32 j = tall + 1; j >= wh; j--) s2[i][j] = B_LAVA;
        if(rnd(15) == 2 && water <= 0 && i >= 2 && i < W - 2)
        {
            i32 a = tall;
            s2[i][tall] = B_LOG; tall--;
            s2[i][tall] = B_LOG; tall--;
            s2[i][tall] = B_LOG;
            while(rnd(2) == 0) { tall--; s2[i][tall] = B_LOG; }
            if(rnd(2) == 0)
            {
                s2[i - 1][tall - 1] = B_LEAF; s2[i][tall - 1] = B_LEAF;
                s2[i + 1][tall - 1] = B_LEAF; s2[i - 1][tall] = B_LEAF;
                s2[i + 1][tall] = B_LEAF;     s2[i - 1][tall - 2] = B_LEAF;
                s2[i][tall - 2] = B_LEAF;     s2[i + 1][tall - 2] = B_LEAF;
            }
            else
            {
                s2[i - 1][tall - 1] = B_LEAF; s2[i - 2][tall - 1] = B_LEAF;
                s2[i][tall - 1] = B_LEAF;     s2[i + 1][tall - 1] = B_LEAF;
                s2[i + 2][tall - 1] = B_LEAF; s2[i - 2][tall] = B_LEAF;
                s2[i - 1][tall] = B_LEAF;     s2[i + 1][tall] = B_LEAF;
                s2[i + 2][tall] = B_LEAF;     s2[i - 2][tall - 2] = B_LEAF;
                s2[i - 1][tall - 2] = B_LEAF; s2[i][tall - 2] = B_LEAF;
                s2[i + 1][tall - 2] = B_LEAF; s2[i + 2][tall - 2] = B_LEAF;
                s2[i - 1][tall - 3] = B_LEAF; s2[i][tall - 3] = B_LEAF;
                s2[i + 1][tall - 3] = B_LEAF;
            }
            tall = a;
        }
    }
    report("正在创建下界...", 0.72f);
    spawn_noise_nether();
    for(i32 i = 5; i <= W - 6; i++)
    {
        if(i % (W / 20) == 5) report("正在雕刻下界地形...", 0.72f + 0.08f * i / W);
        for(i32 j = H - 6; j >= 0; j--)
            if(noise_result[i][j] < -15)
                s2[i][j] = (rnd(100) == 0) ? B_LAVA : B_NETHERRACK;
    }
    for(i32 i = 5; i <= W - 6; i++)
    {
        for(i32 j = H - 6; j >= 0; j--)
            if(rnd(300) == 0) st_nether(B_NETHER_GOLD, i, j, 3);
        for(i32 j = H - 2; j >= 900; j--)
            if(rnd(1000) == 0)
            {
                s2[i][j] = B_ANCIENT_DEBRIS;
                if(rnd(5) < 2) s2[i + rnd(3) - 1][j + rnd(3) - 1] = B_ANCIENT_DEBRIS;
                if(rnd(5) < 2) s2[i + rnd(3) - 1][j + rnd(3) - 1] = B_ANCIENT_DEBRIS;
            }
    }
}

}   // namespace

// ============================================================================
//                                光照
// ============================================================================
void spread_light(i32 i, i32 j, i32 lt, i32 v)
{
    if(i < 0 || i >= W || j < 0 || j >= H || lt <= 0) return;
    v = touch1(s[i][j]) ? 3 : 1;
    light[i][j] = (i16)lt;
    if(lt - v > light[i + 1][j]) spread_light(i + 1, j, lt - v, v);
    if(lt - v > light[i - 1][j]) spread_light(i - 1, j, lt - v, v);
    if(lt - v > light[i][j + 1]) spread_light(i, j + 1, lt - v, v);
    if(lt - v > light[i][j - 1]) spread_light(i, j - 1, lt - v, v);
}

void set_light()
{
    const i32 px = g_light_cx, py = g_light_cy;

    if(gamemode == 0)
    {
        for(i32 i = px - 169; i <= px + 170; i++)
        {
            if(i < 0 || i >= W) continue;
            for(i32 j = 0; j < H; j++) light[i][j] = 15;
        }
        return;
    }
    for(i32 i = px - 169; i <= px + 170; i++)
    {
        if(i < 0 || i >= W) continue;
        for(i32 j = 0; j < H; j++) light[i][j] = 0;
    }

    // 玩家手持火把（旧版 beibao[cho][0]==16）与魔光护符光源。
    // 火把改成扫描玩家脚下那一格有没有火把（语义等价，world 不必依赖背包模块）；
    // 魔光护符由 game 层每帧写入 world::g_amulet。
    if(px >= 0 && px < W && py >= 0 && py < H)
    {
        if(s[px][py] == B_TORCH) light[px][py] = 19;
        // 玩家身上插着火把（旧版行为）：脚下与头顶都点亮
        if(py + 1 < H && s[px][py + 1] == B_TORCH) light[px][py] = 19;
        // 魔光护符（旧版 setlight 1432）：背包里有就随身 12 级光。
        // 旧版是无条件写 12（会把同时手持的火把 19 压暗），这里取较大值。
        if(g_amulet && light[px][py] < 12) light[px][py] = 12;
    }

    if(dim == DIM_NETHER)
    {
        for(i32 i = px - 149; i <= px + 150; i++)
        {
            if(i < 0 || i >= W) continue;
            for(i32 j = 0; j < H; j++)
                if((s[i][j] == B_AIR || s[i][j] == B_SKY) && light[i][j] <= 8)
                    light[i][j] = 8;
        }
    }

    i32 now_light = 20;
    if(night > 0) now_light = 7;
    if(night == NIGHT_DUSK) now_light += (20000 - now_time) / 153;
    if(night == NIGHT_DAWN) now_light += (now_time - 38000) / 153;

    if(dim != DIM_NETHER)
    {
        for(i32 i = px - 149; i <= px + 150; i++)
        {
            if(i < 0 || i >= W) continue;
            if((s[i][0] == B_AIR || s[i][0] == B_SKY) && light[i][0] < now_light)
                light[i][0] = (i16)now_light;
        }
        for(i32 i = px - 149; i <= px + 150; i++)
        {
            if(i < 0 || i >= W) continue;
            for(i32 j = 1; j < H; j++)
                if((s[i][j] == B_AIR || s[i][j] == B_SKY) && light[i][j] < now_light &&
                        light[i][j - 1] == now_light)
                    light[i][j] = (i16)now_light;
        }
    }

    // 光源传播
    for(i32 i = px - 149; i <= px + 150; i++)
    {
        if(i < 0 || i >= W) continue;
        for(i32 j = py - 199; j <= py + 200; j++)
        {
            if(j < 0 || j >= H) continue;
            if(s[i][j] == B_TORCH || is_lava(s[i][j]) || s[i][j] == B_EBONWALL)
            {
                if(s[i][j] == B_EBONWALL)
                {
                    if(light[i][j] > 7) spread_light(i, j, light[i][j], 1);
                    else spread_light(i, j, 7, 1);
                }
                else spread_light(i, j, 19, 1);
            }
            else if(light[i][j] > 0)
                spread_light(i, j, light[i][j], 1);
        }
    }
}

// ============================================================================
//                                流体
// ============================================================================
// 旧版把 getnew 声明成 block_update() 内的局部静态数组（20 MB）。
// 这里提到文件作用域，语义相同但避免每帧构造/析构。
i16 getnew[W][H];
i32 TT = 0;

void reset_block_update()
{
    memset(getnew, 0, sizeof(getnew));
    TT = 0;
}
i32 block_update_phase() { return TT; }

// 旧版 block_update()：水/岩浆流动。逐行照抄，只把 rand 换掉。
void block_update()
{
    const i32 px = g_light_cx, py = g_light_cy;

    TT++;
    if(TT < 10)
    {
        for(i32 i = px - 150; i <= px + 150; i++)
            for(i32 j = py + (TT - 5) * 22; j <= py + (TT - 4) * 22; j++)
            {
                if(i <= 0 || j <= 0 || i >= W - 1 || j >= H - 1) continue;
                getnew[i][j] = 0;
            }
    }
    if(TT >= 10)
    {
        TT = 0;
        for(i32 i = px - 120; i <= px + 120; i++)
            for(i32 j = py - 80; j <= py + 80; j++)
            {
                if(i <= 0 || j <= 0 || i >= W - 1 || j >= H - 1) continue;
                if(s[i][j] == B_LAVA)
                {
                    if(s[i][j + 1] == B_AIR || s[i][j + 1] == B_SKY) getnew[i][j + 1] = 1;
                    if(is_water(s[i][j + 1]))
                        getnew[i][j + 1] = (s[i][j + 1] == B_WATER) ? -2 : -1;
                    if(is_water(s[i - 1][j]) && s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i - 1][j] = (s[i - 1][j] == B_WATER) ? -2 : -1;
                    if(is_water(s[i + 1][j]) && s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i + 1][j] = (s[i + 1][j] == B_WATER) ? -2 : -1;
                    if((s[i - 1][j] == B_AIR || s[i - 1][j] == B_SKY) &&
                            s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i - 1][j] = 33;
                    if((s[i + 1][j] == B_AIR || s[i + 1][j] == B_SKY) &&
                            s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i + 1][j] = 33;
                }
                if(s[i][j] == B_LAVA2)
                {
                    if(s[i][j + 1] == B_AIR || s[i][j + 1] == B_SKY) getnew[i][j + 1] = 1;
                    if(is_water(s[i][j + 1])) getnew[i][j + 1] = -1;
                    if(is_water(s[i - 1][j]) && s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i - 1][j] = -1;
                    if(is_water(s[i + 1][j]) && s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i + 1][j] = -1;
                    if((s[i - 1][j] == B_AIR || s[i - 1][j] == B_SKY) &&
                            s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i - 1][j] = 34;
                    if((s[i + 1][j] == B_AIR || s[i + 1][j] == B_SKY) &&
                            s[i][j + 1] != B_LAVA && getnew[i][j + 1] != 1)
                        getnew[i + 1][j] = 34;
                }
                if(s[i][j] == B_LAVA1)
                {
                    if(s[i][j + 1] == B_AIR || s[i][j + 1] == B_SKY) getnew[i][j + 1] = 1;
                    if(is_water(s[i][j + 1])) getnew[i][j + 1] = -1;
                }
            }
        for(i32 i = px - 120; i <= px + 120; i++)
            for(i32 j = py - 80; j <= py + 80; j++)
            {
                if(i <= 0 || j <= 0 || i >= W - 1 || j >= H - 1) continue;
                if(s[i][j] == B_WATER)
                {
                    if(s[i][j + 1] == B_AIR || s[i][j + 1] == B_SKY) getnew[i][j + 1] = 2;
                    if(is_lava(s[i][j + 1]))
                        getnew[i][j + 1] = (s[i][j + 1] == B_LAVA) ? -2 : -1;
                    if(is_lava(s[i - 1][j]) && s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i - 1][j] = (s[i - 1][j] == B_LAVA) ? -2 : -1;
                    if(is_lava(s[i + 1][j]) && s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i + 1][j] = (s[i + 1][j] == B_LAVA) ? -2 : -1;
                    if((s[i - 1][j] == B_AIR || s[i - 1][j] == B_SKY) &&
                            s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i - 1][j] = 20;
                    if((s[i + 1][j] == B_AIR || s[i + 1][j] == B_SKY) &&
                            s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i + 1][j] = 20;
                }
                if(s[i][j] == B_FLOW3)
                {
                    if(s[i][j + 1] == B_AIR || s[i][j + 1] == B_SKY) getnew[i][j + 1] = 2;
                    if(is_lava(s[i][j + 1]))
                    {
                        getnew[i][j + 1] = -1;
                        if(s[i][j + 1] == B_LAVA) getnew[i][j + 1] = -2;
                    }
                    if(is_lava(s[i - 1][j]) && s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                    {
                        getnew[i - 1][j] = -1;
                        if(s[i - 1][j] == B_LAVA) getnew[i - 1][j] = -2;
                    }
                    if(is_lava(s[i + 1][j]) && s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                    {
                        getnew[i + 1][j] = -1;
                        if(s[i + 1][j] == B_LAVA) getnew[i + 1][j] = -2;
                    }
                    if((s[i - 1][j] == B_AIR || s[i - 1][j] == B_SKY) &&
                            s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i - 1][j] = 21;
                    if((s[i + 1][j] == B_AIR || s[i + 1][j] == B_SKY) &&
                            s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i + 1][j] = 21;
                }
                if(s[i][j] == B_FLOW2)
                {
                    if(s[i][j + 1] == B_AIR || s[i][j + 1] == B_SKY) getnew[i][j + 1] = 2;
                    if(is_lava(s[i][j + 1]))
                    {
                        getnew[i][j + 1] = -1;
                        if(s[i][j + 1] == B_LAVA) getnew[i][j + 1] = -2;
                    }
                    if(is_lava(s[i - 1][j]) && s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                    {
                        getnew[i - 1][j] = -1;
                        if(s[i - 1][j] == B_LAVA) getnew[i - 1][j] = -2;
                    }
                    if(is_lava(s[i + 1][j]) && s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                    {
                        getnew[i + 1][j] = -1;
                        if(s[i + 1][j] == B_LAVA) getnew[i + 1][j] = -2;
                    }
                    if((s[i - 1][j] == B_AIR || s[i - 1][j] == B_SKY) &&
                            s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i - 1][j] = 22;
                    if((s[i + 1][j] == B_AIR || s[i + 1][j] == B_SKY) &&
                            s[i][j + 1] != B_WATER && getnew[i][j + 1] != 2)
                        getnew[i + 1][j] = 22;
                }
                if(s[i][j] == B_FLOW1)
                {
                    if(s[i][j + 1] == B_AIR || s[i][j + 1] == B_SKY) getnew[i][j + 1] = 2;
                    if(is_lava(s[i][j + 1]))
                    {
                        getnew[i][j + 1] = -1;
                        if(s[i][j + 1] == B_LAVA) getnew[i][j + 1] = -2;
                    }
                }
            }
        for(i32 i = px - 120; i <= px + 120; i++)
            for(i32 j = py - 80; j <= py + 80; j++)
            {
                if(i <= 0 || j <= 0 || i >= W - 1 || j >= H - 1) continue;
                if(getnew[i][j] > 3) { s[i][j] = getnew[i][j]; getnew[i][j] = 0; }
                if(getnew[i][j] == 2) { s[i][j] = B_WATER; getnew[i][j] = 0; }
                if(getnew[i][j] == 1) { s[i][j] = B_LAVA; getnew[i][j] = 0; }
                if(getnew[i][j] == -1) { s[i][j] = B_STONE; getnew[i][j] = 0; }
                if(getnew[i][j] == -2) { s[i][j] = B_OBSIDIAN; getnew[i][j] = 0; }
            }
        mark_dirty_range(px - 120, px + 120);
    }
}

// ============================================================================
//                            主世界生成
// ============================================================================
void generate(void (*progress)(const char *, f32, void *), void *ud)
{
    g_progress = progress;
    g_progress_ud = ud;

    for(i32 c = 0; c <= W; c++) g_col_chunk[c] = (c >= W) ? NCHUNKS - 1 : c / CHUNK_COLS;
    memset(g_dirty, 1, sizeof(g_dirty));
    g_dirty_meta = true;

    // ---------------- 生物群系 ----------------
    memset(boimes, 0, sizeof(boimes));
    memset(boimes2, 0, sizeof(boimes2));
    i32 mouns[64], mouns2[64];
    i32 js_ = 0, js_2 = 0;
    for(i32 i = 50; i <= W - 301; i++)
    {
        if(rnd(700) == 0 && js_ < 64)
        {
            i32 ran = rnd(300) + 100;
            for(i32 j = i - 25; j < i + ran; j++) boimes[j] = 1;
            mouns[js_] = i;
            mouns2[js_] = ran + i;
            js_++;
            i += ran;
        }
    }
    for(i32 i = 50; i <= W - 301; i++)
    {
        if(rnd(700) == 0)
        {
            i32 ran = rnd(300) + 100;
            for(i32 j = i - 25; j < i + ran; j++) boimes2[j] = 2;
            js_2++;
            i += ran;
        }
    }

    report("正在生成地形高度...", 0.02f);
    memset(s, 0, sizeof(s));
    memset(s2, 0, sizeof(s2));
    memset(ep, 0, sizeof(ep));
    memset(light, 0, sizeof(light));
    water = 0;

    for(i32 i = 0; i < W; i++) zs_ls[i] = 740;
    perlin(400, 60);
    perlin(400, 60);
    perlin(200, 45);
    perlin(100, 22);
    perlin(50, 11);
    perlin(25, 5);
    perlin(12, 2);
    for(i32 i = 0; i < js_; i++)
    {
        perlin_2(25, 30, mouns[i], mouns2[i], rnd(4) + 2);
        perlin_2(12, 10, mouns[i], mouns2[i], rnd(3) + 2);
        perlin_2(6, 5, mouns[i], mouns2[i], rnd(2) + 2);
    }
    result_heights();

    // ---------------- 地表 ----------------
    report("正在铺设地表...", 0.08f);
    i32 tall = 149;
    i32 wmax = 0, wh = 0;
    water = 0;
    for(i32 i = 0; i < W; i++)
    {
        tall = H - 1 - zs[i];
        if(tall < 0) tall = 0;
        if(i == W / 2)
        {
            back_x = W / 2;
            back_y = tall;
            g_light_cx = W / 2;
            g_light_cy = tall;
        }
        for(i32 j = H - 1; j >= tall + 1; j--)
        {
            i32 dirt = B_DIRT, store = B_STONE;
            bool check = true;
            for(i32 ii = 0; ii <= 4 && check; ii++)
                if(boimes2[i + ii] == 2)
                    if(rnd(5) < 5 - ii) { store = B_ICE; dirt = B_SNOW; check = false; }
            for(i32 ii = -4; ii <= -1 && check; ii++)
                if(boimes2[i + ii] == 2)
                    if(rnd(5) < 5 + ii) { store = B_ICE; dirt = B_SNOW; check = false; }
            s[i][j] = (i16)store;
            if(j == H - 1) s[i][j] = B_BEDROCK;
            if(boimes[i] == 1 && tall < 100) continue;
            if(boimes[i] == 1 && tall <= 105)
            {
                if(j <= tall + (tall - 100)) s[i][j] = (i16)dirt;
                if(j <= tall + 1 + (tall - 100) && rnd(2) == 0) s[i][j] = (i16)dirt;
                continue;
            }
            if(tall > 160 && tall <= 165)
            {
                if(j <= 165) s[i][j] = (i16)dirt;
                if(j <= 166 && rnd(2) == 0) s[i][j] = (i16)dirt;
            }
            else
            {
                if(j <= tall + 6 && tall < 165) s[i][j] = (i16)dirt;
                if(j <= tall + 7 && tall < 165 && rnd(2) == 0) s[i][j] = (i16)dirt;
            }
        }
        if(tall >= 160)
            for(i32 j = tall + 1; j >= 160; j--) s[i][j] = B_WATER;
        else if(rnd(15) == 2 && water <= 0 && i > 2 && i < W - 2 &&
                !(boimes[i] == 1 && tall < 100))
            make_tree(i, tall);
    }

    // ---------------- 矿脉 ----------------
    report("正在埋藏矿脉...", 0.16f);
    for(i32 i = 5; i <= W - 6; i++)
    {
        for(i32 j = 995; j >= 125; j--)
        {
            if(rnd(450) == 0) st(B_COAL, i, j, 2);
            if(j > 165 && rnd(700 - (i32)(j * 0.3)) == 0) st(B_RAW_IRON, i, j, 3);
            if(boimes2[i] == 0 && rnd(1000) == 0) st(B_DIAMOND, i, j, 2);
            if(j > 250 && rnd(800 - (i32)(j * 0.3)) == 0) st(B_RAW_GOLD, i, j, 4);
            if(j > 350 && rnd(1000 - (i32)(j * 0.5)) == 0) st(B_NETHER_GOLD, i, j, 5);
        }
        if(boimes[i] == 1)
            for(i32 j = 175; j >= 15; j--)
            {
                if(rnd(235 + j) == 0) st(B_COAL, i, j, 2);
                if(rnd(485 + j) == 0) st(B_RAW_IRON, i, j, 3);
            }
    }

    // ---------------- 洞穴 ----------------
    report("正在开凿洞穴...", 0.26f);
    spawn_noise_cave();
    for(i32 i = 5; i <= W - 6; i++)
    {
        if(i % (W / 20) == 5) report("正在开凿洞穴...", 0.26f + 0.16f * i / W);
        if(rnd(200) == 0) make_deep(i);
        if(rnd(200) == 0) make_skycave(i);
        for(i32 j = 995; j >= 1030 - zs[i]; j--)
        {
            if(noise_result[i][j] < -40 + (j - 1030 + zs[i]) * 0.04) s[i][j] = B_AIR;
            if(rnd(4000) == 0) noodle_cave(i, j);
        }
        for(i32 j = 1029 - zs[i]; j >= H - 1 - zs[i]; j--)
            if(noise_result[i][j] < -40 + (j - 1030 + zs[i]) * 2) s[i][j] = B_AIR;
    }
    sweep_floating_snow(1);

    // ---------------- 水/岩浆/湖 ----------------
    report("正在灌注水流与岩浆...", 0.44f);
    for(i32 i = 20; i <= W - 21; i++)
    {
        if(i % (W / 20) == 0) report("正在灌注水流与岩浆...", 0.44f + 0.08f * i / W);
        for(i32 j = 995; j >= 1030 - zs[i]; j--)
        {
            if(rnd(200) == 0 && (s[i][j + 1] == B_STONE || s[i][j + 1] == B_ICE) &&
                    (s[i][j - 1] == B_STONE || s[i][j - 1] == B_ICE))
            {
                if((s[i - 1][j] == B_AIR && (s[i + 1][j] == B_STONE || s[i + 1][j] == B_ICE)) ||
                        (s[i + 1][j] == B_AIR && (s[i - 1][j] == B_STONE || s[i - 1][j] == B_ICE)))
                    s[i][j] = B_WATER;
            }
            if(rnd(std::max(1, (i32)(400 - j * 0.02))) == 0 &&
                    (s[i][j + 1] == B_STONE || s[i][j + 1] == B_ICE) &&
                    (s[i][j - 1] == B_STONE || s[i][j - 1] == B_ICE))
            {
                if((s[i - 1][j] == B_AIR && (s[i + 1][j] == B_STONE || s[i + 1][j] == B_ICE)) ||
                        (s[i + 1][j] == B_AIR && (s[i - 1][j] == B_STONE || s[i - 1][j] == B_ICE)))
                    s[i][j] = B_LAVA;
            }
            if(rnd(10000) == 0) make_water((i16)i, (i16)j, (i16)(rnd(50)));
            if(rnd(18000) == 0 && j > 300) make_lava((i16)i, (i16)j, (i16)(30 + rnd(20)));
        }
        for(i32 j = 1029 - zs[i]; j >= H - 1 - zs[i]; j--)
        {
            if(rnd(200) == 0 && (s[i][j + 1] == B_STONE || s[i][j + 1] == B_ICE) &&
                    (s[i][j - 1] == B_STONE || s[i][j - 1] == B_ICE))
            {
                if((s[i - 1][j] == B_AIR && (s[i + 1][j] == B_STONE || s[i + 1][j] == B_ICE)) ||
                        (s[i + 1][j] == B_AIR && (s[i - 1][j] == B_STONE || s[i - 1][j] == B_ICE)))
                    s[i][j] = B_WATER;
            }
            if(rnd(std::max(1, (i32)(400 - j * 0.02))) == 0 &&
                    (s[i][j + 1] == B_STONE || s[i][j + 1] == B_ICE) &&
                    (s[i][j - 1] == B_STONE || s[i][j - 1] == B_ICE))
            {
                if((s[i - 1][j] == B_AIR && (s[i + 1][j] == B_STONE || s[i + 1][j] == B_ICE)) ||
                        (s[i + 1][j] == B_AIR && (s[i - 1][j] == B_STONE || s[i - 1][j] == B_ICE)))
                    s[i][j] = B_LAVA;
            }
        }
        if(rnd(10) == 0)
        {
            if(rnd(10) == 0) make_lake(i, false);
            else make_lake(i, true);
        }
    }

    // ---------------- 结构 ----------------
    report("正在挖掘废弃矿井...", 0.54f);
    make_mineshaft();
    report("正在生成腐化空腔...", 0.60f);
    try_spawn_ebon();

    report("正在安置宝箱...", 0.64f);
    for(i32 i = 10; i <= W - 11; i++)
    {
        for(i32 j = 200; j <= 950; j++)
            if(rnd(12000) == 0) spawn_chest(i, j);
        if(rnd(250) == 0)
            for(i32 t = 0; t < 2; t++) spawn_chest(i, 200 + rnd(750));
    }

    clear_formap();
    report("正在点燃下界...", 0.70f);
    gen_nether();
    report("正在传播光照...", 0.86f);
    set_light();
    report("生成完成", 1.0f);
}

// ============================================================================
//                        旧存档扩展（新列补生成）
// ============================================================================
void expand_from(i32 from_x)
{
    if(from_x >= W) return;
    i32 start_biome = from_x > 50 ? from_x : 50;
    for(i32 i = from_x; i < W; i++) { boimes[i] = 0; boimes2[i] = 0; }
    for(i32 i = start_biome; i <= W - 301; i++)
        if(rnd(700) == 0)
        {
            i32 ran = rnd(300) + 100;
            for(i32 j = i - 25; j < i + ran && j < W; j++) boimes[j] = 1;
            i += ran;
        }
    for(i32 i = start_biome; i <= W - 301; i++)
        if(rnd(700) == 0)
        {
            i32 ran = rnd(300) + 100;
            for(i32 j = i - 25; j < i + ran && j < W; j++) boimes2[j] = 2;
            i += ran;
        }

    for(i32 i = 0; i < W; i++) zs_ls[i] = 785;
    perlin(200, 60);
    perlin(100, 30);
    perlin(50, 15);
    perlin(25, 7);
    perlin(12, 3);
    result_heights();

    // 接缝平滑，避免垂直悬崖
    if(from_x > 0)
    {
        i32 old_surface = H - 2;
        for(i32 j = 0; j < H - 1; j++)
        {
            i16 b = s[from_x - 1][j];
            if(b != B_AIR && b != B_SKY && !is_liquid(b)) { old_surface = j; break; }
        }
        i32 blend = W - from_x;
        if(blend > 200) blend = 200;
        for(i32 i = from_x; i < from_x + blend; i++)
        {
            f64 t = (i - from_x + 1.0) / blend;
            i32 new_surface = H - 1 - zs[i];
            i32 surf = (i32)(old_surface * (1.0 - t) + new_surface * t + 0.5);
            if(surf < 0) surf = 0;
            if(surf > H - 1) surf = H - 1;
            zs[i] = (i16)(H - 1 - surf);
        }
    }

    for(i32 i = from_x; i < W; i++)
    {
        i32 t = H - 1 - zs[i];
        for(i32 j = H - 1; j >= t + 1; j--)
        {
            i32 dirt = B_DIRT, store = B_STONE;
            if(boimes2[i] == 2) { store = B_ICE; dirt = B_SNOW; }
            s[i][j] = (i16)store;
            if(j == H - 1) s[i][j] = B_BEDROCK;
            if(t > 160 && t <= 165) { if(j <= 165) s[i][j] = (i16)dirt; }
            else { if(j <= t + 6 && t < 165) s[i][j] = (i16)dirt; }
        }
        if(t >= 160)
            for(i32 j = t + 1; j >= 160; j--) s[i][j] = B_WATER;
    }
    for(i32 i = from_x; i < W; i++)
    {
        if(i <= 2 || i >= W - 2) continue;
        i32 t = H - 1 - zs[i];
        if(t < 0) t = 0;
        if(t >= 160) continue;
        if(boimes[i] == 1 && t < 100) continue;
        if(rnd(15) == 2) make_tree(i, t);
    }
    for(i32 i = from_x; i < W; i++)
        for(i32 j = 995; j >= 125; j--)
        {
            if(rnd(450) == 0) st(B_COAL, i, j, 2);
            if(j > 165 && rnd(700 - (i32)(j * 0.3)) == 0) st(B_RAW_IRON, i, j, 3);
            if(boimes2[i] == 0 && rnd(1000) == 0) st(B_DIAMOND, i, j, 2);
            if(j > 250 && rnd(800 - (i32)(j * 0.3)) == 0) st(B_RAW_GOLD, i, j, 4);
            if(j > 350 && rnd(1000 - (i32)(j * 0.5)) == 0) st(B_NETHER_GOLD, i, j, 5);
        }
    spawn_noise_cave();
    for(i32 i = from_x; i < W; i++)
    {
        if(rnd(400) == 0) make_deep(i);
        if(rnd(300) == 0) make_skycave(i);
        for(i32 j = 995; j >= 1030 - zs[i]; j--)
        {
            if(noise_result[i][j] < -45) s[i][j] = B_AIR;
            if(rnd(10000) == 0) make_water((i16)i, (i16)j, (i16)rnd(50));
            if(rnd(18000) == 0 && j > 300) make_lava((i16)i, (i16)j, (i16)(30 + rnd(20)));
            if(rnd(4000) == 0) noodle_cave(i, j);
        }
    }
    sweep_floating_snow(from_x);
    for(i32 i = from_x; i <= W - 21; i++)
    {
        if(i < 20) continue;
        if(rnd(20) == 0) make_lake(i, rnd(10) == 0 ? false : true);
    }
    // 下界新列
    for(i32 i = from_x; i < W; i++)
    {
        i32 ntall = 900;
        if(rnd(15) == 0) ntall++;
        if(rnd(15) == 1) ntall--;
        if(ntall < 0) ntall = 0;
        for(i32 j = H - 1; j >= ntall + 1; j--) s2[i][j] = B_NETHERRACK;
    }
    spawn_noise_nether();
    for(i32 i = (from_x > 5 ? from_x : 5); i <= W - 6; i++)
    {
        for(i32 j = H - 6; j >= 0; j--)
            if(noise_result[i][j] < -15)
                s2[i][j] = (rnd(100) == 0) ? B_LAVA : B_NETHERRACK;
        for(i32 j = H - 2; j >= 900; j--)
            if(rnd(1000) == 0)
            {
                s2[i][j] = B_ANCIENT_DEBRIS;
                if(rnd(5) < 2) s2[i + rnd(3) - 1][j + rnd(3) - 1] = B_ANCIENT_DEBRIS;
                if(rnd(5) < 2) s2[i + rnd(3) - 1][j + rnd(3) - 1] = B_ANCIENT_DEBRIS;
            }
        for(i32 j = H - 6; j >= 0; j--)
            if(rnd(300) == 0) st_nether(B_NETHER_GOLD, i, j, 3);
    }
    for(i32 i = from_x; i <= W - 11; i++)
    {
        if(i < 10) continue;
        for(i32 j = 200; j <= 950; j++)
            if(rnd(12000) == 0) spawn_chest(i, j);
        if(rnd(250) == 0)
            for(i32 t = 0; t < 2; t++) spawn_chest(i, 200 + rnd(750));
    }
    mark_all_dirty();
}

// ============================================================================
//                            维度切换
// ============================================================================
void swap_dim(bool make_door)
{
    (void)make_door;   // 传送门的生成由 game 层负责（它才知道玩家与方块表）
    dim = (dim == DIM_OVERWORLD) ? DIM_NETHER : DIM_OVERWORLD;
    mark_all_dirty();
    // 旧版把整个 s/s2 对调。这里同样交换，保证两边的方块数据都跟着走。
    for(i32 i = 0; i < W; i++)
        for(i32 j = 0; j < H; j++)
        {
            i16 t = s[i][j];
            s[i][j] = s2[i][j];
            s2[i][j] = t;
        }
    set_light();
}

}   // namespace world






