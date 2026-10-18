// ===================== 游戏渲染实现 =====================
// 世界：天空渐变 + 星空 + 日月 -> 地形（四角顶点色做平滑光照）-> 流体
//       -> 裂纹 -> 实体 -> 粒子 -> 玩家
// HUD ：血条 / 护甲 / 氧气 / 快捷栏（含★耐久条）/ Boss 血条 / 提示
#include "game_render.h"
#include "../entity/entity.h"
#include "../platform/platform.h"
#include "../world/blocks.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

namespace game {

namespace {

// ---------------- 昼夜环境色 ----------------
struct Ambient { f32 r, g, b; };

Ambient ambient_of(i32 now_time, i32 night)
{
    f32 r, g, b;
    if(night == world::NIGHT_NOON)      { r = 1.00f; g = 0.99f; b = 0.95f; }
    else if(night == world::NIGHT_DUSK) { r = 1.00f; g = 0.72f; b = 0.52f; }
    else if(night == world::NIGHT_NIGHT){ r = 0.42f; g = 0.50f; b = 0.78f; }
    else                                { r = 0.78f; g = 0.72f; b = 0.86f; }
    if(night == world::NIGHT_DUSK)
    {
        f32 t = (now_time - 18000) / 2000.0f;
        if(t < 0) t = 0;
        if(t > 1) t = 1;
        r = 1.00f + (0.62f - 1.00f) * t;
        g = 0.99f + (0.52f - 0.99f) * t;
        b = 0.95f + (0.72f - 0.95f) * t;
    }
    else if(night == world::NIGHT_DAWN)
    {
        f32 t = (now_time - 38000) / 2000.0f;
        if(t < 0) t = 0;
        if(t > 1) t = 1;
        r = 0.42f + (1.00f - 0.42f) * t;
        g = 0.50f + (0.99f - 0.50f) * t;
        b = 0.78f + (0.95f - 0.78f) * t;
    }
    return {r, g, b};
}

// 可见度：环境光下限（按是否封闭区分）与光照项取 max，避免两个暗因子相乘
inline f32 light_visibility(i16 lightv, i32 y, bool nether)
{
    f32 d = (y - 120) / 780.0f;
    if(d < 0) d = 0;
    if(d > 1) d = 1;
    f32 ambFloor;
    if(lightv <= 0) ambFloor = 0.46f - 0.22f * d;   // 洞里 0.46 -> 深处 0.24
    else            ambFloor = 0.60f - 0.14f * d;   // 露天 0.60 -> 0.46
    if(nether) ambFloor += 0.05f;

    f32 t = lightv;
    if(t < 0) t = 0;
    if(t > 19) t = 19;
    f32 n = t / 19.0f;
    f32 litTerm = 0.15f + 1.09f * n * n;
    return std::max(ambFloor, litTerm);
}

inline Color lit(i16 lightv, i32 y, bool nether, Ambient a, f32 extra = 1.0f)
{
    f32 kk = light_visibility(lightv, y, nether) * extra;
    if(kk > 1.30f) kk = 1.30f;
    u8 r = (u8)std::min(255.0f, a.r * kk * 255.0f);
    u8 g = (u8)std::min(255.0f, a.g * kk * 255.0f);
    u8 b = (u8)std::min(255.0f, a.b * kk * 255.0f);
    return rgba(r, g, b);
}

// ---------------- 天空 ----------------
void draw_sky(i32 now_time, i32 night, i32 dim)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    Color top, bottom;
    if(dim == world::DIM_NETHER)
    {
        top = rgba(48, 14, 18);
        bottom = rgba(104, 32, 24);
    }
    else if(night == world::NIGHT_NOON)
    {
        top = rgba(96, 146, 214);
        bottom = rgba(168, 206, 238);
    }
    else if(night == world::NIGHT_DUSK)
    {
        f32 t = (now_time - 18000) / 2000.0f;
        f32 u = t < 0 ? 0 : (t > 1 ? 1 : t);
        top = rgba((u8)(96 + (48 - 96) * u), (u8)(146 + (52 - 146) * u),
                   (u8)(214 + (96 - 214) * u));
        bottom = rgba((u8)(168 + (198 - 168) * u), (u8)(206 + (108 - 206) * u),
                      (u8)(238 + (86 - 238) * u));
    }
    else if(night == world::NIGHT_NIGHT)
    {
        top = rgba(10, 14, 40);
        bottom = rgba(28, 36, 72);
    }
    else
    {
        f32 t = (now_time - 38000) / 2000.0f;
        f32 u = t < 0 ? 0 : (t > 1 ? 1 : t);
        top = rgba((u8)(12 + (96 - 12) * u), (u8)(16 + (146 - 16) * u),
                   (u8)(44 + (214 - 44) * u));
        bottom = rgba((u8)(30 + (168 - 30) * u), (u8)(40 + (206 - 40) * u),
                      (u8)(78 + (238 - 78) * u));
    }

