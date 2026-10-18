// ===================== 游戏层实现 =====================
#include "game.h"
#include "../platform/platform.h"
#include "../render/renderer.h"
#include "../render/font.h"
#include "../render/atlas.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace game {

// ===================== 武器表 =====================
// 数值表：cd 决定节奏，spread 决定准度，knock 决定打击反馈，range 是近战半径/弹丸射程
static const WeaponDef WEAPONS[] = {
    {"拳头",   -1,              WK_MELEE, 5,  0.32f, 0,     1, 0,  0, 3.0f, 1.6f, 0},
    {"木剑",   B_STONE_SWORD,   WK_MELEE, 25, 0.36f, 0,     1, 0,  0, 4.0f, 1.9f, 0},
    {"铁剑",   B_IRON_SWORD,    WK_MELEE, 30, 0.32f, 0,     1, 0,  0, 4.5f, 2.0f, 0},
    {"钻石剑", B_DIAMOND_SWORD, WK_MELEE, 35, 0.30f, 0,     1, 0,  0, 5.0f, 2.1f, 0},
    {"魔光剑", B_LIGHTS_BANE,   WK_MELEE, 40, 0.26f, 0,     1, 0,  0, 5.5f, 2.2f, 0},
    {"最强剑", B_OI_SWORD,     WK_MELEE, 55, 0.50f, 0,     1, 0,  0, 6.0f, 2.4f, 0},
    {"手枪",   B_PISTOL,        WK_GUN,   20, 0.28f, 0.035f, 1, 26, 2, 2.4f, 18.0f, 0},
    {"冲锋枪", B_SMG,           WK_GUN,   10, 0.09f, 0.100f, 1, 30, 1, 1.4f, 17.0f, 0},
    {"霰弹枪", B_SHOTGUN,       WK_GUN,   15, 0.72f, 0.220f, 6, 24, 4, 5.5f, 12.0f, 0},
    {"狙击枪", B_SNIPER,        WK_GUN,   80, 1.10f, 0.005f, 1, 46, 8, 6.5f, 26.0f, 2},
    {"木弓",   B_BOW,           WK_BOW,   25, 0.52f, 0.040f, 1, 22, 0, 3.0f, 20.0f, 0},
    {"恶魔弓", B_DEMON_BOW,     WK_BOW,   34, 0.42f, 0.030f, 1, 30, 0, 3.4f, 24.0f, 1},
};

const WeaponDef *weapon_of(i32 itemId)
{
    for(const WeaponDef &w : WEAPONS)
        if(w.itemId == itemId) return &w;
    return &WEAPONS[0];   // 兜底：拳头
}

bool is_weapon(i32 itemId)
{
    for(const WeaponDef &w : WEAPONS)
        if(w.itemId == itemId) return true;
    return false;
}

// ===================== 状态 =====================
static State g_st;

State &st() { return g_st; }
Player &player() { return g_st.pl; }

void init()
{
    g_st = State();
    g_st.screen = 0;
}

// ===================== 飘字 =====================
void add_float(f32 x, f32 y, f32 value, bool crit)
{
    if(value < 0.5f) return;
    if(g_st.floats.size() > 128) g_st.floats.erase(g_st.floats.begin());
    FloatText t;
    t.pos = {x + g_rng.unit(-0.18f, 0.18f), y - 0.5f};
    t.vy = crit ? -2.2f : -1.6f;
    t.lifeMax = t.life = crit ? 0.95f : 0.75f;
    t.value = value;
    t.crit = crit;
    t.color = crit ? 0 : 1;      // 0=暴击紫 1=普通黄
    t.kind = 0;
    t.text[0] = 0;
    g_st.floats.push_back(t);
}

void add_float_text(f32 x, f32 y, const char *text, i32 color)
{
    if(g_st.floats.size() > 128) g_st.floats.erase(g_st.floats.begin());
    FloatText t;
    t.pos = {x, y - 0.6f};
    t.vy = -0.9f;
    t.lifeMax = t.life = 1.3f;
    t.value = 0;
    t.crit = false;
    t.color = color;
    t.kind = 1;
    snprintf(t.text, sizeof(t.text), "%s", text);
    g_st.floats.push_back(t);
}

void show_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_st.msg, sizeof(g_st.msg), fmt, ap);
    va_end(ap);
    g_st.msgTimer = 2.6f;
}

// ===================== 背包 =====================
// 模型与控制台版一致：30 格、前 9 格快捷栏、同类叠加 64、武器独立占格。
// 手持武器 = 选中快捷栏槽里的武器（空槽 = 拳头），选槽即换手。

// 叠加上限：武器不可叠加，其余 64
static i32 inv_stack_max(i32 id) { return is_weapon(id) ? 1 : 64; }

// 使用消耗品（回血/能量/护甲/大治疗），返回是否生效
static bool use_consumable(i32 id)
{
    Player &p = g_st.pl;
    if(id == B_HEALTH_POTION)
    {
        p.hp = std::min(p.hpMax, p.hp + 30);
        add_float_text(p.pos.x, p.pos.y - 0.5f, "+30 生命", 4);
        return true;
    }
    if(id == B_ENERGY_POTION)
    {
        p.energy = std::min(p.energyMax, p.energy + 40);
        add_float_text(p.pos.x, p.pos.y - 0.5f, "+40 能量", 5);
        return true;
    }
    if(id == B_ARMOR_POTION)
    {
        p.armor = std::min(p.armorMax, p.armor + 60);
        add_float_text(p.pos.x, p.pos.y - 0.5f, "+60 护甲", 5);
        return true;
    }
    if(id == B_BIG_HEALTH)
    {
        p.hp = p.hpMax;
        add_float_text(p.pos.x, p.pos.y - 0.5f, "生命全满", 4);
        return true;
    }
    return false;
}

bool inv_add(i32 id, i32 n)
{
    if(id <= 0 || n <= 0) return false;
    i32 cap = inv_stack_max(id);
    // 先并入同类未满的堆（武器 cap=1 永远找不到未满堆，自动走空格）
    if(cap > 1)
    {
        for(i32 i = 0; i < INV_SLOTS; i++)
        {
            InvSlot &s = g_st.inv[i];
            if(s.id == id && s.count < cap)
            {
                i32 mv = std::min(cap - s.count, n);
                s.count += mv;
                n -= mv;
                if(n <= 0) return true;
            }
        }
    }
    // 再找空格（武器一分配一，避免同格塞多把）
    for(i32 i = 0; i < INV_SLOTS && n > 0; i++)
    {
        InvSlot &s = g_st.inv[i];
        if(s.id == 0)
        {
            s.id = id;
            s.count = std::min(cap, n);
            n -= s.count;
        }
    }
    return n <= 0;
}

void inv_refresh_weapon()
{
    InvSlot &s = g_st.inv[g_st.hotSel];
    g_st.pl.weapon = (s.id > 0 && is_weapon(s.id)) ? s.id : -1;   // -1 = 拳头
}

void inv_select(i32 slot)
{
    if(slot < 0) slot = 0;
    if(slot >= HOT_SLOTS) slot = HOT_SLOTS - 1;
    g_st.hotSel = slot;
    inv_refresh_weapon();
}

void inv_wheel(f32 d)
{
    if(d == 0) return;
    i32 step = d > 0 ? -1 : 1;
    i32 ns = (g_st.hotSel + step + HOT_SLOTS) % HOT_SLOTS;
    inv_select(ns);
}

f32 inv_grid_x0()
{
    return ((f32)platform::width()
            - (INV_COLS * INV_CELL + (INV_COLS - 1) * INV_GAP)) * 0.5f;
}

f32 inv_grid_y0() { return 120.0f; }

i32 inv_slot_at(i32 mx, i32 my)
{
    f32 gx = inv_grid_x0(), gy = inv_grid_y0();
    f32 step = INV_CELL + INV_GAP;
    f32 fx = (f32)mx - gx, fy = (f32)my - gy;
    if(fx < 0 || fy < 0) return -1;
    i32 c = (i32)(fx / step), r = (i32)(fy / step);
    if(c >= INV_COLS || r >= INV_ROWS) return -1;
    // 间隙里不算命中
    if(fx - c * step >= INV_CELL || fy - r * step >= INV_CELL) return -1;
    return r * INV_COLS + c;
}

