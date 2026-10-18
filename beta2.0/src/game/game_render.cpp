// ===================== 世界与 HUD 渲染 =====================
#include "game.h"
#include "../platform/platform.h"
#include "../render/renderer.h"
#include "../render/atlas.h"

#include <algorithm>
#include <cstdio>

namespace game {

static const f32 TILE_PX = 34.0f;

// 受击/濒死闪烁用的白色
static const Color C_WHITE = rgba(255, 255, 255, 255);

static Color light_tint(u8 light)
{
    f32 f = 0.34f + 0.66f * (light / 15.0f);
    return tint_of(C_WHITE, f);
}

// ---------------- 世界 ----------------
static void draw_tiles(const world::Level &lv)
{
    f32 halfW = platform::width() * 0.5f / TILE_PX;
    f32 halfH = platform::height() * 0.5f / TILE_PX;
    i32 x0 = (i32)std::floor(st().cam.x - halfW) - 1;
    i32 x1 = (i32)std::ceil(st().cam.x + halfW) + 1;
    i32 y0 = (i32)std::floor(st().cam.y - halfH) - 1;
    i32 y1 = (i32)std::ceil(st().cam.y + halfH) + 1;

    for(i32 y = y0; y <= y1; y++)
        for(i32 x = x0; x <= x1; x++)
        {
            if(!lv.in_bounds(x, y)) continue;
            i32 id = lv.get(x, y);
            u8 lt = lv.light_at(x, y);
            Color t = light_tint(lt);
            f32 fx = (f32)x, fy = (f32)y;

            if(id == B_AIR || id == B_TORCH || id == B_CHEST
                    || id == B_GACHA || id == B_LEVEL_PORTAL)
            {
                // 地面：灰石地砖 + 轻微棋盘格，帮助玩家感知移动
                f32 chk = ((x + y) & 1) ? 1.0f : 0.86f;
                render::draw(atlas::slot_of_block(B_STONE), fx, fy, 1, 1,
                             tint_of(t, chk * 0.62f));
            }
            if(id != B_AIR)
                render::draw(atlas::slot_of_block(id), fx, fy, 1, 1, t);
        }
}

// 实体排序用（按 y 从小到大，后画的下压前面的）
struct DrawEnt
{
    i32 idx;
    f32 y;
};

static void draw_entity_shadow(const ent::Entity &e, f32 size)
{
    render::draw(atlas::E_SHADOW, e.pos.x - size * 0.5f, e.pos.y + size * 0.30f,
                 size * 0.95f, size * 0.42f, rgba(255, 255, 255, 120));
}

static void draw_monster(const ent::Entity &e)
{
    const ent::MonDef *d = ent::mon_def(e.type);
    f32 size = d->scale * (e.type == ent::BOSS ? 3.2f : 1.0f);
    size *= 1.15f;

    draw_entity_shadow(e, size);

    Color tint = C_WHITE;
    if(e.type == ent::MERC) tint = rgba(150, 205, 255, 255);      // 友方蓝色
    else if(e.type == ent::NPC) tint = rgba(255, 216, 130, 255);  // 商人金色
    if(e.flash > 0.01f) tint = rgba(255, 235, 235, 255);

    // 苦力怕引信：用红色闪烁预警，让玩家知道该跑了
    if(e.type == ent::CREEPER && e.fuse > 0)
    {
        f32 blink = (std::sin(e.anim * 4.0f) > 0) ? 1.0f : 0.55f;
        tint = rgba(255, (u8)(140 * (1.0f - e.fuse * 0.5f)), (u8)(140 * (1.0f - e.fuse)),
                    255);
        // 危险范围提示圈
        render::ring(e.pos.x + 0.5f, e.pos.y + 0.5f, 3.0f,
                     0, rgba(255, 70, 60, (u8)(90 + 120 * e.fuse)));
        (void)blink;
    }
    // Boss 冲刺预警：画一条红色指向线
    if(e.type == ent::BOSS && e.state == 1)
    {
        f32 len = 7.0f;
        for(i32 k = 1; k <= 7; k++)
        {
            f32 cx = e.pos.x + 0.5f + std::cos(e.rot) * (k * len / 7.0f);
            f32 cy = e.pos.y + 0.5f + std::sin(e.rot) * (k * len / 7.0f);
            render::draw(atlas::U_CIRCLE, cx - 0.35f, cy - 0.35f, 0.7f, 0.7f,
                         rgba(255, 60, 60, (u8)(200 - k * 22)));
        }
    }

    bool flip = (e.pos.x < st().pl.pos.x) ? false : true;
    render::draw_ex(d->atlasSlot, e.pos.x - size * 0.5f, e.pos.y - size * 0.5f,
                    size, size, tint, 0, flip, false);

    // 血条：受伤后显示，Boss 常显，友方佣兵用绿色
    if(e.hp < e.hpMax && e.hp > 0)
    {
        f32 w = size * 0.85f;
        f32 barX = e.pos.x + 0.5f - w * 0.5f;
        f32 barY = e.pos.y - size * 0.5f - 0.22f;
        render::rect(barX - 0.03f, barY - 0.03f, w + 0.06f, 0.20f, rgba(12, 12, 16, 210));
        f32 ratio = clampf(e.hp / e.hpMax, 0, 1);
        Color bc = e.type == ent::BOSS ? rgba(220, 70, 200, 255)
                  : (e.type == ent::MERC ? rgba(110, 220, 130, 255)
                                        : rgba(226, 72, 78, 255));
        render::rect(barX, barY, w * ratio, 0.14f, bc);
    }
}

static void draw_projectile(const ent::Entity &e)
{
    switch(e.type)
    {
    case ent::ARROW:
    {
        i32 slot = atlas::E_ARROW;
        render::draw_ex(slot, e.pos.x - 0.5f, e.pos.y - 0.5f, 1.0f, 1.0f,
                        C_WHITE, e.rot, false, false);
        break;
    }
    case ent::BULLET:
        render::draw(atlas::E_BULLET, e.pos.x - 0.28f, e.pos.y - 0.28f, 0.56f, 0.56f);
        // 拖尾
        render::draw(atlas::U_CIRCLE, e.pos.x - 0.55f, e.pos.y - 0.55f, 1.1f, 1.1f,
                     rgba(255, 220, 140, 60));
        break;
    case ent::GRENADE:
        if(e.state == 2)
            render::draw(atlas::E_SPARK, e.pos.x - 0.4f, e.pos.y - 0.4f, 0.8f, 0.8f,
                         rgba(255, 220, 150, 220));
        else
        {
            render::draw(atlas::U_CIRCLE, e.pos.x - 0.42f, e.pos.y - 0.42f, 0.84f, 0.84f,
                         rgba(220, 110, 255, 200));
            render::draw(atlas::U_CIRCLE, e.pos.x - 0.22f, e.pos.y - 0.22f, 0.44f, 0.44f,
                         rgba(255, 240, 255, 230));
        }
        break;
    case ent::PICKUP:
        if(e.variant == B_COIN)
            render::draw(atlas::E_COIN, e.pos.x - 0.32f, e.pos.y - 0.32f, 0.64f, 0.64f);
        else
        {
            f32 bob = std::sin(e.anim * 2.0f) * 0.06f;
            render::draw(atlas::slot_of_block(e.variant), e.pos.x - 0.45f,
                         e.pos.y - 0.45f + bob, 0.9f, 0.9f);
        }
        break;
    default: break;
    }
}

static void draw_player()
{
    const Player &p = st().pl;
    const f32 size = 1.05f;

    render::draw(atlas::E_SHADOW, p.pos.x - size * 0.5f, p.pos.y + size * 0.30f,
                 size * 0.95f, size * 0.42f, rgba(255, 255, 255, 130));

    Color tint = C_WHITE;
    // 无敌帧闪烁
    if(p.invuln > 0 && ((i32)(p.invuln * 20.0f) & 1)) tint = rgba(255, 200, 200, 150);
    if(p.flash > 0) tint = rgba(255, 180, 180, 255);

    bool flip = (std::cos(p.aim) < 0);
    render::draw_ex(atlas::E_PLAYER, p.pos.x - size * 0.5f, p.pos.y - size * 0.5f,
                    size, size, tint, 0, flip, false);

    // 手里的武器：沿瞄准方向偏移画出，让朝向一目了然
    const WeaponDef *w = weapon_of(p.weapon);
    f32 hx = p.pos.x + 0.5f + std::cos(p.aim) * 0.62f;
    f32 hy = p.pos.y + 0.5f + std::sin(p.aim) * 0.62f;
    f32 rec = p.recoil * 0.22f;
    hx -= std::cos(p.aim) * rec;
    hy -= std::sin(p.aim) * rec;
    if(w->itemId >= 0)
        render::draw_ex(atlas::slot_of_block(w->itemId), hx - 0.34f, hy - 0.34f,
                        0.68f, 0.68f, C_WHITE, p.aim, false, false);

    // 开火枪口闪光
    if(p.muzzle > 0.02f && w->kind != WK_MELEE)
    {
        f32 mx = p.pos.x + 0.5f + std::cos(p.aim) * 0.95f;
        f32 my = p.pos.y + 0.5f + std::sin(p.aim) * 0.95f;
        f32 s = 0.9f * p.muzzle;
        render::draw(atlas::E_MUZZLE, mx - s * 0.5f, my - s * 0.5f, s, s,
                     rgba(255, 240, 190, (u8)(210 * p.muzzle)));
    }

    // 冲刺残影
    if(p.dashT > 0)
        render::draw(atlas::E_PLAYER, p.pos.x - size * 0.5f - std::cos(p.aim) * 0.35f,
                     p.pos.y - size * 0.5f - std::sin(p.aim) * 0.35f,
                     size, size, rgba(160, 200, 255, 110));
}

void render_world()
{
    // 相机抖动
    Vec2 cam = st().cam;
    if(st().shake > 0.05f)
    {
        cam.x += g_rng.unit(-st().shake, st().shake) / TILE_PX * 1.6f;
        cam.y += g_rng.unit(-st().shake, st().shake) / TILE_PX * 1.6f;
    }

    render::begin_world(cam.x, cam.y, TILE_PX);
    draw_tiles(st().level);

    // 收集需要绘制的实体并按 y 排序（NPC 商人也参与）
    std::vector<DrawEnt> list;
    ent::Entity *pool = ent::pool();
    for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
    {
        if(!pool[i].alive) continue;
        list.push_back({i, pool[i].pos.y});
    }
    std::sort(list.begin(), list.end(),
              [](const DrawEnt &a, const DrawEnt &b) { return a.y < b.y; });

    bool playerDrawn = false;
    for(const DrawEnt &de : list)
    {
        const ent::Entity &e = pool[de.idx];
        // 玩家应与实体一起参与排序
        if(!playerDrawn && st().pl.pos.y < e.pos.y)
        {
            draw_player();
            playerDrawn = true;
        }
        if(e.type >= ent::ARROW) draw_projectile(e);
        else draw_monster(e);
    }
    if(!playerDrawn) draw_player();

    // 头顶标签：商人（靠近时提示可交互）
    {
        const Player &p = st().pl;
        for(i32 i = 0; i < ent::MAX_ENTITIES; i++)
        {
            const ent::Entity &e = pool[i];
            if(!e.alive || e.type != ent::NPC) continue;
            f32 d = dist(e.pos.x, e.pos.y, p.pos.x, p.pos.y);
            if(d > 5.0f) continue;
            f32 sc = 0.62f / TILE_PX;
            render::text_center(e.pos.x + 0.5f, e.pos.y - 0.85f, "商人 · E 交谈",
                                rgba(255, 226, 140, 235), sc);
        }
    }

    // 伤害飘字（世界坐标，随镜头缩放）
    for(const FloatText &f : st().floats)
    {
        f32 a = clampf(f.life / f.lifeMax, 0, 1);
        u8 alpha = (u8)(255 * (a > 0.35f ? 1.0f : a / 0.35f));
        f32 pop = 1.0f + (1.0f - a) * 0.35f;
        if(f.kind == 0)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", (i32)(f.value + 0.5f));
            Color c = f.crit ? rgba(226, 140, 255, alpha) : rgba(255, 232, 150, alpha);
            // 世界 pass 的 1 个单位 = 1 格 = TILE_PX 像素，字号要换算回格
            f32 sc = (f.crit ? 0.52f : 0.42f) * pop / TILE_PX;
            render::text_center(f.pos.x + 0.5f, f.pos.y, buf, c, sc);
        }
        else
        {
            Color c = rgba(180, 240, 200, alpha);
            if(f.color == 2) c = rgba(255, 226, 130, alpha);
            else if(f.color == 4) c = rgba(255, 140, 140, alpha);
            else if(f.color == 5) c = rgba(150, 200, 255, alpha);
            render::text_center(f.pos.x + 0.5f, f.pos.y, f.text, c, 0.40f * pop / TILE_PX);
        }
    }
    render::end();
}