    render::begin_ui();
    render::rect_lit(0, 0, w, h, top, top, bottom, bottom);

    if(dim == world::DIM_OVERWORLD &&
            (night == world::NIGHT_NIGHT || night == world::NIGHT_DUSK))
    {
        f32 alpha = (night == world::NIGHT_NIGHT) ? 1.0f
                  : (now_time - 18000) / 2000.0f;
        if(night == world::NIGHT_DUSK)
            alpha = 1.0f - (alpha < 0 ? 0 : (alpha > 1 ? 1 : alpha));
        for(i32 k = 0; k < 260; k++)
        {
            u32 hs = (u32)k * 2654435761u;
            f32 sx = (f32)(hs % 1000) / 1000.0f * w;
            f32 sy = (f32)((hs >> 10) % 400) / 400.0f * h * 0.72f;
            f32 sz = 1.0f + (f32)((hs >> 20) % 3);
            u8 a = (u8)(alpha * (110 + (hs >> 24) % 145));
            render::rect(sx, sy, sz, sz, rgba(255, 255, 240, a));
        }
    }

    if(dim == world::DIM_OVERWORLD)
    {
        f32 phase = now_time / 40000.0f * 6.2831853f;
        f32 ang = phase - 1.5707963f;
        f32 cxp = w * 0.5f + std::cos(ang) * w * 0.40f;
        f32 cyp = h * 0.78f + std::sin(ang) * h * 0.72f;
        bool isSun = (now_time < 19000 || now_time > 39000);
        if(isSun)
        {
            render::circle(cxp, cyp, 46, rgba(255, 240, 190, 60));
            render::circle(cxp, cyp, 30, rgba(255, 248, 214, 220));
            render::circle(cxp, cyp, 26, rgba(255, 255, 240, 255));
        }
        else
        {
            render::circle(cxp, cyp, 34, rgba(200, 214, 255, 50));
            render::circle(cxp, cyp, 22, rgba(226, 232, 250, 230));
            render::circle(cxp - 6, cyp - 5, 18, rgba(248, 250, 255, 255));
        }
    }
}

// ---------------- 地形 ----------------
// 水/岩浆的流动动画：整张面片做一点点 UV 摆动
f32 g_water_phase = 0;

void draw_terrain(Ambient amb)
{
    Player &p = st().pl;
    f32 tilePx = (f32)st().tile_px;
    i32 vw = (i32)(platform::width() / tilePx) + 3;
    i32 vh = (i32)(platform::height() / tilePx) + 3;
    i32 x0 = (i32)p.jx - vw / 2, x1 = x0 + vw;
    i32 y0 = (i32)p.jy - vh / 2, y1 = y0 + vh;
    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    if(x1 >= world::W) x1 = world::W - 1;
    if(y1 >= world::H) y1 = world::H - 1;

    const bool nether = (world::dim == world::DIM_NETHER);
    i32 water_off = (i32)(g_water_phase * 2.0f) % 2;

    for(i32 y = y0; y <= y1; y++)
        for(i32 x = x0; x <= x1; x++)
        {
            i16 id = nether ? world::s2[x][y] : world::s[x][y];
            if(id == B_AIR) continue;   // 空气/天空留给天空层

            // 世界里用「嵌在石头里」的矿脉贴图；背包/合成里仍用物品图标
            i32 slot = atlas::slot_of_world_block(id);
            i16 l00 = world::light_at(x, y);
            i16 l10 = world::light_at(x + 1, y);
            i16 l11 = world::light_at(x + 1, y + 1);
            i16 l01 = world::light_at(x, y + 1);
            // 液体：半透明，并且整体加一点亮度让它有通透感
            bool liquid = is_liquid(id);
            f32 base = liquid ? 1.10f : 1.0f;
            Color c00 = lit(l00, y, nether, amb, 1.06f * base);
            Color c10 = lit(l10, y, nether, amb, 1.06f * base);
            Color c11 = lit(l11, y + 1, nether, amb, 0.92f * base);
            Color c01 = lit(l01, y + 1, nether, amb, 0.92f * base);

            f32 fx = (f32)x, fy = (f32)y;
            if(liquid)
            {
                // 波浪：同一格在两张水贴图之间切换（旧版也是 3 帧流动动画）
                f32 wave = std::sin((x * 0.35f + g_water_phase * 2.2f)) * 0.5f + 0.5f;
                f32 dy = wave * 0.10f;
                render::draw_lit(slot, fx, fy + dy, 1.0f, 1.0f, c00, c10, c11, c01);
                // 液面高光
                if(world::get(x, y - 1) == B_AIR)
                    render::rect_lit(fx, fy + dy, 1.0f, 0.10f,
                                     rgba(255, 255, 255, 70), rgba(255, 255, 255, 70),
                                     rgba(255, 255, 255, 0), rgba(255, 255, 255, 0));
                (void)water_off;
                continue;
            }

            render::draw_lit(slot, fx, fy, 1.0f, 1.0f, c00, c10, c11, c01);

            // 草皮/雪皮：泥土或雪、且上方是空气时叠一条顶边
            if(!nether && (id == B_DIRT || id == B_SNOW) &&
                    world::get(x, y - 1) == B_AIR)
            {
                i16 ul = world::light_at(x, y - 1);
                i16 ur = world::light_at(x + 1, y - 1);
                i32 ov = (id == B_SNOW) ? atlas::O_SNOW_FRINGE : atlas::O_GRASS_FRINGE;
                render::draw_lit(ov, fx, fy, 1.0f, 1.0f,
                                 lit(ul, y - 1, nether, amb, 1.10f),
                                 lit(ur, y - 1, nether, amb, 1.10f),
                                 lit(l10, y, nether, amb, 1.00f),
                                 lit(l00, y, nether, amb, 1.00f));
            }
        }
}

// ---------------- 破坏裂纹 ----------------
i32 g_crack_x = -1, g_crack_y = -1, g_crack_lv = 0;

void draw_crack()
{
    if(g_crack_lv <= 0 || g_crack_x < 0) return;
    i32 lv = g_crack_lv;
    if(lv > 9) lv = 9;
    render::draw(atlas::F_CRACK0 + lv, (f32)g_crack_x, (f32)g_crack_y, 1.0f, 1.0f,
                 rgba(255, 255, 255, 200));
}

// ---------------- 实体 ----------------
i32 atlas_slot_of(const entity::Entity &e)
{
    switch(e.type)
    {
    case entity::T_ZOMBIE:    return atlas::E_ZOMBIE;
    case entity::T_CREEPER:   return atlas::E_CREEPER;
    case entity::T_SKELETON:  return atlas::E_SKELETON;
    case entity::T_SPIDER:    return atlas::E_SPIDER;
    case entity::T_PIGLIN:    return atlas::E_PIGLIN;
    case entity::T_SHEEP:     return atlas::E_SHEEP;
    case entity::T_PIG:       return atlas::E_PIG;
    case entity::T_OI:        return e.islife2 ? atlas::E_OI_SHIELDED : atlas::E_OI;
    case entity::T_RECURSIVE: return (e.nbt >= 3) ? atlas::E_RECURSIVE_BIG
                                                  : atlas::E_RECURSIVE_SMALL;
    case entity::T_WORM_HEAD: return atlas::E_WORM_HEAD;
    case entity::T_WORM_BODY: return atlas::E_WORM_BODY;
    case entity::T_WORM_TAIL: return atlas::E_WORM_TAIL;
    case entity::T_ARROW:
        if(e.nbt == 2) return atlas::E_ARROW_WOOD;
        if(e.nbt == 3) return atlas::E_ARROW_EXPLOSIVE;
        return atlas::E_ARROW_IRON;
    default: return atlas::E_SHADOW;
    }
}

// 地面怪的坐标点是**脚点**（物理上被推到方块上沿），贴图要按底边对齐；
// 箭矢 / 凋灵 / 递归 / 吞噬者的点是**身体中心**（它们悬空飞行），保持居中。
bool ent_on_ground(i32 t)
{
    switch(t)
    {
    case entity::T_ZOMBIE:
    case entity::T_CREEPER:
    case entity::T_SKELETON:
    case entity::T_SPIDER:
    case entity::T_PIGLIN:
    case entity::T_SHEEP:
    case entity::T_PIG:
        return true;
    default:
        return false;
    }
}

f32 ent_size_of(const entity::Entity &e)
{
    switch(e.type)
    {
    case entity::T_OI:        return 4.2f;
    case entity::T_RECURSIVE: return (e.nbt >= 3) ? 3.0f : 1.8f;
    case entity::T_WORM_HEAD: return 2.6f;
    case entity::T_WORM_BODY: return 2.4f;
    case entity::T_WORM_TAIL: return 2.2f;
    case entity::T_SPIDER:    return 2.0f;
    case entity::T_CREEPER:   return 1.7f;
    case entity::T_ARROW:     return 1.0f;
    default:                  return 1.8f;
    }
}

// MC 皮肤立绘怪（僵尸/骷髅/苦力怕/蜘蛛/猪灵）：16x32 立绘存在槽位**左半边**，
// 画法同 draw_player_only（宽高比 1:2、底边对齐脚点），其余怪仍整格画
bool ent_portrait(i32 t)
{
    switch(t)
    {
    case entity::T_ZOMBIE:
    case entity::T_CREEPER:
    case entity::T_SKELETON:
    case entity::T_SPIDER:
    case entity::T_PIGLIN:
        return true;
    default:
        return false;
    }
}

void draw_entities()
{
    f32 px = (f32)st().pl.x, py = (f32)st().pl.y;
    for(i32 i = 0; i < entity::MAX_ENT; i++)
    {
        entity::Entity &e = entity::ents[i];
        if(e.type <= 0 || e.hp <= 0) continue;
        // 屏外剔除
        if(std::abs(e.x - px) > 60 || std::abs(e.y - py) > 45) continue;

        f32 s = ent_size_of(e);
        i32 slot = atlas_slot_of(e);
        Color tint = rgba(255, 255, 255, 255);
        bool nether = (world::dim == world::DIM_NETHER);
        Ambient amb = ambient_of(world::now_time, world::night);
        i16 lt = world::light_at((i32)e.x, (i32)e.y);
        f32 k = light_visibility(lt, (i32)e.y, nether);
        tint = tint_of(tint, 0.45f + 0.75f * k);
        // 受击闪白（entity.cpp 只维护 wd 计数，flash 字段由渲染层忽略）
        if(e.wd > 0) tint = rgba(255, 200, 190, 255);

        bool flip = (e.x > px);
        // anim / flash 由渲染层按时间推导（entity.cpp 不写这两个字段）
        f32 anim_ph = (f32)platform::now_seconds() * 3.0f + (f32)i;
        f32 bob = std::sin(anim_ph) * 0.05f;
        // 地面怪按底边对齐到脚点，飞行/弹道按中心对齐（见 ent_on_ground）
        const bool grounded = ent_on_ground(e.type);
        const f32 foot_y = (f32)e.y + 0.5f;
        const f32 top_y = grounded ? (foot_y - s) : (foot_y - s * 0.5f);
        // 阴影
        render::draw(atlas::F_GLOW, (f32)e.x + 0.5f - s * 0.5f,
                     grounded ? (foot_y - s * 0.16f) : (foot_y + s * 0.30f),
                     s, s * 0.42f, rgba(0, 0, 0, 70));
        if(ent_portrait(e.type))
        {
            // MC 立绘是 16x32（宽高比 1:2），画「宽 = 高 / 2」、取槽位左半边；
            // 翻转 = 交换 u（同 draw_player_only）
            const f32 pw = s * 0.5f;
            if(flip)
                render::draw_sub(slot, (f32)e.x + 0.5f - pw * 0.5f, top_y + bob, pw, s,
                                 0.5f, 0.0f, 0.0f, 1.0f, tint);
            else
                render::draw_sub(slot, (f32)e.x + 0.5f - pw * 0.5f, top_y + bob, pw, s,
                                 0.0f, 0.0f, 0.5f, 1.0f, tint);
        }
        else
        {
            render::draw_ex(slot, (f32)e.x + 0.5f - s * 0.5f,
                            top_y + bob, s, s, tint,
                            0.0f, flip, false);
        }

        // 苦力怕引信：红色闪烁
        if(e.type == entity::T_CREEPER && e.fuse > 0.01f)
        {
            u8 a = (u8)(90 + 150 * e.fuse);
            render::ring((f32)e.x + 0.5f, (f32)e.y + 0.5f, 3.0f, 0, rgba(255, 70, 60, a));
        }

        // 小怪血条
        if(e.hp < e.hp_max && e.type != entity::T_ARROW)
        {
            f32 w = 1.6f;
            f32 bx = (f32)e.x + 0.5f - w * 0.5f;
            f32 by = top_y - 0.32f;   // 贴在贴图头顶上方（跟着锚点走）
            render::rect(bx - 0.05f, by - 0.05f, w + 0.10f, 0.24f, rgba(10, 10, 14, 210));
            f32 ratio = (f32)(e.hp / e.hp_max);
            if(ratio < 0) ratio = 0;
            if(ratio > 1) ratio = 1;
            render::rect(bx, by, w * ratio, 0.16f, rgba(226, 72, 78, 255));
        }
    }
}

// ---------------- 玩家 ----------------
void draw_player_only()
{
    Player &p = st().pl;
    Ambient amb = ambient_of(world::now_time, world::night);
    bool nether = (world::dim == world::DIM_NETHER);
    i16 lt = world::light_at((i32)p.x, (i32)p.y);
    f32 k = light_visibility(lt, (i32)p.y, nether);
    Color tint = tint_of(rgba(255, 255, 255, 255), 0.55f + 0.70f * k);
    if(p.wudi > 0 && (p.wudi / 3) % 2 == 0) tint = rgba(255, 210, 210, 210);

    // MC 立绘是 16x32（宽高比 1:2），所以画 1 格宽 x 2 格高。
    const f32 sh = 2.0f;                   // ★ 人物 2 格高
    const f32 sw = 1.0f;                   // 宽 = 高 / 2
    // 物理坐标 (p.x,p.y) 是**脚点**（落地时停在地面格的上沿），所以贴图要按
    // **底边**对齐到脚点画，不能按中心对齐 —— 否则贴图会往下多伸半个身位，
    // 看起来就像人卡在方块里（实际人是站在上面的）。
    const f32 foot_y = (f32)p.y + 0.5f;    // 脚点换算到渲染坐标系
    const f32 cx = (f32)p.x + 0.5f;
    // 阴影：椭圆中心落在脚点上
    render::draw(atlas::F_GLOW, cx - sw * 0.5f,
                 foot_y - sw * 0.18f, sw, sw * 0.36f, rgba(0, 0, 0, 80));
    bool flip = ((f32)p.mpx < (f32)p.x);
    f32 bob = std::sin((f32)platform::now_seconds() * 6.0f) *
              (std::abs(p.spx) > 0.05 ? 0.05f : 0.0f);
    // 立绘存在槽位的左半边（16x32），用 draw_sub 取左半边；翻转 = 交换 u
    if(flip)
        render::draw_sub(atlas::E_PLAYER, cx - sw * 0.5f, foot_y - sh + bob,
                         sw, sh, 0.5f, 0.0f, 0.0f, 1.0f, tint);
    else
        render::draw_sub(atlas::E_PLAYER, cx - sw * 0.5f, foot_y - sh + bob,
                         sw, sh, 0.0f, 0.0f, 0.5f, 1.0f, tint);

    // 手持物品：画在朝鼠标那一侧的"手上"（约在身体中下部）
    i32 held = item::beibao[p.cho].id;
    if(held > 0)
    {
        f32 hx = cx + (flip ? -0.45f : 0.45f);
        f32 hy = foot_y - sh * 0.45f + bob;
        render::draw(atlas::slot_of_block(held), hx - 0.30f, hy - 0.30f, 0.60f, 0.60f,
                     tint_of(rgba(255, 255, 255, 255), 0.60f + 0.65f * k));
    }
}

}   // namespace