// 背包界面点击：左键拿起/放下/交换，右键装备武器或使用药水
void inv_click(i32 button)
{
    i32 slot = inv_slot_at((i32)platform::mouse_x(), (i32)platform::mouse_y());
    if(button == 0)
    {
        if(slot < 0) return;
        InvSlot &s = g_st.inv[slot];
        if(!g_st.holding)
        {
            if(s.id > 0)
            {
                g_st.held = s;
                g_st.holding = true;
                s = {0, 0};
            }
        }
        else
        {
            if(s.id == 0)                     // 空格：放下
            {
                s = g_st.held;
                g_st.holding = false;
            }
            else if(s.id == g_st.held.id && inv_stack_max(s.id) > 1)
            {
                // 同类可叠加：并入，放不下的留在手上
                i32 mv = std::min(inv_stack_max(s.id) - s.count, g_st.held.count);
                s.count += mv;
                g_st.held.count -= mv;
                if(g_st.held.count <= 0) g_st.holding = false;
            }
            else                             // 不同物品或武器：整格交换
            {
                InvSlot t = s;
                s = g_st.held;
                g_st.held = t;
            }
        }
        inv_refresh_weapon();
        return;
    }

    // ---- 右键：装备 / 使用 ----
    if(g_st.holding || slot < 0) return;
    InvSlot &s = g_st.inv[slot];
    if(s.id <= 0) return;
    if(is_weapon(s.id))
    {
        // 装备 = 换到选中的快捷栏槽（原槽内容对调）
        if(slot == g_st.hotSel) { inv_refresh_weapon(); return; }
        InvSlot t = g_st.inv[g_st.hotSel];
        g_st.inv[g_st.hotSel] = s;
        s = t;
        inv_refresh_weapon();
        show_msg("装备：%s", tile_of(g_st.pl.weapon).cn);
        g_st.shake = std::max(g_st.shake, 3.0f);
    }
    else if(use_consumable(s.id))
    {
        s.count--;
        if(s.count <= 0) s = {0, 0};
    }
}

void inv_close()
{
    if(g_st.holding)
    {
        // 逐件放回背包，只有确实放不下的才掉在脚边。
        // （整堆调用 inv_add 时它可能只放入一部分却仍返回 false，
        //   若照旧把 held 整堆清零，剩余数量会被凭空吞掉）
        while(g_st.held.count > 0 && inv_add(g_st.held.id, 1))
            g_st.held.count--;
        for(i32 left = g_st.held.count; left > 0; left--)
        {
            ent::Entity *drop = ent::spawn(ent::PICKUP, g_st.pl.pos.x + 0.6f,
                                           g_st.pl.pos.y + 0.3f);
            if(!drop) break;
            drop->variant = g_st.held.id;
        }
        g_st.held = {0, 0};
        g_st.holding = false;
    }
    g_st.invOpen = false;
    inv_refresh_weapon();
}

// ===================== 判定工具 =====================
static bool circle_hits_solid(const world::Level &lv, f32 cx, f32 cy, f32 r)
{
    i32 x0 = (i32)std::floor(cx - r), x1 = (i32)std::floor(cx + r);
    i32 y0 = (i32)std::floor(cy - r), y1 = (i32)std::floor(cy + r);
    for(i32 y = y0; y <= y1; y++)
        for(i32 x = x0; x <= x1; x++)
        {
            if(!lv.solid(x, y)) continue;
            f32 nx = clampf(cx, (f32)x, (f32)x + 1.0f);
            f32 ny = clampf(cy, (f32)y, (f32)y + 1.0f);
            f32 dx = cx - nx, dy = cy - ny;
            if(dx * dx + dy * dy < r * r) return true;
        }
    return false;
}

bool point_blocked(const world::Level &lv, f32 x, f32 y) { return lv.solid_at(x, y); }

// 带滑动碰撞的圆形移动：分小步推进，撞墙就停在该轴
static void move_circle(const world::Level &lv, Vec2 &pos, f32 r, f32 dx, f32 dy)
{
    const f32 STEP = 0.12f;
    i32 n = (i32)(std::max(std::abs(dx), std::abs(dy)) / STEP) + 1;
    f32 sx = dx / n, sy = dy / n;
    for(i32 i = 0; i < n; i++)
    {
        if(sx != 0)
        {
            f32 nx = pos.x + sx;
            if(!circle_hits_solid(lv, nx, pos.y, r)) pos.x = nx;
            else sx = 0;
        }
        if(sy != 0)
        {
            f32 ny = pos.y + sy;
            if(!circle_hits_solid(lv, pos.x, ny, r)) pos.y = ny;
            else sy = 0;
        }
        if(sx == 0 && sy == 0) break;
    }
}

// ===================== 伤害 =====================
void damage_player(f32 dmg, f32, f32)
{
    Player &p = g_st.pl;
    if(p.invuln > 0) return;
    // 护甲 1:1 吸收，甲尽后才掉血（与旧版数值体系一致）
    if(p.armor > 0)
    {
        f32 absorb = std::min(p.armor, dmg);
        p.armor -= absorb;
        dmg -= absorb;
    }
    if(dmg > 0)
    {
        p.hp -= dmg;
        g_st.stats.damageTaken += (i32)(dmg + 0.5f);
        // 玩家挨打用红色 "-N" 飘字，和打怪的金色数字区分开
        char db[16];
        snprintf(db, sizeof(db), "-%d", (i32)(dmg + 0.5f));
        add_float_text(p.pos.x, p.pos.y, db, 4);
    }
    p.invuln = 0.55f;
    p.flash = 0.35f;
    g_st.shake = std::max(g_st.shake, 5.0f);
    if(p.hp <= 0)
    {
        p.hp = 0;
        show_msg("你倒下了…… 按 R 重新挑战第 %d 关", g_st.currentLevel);
        g_st.inGame = false;
    }
}

void damage_monster(i32 idx, f32 dmg, bool crit, f32 fromX, f32 fromY)
{
    ent::Entity *pool = ent::pool();
    if(idx < 0 || idx >= ent::MAX_ENTITIES || !pool[idx].alive) return;
    ent::Entity &e = pool[idx];
    if(e.type >= ent::ARROW) return;
    if(e.type == ent::NPC) return;          // 商人不可攻击

    e.hp -= dmg;
    e.flash = 0.22f;
    if(e.type != ent::MERC) add_float(e.pos.x, e.pos.y, dmg, crit);
    g_st.shake = std::max(g_st.shake, crit ? 4.5f : 2.4f);
    g_st.hitStop = std::max(g_st.hitStop, crit ? 0.045f : 0.022f);

    // 击退（Boss 免疫）
    f32 dx = e.pos.x - fromX, dy = e.pos.y - fromY;
    f32 d = std::sqrt(dx * dx + dy * dy);
    if(d > 1e-4f && e.type != ent::BOSS)
    {
        f32 k = crit ? 0.55f : 0.32f;
        e.pos.x += dx / d * k;
        e.pos.y += dy / d * k;
    }

    if(e.hp <= 0)
    {
        if(e.type == ent::MERC)
        {
            show_msg("雇佣兵倒下了……");
            ent::kill(idx);
            return;
        }
        g_st.stats.kills++;
        // 击杀奖励（旧版规则：30% 回蓝 8-15，25% 回血 4-8）
        Player &p = g_st.pl;
        if(g_rng.chance(30))
        {
            i32 v = 8 + g_rng.range(0, 7);
            p.energy = std::min(p.energyMax, p.energy + (f32)v);
            char fb[24];
            snprintf(fb, sizeof(fb), "+%d 能量", v);
            add_float_text(e.pos.x, e.pos.y, fb, 5);
        }
        if(g_rng.chance(25))
        {
            i32 v = 4 + g_rng.range(0, 4);
            p.hp = std::min(p.hpMax, p.hp + (f32)v);
            char fb[24];
            snprintf(fb, sizeof(fb), "+%d 生命", v);
            add_float_text(e.pos.x, e.pos.y, fb, 4);
        }
        ent::kill(idx);
    }
}

