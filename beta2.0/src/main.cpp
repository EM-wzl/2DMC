// ===================== 2DMC beta2.0 主程序 =====================
// 模块划分：
//   core/     基础类型、数学、随机数
//   platform/ Win32 窗口 + OpenGL 上下文 + 输入 + 计时
//   render/   程序化纹理图集、GDI 中文字体、批渲染与图元
//   world/    方块表、关卡网格与地牢生成
//   entity/   实体池、空间哈希、不可重叠的硬分离
//   game/     玩家、武器、怪物 AI、关卡流程、HUD
#include "platform/platform.h"
#include "render/renderer.h"
#include "render/font.h"
#include "render/atlas.h"
#include "world/tiles.h"
#include "world/world.h"
#include "entity/entity.h"
#include "game/game.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace game;

// ---------------- 标题界面 ----------------
static void render_title(f32 t)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();

    render::begin_ui();

    // 背景：缓慢移动的地牢砖纹
    f32 scroll = t * 12.0f;
    for(i32 y = 0; y < (i32)(h / 64.0f) + 2; y++)
        for(i32 x = 0; x < (i32)(w / 64.0f) + 2; x++)
        {
            f32 px = x * 64.0f - (f32)((i32)scroll % 64);
            f32 py = y * 64.0f - (f32)(((i32)(scroll * 0.6f)) % 64);
            render::draw(atlas::slot_of_block(B_WALL), px, py, 64, 64,
                         rgba(60, 64, 80, 255));
        }
    render::rect(0, 0, w, h, rgba(10, 10, 18, 150));

    // 装饰：几个怪物剪影在下方缓慢漂浮
    for(i32 i = 0; i < 6; i++)
    {
        f32 bx = 120.0f + i * 190.0f;
        f32 by = h - 170.0f + std::sin(t * 1.3f + i) * 14.0f;
        i32 slots[6] = {atlas::E_ZOMBIE, atlas::E_SKELETON, atlas::E_CREEPER,
                        atlas::E_SPIDER, atlas::E_PIGLIN, atlas::E_BOSS};
        f32 s = (i == 5) ? 130.0f : 84.0f;
        render::draw(slots[i], bx - s * 0.5f, by, s, s, rgba(150, 160, 190, 210));
    }

    // 标题
    render::text_center(w * 0.5f, h * 0.20f, "2DMC", rgba(255, 226, 140, 255), 5.0f);
    render::text_center(w * 0.5f, h * 0.20f + 118.0f, "深渊地牢 beta2.0",
                        rgba(230, 236, 250, 255), 1.8f);

    // 开始提示（呼吸闪烁）
    f32 pulse = 0.62f + 0.38f * std::sin(t * 3.4f);
    render::text_center(w * 0.5f, h * 0.56f, "点击鼠标或按 Enter 开始",
                        rgba(150, 230, 180, (u8)(255 * pulse)), 1.5f);
    if(game::has_save())
        render::text_center(w * 0.5f, h * 0.56f + 30,
                            "按 F9 继续上次进度", rgba(255, 226, 140, 235), 1.1f);

    // 操作说明
    f32 ly = h * 0.68f;
    render::text_center(w * 0.5f, ly, "WASD 移动　Shift 冲刺　鼠标瞄准　左键攻击　右键用药",
                        rgba(200, 210, 230, 235), 1.0f);
    render::text_center(w * 0.5f, ly + 26, "B 背包（物品自由排列）　1-9/滚轮 换武器　E 交互（宝箱/商店/扭蛋机）",
                        rgba(200, 210, 230, 235), 1.0f);
    render::text_center(w * 0.5f, ly + 52, "F5 存档　F9 读档　Esc 暂停　R 死亡后满状态重挑本关",
                        rgba(200, 210, 230, 235), 1.0f);

    render::text_right(w - 16, h - 24, "程序化贴图 · OpenGL 渲染 · 无外部素材",
                       rgba(140, 150, 175, 200), 0.75f);
    render::text(16, h - 24, "2DMC beta2.0", rgba(140, 150, 175, 200), 0.75f);
    render::end();
}

