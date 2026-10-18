// ===================== 游戏核心（玩家 / 挖掘 / 放置 / 使用）=====================
// 旧版对照：
//   game_loop() 内的玩家移动物理  -> step_player()
//   wajue()                      -> game::mine_at()
//   block_()                     -> game::place_at()
//   checkuseitem() + 右键逻辑     -> game::use_selected()
//   hujia_update()               -> game::armor_update()
//   x_touch()/y_touch()          -> touch_x()/touch_y()
//   gohome()/Start()/shuaxin()   -> go_home()/start_respawn()/refresh_hud()
#include "game.h"
#include "game_render.h"
#include "../entity/entity.h"
#include "../platform/platform.h"
#include "../render/renderer.h"
#include "../item/chest.h"
#include "../world/blocks.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>

namespace game {

namespace {

State g_st;
i32 g_tick = 0;
i32 g_light_tick = 0;

// 挖掘状态（旧版 wj）：进度 + 当前正在挖的格子
f64 g_wj = 0;
i32 g_mine_x = -1, g_mine_y = -1;

// 挖掘速度倍率。1.0 = 旧版原速。
// 之前调到 2.6 太快（木镐挖石头 0.27 秒），现在按「木镐挖石头约 0.7 秒」调回。
constexpr f64 MINE_SPEED = 1.0;
}

// --trace 诊断用的合成输入开关（放在 game 命名空间，不在匿名命名空间里）
bool g_force_right = false;
bool g_force_jump = false;

State &st() { return g_st; }

void force_input(bool right, bool jump)
{
    g_force_right = right;
    g_force_jump = jump;
}

char world_name[64] = {0};

bool has_world() { return world_name[0] != 0; }

void toast(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_st.tip, sizeof(g_st.tip), fmt, ap);
    va_end(ap);
    g_st.tip_life = 3.2f;
}

void hud_tick(f32 dt)
{
    if(g_st.tip_life > 0) g_st.tip_life -= dt;
    if(g_st.l_life > 0) g_st.l_life--;
}

// ============================================================================
//                          护甲（旧版 hujia_update）
// ============================================================================
void armor_update()
{
    // 旧版：playerspeed=1, playerjump=12 起算，再按护甲加值
    g_st.pl.playerspeed = 0.5;
    g_st.pl.playerjump = 12;
    g_st.pl.hujiazhi = 0;

    // 魔光护符（旧版 setlight 1432 / hujia_update 1547）：
    // 背包里有就随身发光 12 级，并把跳跃上限从 12 提到 14。
    if(item::inv_find_existing(B_LIGHTS_AMULET) != 0) g_st.pl.playerjump = 14;

    // 旧版按 wear[] 里的物品 id 累加护甲值。新版本直接读护甲槽。
    for(i32 s = 1; s <= 4; s++)
    {
        i32 id = item::armor[s].id;
        if(id <= 0 || item::armor[s].count <= 0) continue;
        i32 rest = item::armor[s].remain();
        i32 mx = item::armor[s].max_dur();
        // 耐久越低护甲越薄：低于 25% 时护甲值减半
        f32 dur_k = (mx > 0 && rest * 4 < mx) ? 0.5f : 1.0f;
        f32 v = 0;
        switch(id)
        {
        case B_IRON_CHEST: v = 6; break;       case B_IRON_LEGS: v = 5; break;
        case B_IRON_HELM: v = 2; break;        case B_IRON_BOOTS: v = 2; break;
        case B_GOLD_CHEST: v = 5; break;       case B_GOLD_LEGS: v = 4; break;
        case B_GOLD_HELM: v = 1; break;        case B_GOLD_BOOTS: v = 1; break;
        case B_DIAMOND_CHEST: v = 8; break;    case B_DIAMOND_LEGS: v = 6; break;
        case B_DIAMOND_HELM: v = 3; break;     case B_DIAMOND_BOOTS: v = 3; break;
        case B_NETHERITE_CHEST: v = 9; break;  case B_NETHERITE_LEGS: v = 7; break;
        case B_NETHERITE_HELM: v = 4; break;   case B_NETHERITE_BOOTS: v = 4; break;
        case B_SHADOW_CHEST: v = 8; break;     case B_SHADOW_LEGS: v = 6; break;
        case B_SHADOW_HELM: v = 3; break;      case B_SHADOW_BOOTS: v = 3; break;
        default: break;
        }
        g_st.pl.hujiazhi += (i32)(v * dur_k);
        // 暗影套装额外加速度（旧版 96/97/95/98 各 +0.05）
        if(id >= B_SHADOW_HELM && id <= B_SHADOW_BOOTS) g_st.pl.playerspeed += 0.05;
    }

    // 旧版减伤公式
    i32 h = g_st.pl.hujiazhi;
    if(h <= 20) g_st.pl.jianshang = 1.0 - h * 0.04;
    else        g_st.pl.jianshang = 0.4 - h * 0.01;
    if(g_st.pl.jianshang < 0.05) g_st.pl.jianshang = 0.05;
}

// ============================================================================
//                              光照（旧版 setlight）
// ============================================================================
void light_update()
{
    world::g_light_cx = (i32)(g_st.pl.x + 0.5);
    world::g_light_cy = (i32)(g_st.pl.y + 0.5);
    world::set_light();
}