// ===================== 关卡流程 =====================
static void spawn_wave(world::Room &room)
{
    i32 lv = g_st.currentLevel;
    i32 n = 2 + g_rng.range(0, 2);                 // 每波 2-3 只
    if(lv >= 5) n += 1;
    if(lv >= 9) n += 1;
    if(lv >= 13) n += 1;
    if(n > 8) n = 8;

    for(i32 i = 0; i < n; i++)
    {
        // 在房间内随机取一个不撞墙的落点，并避开玩家附近
        f32 x = 0, y = 0;
        for(i32 tries = 0; tries < 30; tries++)
        {
            x = (f32)(room.x0 + 2 + g_rng.range(0, world::ROOM_W - 5)) + 0.5f;
            y = (f32)(room.y0 + 2 + g_rng.range(0, world::ROOM_H - 5)) + 0.5f;
            if(g_st.level.solid_at(x, y)) continue;
            if(dist(x, y, g_st.pl.pos.x, g_st.pl.pos.y) < 3.0f) continue;
            break;
        }

        // 刷怪池：随关卡逐步加入新种类
        i32 t = ent::ZOMBIE;
        i32 roll = g_rng.range(100);
        if(lv <= 1) t = ent::ZOMBIE;
        else if(lv == 2) t = roll < 55 ? ent::ZOMBIE : ent::SKELETON;
        else if(lv <= 4)
            t = roll < 40 ? ent::ZOMBIE : (roll < 72 ? ent::SKELETON : ent::CREEPER);
        else if(lv <= 7)
            t = roll < 28 ? ent::ZOMBIE : (roll < 50 ? ent::SKELETON
                : (roll < 68 ? ent::CREEPER : (roll < 85 ? ent::SPIDER : ent::PIGLIN)));
        else
            t = roll < 20 ? ent::ZOMBIE : (roll < 38 ? ent::SKELETON
                : (roll < 54 ? ent::CREEPER : (roll < 70 ? ent::SPIDER
                : (roll < 84 ? ent::PIGLIN : (roll < 93 ? ent::MAGE : ent::BAT)))));

        ent::Entity *e = ent::spawn(t, x, y);
        if(!e) break;
        // 旧版规则：普通怪 120+25×关；近战伤害 +2/关、箭 +1/关
        const ent::MonDef *d = ent::mon_def(t);
        e->hpMax = e->hp = d->hp + 25.0f * (lv - 1);
        e->dmg = d->dmg + (d->ranged ? 1.0f : 2.0f) * (lv - 1);
    }
}

static void spawn_boss(world::Room &room)
{
    i32 lv = g_st.currentLevel;
    f32 x = (f32)(room.x0 + world::ROOM_W / 2) + 0.5f;
    f32 y = (f32)(room.y0 + 4) + 0.5f;
    ent::Entity *e = ent::spawn(ent::BOSS, x, y);
    if(!e) return;
    const ent::MonDef *d = ent::mon_def(ent::BOSS);
    // 旧版规则：Boss 1500+250×关
    e->hpMax = e->hp = d->hp + 250.0f * (lv - 1);
    e->dmg = d->dmg + 1.0f * (lv - 1);
    show_msg("第 %d 关 · 深渊吞噬者 苏醒！", lv);
}

void start_level(i32 n, bool offerTalent)
{
    if(n < 1) n = 1;
    // 关卡尺寸固定，仅靠关卡号 + 本局种子确定性重算
    if(g_st.runSeed == 0)
        g_st.runSeed = (u32)(platform::now_seconds() * 1000.0) + 12345u;
    world::generate(g_st.level, n, g_st.runSeed + (u32)n * 7919u);
    ent::clear();
    g_st.currentLevel = n;
    g_st.stats.level = n;
    g_st.floats.clear();
    g_st.transition = 1.0f;
    g_st.shopOpen = false;
    g_st.invOpen = false;

    // 玩家落到起始房中央
    if(!g_st.level.rooms.empty())
    {
        const world::Room &r = g_st.level.rooms[0];
        g_st.pl.pos = {(f32)(r.x0 + world::ROOM_W / 2) + 0.5f,
                       (f32)(r.y0 + world::ROOM_H / 2) + 0.5f};
        g_st.currentRoomIdx = 0;
    }
    g_st.pl.vel = {0, 0};
    g_st.cam = g_st.pl.pos;
    ent::rebuild_hash(g_st.level);

    // 商店房：放一个商人 NPC
    for(const world::Room &r : g_st.level.rooms)
    {
        if(!r.shop) continue;
        ent::spawn(ent::NPC, (f32)(r.x0 + world::ROOM_W / 2) + 0.5f,
                   (f32)(r.y0 + world::ROOM_H / 2) + 0.5f);
    }

    g_st.talentOpen = offerTalent;
    show_msg("第 %d 关 · 探索分叉地牢，找到深渊吞噬者", n);
}

void new_run()
{
    Player &p = g_st.pl;
    p = Player();
    g_st.stats = RunStats();
    g_st.runSeed = 0;        // 新开一局：换一套关卡布局
    g_st.currentLevel = 1;
    p.hp = p.hpMax;
    p.armor = p.armorMax;
    p.energy = p.energyMax;
    p.coins = 0;
    // 背包清空，起始铁剑放进快捷栏第 1 格并选中
    for(i32 i = 0; i < INV_SLOTS; i++) g_st.inv[i] = {0, 0};
    g_st.held = {0, 0};
    g_st.holding = false;
    g_st.inv[0] = {B_IRON_SWORD, 1};
    inv_select(0);
    g_st.inGame = true;
    g_st.paused = false;
    g_st.talentCount = 0;
    start_level(1, true);
}

void abandon_run()
{
    g_st.inGame = false;
    g_st.paused = false;
    g_st.shopOpen = false;
    g_st.invOpen = false;
    g_st.talentOpen = false;
    g_st.holding = false;
    g_st.held = {0, 0};
    g_st.screen = 0;
}

// 死亡重挑：满状态重进本关第一个房间（旧版规则：血/甲/蓝全满重打本关）
// 背包保留（同局重挑不没收装备），手持武器仍由选中快捷栏槽决定
void restart_level()
{
    Player &p = g_st.pl;
    p.hp = p.hpMax;
    p.armor = p.armorMax;
    p.energy = p.energyMax;
    g_st.inGame = true;
    g_st.paused = false;
    g_st.shopOpen = false;
    g_st.invOpen = false;
    g_st.holding = false;
    g_st.held = {0, 0};
    start_level(g_st.currentLevel, false);
    inv_refresh_weapon();
    show_msg("满状态重挑第 %d 关", g_st.currentLevel);
}

// ---- 天赋三选一（旧版规则：上限与当前值一起加）----
void apply_talent(i32 k)
{
    Player &p = g_st.pl;
    if(k == 1)
    {
        p.hpMax += 25;
        p.hp += 25;
        show_msg("天赋：生命上限 +25");
    }
    else if(k == 2)
    {
        p.armorMax += 50;
        p.armor += 50;
        show_msg("天赋：护甲上限 +50");
    }
    else if(k == 3)
    {
        p.energyMax += 50;
        p.energy += 50;
        show_msg("天赋：能量上限 +50");
    }
    else return;
    g_st.talentCount++;
    g_st.talentOpen = false;
}

// ---- 雇佣兵随从（旧版：伤害 30+10×关）----
bool hire_merc()
{
    if(ent::alive_count_of_type(ent::MERC) > 0)
    {
        show_msg("你已经有一名雇佣兵了");
        return false;
    }
    Player &p = g_st.pl;
    ent::Entity *m = ent::spawn(ent::MERC, p.pos.x - 1.0f, p.pos.y + 1.0f);
    if(!m) return false;
    i32 lv = g_st.currentLevel;
    m->hpMax = m->hp = 150.0f + 30.0f * (lv - 1);
    m->dmg = 30.0f + 10.0f * (lv - 1);
    show_msg("雇佣兵加入队伍（伤害 %.0f）", (f64)m->dmg);
    return true;
}