// ---------------- 开局选武器 ----------------
static const i32 STARTER_WEAPONS[3] = {B_IRON_SWORD, B_BOW, B_PISTOL};

static void render_weapon_select(f32 t)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();
    render::rect(0, 0, w, h, rgba(10, 10, 18, 255));

    render::text_center(w * 0.5f, h * 0.14f, "选择你的起始武器",
                        rgba(255, 226, 140, 255), 2.2f);
    f32 pulse = 0.62f + 0.38f * std::sin(t * 3.4f);
    render::text_center(w * 0.5f, h * 0.14f + 48, "按 1 / 2 / 3 选择    ·    Esc 返回标题",
                        rgba(200, 210, 230, (u8)(255 * pulse)), 1.1f);

    const f32 cardW = 240.0f, cardH = 300.0f, gap = 40.0f;
    f32 totalW = 3 * cardW + 2 * gap;
    f32 x0 = (w - totalW) * 0.5f, y0 = h * 0.34f;
    char buf[32];
    for(i32 i = 0; i < 3; i++)
    {
        i32 id = STARTER_WEAPONS[i];
        const WeaponDef *wp = weapon_of(id);
        f32 cx = x0 + i * (cardW + gap);

        render::draw(atlas::U_PANEL, cx, y0, cardW, cardH, rgba(40, 44, 60, 235));
        render::rect_outline(cx, y0, cardW, cardH, rgba(150, 160, 190, 200), 2);
        render::draw(atlas::slot_of_block(id), cx + cardW * 0.5f - 56, y0 + 34, 112, 112);

        snprintf(buf, sizeof(buf), "%d", i + 1);
        render::text(cx + 12, y0 + 10, buf, rgba(255, 226, 140, 235), 1.2f);
        render::text_center(cx + cardW * 0.5f, y0 + 168, wp->cn, rgba(250, 250, 250, 255), 1.6f);

        const char *desc = wp->kind == WK_MELEE ? "近战挥砍 · 无消耗"
                         : (wp->kind == WK_BOW ? "远程蓄力 · 射程远" : "远程连射 · 耗能量");
        render::text_center(cx + cardW * 0.5f, y0 + 208, desc, rgba(190, 200, 220, 235), 0.95f);
        snprintf(buf, sizeof(buf), "伤害 %.0f", (double)wp->dmg);
        render::text_center(cx + cardW * 0.5f, y0 + 238, buf, rgba(255, 180, 150, 235), 1.0f);
    }
    render::end();
}