// ============================================================================
//                        碰撞（旧版 x_touch / y_touch）
// ============================================================================
namespace {

// 旧版直接用 blocktouch[s[i][j]]。这里保留 1=固体 / 2=可穿越平台 的语义。
inline i32 touch_of(i32 bx, i32 by)
{
    if(bx < 0 || bx >= world::W || by < 0 || by >= world::H) return 1;   // 边界当墙
    i16 b = (world::dim == world::DIM_NETHER) ? world::s2[bx][by] : world::s[bx][by];
    return block_of(b).touch;
}
inline i32 touch_at(f64 fx, f64 fy)
{
    return touch_of((i32)(fx + 0.5), (i32)(fy + 0.5));
}

// 把玩家精确贴到「当前所在格」的地面/天花板边界上。
//
// 旧版是 while(碰撞) y -= 0.1 这样一点点挪，挪到不碰为止。问题是落点会
// 停在任意小数上（比如 156.9），下一帧重力一加又压进下一格、触发一次
// touch_y，于是 y 每隔一帧跳一次 —— 画面表现就是「抽搐」。
// 这里改成一次算准：让 (y + 0.5) 正好落在目标格的合法区间内。
//   down=true  -> 贴这一格的上边界（站到地面上）
//   down=false -> 贴这一格的下边界（撞到天花板）
void snap_to_tile_boundary(bool down)
{
    Player &p = g_st.pl;
    i32 ty = (i32)(p.y + 0.5);
    if(down)
    {
        // 玩家中心现在落在固体格 ty 内，要「站到这一格的上表面」上，
        // 也就是把中心推到**上一格** (ty-1) 的最底部。
        // 格 ty 的覆盖范围是 [ty-0.5, ty+0.5)，上一格底部 = ty-0.5，
        // 所以目标 y = (ty-1) + 0.4999 = ty - 0.5001。
        // （旧实现写成 ty+0.4999 是这一格的**下**边界，等于让玩家沉进方块
        //   一整格再卡住，表现就是「在方块内也会往下掉」。）
        f64 want = (f64)ty - 0.5001;
        if(p.y > want) p.y = want;
    }
    else
    {
        // 撞天花板：把中心推到**下一格** (ty+1) 的最顶部 = ty + 0.5001。
        f64 want = (f64)ty + 0.5001;
        if(p.y < want) p.y = want;
    }
}

void fall_damage();      // 前置声明：touch_y 里要用

void touch_x()
{
    Player &p = g_st.pl;
    if(touch_at(p.x, p.y) != 1) return;

    // 世界边界当墙处理
    if(p.x < 3) { p.x = 3; p.spx = 0; return; }
    if(p.x > world::W - 4) { p.x = world::W - 4; p.spx = 0; return; }

    // 找离玩家最近的一个「中心不落在固体里」的 x。
    // 不做方向判断，直接两侧各扫一遍，取位移更小的那个解 —— 这样
    // 即使玩家已经被推进墙里好几格也能拉回来，而且不会把人往墙里推。
    // 旧版是 ±0.03/0.06 来回试探地推，落点停在任意小数上，
    // 下一帧继续推进又触发一次，贴着墙走时画面就会抖。
    f64 best = 0;
    bool found = false;
    for(i32 k = 1; k <= 60; k++)
    {
        f64 cand = p.x - 0.05 * k;
        if(touch_at(cand, p.y) != 1) { best = cand; found = true; break; }
    }
    for(i32 k = 1; k <= 60 && !found; k++)
    {
        f64 cand = p.x + 0.05 * k;
        if(touch_at(cand, p.y) != 1) { best = cand; found = true; break; }
    }
    if(found)
    {
        // 贴到该格边界：让 (x + 0.5) 落在合法格的区间内
        i32 tx = (i32)(best + 0.5);
        if(best < p.x) p.x = (f64)tx + 0.4999;   // 从左边靠上
        else           p.x = (f64)tx - 0.4999;   // 从右边靠上
    }
    if(p.x < 1) p.x = 1;
    if(p.x > world::W - 2) p.x = world::W - 2;
    p.spx = 0;
}

void touch_y()
{
    i32 pb = touch_at(g_st.pl.x, g_st.pl.y);
    // 平台（touch==2）：只在下落且脚已过中心线时踩住
    if(pb == 2)
    {
        if(g_st.pl.y - (i32)g_st.pl.y > 0.5)
        {
            if(g_st.pl.spy > 0)
            {
                i32 guard = 0;
                while(touch_at(g_st.pl.x, g_st.pl.y) == 2 &&
                        g_st.pl.y - (i32)g_st.pl.y > 0.5 && guard++ < 200)
                    g_st.pl.y -= 0.05;
                fall_damage();
                g_st.pl.jump = 0;
                g_st.pl.spy = 0;
            }
            else
            {
                g_st.pl.fell = g_st.pl.y;
                if(g_st.pl.spy >= 0) g_st.pl.spy = 2;
            }
        }
    }
    pb = touch_at(g_st.pl.x, g_st.pl.y);
    if(pb == 1)
    {
        if(g_st.pl.spy > 0)
        {
            // 落地：精确贴到地面，避免落点停在格边界外导致每帧抖动
            snap_to_tile_boundary(true);
            fall_damage();
            g_st.pl.jump = 0;
            g_st.pl.spy = 0;
        }
        if(g_st.pl.spy < 0)
        {
            // 撞天花板：精确贴到天花板下沿
            snap_to_tile_boundary(false);
            g_st.pl.spy = 3;
            g_st.pl.jump = 5;
        }
    }
}

// 落地结算摔落伤害。
// fell 记录滞空期间的**最高点**（y 越小越高），落地时落差超过 playerjump
// 才算摔伤。旧版直接把「当前 y - fell」当落差，跳一下就会掉血。
// 系数从 0.5 降到 0.2：旧版一格伤害太狠，掉两格血就没了。
void fall_damage()
{
    Player &p = g_st.pl;
    f64 drop = p.y - p.fell;
    if(drop > p.playerjump)
        p.life -= (drop - p.playerjump) * 0.2;
    p.fell = p.y;
}

}   // namespace