// ---- 商店购买 ----
void shop_buy(i32 k)
{
    Player &p = g_st.pl;
    if(k == 1)
    {
        if(p.coins < 15) { show_msg("金币不足（护甲药水 15 金币）"); return; }
        p.coins -= 15;
        p.armor = std::min(p.armorMax, p.armor + 60);
        show_msg("购买护甲药水：护甲 %d/%d", (i32)p.armor, (i32)p.armorMax);
    }
    else if(k == 2)
    {
        if(p.coins < 20) { show_msg("金币不足（大治疗药水 20 金币）"); return; }
        p.coins -= 20;
        p.hp = p.hpMax;
        show_msg("生命完全恢复");
    }
    else if(k == 3)
    {
        if(p.coins < 30) { show_msg("金币不足（雇佣兵 30 金币）"); return; }
        if(!hire_merc()) return;
        p.coins -= 30;
    }
}

// ---- 开发/验证：把玩家丢进指定房间（复用 update_level_logic 的真实进房流程）----
void warp_to_room(i32 idx)
{
    world::Level &lv = g_st.level;
    if(idx < 0 || idx >= (i32)lv.rooms.size()) return;
    const world::Room &r = lv.rooms[(size_t)idx];
    g_st.pl.pos = {(f32)(r.x0 + world::ROOM_W / 2) + 0.5f,
                   (f32)(r.y0 + world::ROOM_H / 2) + 0.5f};
    // warp 不要盖住商店房正中的商人（玩家会挡住 NPC 导致看不见）
    if(r.shop) g_st.pl.pos.y += 2.5f;
    g_st.pl.vel = {0, 0};
    g_st.cam = g_st.pl.pos;
    g_st.currentRoomIdx = -1;      // 强制下一帧识别为"刚进房"
    ent::rebuild_hash(lv);
}

// ---- 开发/验证：在玩家右侧叠着挤出一群怪，看分离求解能不能把它们摊开 ----
void debug_spawn_crowd(i32 n)
{
    Vec2 c = {g_st.pl.pos.x + 3.5f, g_st.pl.pos.y};
    const ent::MonDef *d = ent::mon_def(ent::ZOMBIE);
    for(i32 i = 0; i < n; i++)
    {
        // 故意都叠在几乎同一个点上（只差 0.02 格），逼分离求解去解决
        ent::Entity *e = ent::spawn(ent::ZOMBIE, c.x + i * 0.02f, c.y);
        if(!e) break;
        e->hpMax = e->hp = d->hp * 30.0f;    // 血厚，方便观察
    }
}

// ---- 开发/验证：丢一把武器在玩家旁边（验证掉落物不会消失）----
void debug_give_weapon(i32 itemId)
{
    if(!is_weapon(itemId)) return;
    ent::Entity *w = ent::spawn(ent::PICKUP, g_st.pl.pos.x + 1.3f, g_st.pl.pos.y + 0.3f);
    if(w) w->variant = itemId;
}

// ---- 开发/验证：直接入包并选中该武器（验证背包装备链）----
void debug_swap_weapon(i32 itemId)
{
    // 直接入包并选中：验证背包->装备链路（--swap 用）
    if(!is_weapon(itemId)) return;
    if(!inv_add(itemId, 1)) return;
    for(i32 i = 0; i < INV_SLOTS; i++)
    {
        if(g_st.inv[i].id == itemId)
        {
            if(i < HOT_SLOTS) inv_select(i);
            else
            {
                // 落在背包区：与选中槽对调后选中
                InvSlot t = g_st.inv[g_st.hotSel];
                g_st.inv[g_st.hotSel] = g_st.inv[i];
                g_st.inv[i] = t;
                inv_refresh_weapon();
            }
            return;
        }
    }
}

void debug_add_item(i32 itemId, i32 n)
{
    inv_add(itemId, n);
}

// ===================== 玩家 =====================
static void update_player(f32 dt)
{
    Player &p = g_st.pl;
    const world::Level &lv = g_st.level;

    if(p.invuln > 0) p.invuln -= dt;
    if(p.flash > 0) p.flash -= dt * 3.0f;
    if(p.fireCd > 0) p.fireCd -= dt;
    if(p.recoil > 0) p.recoil = std::max(0.0f, p.recoil - dt * 6.0f);
    if(p.muzzle > 0) p.muzzle -= dt * 6.0f;
    if(p.dashCd > 0) p.dashCd -= dt;

    // ---- 移动 ----
    f32 ix = 0, iy = 0;
    if(platform::key_down(platform::K_A)) ix -= 1;
    if(platform::key_down(platform::K_D)) ix += 1;
    if(platform::key_down(platform::K_W)) iy -= 1;
    if(platform::key_down(platform::K_S)) iy += 1;
    Vec2 dir = Vec2(ix, iy).norm();

    // 冲刺：Shift，短距离位移 + 无敌帧（给玩家一个「操作」按钮）
    if(platform::key_pressed(platform::K_SHIFT) && p.dashCd <= 0 && dir.len2() > 0.01f)
    {
        p.dashT = 0.16f;
        p.dashDir = dir;
        p.dashCd = 1.1f;
        p.invuln = std::max(p.invuln, 0.26f);
    }
    Vec2 vel;
    if(p.dashT > 0)
    {
        p.dashT -= dt;
        vel = p.dashDir * (p.speed * 3.1f);
    }
    else
    {
        vel = dir * p.speed;
        // 加速度平滑，避免瞬间起步/急停的生硬手感
        f32 k = 1.0f - std::exp(-16.0f * dt);
        p.vel.x += (vel.x - p.vel.x) * k;
        p.vel.y += (vel.y - p.vel.y) * k;
        vel = p.vel;
    }
    if(dir.len2() > 0.01f || p.dashT > 0) p.anim += dt * 8.0f;

    move_circle(lv, p.pos, p.radius, vel.x * dt, vel.y * dt);

    // ---- 瞄准 ----
    p.aim = std::atan2(camera_mouse_world().y - p.pos.y,
                       camera_mouse_world().x - p.pos.x);

    // ---- 开火 ----
    const WeaponDef *w = weapon_of(p.weapon);
    bool wantFire = platform::mouse_down(platform::MB_LEFT);
    if(wantFire && p.fireCd <= 0)
    {
        if(w->kind == WK_MELEE)
        {
            // 近战扇形：范围判定 + 打击感
            p.fireCd = w->cd;
            p.recoil = 1.0f;
            i32 hitAny = 0;
            std::vector<i32> near;
            ent::query(p.pos.x, p.pos.y, w->range + 1.0f, near);
            ent::Entity *pool = ent::pool();
            for(i32 j : near)
            {
                if(!pool[j].alive || pool[j].type >= ent::ARROW
                        || pool[j].type == ent::NPC) continue;
                if(pool[j].type == ent::MERC) continue;   // 不打自己人
                f32 dx = pool[j].pos.x - p.pos.x, dy = pool[j].pos.y - p.pos.y;
                f32 d = std::sqrt(dx * dx + dy * dy);
                if(d > w->range + pool[j].radius) continue;
                f32 ang = std::atan2(dy, dx);
                if(std::abs(wrap_angle(ang - p.aim)) > 1.05f && d > 1.0f) continue;
                bool crit = g_rng.chance(15);
                f32 dmg = w->dmg * (crit ? 1.8f : 1.0f);
                damage_monster(j, dmg, crit, p.pos.x, p.pos.y);
                hitAny++;
            }
            if(hitAny == 0)
            {
                // 空挥也有反馈：轻微前冲（走碰撞求解，避免贴墙时被推进墙里）
                move_circle(lv, p.pos, p.radius,
                            std::cos(p.aim) * 0.12f, std::sin(p.aim) * 0.12f);
            }
        }
        else
        {
            if(w->energy > 0 && p.energy < w->energy)
            {
                // 能量不足：只给提示，不消耗
                if(p.fireCd <= 0)
                {
                    p.fireCd = 0.25f;
                    show_msg("能量不足（需要 %d）", w->energy);
                }
            }
            else
            {
                p.energy -= (f32)w->energy;
                p.fireCd = w->cd;
                p.recoil = 1.0f;
                p.muzzle = 1.0f;
                for(i32 i = 0; i < w->pellets; i++)
                {
                    f32 a = p.aim + g_rng.unit(-w->spread, w->spread);
                    ent::Entity *b = ent::spawn(
                                        w->kind == WK_BOW ? ent::ARROW : ent::BULLET,
                                        p.pos.x + std::cos(a) * 0.6f,
                                        p.pos.y + std::sin(a) * 0.6f);
                    if(!b) break;
                    b->vel = {std::cos(a) * w->speed, std::sin(a) * w->speed};
                    b->rot = a;
                    b->radius = 0.16f;
                    b->ttl = (i32)(w->range / std::max(1.0f, w->speed) * 60.0f);
                    if(b->ttl < 20) b->ttl = 20;
                    b->variant = (i32)w->dmg;
                    b->pierceLeft = w->pierce;
                    b->knock = w->knock;
                    b->state = 9;      // 9 = 玩家发射，用于命中判定分流
                }
            }
        }
    }

    // 能量缓慢自然回复（枪械经济：打光了要等）
    p.energy = std::min(p.energyMax, p.energy + dt * 6.0f);

    // ---- 右键：使用选中的快捷栏消耗品（控制台版"选中+右键"用法）----
    if(platform::mouse_pressed(platform::MB_RIGHT))
    {
        InvSlot &hs = g_st.inv[g_st.hotSel];
        if(hs.id > 0 && !is_weapon(hs.id) && use_consumable(hs.id))
        {
            hs.count--;
            if(hs.count <= 0) hs = {0, 0};
        }
    }

    // ---- 拾取：金币直接进钱袋，其余物品入背包（武器也走过去自动捡）----
    ent::Entity *pool = ent::pool();
    bool bagFull = false;
    for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
    {
        ent::Entity &e = pool[i];
        if(!e.alive || e.type != ent::PICKUP) continue;
        if(dist(e.pos.x, e.pos.y, p.pos.x, p.pos.y) > 0.9f) continue;
        if(e.variant == B_COIN)
        {
            // 金币价值存在 target 里（旧实现存 ttl 会被当寿命倒计时烧掉）
            i32 v = e.target > 0 ? e.target : 1;
            p.coins += v;
            char fb[24];
            snprintf(fb, sizeof(fb), "+%d 金币", v);
            add_float_text(e.pos.x, e.pos.y, fb, 2);
            ent::kill(i);
        }
        else if(inv_add(e.variant, 1))
        {
            char fb[32];
            snprintf(fb, sizeof(fb), "获得 %s", tile_of(e.variant).cn);
            add_float_text(e.pos.x, e.pos.y, fb, 1);
            ent::kill(i);
        }
        else bagFull = true;   // 背包满：留在地上，不kill
    }
    if(bagFull && g_st.msgTimer <= 0) show_msg("背包已满！");
}

