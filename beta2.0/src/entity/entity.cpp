// ===================== 实体层实现 =====================
#include "entity.h"
#include "../render/atlas.h"

namespace ent {

// 数值表：速度整体压到「看得清、躲得开」的节奏（旧版偏快）
static const MonDef DEFS[TYPE_COUNT] = {
    /* NONE     */ {"",                0,                    0,    0,    0.0f,  0,  0,  0, 1.0f},
    /* ZOMBIE   */ {"僵尸",            atlas::E_ZOMBIE,      120,  1.75f, 0.36f, 10, 60, 0, 1.0f},
    /* SKELETON */ {"骷髅",            atlas::E_SKELETON,    100,  1.45f, 0.34f, 6,  96, 1, 1.0f},
    /* CREEPER  */ {"苦力怕",          atlas::E_CREEPER,     110,  1.95f, 0.38f, 34, 0,  0, 1.0f},
    /* SPIDER   */ {"蜘蛛",            atlas::E_SPIDER,      90,   2.60f, 0.40f, 8,  45, 0, 1.0f},
    /* PIGLIN   */ {"僵尸猪灵",        atlas::E_PIGLIN,      220,  1.60f, 0.40f, 14, 70, 0, 1.05f},
    /* BAT      */ {"蝙蝠",            atlas::E_SPIDER,      60,   2.20f, 0.26f, 5,  50, 0, 0.75f},
    /* MAGE     */ {"施法者",          atlas::E_RECURSIVE,   130,  1.25f, 0.36f, 9,  110, 1, 1.0f},
    /* BOSS     */ {"深渊吞噬者",      atlas::E_BOSS,        1500, 1.35f, 1.55f, 18, 70, 0, 1.0f},
    /* MERC     */ {"雇佣兵",          atlas::E_OI,           150,  3.30f, 0.36f, 30, 48, 0, 1.0f},
    /* NPC      */ {"商人",            atlas::E_PIGLIN,      9999, 0,    0.42f, 0,  0,  0, 1.1f},
    /* ARROW    */ {"",                atlas::E_ARROW,       1,    0,    0.16f, 0,  0,  0, 1.0f},
    /* BULLET   */ {"",                atlas::E_BULLET,      1,    0,    0.14f, 0,  0,  0, 1.0f},
    /* GRENADE  */ {"",                atlas::E_SPARK,       1,    0,    0.22f, 0,  0,  0, 1.0f},
    /* PICKUP   */ {"",                atlas::E_COIN,        1,    0,    0.30f, 0,  0,  0, 1.0f},
};

const MonDef *mon_def(i32 type)
{
    if(type < 0 || type >= TYPE_COUNT) return &DEFS[NONE];
    return &DEFS[type];
}

static Entity g_ents[MAX_ENTITIES];

// ---------------- 空间哈希 ----------------
constexpr i32 HASH_CELL = 4;
static std::vector<i32> g_hash[world::MAX_W / HASH_CELL + 2][world::MAX_H / HASH_CELL + 2];
static i32 g_hw = 0, g_hh = 0;

// ---------------- 生命周期 ----------------
void clear()
{
    for(i32 i = 0; i < MAX_ENTITIES; i++) g_ents[i] = Entity();
    for(i32 y = 0; y < world::MAX_H / HASH_CELL + 2; y++)
        for(i32 x = 0; x < world::MAX_W / HASH_CELL + 2; x++)
            g_hash[x][y].clear();
}

Entity *spawn(i32 type, f32 x, f32 y)
{
    for(i32 i = 0; i < MAX_ENTITIES; i++)
    {
        if(g_ents[i].alive) continue;
        Entity &e = g_ents[i];
        e = Entity();
        e.alive = true;
        e.type = type;
        e.pos = {x, y};
        const MonDef *d = mon_def(type);
        e.hpMax = e.hp = d->hp;
        e.speed = d->speed;
        e.radius = d->radius;
        e.dmg = d->dmg;
        e.cd = d->atkCd > 0 ? (d->atkCd / 2) : 0;
        return &e;
    }
    return nullptr;
}

void kill(i32 index)
{
    if(index < 0 || index >= MAX_ENTITIES) return;
    g_ents[index] = Entity();
}

i32 count()
{
    i32 n = 0;
    for(i32 i = 0; i < MAX_ENTITIES; i++) if(g_ents[i].alive) n++;
    return n;
}

i32 alive_count_of_type(i32 type)
{
    i32 n = 0;
    for(i32 i = 0; i < MAX_ENTITIES; i++)
        if(g_ents[i].alive && g_ents[i].type == type) n++;
    return n;
}

i32 alive_monster_count()
{
    i32 n = 0;
    for(i32 i = 0; i < MAX_ENTITIES; i++)
    {
        if(!g_ents[i].alive) continue;
        i32 t = g_ents[i].type;
        if(t >= ZOMBIE && t <= BOSS) n++;
    }
    return n;
}

void rebuild_hash(const world::Level &lv)
{
    g_hw = lv.width / HASH_CELL + 1;
    g_hh = lv.height / HASH_CELL + 1;
    if(g_hw > world::MAX_W / HASH_CELL + 1) g_hw = world::MAX_W / HASH_CELL + 1;
    if(g_hh > world::MAX_H / HASH_CELL + 1) g_hh = world::MAX_H / HASH_CELL + 1;
    for(i32 y = 0; y <= g_hh; y++)
        for(i32 x = 0; x <= g_hw; x++)
            g_hash[x][y].clear();
    for(i32 i = 0; i < MAX_ENTITIES; i++)
    {
        if(!g_ents[i].alive) continue;
        i32 cx = clampi((i32)(g_ents[i].pos.x / HASH_CELL), 0, g_hw);
        i32 cy = clampi((i32)(g_ents[i].pos.y / HASH_CELL), 0, g_hh);
        g_hash[cx][cy].push_back(i);
    }
}

void query(f32 x, f32 y, f32 r, std::vector<i32> &out)
{
    out.clear();
    i32 x0 = clampi((i32)((x - r) / HASH_CELL), 0, g_hw);
    i32 x1 = clampi((i32)((x + r) / HASH_CELL), 0, g_hw);
    i32 y0 = clampi((i32)((y - r) / HASH_CELL), 0, g_hh);
    i32 y1 = clampi((i32)((y + r) / HASH_CELL), 0, g_hh);
    for(i32 cy = y0; cy <= y1; cy++)
        for(i32 cx = x0; cx <= x1; cx++)
            for(i32 idx : g_hash[cx][cy]) out.push_back(idx);
}

// ---------------- 硬分离（不可重叠的核心）----------------
namespace {

// 圆 vs 实心方块：把圆心推出所有重叠的墙格，保证实体不会被挤进墙体
void clamp_circle(const world::Level &lv, Vec2 &pos, f32 radius)
{
    for(i32 pass = 0; pass < 2; pass++)
    {
        i32 cx = (i32)std::floor(pos.x);
        i32 cy = (i32)std::floor(pos.y);
        bool moved = false;
        for(i32 dy = -1; dy <= 1; dy++)
            for(i32 dx = -1; dx <= 1; dx++)
            {
                i32 bx = cx + dx, by = cy + dy;
                if(!lv.solid(bx, by)) continue;
                f32 nx = clampf(pos.x, (f32)bx, (f32)bx + 1.0f);
                f32 ny = clampf(pos.y, (f32)by, (f32)by + 1.0f);
                f32 ddx = pos.x - nx, ddy = pos.y - ny;
                f32 d2 = ddx * ddx + ddy * ddy;
                if(d2 >= radius * radius) continue;
                f32 d = std::sqrt(d2);
                if(d < 1e-4f)
                {
                    // 圆心落在方块内：沿最短轴推出
                    f32 left = pos.x - (f32)bx;
                    f32 right = (f32)bx + 1.0f - pos.x;
                    f32 up = pos.y - (f32)by;
                    f32 down = (f32)by + 1.0f - pos.y;
                    f32 m = std::min(std::min(left, right), std::min(up, down));
                    if(m == left) pos.x = (f32)bx - radius;
                    else if(m == right) pos.x = (f32)bx + 1.0f + radius;
                    else if(m == up) pos.y = (f32)by - radius;
                    else pos.y = (f32)by + 1.0f + radius;
                }
                else
                {
                    f32 push = (radius - d) / d;
                    pos.x += ddx * push;
                    pos.y += ddy * push;
                }
                moved = true;
            }
        if(!moved) break;
    }
}

// 把实体拉回可行走区域：既不要卡进墙，也不要越过房间边界
void clamp_to_walkable(const world::Level &lv, Entity &e)
{
    clamp_circle(lv, e.pos, e.radius);
}

}   // namespace

void separate(const world::Level &lv)
{
    // 迭代 3 轮：一轮只能解决相邻对，多轮可让密集怪群收敛
    for(i32 iter = 0; iter < 3; iter++)
    {
        bool anyOverlap = false;
        std::vector<i32> near;
        for(i32 i = 0; i < MAX_ENTITIES; i++)
        {
            Entity &a = g_ents[i];
            if(!a.alive) continue;
            // 弹丸/拾取物不参与占位分离（否则会挤成一团）；NPC 锚定商店位不被推走
            if(a.type >= ARROW || a.type == NPC) continue;
            query(a.pos.x, a.pos.y, a.radius * 2 + 0.6f, near);
            for(i32 j : near)
            {
                if(j <= i) continue;
                Entity &b = g_ents[j];
                if(!b.alive || b.type >= ARROW || b.type == NPC) continue;

                f32 dx = b.pos.x - a.pos.x;
                f32 dy = b.pos.y - a.pos.y;
                f32 minD = a.radius + b.radius;
                f32 d2 = dx * dx + dy * dy;
                if(d2 >= minD * minD) continue;

                anyOverlap = true;
                f32 d = std::sqrt(d2);
                f32 nx, ny, overlap;
                // 圆心几乎重合时，"连线方向"是退化的（怪会挤成一条线）。
                // 这时改用由两只怪下标决定的确定性角度，让它们呈放射状摊开。
                if(d < minD * 0.30f)
                {
                    u32 h = (u32)i * 73856093u ^ (u32)j * 19349663u;
                    f32 ang = (f32)(h % 6283u) / 1000.0f;
                    nx = std::cos(ang);
                    ny = std::sin(ang);
                    overlap = minD * 0.5f;      // 明确推开半个身位
                }
                else
                {
                    nx = dx / d;
                    ny = dy / d;
                    overlap = minD - d;
                }

                // 按「质量倒数」分摊位移：Boss 质量大，被推得少，不会被小怪顶飞
                f32 aInv = (a.type == BOSS) ? 0.125f : 1.0f;
                f32 bInv = (b.type == BOSS) ? 0.125f : 1.0f;
                f32 total = aInv + bInv;
                f32 ka = aInv / total;
                f32 kb = bInv / total;
                a.pos.x -= nx * overlap * ka;
                a.pos.y -= ny * overlap * ka;
                b.pos.x += nx * overlap * kb;
                b.pos.y += ny * overlap * kb;
            }
        }
        if(!anyOverlap) break;
    }

    // 分离可能把人推进墙里，统一做一次可行走修正
    for(i32 i = 0; i < MAX_ENTITIES; i++)
    {
        if(!g_ents[i].alive) continue;
        if(g_ents[i].type >= ARROW || g_ents[i].type == NPC) continue;
        clamp_to_walkable(lv, g_ents[i]);
    }
}

// 玩家（不在实体池里）与怪物的硬分离：
// 怪被推得多、玩家被推得少（玩家有重量感），最后把玩家夹回可行走区域。
void separate_player(const world::Level &lv, Vec2 &ppos, f32 pradius)
{
    std::vector<i32> near;
    query(ppos.x, ppos.y, pradius + 2.5f, near);
    for(i32 j : near)
    {
        Entity &b = g_ents[j];
        if(!b.alive || b.type >= ARROW || b.type == NPC) continue;
        f32 dx = b.pos.x - ppos.x, dy = b.pos.y - ppos.y;
        f32 minD = pradius + b.radius;
        f32 d2 = dx * dx + dy * dy;
        if(d2 >= minD * minD) continue;
        f32 d = std::sqrt(d2);
        f32 nx, ny, overlap;
        if(d < minD * 0.30f)
        {
            f32 ang = (f32)(((u32)j * 2654435761u) % 6283u) / 1000.0f;
            nx = std::cos(ang);
            ny = std::sin(ang);
            d = 1.0f;
            overlap = minD * 0.5f;
        }
        else
        {
            nx = dx / d;
            ny = dy / d;
            overlap = minD - d;
        }
        // 怪承担 85%，玩家只让开 15%；Boss 更硬，玩家让开 30%
        f32 back = (b.type == BOSS) ? 0.30f : 0.15f;
        b.pos.x += nx * overlap * (1.0f - back);
        b.pos.y += ny * overlap * (1.0f - back);
        ppos.x -= nx * overlap * back;
        ppos.y -= ny * overlap * back;
    }
    // 玩家不会被挤进墙里
    clamp_circle(lv, ppos, pradius);
}

// 供游戏层访问实体池
Entity *pool() { return g_ents; }

}   // namespace ent