// ============================================================================
//                        玩家物理（旧版 game_loop 内联大段）
// ============================================================================
namespace {

void step_player(i32 mx, i32 my, bool shift_down)
{
    Player &p = g_st.pl;
    const f64 g = 0.6;

    if(world::gamemode == 0)
    {
        // 创造飞行（旧版 else 分支）
        p.spy = -my * 8;
        p.x += p.spx * 0.16;
        p.y += p.spy * 0.16;
        p.fell = p.y;   // 飞行不累计摔落高度
        return;
    }

    i32 pfb = touch_at(p.x, p.y);
    (void)pfb;
    // 先看脚下那一格是不是液体
    i32 px = (i32)(p.x + 0.5), py = (i32)(p.y + 0.5);
    i16 here = (px >= 0 && px < world::W && py >= 0 && py < world::H)
             ? ((world::dim == world::DIM_NETHER) ? world::s2[px][py] : world::s[px][py])
             : 0;

    if(is_liquid(here))
    {
        // 旧版液体物理：岩浆扣血，水消耗氧气
        if(is_lava(here))
        {
            if(p.wudi == 0) { p.life -= 4 * p.jianshang; p.wudi = 30; }
        }
        else
        {
            // 水：氧气按 tick 递减，耗尽后开始掉血。
            // 旧版是每 tick -1，900 点约 15 秒见底、再过 0.3 秒就开始扣血，
            // 潜个水就濒死。这里放慢到每 8 tick -1（约 2 分钟见底），
            // 扣血也放慢到每 1.5 秒 1 点。
            if(++p.drown_ticks >= 8)
            {
                p.drown_ticks = 0;
                p.oxygen--;
            }
            if(p.oxygen <= -20 && p.wudi == 0)
            {
                p.life -= 1;
                p.wudi = 90;
            }
        }
        p.fell = p.y;
        p.jump = 5;
        if(my == 1)
        {
            i32 ux = (i32)p.x, uy = (i32)(p.y - 0.5);
            i16 up = (ux >= 0 && ux < world::W && uy >= 0 && uy < world::H)
                   ? ((world::dim == world::DIM_NETHER) ? world::s2[ux][uy] : world::s[ux][uy])
                   : 0;
            if(is_liquid(up))
            {
                p.spy += -2.5;
                if(p.spy > -8) p.spy = -8;
            }
            else { p.spy = p.spy * 1.3; p.spx *= 0.85; }
        }
        if(shift_down) p.spy += 2;
        p.spy += g;
        p.spy *= 0.75;
        p.y += p.spy * 0.02; touch_y();
        p.y += p.spy * 0.02; touch_y();
        p.spx *= 0.92;
        p.x += p.spx * 0.06; touch_x();
        p.x += p.spx * 0.06; touch_x();
        return;
    }

    // 干燥地面
    // 氧气缓慢回复（旧版 +5/tick，几乎瞬间回满）
    p.drown_ticks = 0;
    p.oxygen += 1;
    if(p.oxygen > 900) p.oxygen = 900;
    if(my == 1 && p.jump == 0)      { p.jump = 1; p.spy = -p.playerjump; }
    else if(my == 0 && p.jump == 1)   p.jump = 2;
    else if(my == 1 && p.jump == 2) { p.jump = 3; p.spy = -p.playerjump; }
    p.spy += g;
    p.spy *= 0.96;
    p.y += p.spy * 0.04; touch_y();
    p.y += p.spy * 0.04; touch_y();
    if(shift_down)
    {
        // 潜行：贴着边缘不走出去
        p.x += p.spx * 0.04;
        i32 cx = (i32)(p.x + 0.5), cy = (i32)(p.y + 1);
        i32 t = touch_of(cx, cy);
        if(t == 0 && std::abs((i32)(p.spy + 0.5)) <= 1) p.x -= p.spx * 0.04;
        touch_x();
    }
    else
    {
        p.x += p.spx * 0.04; touch_x();
        p.x += p.spx * 0.04; touch_x();
    }
}

}   // namespace