// ===================== 怪物 AI =====================
static void monster_attack_melee(ent::Entity &e)
{
    Player &p = g_st.pl;
    f32 d = dist(e.pos.x, e.pos.y, p.pos.x, p.pos.y);
    if(d < e.radius + p.radius + 0.35f)
        damage_player(e.dmg, e.pos.x, e.pos.y);
}

static void update_monsters(f32 dt)
{
    ent::Entity *pool = ent::pool();
    Player &p = g_st.pl;
    const world::Level &lv = g_st.level;
    // 帧率无关计时：以 60fps 帧为基准做 f32 计数（180Hz 下也能正常走秒）
    f32 tick = dt * 60.0f;

    for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
    {
        ent::Entity &e = pool[i];
        if(!e.alive) continue;
        if(e.type >= ent::ARROW) continue;
        if(e.type == ent::NPC) continue;   // 商人锚定商店房，不跑 AI

        e.anim += dt * 6.0f;
        if(e.flash > 0) e.flash -= dt * 4.0f;
        if(e.cd > 0) e.cd -= tick;
        if(e.meleeCd > 0) e.meleeCd -= tick;

        f32 dx = p.pos.x - e.pos.x, dy = p.pos.y - e.pos.y;
        f32 d = std::sqrt(dx * dx + dy * dy);
        f32 nx = d > 1e-4f ? dx / d : 0, ny = d > 1e-4f ? dy / d : 0;
        bool canSee = d < 22.0f;
        Vec2 mv = {0, 0};

        switch(e.type)
        {
        case ent::MERC:
        {
            // 雇佣兵：主动攻击最近的怪，没敌人时跟在玩家身边
            i32 bestIdx = -1;
            f32 bestD = 7.5f;
            for(i32 j = 0; j < ent::MAX_ENTITIES; j++)
            {
                ent::Entity &m = pool[j];
                if(!m.alive || m.type < ent::ZOMBIE || m.type > ent::BOSS) continue;
                f32 dd = dist(m.pos.x, m.pos.y, e.pos.x, e.pos.y);
                if(dd < bestD) { bestD = dd; bestIdx = j; }
            }
            if(bestIdx >= 0)
            {
                ent::Entity &m = pool[bestIdx];
                f32 mdx = m.pos.x - e.pos.x, mdy = m.pos.y - e.pos.y;
                f32 md = std::sqrt(mdx * mdx + mdy * mdy);
                if(md > e.radius + m.radius + 0.15f)
                    mv = {mdx / md * e.speed, mdy / md * e.speed};
                else if(e.cd <= 0)
                {
                    e.cd = 48;
                    damage_monster(bestIdx, e.dmg, false, e.pos.x, e.pos.y);
                }
            }
            else
            {
                f32 fdx = p.pos.x - 1.2f - e.pos.x, fdy = p.pos.y + 1.2f - e.pos.y;
                f32 fd = std::sqrt(fdx * fdx + fdy * fdy);
                if(fd > 2.2f) mv = {fdx / fd * e.speed, fdy / fd * e.speed};
            }
            break;
        }

        case ent::ZOMBIE:
        case ent::PIGLIN:
            if(canSee && d > e.radius + p.radius + 0.1f)
                mv = {nx * e.speed, ny * e.speed};
            // 攻击必须受 atkCd 约束：原来只重置 cd 却没有判定条件，
            // 导致每帧都结算一次伤害，实际攻速被玩家无敌帧(0.55s)接管，
            // 僵尸/猪灵的 atkCd 数据形同虚设
            if(e.cd <= 0 && canSee && d < 1.2f)
            {
                e.cd = (f32)ent::mon_def(e.type)->atkCd;
                monster_attack_melee(e);
            }
            break;

        case ent::SKELETON:
            if(!canSee) break;
            // 保持中距：太近后退，太远靠近
            if(d < 4.5f) mv = {-nx * e.speed, -ny * e.speed};
            else if(d > 7.5f) mv = {nx * e.speed, ny * e.speed};
            if(e.cd <= 0)
            {
                e.cd = 96;
                f32 a = std::atan2(dy, dx) + g_rng.unit(-0.06f, 0.06f);
                ent::Entity *b = ent::spawn(ent::ARROW, e.pos.x + nx * 0.6f, e.pos.y + ny * 0.6f);
                if(b)
                {
                    b->vel = {std::cos(a) * 17.0f, std::sin(a) * 17.0f};
                    b->rot = a;
                    b->radius = 0.16f;
                    b->ttl = 150;
                    b->variant = (i32)e.dmg;
                }
            }
            break;

        case ent::CREEPER:
            if(!canSee) break;
            if(e.fuse > 0)
            {
                // 引信已点：闪红 + 停下，给玩家逃开的机会
                e.fuse -= dt / 1.25f;
                if(e.fuse <= 0)
                {
                    // 爆炸：范围伤害 + 冲击波（跳过自身，避免自爆触发击杀奖励）
                    boom(e.pos.x, e.pos.y, 3.2f, e.dmg, true, i);
                    ent::kill(i);
                    continue;
                }
            }
            else
            {
                mv = {nx * e.speed, ny * e.speed};
                if(d < 1.9f) e.fuse = 1.0f;
            }
            break;

        case ent::SPIDER:
            if(!canSee) break;
            if(e.stateT <= 0 && d < 6.0f && d > 1.4f)
            {
                e.stateT = 90;               // 蓄力跳扑
                e.state = 1;
            }
            if(e.state == 1)
            {
                e.stateT -= tick;
                if(e.stateT <= 40 && e.stateT > 34)
                {
                    // 起跳：给一个明显的位移
                    e.vel = {nx * e.speed * 3.4f, ny * e.speed * 3.4f};
                }
                if(e.stateT <= 0) { e.state = 0; e.vel = {0, 0}; }
                mv = e.vel;
            }
            else
                mv = {nx * e.speed, ny * e.speed};
            if(e.cd <= 0 && d < 1.1f) { e.cd = 45; monster_attack_melee(e); }
            break;

        case ent::BAT:
            // 乱飞：正弦摆动 + 间歇俯冲
            e.stateT -= tick;
            if(e.stateT <= 0) { e.stateT = 40 + g_rng.range(40); e.state = (u8)g_rng.range(2); }
            if(e.state == 0 && canSee)
            {
                f32 a = std::atan2(dy, dx) + std::sin(e.anim * 0.9f) * 0.9f;
                mv = {std::cos(a) * e.speed, std::sin(a) * e.speed};
            }
            else
                mv = {std::sin(e.anim * 0.7f) * e.speed, std::cos(e.anim * 1.1f) * e.speed};
            if(e.cd <= 0 && d < 1.0f) { e.cd = 50; monster_attack_melee(e); }
            break;

        case ent::MAGE:
            if(!canSee) break;
            if(d < 6.0f) mv = {-nx * e.speed, -ny * e.speed};
            else if(d > 9.5f) mv = {nx * e.speed, ny * e.speed};
            if(e.cd <= 0)
            {
                e.cd = 130;
                f32 a = std::atan2(dy, dx);
                ent::Entity *b = ent::spawn(ent::GRENADE, e.pos.x + nx * 0.7f, e.pos.y + ny * 0.7f);
                if(b)
                {
                    // 追踪弹：慢速但会拐弯
                    b->vel = {std::cos(a) * 7.0f, std::sin(a) * 7.0f};
                    b->rot = a;
                    b->radius = 0.24f;
                    b->ttl = 260;
                    b->variant = (i32)e.dmg;
                    b->target = -1;
                    b->state = 1;            // 追踪
                }
            }
            break;

        case ent::BOSS:
        {
            f32 hpRatio = e.hpMax > 0 ? e.hp / e.hpMax : 0;
            if(hpRatio < 0.5f && !e.bossPhase2)
            {
                e.bossPhase2 = true;
                e.cd = 90;
                show_msg("吞噬者进入狂暴形态！");
                g_st.shake = 14.0f;
                // 召唤 3 只小怪
                for(i32 k = 0; k < 3; k++)
                {
                    f32 a = g_rng.unit(0, 6.2832f);
                    ent::spawn(ent::ZOMBIE, e.pos.x + std::cos(a) * 2.5f,
                               e.pos.y + std::sin(a) * 2.5f);
                }
            }

            // 阶段1：蓄力冲刺；阶段2：环形弹幕 + 冲刺
            if(e.state == 0)
            {
                e.stateT -= tick;
                if(canSee && e.stateT <= 0 && d > 2.0f)
                {
                    e.state = 1;             // 预警
                    e.stateT = 34;
                    e.rot = std::atan2(dy, dx);
                }
                if(canSee) mv = {nx * e.speed * 0.7f, ny * e.speed * 0.7f};
            }
            else if(e.state == 1)
            {
                e.stateT -= tick;
                mv = {0, 0};
                if(e.stateT <= 0)
                {
                    e.state = 2;             // 冲刺
                    e.stateT = 26;
                    e.vel = {std::cos(e.rot) * e.speed * 4.2f,
                             std::sin(e.rot) * e.speed * 4.2f};
                    g_st.shake = 7.0f;
                }
            }
            else
            {
                e.stateT -= tick;
                mv = e.vel;
                if(e.stateT <= 0)
                {
                    e.state = 0;
                    e.stateT = e.bossPhase2 ? 70 : 110;
                    e.vel = {0, 0};
                }
            }

            if(e.cd <= 0)
            {
                e.cd = e.bossPhase2 ? 110 : 165;
                // 环形弹幕
                i32 bullets = e.bossPhase2 ? 14 : 8;
                for(i32 k = 0; k < bullets; k++)
                {
                    f32 a = e.anim * 0.2f + k * 6.2832f / bullets;
                    ent::Entity *b = ent::spawn(ent::GRENADE, e.pos.x, e.pos.y);
                    if(!b) break;
                    b->vel = {std::cos(a) * 8.0f, std::sin(a) * 8.0f};
                    b->rot = a;
                    b->radius = 0.26f;
                    b->ttl = 240;
                    b->variant = (i32)(e.dmg * 0.7f);
                    b->state = 0;
                }
            }
            // 近战碰撞单独冷却：不能被上面的弹幕 cd 抢占，
            // 否则每帧都会结算一次伤害（靠玩家无敌帧兜住）
            if(d < 1.6f && e.meleeCd <= 0)
            {
                e.meleeCd = 40;
                monster_attack_melee(e);
            }
            break;
        }

        default: break;
        }

        // 位移（含与墙的碰撞）
        if(mv.len2() > 1e-6f)
            move_circle(lv, e.pos, e.radius, mv.x * dt, mv.y * dt);
    }
}