static int game_main()
{
    if(!platform::init("2DMC beta2.0 - 深渊地牢", 1280, 720))
    {
        platform::log("平台初始化失败");
        return 1;
    }
    tiles_init();
    if(!render::init())
    {
        platform::log("渲染初始化失败");
        return 1;
    }
    if(platform::arg_value("novsync")) platform::set_vsync(false);
    g_rng.seed((u64)(platform::now_seconds() * 1000000.0));
    game::init();

    // 自动化验证：--shot=xxx.bmp 截图后退出；--autostart 跳过标题
    const char *shotPath = platform::arg_value("shot");
    i32 shotFrame = 90;
    if(const char *sf = platform::arg_value("shotframe")) shotFrame = atoi(sf);
    i32 maxFrames = 0;
    if(const char *mf = platform::arg_value("frames")) maxFrames = atoi(mf);
    if(platform::arg_value("resume"))
    {
        game::st().screen = 1;
        game::load_run();
    }
    if(platform::arg_value("autostart"))
    {
        game::st().screen = 1;
        game::new_run();
        bool debugWarp = false;
        if(const char *rm = platform::arg_value("room")) { game::warp_to_room(atoi(rm)); debugWarp = true; }
        if(platform::arg_value("shop"))
        {
            for(size_t i = 0; i < game::st().level.rooms.size(); i++)
                if(game::st().level.rooms[i].shop)
                {
                    game::warp_to_room((i32)i);
                    debugWarp = true;
                    break;
                }
        }
        if(const char *cd = platform::arg_value("crowd")) { game::debug_spawn_crowd(atoi(cd)); debugWarp = true; }
        if(platform::arg_value("merc")) { game::hire_merc(); debugWarp = true; }
        if(const char *sw = platform::arg_value("swap")) { game::debug_swap_weapon(atoi(sw)); debugWarp = true; }
        if(const char *gv = platform::arg_value("give")) { game::debug_give_weapon(atoi(gv)); debugWarp = true; }
        if(const char *its = platform::arg_value("item"))
        {
            // --item=<id>[:数量][,<id>[:数量]...] 直接入包（验证叠加/背包 UI）
            char tmp[160];
            strncpy(tmp, its, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = 0;
            char *tok = strtok(tmp, ",");
            while(tok)
            {
                i32 id = atoi(tok);
                i32 n = 1;
                if(const char *colon = strchr(tok, ':')) n = atoi(colon + 1);
                if(id > 0 && n > 0) game::debug_add_item(id, n);
                tok = strtok(nullptr, ",");
            }
            debugWarp = true;
        }
        if(platform::arg_value("bag")) { game::st().invOpen = true; debugWarp = true; }
        if(debugWarp) game::st().talentOpen = false;   // 自动化测试跳过天赋界面
    }
    // --bag 对 --resume 同样生效（读档路径在上面已执行完）
    if(platform::arg_value("bag")) game::st().invOpen = true;
    // --select：直接停在开局选武器界面（截图验证用）
    if(platform::arg_value("select")) game::st().screen = 2;
    // --pause：开局即暂停（截图验证暂停菜单用）
    if(platform::arg_value("pause")) { game::st().paused = true; game::st().talentOpen = false; }
    i32 saveAt = 0;
    if(const char *sa = platform::arg_value("saveat")) saveAt = atoi(sa);

    f32 t = 0;
    i32 frame = 0;
    while(platform::pump())
    {
        f32 dt = platform::dt();
        t += dt;
        frame++;

        // ---- 全局按键 ----
        if(platform::key_pressed(platform::K_ESC))
        {
            if(game::st().shopOpen) game::st().shopOpen = false;
            else if(game::st().invOpen) game::inv_close();
            else if(game::st().inGame) game::st().paused = !game::st().paused;
            else if(game::st().screen == 1) game::st().screen = 0;
        }
        // 暂停时按 Q 放弃本局（返回标题）
        if(game::st().paused && platform::key_pressed(platform::K_Q))
            game::abandon_run();
        // 选武器界面按 Esc 返回标题
        if(game::st().screen == 2 && platform::key_pressed(platform::K_ESC))
            game::st().screen = 0;
        // B / Tab 开关背包（天赋、商店打开时不开，避免按键冲突）
        if(platform::key_pressed(platform::K_B) || platform::key_pressed(platform::K_TAB))
        {
            if(game::st().invOpen) game::inv_close();
            else if(game::st().inGame && !game::st().talentOpen && !game::st().shopOpen)
                game::st().invOpen = true;
        }
        if(platform::key_pressed(platform::K_R) && !game::st().inGame
                && game::st().screen == 1)
            game::restart_level();
        // F5 存档 / F9 读档（局内进度：关卡、血量护甲蓝量、金币、武器）
        if(platform::key_pressed(platform::K_F5) && game::st().inGame)
            game::save_run();
        if(platform::key_pressed(platform::K_F9))
            game::load_run();

        // 标题 -> 选武器
        if(game::st().screen == 0)
        {
            if(platform::key_pressed(platform::K_ENTER) ||
                    platform::mouse_pressed(platform::MB_LEFT))
                game::st().screen = 2;
        }
        // ---- 选武器 -> 开局 ----
        if(game::st().screen == 2)
        {
            i32 pick = 0;
            for(i32 i = 0; i < 3; i++)
                if(platform::key_pressed(platform::K_1 + i)) pick = STARTER_WEAPONS[i];
            if(pick > 0)
            {
                game::new_run();
                game::st().inv[0] = {pick, 1};   // 用所选武器替换默认起始铁剑
                game::inv_select(0);
                game::st().screen = 1;
            }
        }

        // ---- 覆盖层按键（天赋/商店/背包打开时世界暂停）----
        bool overlay = game::st().talentOpen || game::st().shopOpen
                     || game::st().invOpen;
        if(game::st().talentOpen)
        {
            if(platform::key_pressed(platform::K_1)) game::apply_talent(1);
            else if(platform::key_pressed(platform::K_2)) game::apply_talent(2);
            else if(platform::key_pressed(platform::K_3)) game::apply_talent(3);
        }
        else if(game::st().shopOpen)
        {
            if(platform::key_pressed(platform::K_1)) game::shop_buy(1);
            else if(platform::key_pressed(platform::K_2)) game::shop_buy(2);
            else if(platform::key_pressed(platform::K_3)) game::shop_buy(3);
            else if(platform::key_pressed(platform::K_E)) game::st().shopOpen = false;
        }
        else if(game::st().invOpen)
        {
            // 背包内：鼠标左键拿起/放下，右键装备/使用
            if(platform::mouse_pressed(platform::MB_LEFT)) game::inv_click(0);
            else if(platform::mouse_pressed(platform::MB_RIGHT)) game::inv_click(1);
        }

        // ---- 快捷栏选择（游戏中或背包打开时都有效；天赋/商店占用 1/2/3 故排除）----
        if(game::st().inGame && !game::st().talentOpen && !game::st().shopOpen)
        {
            for(i32 k = 0; k < game::HOT_SLOTS; k++)
                if(platform::key_pressed(platform::K_1 + k)) game::inv_select(k);
            f32 wh = platform::wheel();
            if(wh != 0) game::inv_wheel(wh);
        }

        // ---- 更新 ----
        if(game::st().screen == 1 && game::st().inGame && !game::st().paused && !overlay)
            game::update(dt);

        if(saveAt > 0 && frame == saveAt && game::st().inGame) game::save_run();

        // ---- 渲染 ----
        render::begin_frame(rgba(12, 12, 18, 255));
        if(game::st().screen == 0)
            render_title(t);
        else if(game::st().screen == 2)
            render_weapon_select(t);
        else
            game::render_all();

        if(shotPath && frame == shotFrame)
        {
            bool ok = render::capture_bmp(shotPath);
            platform::log("截图 %s -> %s (draw calls=%d)", ok ? "成功" : "失败",
                          shotPath, render::last_draw_calls());
        }

        platform::present();
        platform::end_frame();

        if(shotPath && frame >= shotFrame) break;
        if(maxFrames > 0 && frame >= maxFrames) break;

        // 帧率限制交给 vsync；这里只做标题栏更新
        static f32 titleAcc = 0;
        titleAcc += dt;
        if(titleAcc > 0.5f)
        {
            titleAcc = 0;
            char tb[128];
            snprintf(tb, sizeof(tb), "2DMC beta2.0 - 深渊地牢   %.0f FPS   draw calls %d",
                     1.0f / (dt > 0.0001f ? dt : 0.016f), render::last_draw_calls());
            platform::set_title(tb);
        }
    }

    platform::log("退出：%d 帧 / %.2f 秒 = %.1f FPS，draw calls=%d",
                  frame, (double)t, (double)(frame / (t > 0.001f ? t : 1.0f)),
                  render::last_draw_calls());
    render::shutdown();
    platform::shutdown();
    return 0;
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return game_main(); }
#else
int main() { return game_main(); }
#endif