// ============================================================================
//                              一个逻辑 tick
// ============================================================================
void tick_once()
{
    Player &p = g_st.pl;
    g_tick++;
    world::tick++;      // 实体的「每 5 tick 一次视线检测」靠它分频（之前忘了 ++）

    // ---- 魔光护符（旧版 hujia_update 1547 / setlight 1432）----
    // 每 tick 重算：拾取/丢下后立刻生效（跳跃上限 12 -> 14，随身发光 12 级）
    {
        bool amu = item::inv_find_existing(B_LIGHTS_AMULET) != 0;
        p.playerjump = amu ? 14 : 12;
        world::g_amulet = amu;
    }

    // ---- 时间 ----
    if(world::sun != 0)
    {
        world::now_time++;
        if(world::now_time == 18000) { world::night = world::NIGHT_DUSK; }
        if(world::now_time == 20000) { world::night = world::NIGHT_NIGHT; }
        if(world::now_time == 38000) { world::night = world::NIGHT_DAWN; }
        if(world::now_time >= 40000)
        {
            world::night = world::NIGHT_NOON;
            world::now_time = 0;
            world::set_light();
        }
    }

    // ---- 无敌帧 / 回血 ----
    if(p.wudi > 0) p.wudi--;
    if(p.sword > 0) p.sword--;
    if(p.playerCD > 0) p.playerCD--;

    // 每 60 逻辑帧刷一次光照（旧版 slow_update）
    g_light_tick++;
    if(g_light_tick >= 60) { g_light_tick = 0; light_update(); }

    // ---- 输入读取 ----
    using namespace platform;
    bool left = key_down(K_A) || key_down(K_LEFT);
    bool right = key_down(K_D) || key_down(K_RIGHT);
    bool up = key_down(K_W) || key_down(K_UP) || key_down(K_SPACE);
    bool down = key_down(K_S) || key_down(K_DOWN);
    bool shift = key_down(K_SHIFT);
    bool ctrl = key_down(K_CTRL);
    // --trace 诊断用：注入合成输入（等价于一直按住 D，偶尔跳）
    if(g_force_right)
    {
        right = true;
        if(g_force_jump) up = true;
    }

    i32 mx = 0, my = 0;
    if(g_st.ui == UI_NONE)
    {
        if(left)  { if(p.spx > -1) p.spx = -p.playerspeed; p.spx -= p.playerspeed / 4.0; mx--; }
        if(right) { if(p.spx < 1)  p.spx =  p.playerspeed; p.spx += p.playerspeed / 4.0; mx++; }
        if(up)    my++;
        if(down)  my--;
    }
    p.spx *= 0.97;
    if(mx == 0)
    {
        if(p.sword < 40 && p.wudi < 20) p.spx = 0;
        else p.spx *= 0.97;
    }

    // ---- 快捷栏 ----
    if(g_st.ui == UI_NONE)
    {
        for(i32 k = 0; k < 9; k++)
            if(key_pressed(K_1 + k)) select_hotbar(k + 1);

        // ---- 鼠标指向 ----
        f32 tpx = (f32)g_st.tile_px;
        f32 cxp = (f32)platform::width() * 0.5f;
        f32 cyp = (f32)platform::height() * 0.5f;
        p.mpx = (i32)(p.jx + (mouse_x() - cxp) / tpx + 0.5);
        p.mpy = (i32)(p.jy + (mouse_y() - cyp) / tpx + 0.5);

        // ---- 左键：挖掘 / 攻击 / 用弓 ----
        if(mouse_down(MB_LEFT))
        {
            i32 sel = p.cho;
            i32 held = item::beibao[sel].id;
            f64 dist = std::sqrt((f64)(p.mpx - (i32)(p.x + 0.5)) * (p.mpx - (i32)(p.x + 0.5)) +
                                 (f64)(p.mpy - (i32)(p.y + 0.5)) * (p.mpy - (i32)(p.y + 0.5)));
            if(held == B_BOW || held == B_STORM_BOW || held == B_DEMON_BOW)
                use_selected(false);
            else if(dist <= 4.5)
            {
                // 换目标格子就重新累计挖掘进度，避免进度带着走导致「一下一个方块」
                if(p.mpx != g_mine_x || p.mpy != g_mine_y)
                {
                    g_wj = 0;
                    g_mine_x = p.mpx;
                    g_mine_y = p.mpy;
                }
                mine_at(p.mpx, p.mpy);
            }
            else attack_at(p.mpx, p.mpy);
        }
        else
        {
            // 松手就清空挖掘进度（旧版 mine_reset 从来没被调用过）
            g_wj = 0;
            g_mine_x = g_mine_y = -1;
        }
        // ---- 右键：放置 / 使用 ----
        if(mouse_pressed(MB_RIGHT)) use_selected(true);
    }

    // ---- 物理 ----
    step_player(mx, my, shift);

    // ---- 边界 ----
    if(p.x < 0) p.x = 0;
    if(p.y < 0) p.y = 0;
    if(p.x > world::W - 1) p.x = world::W - 1;
    if(p.y > world::H - 1) go_home();

    // ---- 相机缓动 ----
    // 旧版写的是 (p.x - p.jx) / 10，但那是整型除法（jx 是 int）：
    // 差值 1~9 时结果恒为 0，所以「缓动」实际只在差值 ≥10 时跳一格，
    // 表现就是相机一顿一顿的。这里改成浮点指数平滑，并按帧时间归一化，
    // 保证不同刷新率下手感一致。
    {
        f32 k = 1.0f - std::pow(0.0015f, (f32)platform::dt());
        if(k < 0) k = 0;
        if(k > 1) k = 1;
        if(g_st.special_effect == EASE_OFF)
        {
            p.jx = p.x;
            p.jy = p.y;
        }
        else if(g_st.special_effect == EASE_ON)
        {
            // 跟随玩家，水平略快、垂直略慢（跳跃时画面不跟着上下猛甩）
            f32 kx = k * 1.35f;
            if(kx > 1) kx = 1;
            p.jx += (p.x - p.jx) * kx;
            p.jy += (p.y - p.jy) * k * 0.75f;
            // 死区：像素级以下就别动了，避免静止时还在抖
            if(std::abs(p.x - p.jx) < 0.02) p.jx = p.x;
            if(std::abs(p.y - p.jy) < 0.02) p.jy = p.y;
        }
        else   // EASE_STRONG：跟随鼠标，幅度收窄，手不抖
        {
            f64 tx = p.x + (p.mpx - p.x) * 0.28;
            f64 ty = p.y + (p.mpy - p.y) * 0.28;
            f32 kk = k * 0.55f;
            p.jx += (tx - p.jx) * kk;
            p.jy += (ty - p.jy) * kk;
            if(std::abs(tx - p.jx) < 0.02) p.jx = tx;
            if(std::abs(ty - p.jy) < 0.02) p.jy = ty;
        }
    }

    // 相机不要越过世界边界
    f32 halfW = (f32)platform::width() * 0.5f / (f32)g_st.tile_px;
    f32 halfH = (f32)platform::height() * 0.5f / (f32)g_st.tile_px;
    if(p.jx < halfW) p.jx = halfW;
    if(p.jy < halfH) p.jy = halfH;
    if(p.jx > world::W - halfW) p.jx = world::W - halfW;
    if(p.jy > world::H - halfH) p.jy = world::H - halfH;

    // ---- 实体 ----
    entity::tick_all();
    if(g_tick % 20 == 0) entity::try_spawn();
    if(g_tick % 10 == 0)
    {
        entity::explode_tick();
        world::block_update();
    }

    // ---- 摔落追踪 ----
    // fell 记滞空期间的**最高点**（y 越小越高），落地时由 fall_damage()
    // 用「落点 - 最高点」结算摔伤。旧版直接拿当前 y 减 fell，
    // 结果跳一下就掉血。
    if(p.y < p.fell) p.fell = p.y;

    // ---- 死亡 / 回血 ----
    if(p.life <= 0) start_respawn();
    // 回血慢一点：旧版每 300 tick（约 5 秒）回 1 点，一次受伤几秒就补满。
    // 这里放到约 35 秒 1 点，受伤才有压力（吃东西仍是主要恢复手段）。
    if(p.life < 20)
    {
        p.H++;
        if(p.H > 720) { p.life += 1; p.H = 0; }
    }
    else p.H = 0;
    if(p.life > 20) p.life = 20;

    // ---- 移动换位置时刷新光照 ----
    i32 cx = (i32)(p.x + 0.5), cy = (i32)(p.y + 0.5);
    if(cx != p.lx || cy != p.ly)
    {
        p.lx = cx;
        p.ly = cy;
        light_update();
    }

    // ---- 自动保存 ----
    if(g_st.savetype != 4)
    {
        g_st.savetick++;
        i32 up = (g_st.savetype == 1) ? 10800 : (g_st.savetype == 2 ? 18000 : 36000);
        if(g_st.savetick >= up)
        {
            g_st.savetick = 0;
            save_world(false);
        }
    }
}