// ---------------- HUD ----------------
static void draw_bar(f32 x, f32 y, f32 w, f32 h, f32 ratio, i32 slot, const char *label,
                     const char *value)
{
    render::draw(atlas::U_BAR_BG, x, y, w, h, rgba(255, 255, 255, 230));
    ratio = clampf(ratio, 0, 1);
    if(ratio > 0.001f)
        render::draw(slot, x + 2, y + 2, (w - 4) * ratio, h - 4, C_WHITE);
    // 白字 + 深色投影：无论血条满/空都读得清
    f32 ty = y + (h - render::text_h(0.62f)) * 0.5f - 1;
    render::text(x + 9, ty + 1, label, rgba(10, 12, 18, 220), 0.62f);
    render::text(x + 8, ty, label, rgba(255, 255, 255, 245), 0.62f);
    render::text_right(x + w - 5, ty + 1, value, rgba(10, 12, 18, 220), 0.62f);
    render::text_right(x + w - 6, ty, value, rgba(255, 255, 255, 245), 0.62f);
}

// 房间级小地图：每间房一格、连接画粗线——分叉路线一目了然（元气骑士式）
static void draw_minimap()
{
    const world::Level &lv = st().level;
    if(lv.rooms.empty()) return;

    // 按实际占用范围归一化：房间不落在 col/layer 0 起步时，
    // 若直接按 max 计尺寸会在左侧/上方留出大块空白
    i32 minC = lv.rooms[0].col, maxC = minC;
    i32 minL = lv.rooms[0].layer, maxL = minL;
    for(const world::Room &r : lv.rooms)
    {
        minC = std::min(minC, r.col);
        maxC = std::max(maxC, r.col);
        minL = std::min(minL, r.layer);
        maxL = std::max(maxL, r.layer);
    }
    f32 cw = 24.0f, ch = 17.0f;      // 每格房间尺寸
    f32 w = (maxC - minC + 1) * cw, h = (maxL - minL + 1) * ch;
    f32 x = (f32)platform::width() - w - 18, y = 14;

    render::draw(atlas::U_PANEL, x - 6, y - 6, w + 12, h + 12, rgba(255, 255, 255, 200));

    // 连接线（每条画一次即可：开着的亮色，关着的暗色）
    i32 dirs[2] = {1, 3};            // 下、右
    for(const world::Room &r : lv.rooms)
    {
        for(i32 di = 0; di < 2; di++)
        {
            i32 d = dirs[di];
            if(r.link[d] < 0) continue;
            if(d == 1)
                render::rect(x + (r.col - minC) * cw + cw * 0.5f - 2,
                             y + (r.layer - minL + 1) * ch - 3,
                             4, ch - 6, rgba(150, 156, 170, 220));
            else
                render::rect(x + (r.col - minC + 1) * cw - 3,
                             y + (r.layer - minL) * ch + ch * 0.5f - 2,
                             cw - 6, 4, rgba(150, 156, 170, 220));
        }
    }

    // 房间
    for(const world::Room &r : lv.rooms)
    {
        Color c;
        if(r.shop)                 c = rgba(240, 200, 90, 235);
        else if(r.finalRoom)       c = rgba(220, 110, 190, 235);
        else if(r.state == world::ROOM_CLEAR) c = rgba(90, 170, 110, 220);
        else if(r.state == world::ROOM_SAFE)   c = rgba(150, 150, 170, 200);
        else if(r.state == world::ROOM_FIGHT)  c = rgba(220, 90, 80, 240);
        else                       c = rgba(110, 116, 130, 200);
        f32 rx = x + (r.col - minC) * cw, ry = y + (r.layer - minL) * ch;
        render::rect(rx + 3, ry + 3, cw - 6, ch - 6, c);
        render::rect_outline(rx + 3, ry + 3, cw - 6, ch - 6, rgba(16, 18, 24, 190), 1);
    }

    // 当前房间高亮
    if(st().currentRoomIdx >= 0 && st().currentRoomIdx < (i32)lv.rooms.size())
    {
        const world::Room &cr = lv.rooms[(size_t)st().currentRoomIdx];
        render::rect_outline(x + (cr.col - minC) * cw + 2, y + (cr.layer - minL) * ch + 2,
                             cw - 4, ch - 4, rgba(255, 226, 120, 255), 2);
    }

    // 玩家光点（在当前房间格内按实际位置移动）
    if(st().currentRoomIdx >= 0 && st().currentRoomIdx < (i32)lv.rooms.size())
    {
        const world::Room &cr = lv.rooms[(size_t)st().currentRoomIdx];
        f32 relx = clampf((st().pl.pos.x - cr.x0) / world::ROOM_W, 0, 1);
        f32 rely = clampf((st().pl.pos.y - cr.y0) / world::ROOM_H, 0, 1);
        f32 px = x + (cr.col - minC) * cw + 3 + relx * (cw - 9);
        f32 py = y + (cr.layer - minL) * ch + 3 + rely * (ch - 9);
        render::draw(atlas::U_CIRCLE, px - 3, py - 3, 6, 6, rgba(120, 220, 255, 255));
    }
}