// ===================== 爆炸 =====================
void boom(f32 x, f32 y, f32 radius, f32 dmg, bool hurtsPlayer, i32 skipIdx)
{
    // 特效：一圈火花
    for(i32 k = 0; k < 14; k++)
    {
        f32 a = k * 6.2832f / 14;
        ent::Entity *s = ent::spawn(ent::GRENADE, x + std::cos(a) * 0.4f, y + std::sin(a) * 0.4f);
        if(!s) break;
        s->state = 2;                     // 纯特效标记
        s->vel = {std::cos(a) * 5.0f, std::sin(a) * 5.0f};
        s->ttl = 18;
        s->radius = 0.2f;
    }
    g_st.shake = std::max(g_st.shake, 10.0f);

    if(hurtsPlayer)
    {
        Player &p = g_st.pl;
        f32 d = dist(x, y, p.pos.x, p.pos.y);
        if(d < radius) damage_player(dmg * (1.0f - d / radius), x, y);
    }
    ent::Entity *pool = ent::pool();
    for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
    {
        if(i == skipIdx) continue;   // 自爆源不炸自己（否则残血自爆会触发击杀奖励）
        ent::Entity &e = pool[i];
        if(!e.alive || e.type >= ent::ARROW || e.type == ent::NPC) continue;
        f32 d = dist(x, y, e.pos.x, e.pos.y);
        if(d >= radius) continue;
        damage_monster(i, dmg * (1.0f - d / radius), false, x, y);
    }
}