void select_hotbar(i32 n)
{
    if(n < 1 || n > 9) return;
    g_st.pl.cho = n;
    mine_reset();          // 换手持工具，挖掘进度重来
    use_selected(false);
}

void on_wheel(i32 delta)
{
    i32 c = g_st.pl.cho + (delta > 0 ? -1 : 1);
    if(c < 1) c = 9;
    if(c > 9) c = 1;
    g_st.pl.cho = c;
    mine_reset();
}

// ============================================================================
//                          挖掘（旧版 wajue）
// ============================================================================
// g_wj / g_mine_x / g_mine_y / MINE_SPEED 定义在文件顶部（tick_once 也要用）

void mine_at(i32 sx, i32 sy)
{
    Player &p = g_st.pl;
    if(sx < 0 || sx >= world::W || sy < 0 || sy >= world::H) return;
    i16 blk = (world::dim == world::DIM_NETHER) ? world::s2[sx][sy] : world::s[sx][sy];
    if(blk == B_AIR || blk == B_SKY) return;   // 空气/天空不可挖（也不该消耗耐久）
    i32 dig = block_of(blk).dig;
    if(p.issword) return;
    if(dig <= 0 && dig != 0) return;      // -1 = 不可破坏
    if(block_of(blk).touch == 0 && dig == 0 && blk != B_AIR) { /* 瞬时方块 */ }
    if(dig < 0) return;

    i32 sel = p.cho;
    i32 tool = item::beibao[sel].id;
    const bool wood = (blk == B_LOG || blk == B_CHEST || blk == B_PLATFORM ||
                       blk == B_PLANKS);
    f64 add;
    if(wood)
    {
        if(tool == B_NETHERITE_AXE) add = 18;
        else if(tool == B_GOLD_AXE) add = 17;
        else if(tool == B_DIAMOND_AXE) add = 15;
        else if(tool == B_IRON_AXE) add = 12;
        else if(tool == B_STONE_AXE) add = 9;
        else add = 3;
    }
    else
    {
        if(tool == B_NETHERITE_PICK) add = 18;
        else if(tool == B_GOLD_PICK) add = 17;
        else if(tool == B_DIAMOND_PICK) add = 15;
        else if(tool == B_IRON_PICK) add = 11;
        else if(tool == B_STONE_PICK) add = 8;
        else if(tool == B_WOOD_PICK) add = 5;
        else add = 3;
    }
    g_wj += add * MINE_SPEED;
    if(g_wj <= dig) return;               // 还没挖穿

    // 工具等级
    i32 lv = 1;
    if(tool == B_WOOD_PICK) lv = 2;
    if(tool == B_STONE_PICK) lv = 3;
    if(tool == B_GOLD_PICK) lv = 4;
    if(tool == B_IRON_PICK) lv = 4;
    if(tool == B_DIAMOND_PICK) lv = 5;
    if(tool == B_NETHERITE_PICK) lv = 6;

    if(lv < block_of(blk).level)
    {
        // 等级不足：挖得动但可能不掉落（旧版同样行为）
        if(g_wj > dig * 6)
        {
            if(blk == B_LEAF)
            {
                if(world::rnd(10) == 0) item::inv_add(B_APPLE, 1);
                if(world::rnd(10) == 0) item::inv_add(B_STICK, 1);
            }
            p_set_block(sx, sy, B_AIR);
            g_wj = 0;
            if(g_st.durability_on) wear_tool(1);
        }
        return;
    }

    // 掉落
    if(blk == B_NETHER_GOLD)
    {
        for(i32 i = 1; i <= world::rnd(5) + 2; i++) item::inv_add(B_GOLD_NUGGET, 1);
    }
    else if(blk != B_AIR)
    {
        // 基岩不掉落
        if(blk != B_BEDROCK) item::inv_add(blk, 1);
    }
    // 箱子内容物倒出来
    if(blk == B_CHEST)
    {
        i32 del = item::find_box(sx, sy);
        if(del != -1)
        {
            item::Box &b = item::box_at(del);
            for(i32 i = 0; i < item::BOX_USABLE; i++)
            {
                i32 id = b.things[i] % 1000;
                i32 n = b.things[i] / 1000;
                if(id <= 0 || id >= BLOCK_COUNT) continue;
                for(i32 j = 0; j < n; j++) item::inv_add(id, 1);
                b.things[i] = 0;
            }
            b.bx = item::BOX_FREE_X;
            b.by = 0;
        }
    }
    p_set_block(sx, sy, B_AIR);
    g_wj = 0;
    if(g_st.durability_on) wear_tool(1);
    light_update();
}