// ============================================================================
//                                对外接口
// ============================================================================
void set_crack(i32 x, i32 y, i32 level)
{
    g_crack_x = x;
    g_crack_y = y;
    g_crack_lv = level;
}
void clear_crack()
{
    g_crack_x = -1;
    g_crack_lv = 0;
}

void draw_world()
{
    Player &p = st().pl;
    Ambient amb = ambient_of(world::now_time, world::night);
    f32 tilePx = (f32)st().tile_px;

    draw_sky(world::now_time, world::night, world::dim);
    render::begin_world((f32)p.jx, (f32)p.jy, tilePx);
    draw_terrain(amb);
    draw_crack();
    draw_entities();
    draw_player_only();
    fx::draw();
    render::end();
}

// ============================================================================
//                                  HUD
// ============================================================================
namespace {

void draw_bar(f32 x, f32 y, f32 w, f32 h, f32 ratio, Color fill, Color back)
{
    if(ratio < 0) ratio = 0;
    if(ratio > 1) ratio = 1;
    render::rect(x - 2, y - 2, w + 4, h + 4, rgba(0, 0, 0, 180));
    render::rect(x, y, w, h, back);
    if(ratio > 0) render::rect(x, y, w * ratio, h, fill);
}

// ★ 耐久条：物品格内底部一条细条，颜色随剩余比例
void draw_durability_bar(const item::Stack &s, f32 x, f32 y, f32 slot_px)
{
    i32 mx = s.max_dur();
    if(mx <= 0) return;
    f32 f = (f32)s.remain() / (f32)mx;
    if(f > 1) f = 1;
    f32 bw = slot_px - 8.0f;
    f32 bh = 4.0f;
    f32 bx = x + 4.0f;
    f32 by = y + slot_px - 8.0f;
    render::rect(bx - 1, by - 1, bw + 2, bh + 2, rgba(0, 0, 0, 200));
    render::draw(atlas::U_DUR_BG, bx, by, bw, bh);
    if(f > 0)
    {
        render::draw(atlas::U_DUR_FILL, bx, by, bw * f, bh,
                     tint_of(s.dur_color(), 1.0f, 235));
    }
    // 低于 10% 闪红
    if(f < 0.10f)
    {
        f32 bl = 0.5f + 0.5f * std::sin((f32)platform::now_seconds() * 12.0f);
        render::rect_outline(bx - 1, by - 1, bw + 2, bh + 2,
                             rgba(255, 60, 50, (u8)(120 + 135 * bl)), 1);
    }
}

void draw_item_in_slot(const item::Stack &s, f32 x, f32 y, f32 slot_px, bool sel)
{
    render::draw(sel ? atlas::U_SLOT_SEL : atlas::U_SLOT, x, y, slot_px, slot_px);
    if(s.empty()) return;
    f32 ip = slot_px - 10.0f;
    render::draw(atlas::slot_of_block(s.id), x + 5, y + 5, ip, ip);
    // 数量
    if(s.count > 1)
    {
        char b[8];
        snprintf(b, sizeof(b), "%d", (int)s.count);
        f32 sc = slot_px / 40.0f;
        render::text_right(x + slot_px - 4, y + slot_px - 8 - 14 * sc, b,
                           rgba(255, 255, 255), sc);
    }
    draw_durability_bar(s, x, y, slot_px);
}

}   // namespace