// ===================== 弹丸/特效 =====================
static void update_projectiles(f32 dt)
{
    ent::Entity *pool = ent::pool();
    Player &p = g_st.pl;
    const world::Level &lv = g_st.level;
    f32 tick = dt * 60.0f;

    for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
    {
        ent::Entity &e = pool[i];
        if(!e.alive) continue;
        if(e.type < ent::ARROW) continue;

        if(e.ttl > 0)
        {
            e.ttl -= tick;
            if(e.ttl <= 0) { ent::kill(i); continue; }
        }

        // 追踪弹拐向玩家
        if(e.type == ent::GRENADE && e.state == 1)
        {
            f32 dx = p.pos.x - e.pos.x, dy = p.pos.y - e.pos.y;
            f32 d = std::sqrt(dx * dx + dy * dy);
            if(d > 0.1f)
            {
                f32 want = std::atan2(dy, dx);
                f32 cur = std::atan2(e.vel.y, e.vel.x);
                f32 na = turn_toward(cur, want, 2.2f * dt * 60.0f / 60.0f * 3.0f);
                f32 sp = e.vel.len();
                e.vel = {std::cos(na) * sp, std::sin(na) * sp};
                e.rot = na;
            }
        }

        e.pos.x += e.vel.x * dt;
        e.pos.y += e.vel.y * dt;
        e.anim += dt * 20.0f;

        // 撞墙
        if(e.type != ent::PICKUP && lv.solid_at(e.pos.x, e.pos.y))
        {
            ent::kill(i);
            continue;
        }

        // 命中判定
        if(e.type == ent::ARROW || e.type == ent::BULLET)
        {
            bool hit = false;
            // 玩家发射的弹丸打怪；怪发射的打玩家（用 variant 正负区分不了，
            // 这里用 life 存击退、state 区分归属：state==9 表示玩家发射）
            if(e.state == 9)
            {
                std::vector<i32> near;
                ent::query(e.pos.x, e.pos.y, 1.0f, near);
                for(i32 j : near)
                {
                    ent::Entity &m = pool[j];
                    if(!m.alive || m.type >= ent::ARROW || m.type == ent::NPC || j == i)
                        continue;
                    if(m.type == ent::MERC) continue;      // 玩家弹丸不打自己人
                    f32 r = m.radius + e.radius;
                    if(dist(e.pos.x, e.pos.y, m.pos.x, m.pos.y) > r) continue;
                    bool crit = g_rng.chance(12);
                    f32 dmg = (f32)e.variant * (crit ? 1.9f : 1.0f);
                    damage_monster(j, dmg, crit, e.pos.x - e.vel.x, e.pos.y - e.vel.y);
                    hit = true;
                    // 穿透
                    if(e.pierceLeft > 0) { e.pierceLeft--; hit = false; }
                    break;
                }
            }
            else
            {
                f32 r = p.radius + e.radius;
                if(dist(e.pos.x, e.pos.y, p.pos.x, p.pos.y) < r)
                {
                    damage_player((f32)e.variant, e.pos.x, e.pos.y);
                    hit = true;
                }
            }
            if(hit) { ent::kill(i); continue; }
        }
        else if(e.type == ent::GRENADE && e.state != 2)
        {
            // 敌方的能量弹命中玩家 / 到点自爆
            if(e.state == 1 || e.state == 0)
            {
                f32 r = p.radius + e.radius;
                if(dist(e.pos.x, e.pos.y, p.pos.x, p.pos.y) < r)
                {
                    damage_player((f32)e.variant, e.pos.x, e.pos.y);
                    boom(e.pos.x, e.pos.y, 1.6f, 0.0f, false);
                    ent::kill(i);
                }
            }
        }
    }
}

// ===================== 关卡推进 =====================
// 按房间类型决定宝箱掉落（旧版：大宝箱必出装备+15-30金币；小宝箱3-10金币/18%手枪/10%霰弹；起始房补给）
static void open_chest(i32 bx, i32 by)
{
    world::Level &lv = g_st.level;
    lv.set(bx, by, B_AIR);
    const world::Room *cr = lv.room_at((f32)bx + 0.5f, (f32)by + 0.5f);
    bool big = cr && cr->finalRoom;
    bool starter = cr && lv.room_idx(cr->layer, cr->col) == 0;

    i32 coins = big ? (15 + g_rng.range(0, 15))
                : (starter ? 5 : (3 + g_rng.range(0, 7)));
    ent::Entity *c = ent::spawn(ent::PICKUP, (f32)bx + 0.5f, (f32)by + 0.5f);
    if(c) { c->variant = B_COIN; c->target = coins; }

    if(big)
    {
        // 大宝箱必出装备
        i32 lvn = g_st.currentLevel;
        i32 pool2[] = {B_PISTOL, B_SHOTGUN, B_SNIPER, B_SMG, B_BOW, B_DEMON_BOW,
                       B_DIAMOND_SWORD, B_LIGHTS_BANE, B_OI_SWORD};
        i32 pick = pool2[g_rng.range(lvn >= 4 ? 9 : 8)];
        ent::Entity *w = ent::spawn(ent::PICKUP, (f32)bx + 1.3f, (f32)by + 0.5f);
        if(w) w->variant = pick;
        if(g_rng.chance(35))
        {
            ent::Entity *a = ent::spawn(ent::PICKUP, (f32)bx - 0.3f, (f32)by + 0.5f);
            if(a) a->variant = B_ARMOR_POTION;
        }
        show_msg("大宝箱：%s + %d 金币", tile_of(pick).cn, coins);
    }
    else if(starter)
    {
        ent::Entity *a = ent::spawn(ent::PICKUP, (f32)bx + 1.3f, (f32)by + 0.5f);
        if(a) a->variant = B_ARMOR_POTION;
        show_msg("补给箱：护甲药水 + %d 金币", coins);
    }
    else
    {
        i32 roll = g_rng.range(100);
        i32 pick = -1;
        if(roll < 18) pick = B_PISTOL;
        else if(roll < 28) pick = B_SHOTGUN;
        if(pick >= 0)
        {
            ent::Entity *w = ent::spawn(ent::PICKUP, (f32)bx + 1.3f, (f32)by + 0.5f);
            if(w) w->variant = pick;
            show_msg("宝箱里是：%s", tile_of(pick).cn);
        }
        else show_msg("获得 %d 金币", coins);
    }
    g_st.shake = std::max(g_st.shake, 3.0f);
}

// 扭蛋机：10 金币一抽
static void gacha_roll()
{
    Player &p = g_st.pl;
    if(p.coins < 10) { show_msg("扭蛋需要 10 金币"); return; }
    p.coins -= 10;
    g_st.shake = std::max(g_st.shake, 4.0f);
    i32 roll = g_rng.range(100);
    if(roll < 40)
    {
        i32 lvn = g_st.currentLevel;
        i32 pool2[] = {B_PISTOL, B_SMG, B_SHOTGUN, B_SNIPER, B_BOW, B_DEMON_BOW,
                       B_IRON_SWORD, B_DIAMOND_SWORD, B_LIGHTS_BANE, B_OI_SWORD};
        i32 pick = pool2[g_rng.range(lvn >= 4 ? 10 : 9)];
        ent::Entity *w = ent::spawn(ent::PICKUP, p.pos.x + 1.3f, p.pos.y);
        if(w) w->variant = pick;
        show_msg("扭蛋出：%s", tile_of(pick).cn);
    }
    else if(roll < 65)
    {
        p.armor = std::min(p.armorMax, p.armor + 60);
        add_float_text(p.pos.x, p.pos.y, "+60 护甲", 5);
    }
    else if(roll < 85)
    {
        p.hp = p.hpMax;
        add_float_text(p.pos.x, p.pos.y, "生命全满", 4);
    }
    else if(roll < 95)
    {
        p.coins += 25;
        show_msg("头奖！+25 金币");
    }
    else
    {
        p.coins += 5;
        show_msg("谢谢惠顾，返还 5 金币");
    }
}