void p_set_block(i32 x, i32 y, i32 id)
{
    if(x < 0 || x >= world::W || y < 0 || y >= world::H) return;
    if(world::dim == world::DIM_NETHER) world::s2[x][y] = (i16)id;
    else world::s[x][y] = (i16)id;
    world::mark_dirty_col(x);
}

void p_get_block(i32 x, i32 y, i32 &out)
{
    if(x < 0 || x >= world::W || y < 0 || y >= world::H) { out = B_AIR; return; }
    out = (world::dim == world::DIM_NETHER) ? world::s2[x][y] : world::s[x][y];
}

// ★ 耐久度：工具损耗。返回 true 表示工具碎了
bool wear_tool(i32 dmg)
{
    Player &p = g_st.pl;
    if(!g_st.durability_on) return false;
    i32 sel = p.cho;
    item::Stack &s = item::beibao[sel];
    if(s.max_dur() <= 0) return false;
    i32 id = s.id;
    if(item::dmg_item(sel, dmg))
    {
        toast("你的%s已损坏", block_of(id).cn);
        g_st.hud_dirty = 1;
        return true;
    }
    return false;
}

f64 mine_progress() { return g_wj; }
void mine_reset() { g_wj = 0; g_mine_x = g_mine_y = -1; }

// ============================================================================
//                          放置（旧版 block_）
// ============================================================================
void place_at(i32 xxx, i32 yyy)
{
    Player &p = g_st.pl;
    i32 sel = p.cho;
    item::Stack &s = item::beibao[sel];
    if(s.count <= 0) return;
    if(xxx < 1 || xxx >= world::W - 1 || yyy < 1 || yyy >= world::H - 1) return;
    if(xxx == (i32)(p.x + 0.5) && yyy == (i32)(p.y + 0.5)) return;

    i32 held = s.id;
    if(held == B_OI_CORE || held == B_WORM_FOOD)
    {
        if(held == B_WORM_FOOD) entity::spawn_eow(p.x, p.y);
        else entity::spawn(entity::T_OI, 500, 8, p.mpx, p.mpy);
        s.count--;
        if(s.count <= 0) s.reset();
        return;
    }
    if(held == B_OI_SWORD)
    {
        if(p.sword == 0)
        {
            p.sword = 120;
            f64 ttx = std::abs(p.x - p.mpx), tty = std::abs(p.y - p.mpy);
            f64 total = ttx + tty + 0.001;
            f64 px = ttx / total, py = tty / total;
            if(p.x - p.mpx < 0) px *= -1;
            if(p.y - p.mpy < 0) py *= -1;
            p.wudi = 20;
            p.spx = 30 * -px;
            p.spy = 20 * -py;
            if(g_st.durability_on) wear_tool(1);
        }
        return;
    }
    if(!block_of(held).can_place) return;

    i32 c;
    p_get_block(xxx + 1, yyy, c); bool r1 = is_passable(c);
    p_get_block(xxx - 1, yyy, c); bool r2 = is_passable(c);
    p_get_block(xxx, yyy + 1, c); bool r3 = is_passable(c);
    p_get_block(xxx, yyy - 1, c); bool r4 = is_passable(c);
    if(r1 && r2 && r3 && r4) return;      // 悬空放置

    i32 cur;
    p_get_block(xxx, yyy, cur);
    bool can = is_passable(cur);
    if(!can) return;

    if(held == B_WATER_BUCKET)
    {
        p_set_block(xxx, yyy, B_WATER);
        s.id = B_BUCKET;
        light_update();
        return;
    }
    if(held == B_LAVA_BUCKET)
    {
        p_set_block(xxx, yyy, B_LAVA);
        s.id = B_BUCKET;
        light_update();
        return;
    }
    p_set_block(xxx, yyy, held);
    s.count--;
    if(s.count <= 0) s.reset();
    if(held == B_CHEST)
    {
        bool reused = false;
        for(i32 i = 0; i < item::box_count() && !reused; i++)
            if(item::box_at(i).bx == item::BOX_FREE_X)
            {
                item::Box &b = item::box_at(i);
                b.bx = (u16)xxx;
                b.by = (u16)yyy;
                memset(b.things, 0, sizeof(b.things));
                reused = true;
            }
        if(!reused) item::alloc_box(xxx, yyy);
    }
    light_update();
}