void draw_hud()
{
    Player &p = st().pl;
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();

    // 飘字先画（同为 UI 坐标系，在面板之下）
    fx::draw_float_text();

    const f32 slot_px = 46.0f;
    const f32 gap = 4.0f;
    const f32 bar_w = slot_px * 9 + gap * 8;
    const f32 bar_x = (w - bar_w) * 0.5f;
    // 从底往上排：状态行(26) -> 心/护甲(24) -> 快捷栏
    const f32 status_y = h - 28;
    const f32 stat_y = status_y - 30;
    const f32 bar_y = stat_y - slot_px - 6;

    // ---- 快捷栏 ----
    render::panel(bar_x - 8, bar_y - 8, bar_w + 16, slot_px + 16,
                  rgba(24, 20, 32, 195), rgba(120, 108, 84, 200), 8, 2);
    for(i32 i = 1; i <= 9; i++)
    {
        f32 x = bar_x + (i - 1) * (slot_px + gap);
        draw_item_in_slot(item::beibao[i], x, bar_y, slot_px, p.cho == i);
    }

    // ---- 血量 / 护甲 / 氧气 ----
    const f32 heart = 22.0f;
    f32 ly = stat_y;
    for(i32 i = 0; i < 10; i++)
    {
        f32 x = bar_x + 4 + i * (heart + 2);
        f64 v = p.life - i * 2;
        i32 slot = atlas::U_HEART_BG;
        if(v >= 2) slot = atlas::U_HEART_FULL;
        else if(v > 0) slot = atlas::U_HEART_HALF;
        render::draw(slot, x, ly, heart, heart);
    }
    // 护甲值
    if(p.hujiazhi > 0)
        for(i32 i = 0; i < 10; i++)
        {
            f32 x = bar_x + bar_w - 4 - (i + 1) * (heart + 2) + 2;
            bool on = (p.hujiazhi >= (i + 1) * 2);
            render::draw(atlas::U_ARMOR_ICON, x, ly, heart, heart,
                         on ? rgba(255, 255, 255, 255) : rgba(90, 90, 100, 150));
        }
    // 氧气（水下）
    if(p.oxygen < 880)
    {
        f32 oy = ly - 24;
        f32 f = p.oxygen / 900.0f;
        if(f < 0) f = 0;
        for(i32 i = 0; i < 10; i++)
        {
            f32 x = bar_x + 4 + i * (heart + 2);
            bool on = (f > i / 10.0f);
            render::draw(atlas::U_BUBBLE, x, oy, heart, heart,
                         on ? rgba(190, 230, 255, 240) : rgba(70, 90, 110, 120));
        }
    }

    // ---- ★ 当前手持物品的名称与耐久（放在快捷栏上方居中）----
    {
        const item::Stack &s = item::beibao[p.cho];
        if(!s.empty())
        {
            const BlockDef &b = block_of(s.id);
            char line[128];
            if(s.max_dur() > 0)
                snprintf(line, sizeof(line), "%s   耐久 %d/%d", b.cn, s.remain(),
                         s.max_dur());
            else
                snprintf(line, sizeof(line), "%s   x%d", b.cn, (int)s.count);
            render::text_center_shadow(w * 0.5f, bar_y - 30, line,
                                       rgba(255, 240, 200), 1.0f);
        }
    }

    // ---- Boss 血条 ----
    if(st().bosshp > 0)
    {
        f32 bw = w * 0.55f;
        draw_bar((w - bw) * 0.5f, 26, bw, 16, (f32)st().bosshp / 500.0f,
                 rgba(226, 72, 78), rgba(40, 20, 24));
        char b[64];
        snprintf(b, sizeof(b), "BOSS  %d", st().bosshp);
        render::text_center_shadow(w * 0.5f, 6, b, rgba(255, 226, 140), 1.0f);
    }
    if(st().boss2hp > 0)
    {
        f32 bw = w * 0.40f;
        draw_bar((w - bw) * 0.5f, 52, bw, 12, (f32)st().boss2hp / 689.0f,
                 rgba(200, 90, 220), rgba(30, 16, 34));
        char b[64];
        snprintf(b, sizeof(b), "递归  %d", st().boss2hp);
        render::text_center_shadow(w * 0.5f, 66, b, rgba(240, 200, 255), 0.9f);
    }

    // ---- 坐标 / 时间 / 维度（最底一行）----
    {
        char line[192];
        i32 hh = world::now_time / 1000;
        i32 mm = (world::now_time % 1000) * 60 / 1000;
        i32 dispY = world::H - 1 - (i32)p.y - 1;
        snprintf(line, sizeof(line), "%s  X %d  Y %d   %02d:%02d   %s",
                 world::dim == world::DIM_NETHER ? "下界" : "主世界",
                 (i32)p.x - world::W / 2, dispY, hh, mm,
                 world::gamemode == 0 ? "创造" : "生存");
        render::text_shadow(10, status_y, line, rgba(210, 220, 240), 0.9f);
        if(st().show_fps)
        {
            static f64 last_t = 0;
            static f32 acc = 0;
            static i32 frames = 0;
            static f32 fps_shown = 0;
            f64 nowt = platform::now_seconds();
            if(last_t > 0)
            {
                acc += (f32)(nowt - last_t);
                frames++;
                if(acc > 0.5f) { fps_shown = frames / acc; acc = 0; frames = 0; }
            }
            last_t = nowt;
            char f[96];
            snprintf(f, sizeof(f), "%.0f FPS   绘制 %d 批", (double)fps_shown,
                     render::last_draw_calls());
            render::text_right(w - 10, status_y, f, rgba(170, 180, 200), 0.9f);
        }
    }

    // ---- 提示消息 ----
    if(st().tip_life > 0 && st().tip[0])
    {
        u8 a = (u8)(255 * std::min(1.0f, st().tip_life));
        render::text_center_shadow(w * 0.5f, h * 0.30f, st().tip,
                                   rgba(255, 240, 190, a), 1.2f);
    }

    // ---- 面板 ----
    switch(st().ui)
    {
    case UI_INV:      draw_panel_inventory(); break;
    case UI_CHEST:    draw_panel_chest(); break;
    case UI_CRAFT:    draw_panel_craft(); break;
    case UI_SETTINGS: draw_panel_settings(); break;
    case UI_MAP:      draw_panel_map(); break;
    case UI_COMMAND:  draw_panel_command(); break;
    default: break;
    }

    // ---- 准星 ----
    if(st().ui == UI_NONE)
    {
        f32 tpx = (f32)st().tile_px;
        f32 cxp = w * 0.5f + ((f32)p.mpx + 0.5f - (f32)p.jx) * tpx;
        f32 cyp = h * 0.5f + ((f32)p.mpy + 0.5f - (f32)p.jy) * tpx;
        render::draw(atlas::U_CURSOR_CROSS, cxp - tpx * 0.5f, cyp - tpx * 0.5f,
                     tpx, tpx, rgba(255, 255, 255, 170));
    }
}