void render_hud()
{
    render::begin_ui();
    const Player &p = st().pl;
    char buf[128];

    // ---- 左上：状态条 ----
    f32 bx = 18, by = 16, bw = 250, bh = 20, gap = 6;
    snprintf(buf, sizeof(buf), "%d/%d", (i32)p.hp, (i32)p.hpMax);
    draw_bar(bx, by, bw, bh, p.hp / p.hpMax, atlas::U_BAR_HP, "生命", buf);
    snprintf(buf, sizeof(buf), "%d/%d", (i32)p.armor, (i32)p.armorMax);
    draw_bar(bx, by + bh + gap, bw, bh, p.armor / p.armorMax, atlas::U_BAR_ARMOR, "护甲", buf);
    snprintf(buf, sizeof(buf), "%d/%d", (i32)p.energy, (i32)p.energyMax);
    draw_bar(bx, by + (bh + gap) * 2, bw, bh, p.energy / p.energyMax,
             atlas::U_BAR_ENERGY, "能量", buf);

    // ---- 武器与金币 ----
    f32 wy = by + (bh + gap) * 3 + 6;
    render::draw(atlas::U_SLOT, bx, wy, 44, 44, rgba(255, 255, 255, 235));
    const WeaponDef *wp = weapon_of(p.weapon);
    if(wp->itemId >= 0)
        render::draw(atlas::slot_of_block(wp->itemId), bx + 4, wy + 4, 36, 36);
    render::text(bx + 54, wy + 2, wp->cn, rgba(250, 250, 250, 255), 1.0f);
    if(wp->kind != WK_MELEE)
        snprintf(buf, sizeof(buf), "伤害 %d · 耗能 %d", (int)wp->dmg, wp->energy);
    else
        snprintf(buf, sizeof(buf), "伤害 %d · 范围 %.1f", (int)wp->dmg, (double)wp->range);
    render::text(bx + 54, wy + 24, buf, rgba(180, 190, 210, 255), 0.78f);

    render::draw(atlas::E_COIN, bx, wy + 52, 26, 26);
    snprintf(buf, sizeof(buf), "%d", p.coins);
    render::text(bx + 32, wy + 55, buf, rgba(255, 226, 130, 255), 1.0f);

    // ---- 左上：本局计时与无伤房数（实时）----
    i32 sec = (i32)st().stats.elapsed;
    snprintf(buf, sizeof(buf), "用时 %d:%02d   无伤 %d",
             sec / 60, sec % 60, st().stats.noHitRooms);
    render::text(bx, wy + 86, buf, rgba(170, 180, 200, 220), 0.8f);

    // ---- 顶部中间：关卡进度 ----
    const world::Level &lv = st().level;
    i32 roomTotal = (i32)lv.rooms.size();
    i32 cleared = 0;
    for(const world::Room &r : lv.rooms)
        if(r.state == world::ROOM_CLEAR || r.state == world::ROOM_SAFE) cleared++;
    const world::Room *cur = lv.room_at(p.pos.x, p.pos.y);
    i32 wavesDone = cur ? cur->wavesDone : 0;
    i32 wavesTotal = cur ? cur->wavesTotal : 0;
    i32 aliveLeft = cur ? cur->aliveLeft : 0;

    snprintf(buf, sizeof(buf), "第 %d 关   房间 %d/%d   波次 %d/%d   剩余 %d",
             st().currentLevel, cleared, roomTotal, wavesDone, wavesTotal, aliveLeft);
    render::text_center(platform::width() * 0.5f, 18, buf, rgba(245, 245, 250, 255), 1.0f);

    // ---- 提示消息 ----
    if(st().msgTimer > 0)
    {
        f32 a = clampf(st().msgTimer / 0.6f, 0, 1);
        u8 alpha = (u8)(255 * a);
        f32 w = render::text_w(st().msg, 1.1f) + 40;
        f32 x = platform::width() * 0.5f - w * 0.5f;
        render::draw(atlas::U_PANEL, x, 52, w, 40, rgba(255, 255, 255, alpha));
        render::text_center(platform::width() * 0.5f, 62, st().msg,
                            rgba(255, 240, 200, alpha), 1.1f);
    }

    draw_minimap();

    // ---- 快捷栏（背包前 9 格，1-9 键/滚轮选中，选中槽 = 手持武器）----
    if(st().inGame)
    {
        f32 cell = HOT_CELL, hgap = 6;
        f32 x0 = ((f32)platform::width()
                  - (HOT_SLOTS * cell + (HOT_SLOTS - 1) * hgap)) * 0.5f;
        f32 y0 = (f32)platform::height() - cell - 14;
        for(i32 i = 0; i < HOT_SLOTS; i++)
        {
            f32 x = x0 + i * (cell + hgap);
            bool sel = (i == st().hotSel);
            render::draw(atlas::U_SLOT, x, y0, cell, cell,
                         sel ? rgba(255, 235, 180, 255) : rgba(190, 196, 215, 150));
            const InvSlot &s = st().inv[i];
            if(s.id > 0)
            {
                render::draw(atlas::slot_of_block(s.id), x + 7, y0 + 7,
                             cell - 14, cell - 14);
                if(s.count > 1)
                {
                    snprintf(buf, sizeof(buf), "%d", s.count);
                    render::text(x + cell - 24, y0 + cell - 26, buf,
                                 rgba(255, 255, 255, 240), 0.85f);
                }
            }
            snprintf(buf, sizeof(buf), "%d", i + 1);
            render::text(x + 4, y0 + 3, buf, rgba(150, 156, 175, 200), 0.62f);
            if(sel)
                render::rect_outline(x - 2, y0 - 2, cell + 4, cell + 4,
                                     rgba(255, 226, 120, 255), 2);
        }
        // 操作提示（放在快捷栏上方居中）
        render::text_center(platform::width() * 0.5f, y0 - 24,
                            "左键攻击 · 右键用药 · E 交互（宝箱/商店/扭蛋） · B 背包 · F5/F9 存读",
                            rgba(180, 190, 210, 210), 0.8f);
    }

    // ---- 准星（背包打开时鼠标是 UI 光标，不画准星）----
    if(!st().invOpen)
        render::draw(atlas::U_CURSOR, platform::mouse_x() - 14, platform::mouse_y() - 14,
                     28, 28, rgba(255, 255, 255, 210));

    // ---- 天赋三选一覆盖层 ----
    if(st().talentOpen)
    {
        f32 w = (f32)platform::width(), h = (f32)platform::height();
        render::rect(0, 0, w, h, rgba(8, 6, 14, 200));
        snprintf(buf, sizeof(buf), "第 %d 关 开始 · 选择一项强化", st().currentLevel);
        render::text_center(w * 0.5f, h * 0.14f, buf, rgba(255, 226, 140, 255), 2.0f);

        struct Card { const char *name; const char *desc; i32 slot; };
        Card cards[3] = {
            {"生命上限 +25", "更能抗住围殴",      atlas::E_HEART},
            {"护甲上限 +50", "护甲吸收更多伤害",  atlas::slot_of_block(B_IRON_CHEST)},
            {"能量上限 +50", "枪械弹药更充裕",    atlas::slot_of_block(B_ENERGY_POTION)},
        };
        f32 cw2 = 250, gap2 = 36;
        f32 x0 = (w - cw2 * 3 - gap2 * 2) * 0.5f, y0 = h * 0.26f;
        for(i32 i = 0; i < 3; i++)
        {
            f32 cx = x0 + i * (cw2 + gap2);
            render::draw(atlas::U_PANEL, cx, y0, cw2, 210, rgba(255, 255, 255, 235));
            render::draw(cards[i].slot, cx + cw2 * 0.5f - 40, y0 + 24, 80, 80);
            render::text_center(cx + cw2 * 0.5f, y0 + 122, cards[i].name,
                                rgba(245, 245, 250, 255), 1.35f);
            render::text_center(cx + cw2 * 0.5f, y0 + 162, cards[i].desc,
                                rgba(170, 180, 205, 255), 0.85f);
            snprintf(buf, sizeof(buf), "按 %d 选择", i + 1);
            render::text_center(cx + cw2 * 0.5f, y0 + 184, buf,
                                rgba(255, 226, 140, 255), 0.95f);
        }
        snprintf(buf, sizeof(buf), "本局已选强化 %d 次", st().talentCount);
        render::text_center(w * 0.5f, y0 + 250, buf, rgba(170, 180, 205, 255), 0.95f);
    }
    // ---- 商店覆盖层 ----
    else if(st().shopOpen)
    {
        f32 w = (f32)platform::width(), h = (f32)platform::height();
        f32 pw = 620, ph = 330;
        f32 px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
        render::rect(0, 0, w, h, rgba(8, 6, 14, 170));
        render::draw(atlas::U_PANEL, px, py, pw, ph, rgba(255, 255, 255, 240));
        snprintf(buf, sizeof(buf), "商人 · 金币 %d", p.coins);
        render::text_center(w * 0.5f, py + 22, buf, rgba(255, 226, 140, 255), 1.6f);

        struct Good { const char *name; const char *price; const char *desc; i32 slot; };
        Good goods[3] = {
            {"护甲药水", "15 金币", "护甲 +60（立即生效）", atlas::slot_of_block(B_ARMOR_POTION)},
            {"大治疗药水", "20 金币", "生命完全恢复",       atlas::slot_of_block(B_BIG_HEALTH)},
            {"雇佣兵", "30 金币", "召唤一名随从替你战斗", atlas::E_OI},
        };
        for(i32 i = 0; i < 3; i++)
        {
            f32 gy = py + 68 + i * 64;
            snprintf(buf, sizeof(buf), "[%d]", i + 1);
            render::text(px + 20, gy + 12, buf, rgba(255, 226, 140, 255), 1.2f);
            render::draw(goods[i].slot, px + 64, gy, 48, 48);
            render::text(px + 130, gy + 4, goods[i].name, rgba(245, 245, 250, 255), 1.15f);
            render::text(px + 130, gy + 30, goods[i].desc, rgba(170, 180, 205, 255), 0.85f);
            render::text_right(px + pw - 24, gy + 10, goods[i].price,
                               rgba(255, 226, 130, 255), 1.15f);
        }
        render::text_center(w * 0.5f, py + ph - 34, "[1/2/3] 购买    [E] 离开",
                            rgba(190, 200, 220, 255), 1.0f);
    }
    // ---- 背包覆盖层 ----
    else if(st().invOpen)
    {
        f32 w = (f32)platform::width(), h = (f32)platform::height();
        render::rect(0, 0, w, h, rgba(8, 6, 14, 190));

        snprintf(buf, sizeof(buf), "背包    金币 %d", p.coins);
        render::text_center(w * 0.5f, inv_grid_y0() - 44, buf,
                            rgba(255, 226, 140, 255), 1.6f);

        // 5 列 x 6 行 = 30 格（前 9 格即快捷栏，编号高亮）
        f32 gx = inv_grid_x0(), gy = inv_grid_y0();
        f32 step = INV_CELL + INV_GAP;
        for(i32 r = 0; r < INV_ROWS; r++)
            for(i32 c = 0; c < INV_COLS; c++)
            {
                i32 idx = r * INV_COLS + c;
                f32 x = gx + c * step, y = gy + r * step;
                bool sel = (idx == st().hotSel);
                render::draw(atlas::U_SLOT, x, y, INV_CELL, INV_CELL,
                             sel ? rgba(255, 235, 180, 255) : rgba(190, 196, 215, 165));
                const InvSlot &s = st().inv[idx];
                if(s.id > 0)
                {
                    render::draw(atlas::slot_of_block(s.id), x + 8, y + 8,
                                 INV_CELL - 16, INV_CELL - 16);
                    if(s.count > 1)
                    {
                        snprintf(buf, sizeof(buf), "%d", s.count);
                        render::text(x + INV_CELL - 28, y + INV_CELL - 30, buf,
                                     rgba(255, 255, 255, 240), 0.9f);
                    }
                }
                if(idx < HOT_SLOTS)
                {
                    snprintf(buf, sizeof(buf), "%d", idx + 1);
                    render::text(x + 4, y + 3, buf, rgba(255, 226, 140, 210), 0.62f);
                }
                if(sel)
                    render::rect_outline(x - 2, y - 2, INV_CELL + 4, INV_CELL + 4,
                                         rgba(255, 226, 120, 255), 2);
            }

        render::text_center(w * 0.5f, gy + INV_ROWS * step + 16,
                            "左键拿起/放下 · 右键装备武器或使用药水 · 1-9/滚轮切换选中 · B 关闭",
                            rgba(190, 200, 220, 255), 0.95f);

        // 拿在手上的物品跟随鼠标
        if(st().holding && st().held.id > 0)
        {
            f32 mx = platform::mouse_x(), my = platform::mouse_y();
            render::draw(atlas::slot_of_block(st().held.id), mx - 25, my - 25, 50, 50);
            if(st().held.count > 1)
            {
                snprintf(buf, sizeof(buf), "%d", st().held.count);
                render::text(mx + 2, my + 8, buf, rgba(255, 255, 255, 245), 0.9f);
            }
        }
    }

    // ---- 换关淡入 ----
    if(st().transition > 0.01f)
    {
        u8 a = (u8)(200 * clampf(st().transition, 0, 1));
        render::rect(0, 0, (f32)platform::width(), (f32)platform::height(),
                     rgba(8, 8, 14, a));
    }

    // ---- 死亡 / 暂停遮罩 ----
    if(!st().inGame)
    {
        f32 w = (f32)platform::width(), h = (f32)platform::height();
        render::rect(0, 0, w, h, rgba(8, 6, 12, 170));
        render::text_center(w * 0.5f, h * 0.5f - 50, "你倒下了", rgba(255, 120, 120, 255), 2.0f);
        render::text_center(w * 0.5f, h * 0.5f + 10,
                            "按 R 重新挑战本关    ·    按 Esc 返回标题",
                            rgba(230, 230, 240, 255), 1.1f);
        snprintf(buf, sizeof(buf), "击杀 %d    通过房间 %d    承受伤害 %d",
                 st().stats.kills, st().stats.roomsCleared, st().stats.damageTaken);
        render::text_center(w * 0.5f, h * 0.5f + 46, buf, rgba(190, 200, 220, 255), 0.95f);
        i32 sec = (i32)st().stats.elapsed;
        snprintf(buf, sizeof(buf), "用时 %d:%02d    无伤房间 %d",
                 sec / 60, sec % 60, st().stats.noHitRooms);
        render::text_center(w * 0.5f, h * 0.5f + 74, buf, rgba(190, 200, 220, 255), 0.95f);
    }
    else if(st().paused)
    {
        f32 w = (f32)platform::width(), h = (f32)platform::height();
        render::rect(0, 0, w, h, rgba(8, 6, 12, 150));
        render::text_center(w * 0.5f, h * 0.5f - 46, "已暂停", rgba(240, 240, 250, 255), 2.0f);
        render::text_center(w * 0.5f, h * 0.5f + 8, "Esc  继续游戏",
                            rgba(200, 210, 230, 255), 1.0f);
        render::text_center(w * 0.5f, h * 0.5f + 38, "Q  放弃本局并返回标题",
                            rgba(255, 170, 170, 235), 1.0f);
    }

    render::end();
}

void render_all()
{
    render_world();
    render_hud();
}

}   // namespace game