// ============================================================================
//                       攻击（旧版玩家点击攻击）
// ============================================================================
void attack_at(i32 sx, i32 sy)
{
    Player &p = g_st.pl;
    if(p.playerCD > 0) return;
    // 找点击位置附近的实体
    i32 list[64];
    i32 n = 0;
    entity::query_nearby(sx, sy, 2.0, list, 64, n);
    for(i32 k = 0; k < n; k++)
    {
        i32 i = list[k];
        entity::Entity &e = entity::ents[i];
        if(e.type <= 0) continue;
        if(std::abs(e.x - p.x) >= 4.5 || std::abs(e.y - p.y) >= 4.5) continue;
        if(std::abs(e.x - sx) >= 2.0 || std::abs(e.y - sy) >= 2.0) continue;
        f64 dmg = p.gongji * (p.spy > 2 ? 1.5 : 1.0);
        entity::damage_ent(i, dmg);
        if(e.type != entity::T_OI && e.type > 0)
        {
            e.vy = p.spy > 2 ? -8 : -6;
            p.gj = p.spy > 2 ? 2 : 1;
        }
        // 剑类横扫
        if(p.issword)
        {
            for(i32 LL = 0; LL < entity::MAX_ENT; LL++)
            {
                if(entity::ents[LL].type <= 0 || LL == i) continue;
                if(std::abs(entity::ents[LL].x - sx) <= 0.75 &&
                        std::abs(entity::ents[LL].y - sy) <= 1.5)
                    entity::damage_ent(LL, 1);
            }
        }
        if(g_st.durability_on) wear_tool(1);
        break;
    }
    p.playerCD = p.gongjicd;
}

// ============================================================================
//                        使用物品（旧版右键）
// ============================================================================
void use_selected(bool right_click)
{
    Player &p = g_st.pl;
    i32 sel = p.cho;
    item::Stack &s = item::beibao[sel];
    i32 id = s.id;

    // 旧版 checkuseitem()：刷新攻击力与冷却
    p.gongji = 1;
    p.gongjicd = 20;
    switch(id)
    {
    case B_STONE_SWORD:   p.gongji = 5; p.gongjicd = 37; break;
    case B_STONE_AXE:     p.gongji = 9; p.gongjicd = 75; break;
    case B_IRON_SWORD:    p.gongji = 6; p.gongjicd = 37; break;
    case B_IRON_AXE:      p.gongji = 9; p.gongjicd = 67; break;
    case B_DIAMOND_SWORD: p.gongji = 7; p.gongjicd = 37; break;
    case B_DIAMOND_AXE:   p.gongji = 9; p.gongjicd = 60; break;
    case B_OI_SWORD:      p.gongji = 7; p.gongjicd = 30; break;
    case B_NETHERITE_SWORD: p.gongji = 9; p.gongjicd = 37; break;
    case B_NETHERITE_AXE:   p.gongji = 11; p.gongjicd = 60; break;
    case B_GOLD_SWORD:      p.gongji = 4; p.gongjicd = 37; break;
    case B_GOLD_AXE:        p.gongji = 7; p.gongjicd = 60; break;
    case B_LIGHTS_BANE:     p.gongji = 8; p.gongjicd = 30; break;
    default: break;
    }
    p.issword = (p.gongjicd < 40 && p.gongjicd > 25) ? 1 : 0;

    if(!right_click) return;

    // 指向容器方块 → 打开（空手也能开；MC 是右键工作台开 3x3 合成）
    {
        f64 dist = std::sqrt((f64)(p.mpx - (i32)(p.x + 0.5)) * (p.mpx - (i32)(p.x + 0.5)) +
                             (f64)(p.mpy - (i32)(p.y + 0.5)) * (p.mpy - (i32)(p.y + 0.5)));
        if(dist <= 4.5)
        {
            i32 cur;
            p_get_block(p.mpx, p.mpy, cur);
            if(cur == B_CRAFTING_TABLE) { ui_open(UI_CRAFT); return; }
        }
    }
    if(s.count <= 0) return;

    // 回血药水
    if(id == B_HEALTH_POTION)
    {
        p.life += (p.life + 5 > 20) ? (20 - p.life) : 5;
        s.count--;
        if(s.count <= 0) s.reset();
        return;
    }
    // 食物
    if(id == B_ROTTEN_FLESH || id == B_APPLE || id == B_MUTTON || id == B_PORK ||
            id == B_GOLDEN_APPLE)
    {
        p.eat++;
        if(p.eat > 75)
        {
            p.eat = 0;
            if(id == B_ROTTEN_FLESH) p.life += 2;
            else if(id == B_APPLE)   p.life += 2;
            else if(id == B_MUTTON)  p.life += 3;
            else if(id == B_PORK)    p.life += 4;
            else                     p.life += 8;
            if(p.life > 20) p.life = 20;
            s.count--;
            if(s.count <= 0) s.reset();
        }
        return;
    }
    // 弓：拉弓蓄力在 tick 里处理，这里只做一次性发射（简化按键语义）
    if(id == B_BOW || id == B_STORM_BOW || id == B_DEMON_BOW) return;

    // 桶：装水/岩浆
    if(id == B_BUCKET)
    {
        i32 cur;
        p_get_block(p.mpx, p.mpy, cur);
        if(cur == B_WATER) { p_set_block(p.mpx, p.mpy, B_AIR); s.id = B_WATER_BUCKET; light_update(); }
        else if(cur == B_LAVA) { p_set_block(p.mpx, p.mpy, B_LAVA); s.id = B_LAVA_BUCKET; light_update(); }
        return;
    }

    // 其他物品：右键放置方块 / 使用手上物品（工作台等 can_place 方块、OI 剑冲刺、召唤 OI/蠕虫）
    place_at(p.mpx, p.mpy);
}