void draw_all(f32 dt)
{
    g_water_phase += dt;
    draw_world();
    draw_hud();
}

// ============================================================================
//                                  粒子
// ============================================================================
namespace fx {

struct Particle
{
    f32 x, y, vx, vy, life, max_life, size, rot, spin;
    i32 slot;
    u32 color;
    bool gravity;
};
std::vector<Particle> g_ps;

struct FloatText
{
    f32 x, y, life;
    char text[48];
    u32 color;
};
std::vector<FloatText> g_ft;

void spawn_dust(f32 x, f32 y, i32 block_id, i32 count)
{
    for(i32 i = 0; i < count; i++)
    {
        Particle p;
        p.x = x + 0.5f + (world::rnd(100) / 100.0f - 0.5f) * 0.8f;
        p.y = y + 0.5f + (world::rnd(100) / 100.0f - 0.5f) * 0.8f;
        p.vx = (world::rnd(100) / 100.0f - 0.5f) * 4.0f;
        p.vy = -(world::rnd(100) / 100.0f) * 3.0f - 1.0f;
        p.max_life = 0.45f + world::rnd(40) / 100.0f;
        p.life = p.max_life;
        p.size = 0.22f + world::rnd(20) / 100.0f;
        p.rot = 0;
        p.spin = 0;
        p.slot = atlas::slot_of_world_block(block_id);   // 碎屑是方块材质，用矿脉版
        p.color = rgba(255, 255, 255, 235);
        p.gravity = true;
        g_ps.push_back(p);
    }
}

void spawn_explosion(f32 x, f32 y, f32 radius)
{
    Particle p;
    p.x = x;
    p.y = y;
    p.vx = p.vy = 0;
    p.max_life = 0.45f;
    p.life = p.max_life;
    p.size = radius * 2.4f;
    p.rot = 0;
    p.spin = 0;
    p.slot = atlas::F_EXPLOSION;
    p.color = rgba(255, 255, 255, 255);
    p.gravity = false;
    g_ps.push_back(p);
    for(i32 i = 0; i < 14; i++)
    {
        Particle q;
        f32 an = (f32)world::rnd(628) / 100.0f;
        f32 sp = 3.0f + world::rnd(400) / 100.0f;
        q.x = x;
        q.y = y;
        q.vx = std::cos(an) * sp;
        q.vy = std::sin(an) * sp;
        q.max_life = 0.5f + world::rnd(50) / 100.0f;
        q.life = q.max_life;
        q.size = 0.4f + world::rnd(40) / 100.0f;
        q.rot = 0;
        q.spin = 0;
        q.slot = (world::rnd(2) ? atlas::F_FLAME : atlas::F_SMOKE);
        q.color = rgba(255, 255, 255, 255);
        q.gravity = true;
        g_ps.push_back(q);
    }
}

void spawn_splash(f32 x, f32 y)
{
    for(i32 i = 0; i < 6; i++)
    {
        Particle p;
        p.x = x + 0.5f;
        p.y = y + 0.5f;
        p.vx = (world::rnd(100) / 100.0f - 0.5f) * 5.0f;
        p.vy = -2.0f - world::rnd(300) / 100.0f;
        p.max_life = 0.4f;
        p.life = p.max_life;
        p.size = 0.3f;
        p.rot = 0;
        p.spin = 0;
        p.slot = atlas::F_SPLASH;
        p.color = rgba(200, 235, 255, 230);
        p.gravity = true;
        g_ps.push_back(p);
    }
}

void spawn_flame(f32 x, f32 y)
{
    Particle p;
    p.x = x;
    p.y = y;
    p.vx = (world::rnd(100) / 100.0f - 0.5f) * 0.6f;
    p.vy = -0.6f - world::rnd(60) / 100.0f;
    p.max_life = 0.7f;
    p.life = p.max_life;
    p.size = 0.35f;
    p.rot = 0;
    p.spin = 0;
    p.slot = atlas::F_FLAME;
    p.color = rgba(255, 220, 160, 230);
    p.gravity = false;
    g_ps.push_back(p);
}

void spawn_text(f32 x, f32 y, const char *text, u32 color)
{
    FloatText t;
    t.x = x;
    t.y = y;
    t.life = 1.1f;
    snprintf(t.text, sizeof(t.text), "%s", text);
    t.color = color;
    g_ft.push_back(t);
}

void tick(f32 dt)
{
    if(g_ps.size() > 1400) g_ps.erase(g_ps.begin(), g_ps.begin() + 400);
    for(size_t i = 0; i < g_ps.size();)
    {
        Particle &p = g_ps[i];
        p.life -= dt;
        if(p.life <= 0) { g_ps.erase(g_ps.begin() + i); continue; }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if(p.gravity) p.vy += 14.0f * dt;
        p.vx *= 0.99f;
        i++;
    }
    for(size_t i = 0; i < g_ft.size();)
    {
        g_ft[i].life -= dt;
        g_ft[i].y -= dt * 1.2f;
        if(g_ft[i].life <= 0) g_ft.erase(g_ft.begin() + i);
        else i++;
    }
}

void draw()
{
    // 只画粒子。粒子在 begin_world 坐标系里，由 draw_world() 调用。
    for(const Particle &p : g_ps)
    {
        f32 f = p.life / p.max_life;
        if(f > 1) f = 1;
        u8 a = (u8)(255 * (f > 0.4f ? 1.0f : f / 0.4f));
        Color c = p.color;
        c = (c & 0x00FFFFFFu) | ((u32)((a * ((c >> 24) & 0xFF)) / 255) << 24);
        render::draw(p.slot, p.x - p.size * 0.5f, p.y - p.size * 0.5f, p.size,
                     p.size, c);
    }
}

// 飘字在 UI 坐标系里，必须单独一趟。
//
// 注意：这里**不能**在循环里来回切 begin_ui / begin_world。
// 那会在一帧中间反复改投影矩阵并 flush 顶点批次，把世界那一批的状态搞乱
// （表现为画面抽搐、面板内容被冲掉）。所以粒子归粒子、飘字归飘字，
// 各自在自己的坐标系里一次画完。
void draw_float_text()
{
    if(g_ft.empty()) return;
    f32 tpx = (f32)st().tile_px;
    Player &p = st().pl;
    f32 halfW = (f32)platform::width() * 0.5f;
    f32 halfH = (f32)platform::height() * 0.5f;
    for(const FloatText &t : g_ft)
    {
        f32 f = t.life / 1.1f;
        u8 a = (u8)(255 * (f > 0.6f ? 1.0f : f / 0.6f));
        u32 c = (t.color & 0x00FFFFFFu) | ((u32)a << 24);
        f32 sx = halfW + (t.x - (f32)p.jx) * tpx;
        f32 sy = halfH + (t.y - (f32)p.jy) * tpx;
        render::text_center_shadow(sx, sy, t.text, c, 1.0f);
    }
}

void clear()
{
    g_ps.clear();
    g_ft.clear();
}

}   // namespace fx

}   // namespace game