static void update_level_logic(f32 dt)
{
    world::Level &lv = g_st.level;
    Player &p = g_st.pl;

    // ---- 进房检测 ----
    world::Room *cur = lv.room_at(p.pos.x, p.pos.y);
    i32 curIdx = cur ? lv.room_idx(cur->layer, cur->col) : -1;
    if(curIdx != g_st.currentRoomIdx && cur)
    {
        g_st.currentRoomIdx = curIdx;
        if(cur->state == world::ROOM_LOCKED)
        {
            // 武装房间：波次就绪，但等玩家真正走进来、离开门洞后才落闸
            cur->state = world::ROOM_FIGHT;
            cur->sealed = false;
            cur->wavesTotal = cur->finalRoom ? 1
                                             : 2 + std::min(4, g_st.currentLevel / 3);
            cur->wavesDone = 0;
            cur->aliveLeft = 0;
            cur->spawnCd = 30.0f;
            g_st.roomDmgStart = g_st.stats.damageTaken;   // 无伤房判定起点
        }
    }

    // ---- 战斗房间推进 ----
    for(size_t ri = 0; ri < lv.rooms.size(); ri++)
    {
        world::Room &r = lv.rooms[ri];
        if(r.state != world::ROOM_FIGHT) continue;

        if(!r.sealed)
        {
            // 玩家只踩了门洞又退了出去：解除武装，房间回到未进入状态
            if(lv.room_at(p.pos.x, p.pos.y) != &r)
            {
                r.state = world::ROOM_LOCKED;
                r.wavesTotal = 0;
                r.wavesDone = 0;
                continue;
            }
            // 玩家已离开门洞区域才落闸关门——门永远不可能关在人身上
            if(lv.door_clear(r, p.pos.x, p.pos.y, p.radius + 0.15f))
            {
                // 随从若还堵在门口，直接拽到玩家身边（否则会被封进墙里）
                ent::Entity *pool = ent::pool();
                for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
                {
                    ent::Entity &m = pool[i];
                    if(!m.alive || m.type != ent::MERC) continue;
                    if(!lv.door_clear(r, m.pos.x, m.pos.y, m.radius + 0.15f))
                    {
                        m.pos = {p.pos.x - 0.9f, p.pos.y + 0.9f};
                        m.vel = {0, 0};
                    }
                }
                lv.door_set(r.layer, r.col, 1);
                r.sealed = true;
                if(r.finalRoom)
                {
                    spawn_boss(r);
                    r.wavesDone = 1;
                }
                else show_msg("门已关闭——清空房间才能离开！");
            }
            continue;   // 未落闸绝不刷怪、绝不关门
        }

        i32 alive = 0;
        ent::Entity *pool = ent::pool();
        for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
        {
            if(!pool[i].alive) continue;
            i32 t = pool[i].type;
            if(t < ent::ZOMBIE || t > ent::BOSS) continue;
            if(lv.room_at(pool[i].pos.x, pool[i].pos.y) == &r) alive++;
        }
        r.aliveLeft = alive;

        if(alive == 0)
        {
            if(r.wavesDone < r.wavesTotal)
            {
                // f32 计时：修复 180Hz 下 (i32)(dt*60) 截断成 0、波次永不刷新的 bug
                if(r.spawnCd > 0) r.spawnCd -= dt * 60.0f;
                if(r.spawnCd <= 0)
                {
                    spawn_wave(r);
                    r.wavesDone++;
                    r.spawnCd = 50.0f;
                }
            }
            else
            {
                // 清空：开门、放奖励
                r.state = world::ROOM_CLEAR;
                g_st.stats.roomsCleared++;
                // 无伤房：本房武装后到清空之间玩家没掉过血
                if(g_st.stats.damageTaken == g_st.roomDmgStart)
                    g_st.stats.noHitRooms++;
                save_run();          // 每清空一个房间自动存档
                lv.door_set(r.layer, r.col, 0);
                // 掉金币
                for(i32 k = 0; k < 2 + g_rng.range(0, 3); k++)
                {
                    ent::Entity *c = ent::spawn(ent::PICKUP,
                                                (f32)r.x0 + 2 + g_rng.unit(0, world::ROOM_W - 5),
                                                (f32)r.y0 + 2 + g_rng.unit(0, world::ROOM_H - 5));
                    if(!c) break;
                    c->variant = B_COIN;
                    c->target = 2 + g_rng.range(0, 4);
                }
                if(r.finalRoom)
                {
                    show_msg("本关清空！走到传送门旁按 E 前往下一关");
                    // 最终房中央生成传送门
                    lv.set(r.x0 + world::ROOM_W / 2, r.y0 + world::ROOM_H / 2, B_LEVEL_PORTAL);
                }
                else if(g_rng.chance(45))
                {
                    ent::Entity *hp = ent::spawn(ent::PICKUP,
                                                 (f32)r.x0 + 4 + g_rng.unit(0, 8),
                                                 (f32)r.y0 + 4 + g_rng.unit(0, 4));
                    if(hp)
                    {
                        i32 roll = g_rng.range(100);
                        hp->variant = roll < 40 ? B_HEALTH_POTION
                                   : (roll < 70 ? B_ENERGY_POTION : B_ARMOR_POTION);
                    }
                }
            }
        }
    }

    // 通关判定改为 E 交互（见下方交互块）：传送门不再踩上即过关，
    // 否则玩家从 Boss 身上踩过时会来不及开大箱/捡掉落就被送去下一关

    // ---- E 交互：商人 > 扭蛋机 > 传送门 > 宝箱（武器已改为走过去自动入包）----
    ent::Entity *pool = ent::pool();
    if(platform::key_pressed(platform::K_E))
    {
        i32 nearNpc = -1;
        for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
        {
            if(!pool[i].alive || pool[i].type != ent::NPC) continue;
            if(dist(pool[i].pos.x, pool[i].pos.y, p.pos.x, p.pos.y) < 1.8f)
            {
                nearNpc = i;
                break;
            }
        }
        bool nearGacha = false;
        for(i32 dy = -1; dy <= 1 && !nearGacha; dy++)
            for(i32 dx = -1; dx <= 1; dx++)
            {
                i32 bx = (i32)std::floor(p.pos.x) + dx;
                i32 by = (i32)std::floor(p.pos.y) + dy;
                if(lv.get(bx, by) == B_GACHA) { nearGacha = true; break; }
            }
        bool nearPortal = false;
        for(i32 dy = -1; dy <= 1 && !nearPortal; dy++)
            for(i32 dx = -1; dx <= 1; dx++)
            {
                i32 bx = (i32)std::floor(p.pos.x) + dx;
                i32 by = (i32)std::floor(p.pos.y) + dy;
                if(lv.get(bx, by) == B_LEVEL_PORTAL) { nearPortal = true; break; }
            }

        if(nearNpc >= 0)
        {
            g_st.shopOpen = true;
            show_msg("商人：想看点什么？");
        }
        else if(nearGacha)
        {
            gacha_roll();
        }
        else if(nearPortal)
        {
            i32 next = g_st.currentLevel + 1;
            show_msg("进入第 %d 关", next);
            start_level(next, true);
            save_run();          // 过关即存档
        }
        else
        {
            for(i32 dy = -1; dy <= 1; dy++)
                for(i32 dx = -1; dx <= 1; dx++)
                {
                    i32 bx = (i32)std::floor(p.pos.x) + dx;
                    i32 by = (i32)std::floor(p.pos.y) + dy;
                    if(lv.get(bx, by) == B_CHEST) open_chest(bx, by);
                }
        }
    }
}

// ===================== 相机 =====================
Vec2 camera_mouse_world()
{
    f32 tilePx = 34.0f;
    f32 sw = (f32)platform::width(), sh = (f32)platform::height();
    f32 wx = g_st.cam.x + (platform::mouse_x() - sw * 0.5f) / tilePx;
    f32 wy = g_st.cam.y + (platform::mouse_y() - sh * 0.5f) / tilePx;
    return {wx, wy};
}

static void update_camera(f32 dt)
{
    // 目标点略偏向鼠标方向，视野更舒服
    f32 tilePx = 34.0f;
    f32 sw = (f32)platform::width(), sh = (f32)platform::height();
    Vec2 m = camera_mouse_world();
    Vec2 target = {g_st.pl.pos.x + (m.x - g_st.pl.pos.x) * 0.18f,
                   g_st.pl.pos.y + (m.y - g_st.pl.pos.y) * 0.18f};
    f32 k = 1.0f - std::exp(-8.0f * dt);
    g_st.cam.x += (target.x - g_st.cam.x) * k;
    g_st.cam.y += (target.y - g_st.cam.y) * k;
    (void)sw; (void)sh; (void)tilePx;
}

// ===================== 主更新 =====================
void update(f32 dt)
{
    g_st.stats.elapsed += dt;
    if(g_st.msgTimer > 0) g_st.msgTimer -= dt;
    if(g_st.shake > 0) g_st.shake = std::max(0.0f, g_st.shake - dt * 26.0f);
    if(g_st.transition > 0) g_st.transition = std::max(0.0f, g_st.transition - dt * 1.6f);

    // 命中定格：极短时间冻结世界，打击感的关键
    if(g_st.hitStop > 0)
    {
        g_st.hitStop -= dt;
        dt *= 0.15f;
    }

    ent::rebuild_hash(g_st.level);
    update_player(dt);
    update_monsters(dt);
    update_projectiles(dt);
    update_level_logic(dt);
    ent::separate(g_st.level);
    ent::separate_player(g_st.level, g_st.pl.pos, g_st.pl.radius);
    update_camera(dt);

    // 飘字推进
    for(size_t i = 0; i < g_st.floats.size();)
    {
        FloatText &f = g_st.floats[i];
        f.life -= dt;
        f.pos.y += f.vy * dt;
        f.vy *= 0.94f;
        if(f.life <= 0) g_st.floats.erase(g_st.floats.begin() + i);
        else i++;
    }
}

}   // namespace game