// ============================================================================
//                        重生 / 回家 / 刷新
// ============================================================================
void start_respawn()
{
    Player &p = g_st.pl;
    p.life = 20;
    p.oxygen = 900;
    g_st.l_life = 30;
    if(world::dim == world::DIM_NETHER) world::swap_dim(false);
    // 床还在就睡床边，否则找一块干燥地表
    if(world::get(world::back_x, world::back_y) == B_BED)
    {
        p.y = world::back_y - 1;
        p.x = world::back_x;
    }
    else
    {
        find_dry_spawn(world::W / 2, p.x, p.y);
    }
    p.spx = 0;
    p.spy = 0;
    p.jump = 0;
    p.fell = p.y;
    p.jx = p.x;
    p.jy = p.y;
    p.lx = (i32)(p.x + 0.5);
    p.ly = (i32)(p.y + 0.5);
    light_update();
    toast("你死了，已重生");
}

// 从 from_x 向两边找一块「头顶两格是空气、脚下是固体、不是液体」的干燥地表。
// 找不到时沿用旧版兜底（往下找第一块固体）。
//
// 旧版 MAP()/Start() 只做「往下找第一块能站的地面」，世界中心或重生点
// 正好是湖/海/岩浆时玩家就直接出生在水里，所以单独抽出来。
bool find_dry_spawn(i32 from_x, f64 &out_x, f64 &out_y)
{
    if(from_x < 2) from_x = 2;
    if(from_x > world::W - 3) from_x = world::W - 3;
    for(i32 off = 0; off <= 400; off++)
        for(i32 d = 0; d < 2; d++)
        {
            i32 cx = from_x + (d == 0 ? -off : off);
            if(cx < 2 || cx >= world::W - 2) continue;
            for(i32 j = 1; j < world::H - 1; j++)
            {
                i16 b = world::s[cx][j];
                if(block_of(b).touch < 1) continue;   // 不是能站的地面
                if(is_liquid(b)) break;               // 液面，换一列
                i16 a1 = world::s[cx][j - 1];
                i16 a2 = (j >= 2) ? world::s[cx][j - 2] : B_AIR;
                if(is_liquid(a1) || is_liquid(a2)) break;
                if(block_of(a1).touch >= 1 || block_of(a2).touch >= 1) break;
                if(j >= world::H - 4) break;          // 别贴着基岩层
                out_x = cx;
                out_y = j - 2;
                if(out_y < 2) out_y = 2;
                return true;
            }
        }
    out_x = from_x;
    out_y = world::H - 2;
    for(i32 j = 1; j < world::H; j++)
    {
        i16 b = world::s[from_x][j];
        if(block_of(b).touch >= 1) { out_y = j - 2; break; }
    }
    if(out_y < 2) out_y = 2;
    return false;
}

void go_home()
{
    Player &p = g_st.pl;
    if(world::get(world::back_x, world::back_y) == B_BED)
    {
        p.x = world::back_x;
        p.y = world::back_y - 1;
        if(world::dim == world::DIM_NETHER) world::swap_dim(false);
        refresh_hud();
    }
    else
    {
        toast("无法传送：重生点床已被挖走");
    }
}

void refresh_hud()
{
    g_st.l_life = 30;
    g_st.hud_dirty = 1;
    light_update();
}

void clear_view()
{
    i32 cx = (i32)(g_st.pl.x + 0.5);
    world::clear_around(cx);
}

// ============================================================================
//                        计分 / 初始化
// ============================================================================
void init()
{
    g_st = State();
    item::init();
    entity::clear_all();
    entity::explode_reset();
    world::reset_block_update();
    world::dim = world::DIM_OVERWORLD;
    world::now_time = 0;
    world::night = world::NIGHT_NOON;
    world::sun = 1;
    world::gamemode = 1;
    world::ed = g_st.difficulty;
    world::godeater = 0;
    world::oi = 0;
    g_wj = 0;
    g_tick = 0;
    armor_update();
}

}   // namespace game








