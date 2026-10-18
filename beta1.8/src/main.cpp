// ===================== 2DMC beta1.8 主程序 =====================
// 模块划分：
//   core/     基础类型、数学、随机数
//   platform/ Win32 窗口 + OpenGL 上下文 + 输入 + 计时
//   render/   程序化纹理图集、GDI 中文字体、批渲染与图元
//   world/    101 项方块表、10000x1001 世界生成、光照、流体
//   entity/   实体池、空间哈希、13 种 AI
//   item/     背包 / 护甲 / 箱子 / 耐久度 / 合成
//   game/     玩家、挖掘、放置、UI、存档
#include "platform/platform.h"
#include "render/renderer.h"
#include "render/font.h"
#include "render/atlas.h"
#include "world/blocks.h"
#include "world/world.h"
#include "item/item.h"
#include "item/chest.h"
#include "game/game.h"
#include "game/game_render.h"
#include "entity/entity.h"
#include "mods/mod_loader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

// platform 层的字符消息钩子（platform_win32.cpp 实现）
namespace platform {
void set_char_hook(void (*fn)(u32));
}

// ============================================================================
//                                存档列表
// ============================================================================
namespace {

char g_saves[64][56];
i32  g_save_count = 0;

void read_savelist()
{
    g_save_count = 0;
    for(i32 i = 0; i < 64; i++) g_saves[i][0] = 0;
    FILE *fp = fopen("savelist.txt", "r");
    if(!fp) return;
    i32 n = 0;
    if(fscanf(fp, "%d", &n) != 1 || n <= 0) { fclose(fp); return; }
    if(n > 64) n = 64;
    for(i32 i = 0; i < n; i++)
    {
        if(fscanf(fp, "%55s", g_saves[i]) != 1) break;
        g_save_count++;
    }
    fclose(fp);
}

void write_savelist()
{
    FILE *fp = fopen("savelist.txt", "w");
    if(!fp) return;
    if(g_save_count <= 0) { fprintf(fp, "0\n"); fclose(fp); return; }
    fprintf(fp, "%d ", g_save_count);
    for(i32 i = 0; i < g_save_count; i++) fprintf(fp, "%s ", g_saves[i]);
    fprintf(fp, "\n");
    fclose(fp);
}

void add_savelist(const char *name)
{
    read_savelist();
    for(i32 i = 0; i < g_save_count; i++)
        if(strcmp(g_saves[i], name) == 0) return;
    if(g_save_count >= 64) return;
    snprintf(g_saves[g_save_count], 56, "%s", name);
    g_save_count++;
    write_savelist();
}

void remove_savelist(const char *name)
{
    read_savelist();
    i32 w = 0;
    for(i32 i = 0; i < g_save_count; i++)
        if(strcmp(g_saves[i], name) != 0)
            snprintf(g_saves[w++], 56, "%s", g_saves[i]);
    g_save_count = w;
    write_savelist();
}

}   // namespace

// ============================================================================
//                                标题界面
// ============================================================================
namespace {

enum Screen { SCR_TITLE = 0, SCR_WORLDS, SCR_NEWNAME, SCR_MODS, SCR_GAME };

struct UiButton
{
    std::string label;
    f32 x, y, w, h;
    i32 id;
};

std::vector<UiButton> g_btns;

bool button_hot(const UiButton &b, f32 mx, f32 my)
{
    return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
}

void push_button(const char *label, f32 x, f32 y, f32 w, i32 id)
{
    g_btns.push_back({label, x, y, w, 42.0f, id});
}

void draw_buttons()
{
    f32 mx = platform::mouse_x(), my = platform::mouse_y();
    for(const UiButton &b : g_btns)
    {
        bool hot = button_hot(b, mx, my);
        render::draw(hot ? atlas::U_BTN_HL : atlas::U_BTN, b.x, b.y, b.w, b.h);
        render::text_center_shadow(b.x + b.w * 0.5f, b.y + 10, b.label.c_str(),
                                   hot ? rgba(255, 240, 200) : rgba(226, 232, 246),
                                   1.05f);
    }
}

// 标题背景：视差滚动山峦 + 方块地面 + 怪物剪影
void draw_title_bg(f32 t)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();
    render::rect_lit(0, 0, w, h, rgba(72, 106, 168), rgba(72, 106, 168),
                     rgba(196, 214, 236), rgba(196, 214, 236));

    f32 scroll = t * 10.0f;
    for(i32 layer = 0; layer < 3; layer++)
    {
        f32 depth = 1.0f + layer * 0.6f;
        f32 base = h * (0.60f + layer * 0.11f);
        u32 col = rgba((u8)(58 - layer * 10), (u8)(80 - layer * 12),
                       (u8)(110 - layer * 14));
        for(i32 x = -64; x < (i32)w + 64; x += 32)
        {
            f32 hh = 46.0f + std::sin(x * 0.02f + layer * 2.0f) * 22.0f +
                     std::sin(x * 0.005f + layer) * 34.0f;
            f32 off = std::fmod(scroll * depth * 0.4f, 32.0f);
            render::rect((f32)x - off, base - hh, 32, hh + 140, col);
        }
    }
    // 地面
    render::rect(0, h * 0.84f, w, h * 0.16f, rgba(78, 62, 44));
    for(i32 x = 0; x < (i32)w; x += 32)
        render::draw(atlas::slot_of_block(B_DIRT), (f32)x, h * 0.84f, 32, 32,
                     rgba(150, 130, 100));

    {
        i32 slots[5] = {atlas::E_ZOMBIE, atlas::E_SKELETON, atlas::E_CREEPER,
                        atlas::E_SPIDER, atlas::E_CREEPER};
        for(i32 i = 0; i < 5; i++)
        {
            f32 bx = 60.0f + i * 250.0f + std::sin(t * 1.2f + i * 1.7f) * 12.0f;
            f32 by = h * 0.84f - 60.0f + std::sin(t * 2.1f + i) * 4.0f;
            // 怪物槽位里是 16x32 立绘（存左半边），取左半边按 1:2 画剪影
            render::draw_sub(slots[i], bx, by, 32, 64, 0.0f, 0.0f, 0.5f, 1.0f,
                             rgba(34, 30, 40, 225));
        }
    }
    render::rect(0, 0, w, h, rgba(10, 10, 20, 80));
}

void draw_screen_title(f32 t)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    draw_title_bg(t);
    render::begin_ui();

    f32 pulse = 0.72f + 0.28f * std::sin(t * 2.4f);
    render::text_center_shadow(w * 0.5f, h * 0.12f, "2DMC", rgba(255, 226, 140), 4.4f);
    render::text_center_shadow(w * 0.5f, h * 0.12f + 84, "beta1.8 · OpenGL",
                               rgba(220, 230, 250, (u8)(200 * pulse)), 1.5f);

    g_btns.clear();
    f32 bx = w * 0.5f - 130, by = h * 0.34f;
    push_button("开始游戏 / 世界列表", bx, by, 260, 1);
    {
        char mb[64];
        snprintf(mb, sizeof(mb), "模组 (%d)", mods::count());
        push_button(mb, bx, by + 56, 260, 5);
    }
    push_button("退出游戏", bx, by + 112, 260, 2);
    draw_buttons();

    render::text_center_shadow(w * 0.5f, h - 86,
                               "WASD 移动 · 空格跳跃 · Shift 潜行 · 左键挖/攻击 · 右键放置/使用",
                               rgba(225, 232, 245), 0.95f);
    render::text_center_shadow(w * 0.5f, h - 62,
                               "E 物品栏 · C 合成 · M 地图 · Esc 暂停/设置 · T 命令 · 1-9 切换手持",
                               rgba(225, 232, 245), 0.95f);
    render::text_center_shadow(w * 0.5f, h - 36,
                               "程序化贴图 · 无外部素材 · 存档 v4（兼容 v1/v2/v3 旧档）",
                               rgba(170, 180, 200), 0.8f);
    render::end();
}

void draw_screen_worlds(f32 t)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    draw_title_bg(t * 0.4f);
    render::begin_ui();
    render::panel(w * 0.5f - 340, h * 0.5f - 240, 680, 480,
                  rgba(26, 22, 34, 240), rgba(150, 130, 96, 220), 12, 2);
    render::draw(atlas::U_TITLE_BAR, w * 0.5f - 338, h * 0.5f - 238, 676, 36);
    render::text_center_shadow(w * 0.5f, h * 0.5f - 230, "选择世界",
                               rgba(255, 226, 140), 1.2f);

    g_btns.clear();
    f32 lx = w * 0.5f - 310, ly = h * 0.5f - 175;
    if(g_save_count == 0)
        render::text_center_shadow(w * 0.5f, ly + 24, "还没有存档，点下面新建一个",
                                   rgba(210, 216, 232), 1.0f);
    for(i32 i = 0; i < g_save_count && i < 8; i++)
    {
        f32 by = ly + i * 46;
        push_button(g_saves[i], lx, by, 420, 100 + i);
        push_button("删除", lx + 430, by, 86, 200 + i);
    }
    push_button("新建世界", w * 0.5f - 310, h * 0.5f + 190, 190, 3);
    push_button("返回", w * 0.5f + 120, h * 0.5f + 190, 190, 4);
    draw_buttons();
    render::end();
}

void draw_screen_newname(f32 t, const std::string &name)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    draw_title_bg(t * 0.4f);
    render::begin_ui();
    render::panel(w * 0.5f - 260, h * 0.5f - 140, 520, 280,
                  rgba(26, 22, 34, 240), rgba(150, 130, 96, 220), 12, 2);
    render::draw(atlas::U_TITLE_BAR, w * 0.5f - 258, h * 0.5f - 138, 516, 36);
    render::text_center_shadow(w * 0.5f, h * 0.5f - 130, "新建世界",
                               rgba(255, 226, 140), 1.2f);
    render::text_center_shadow(w * 0.5f, h * 0.5f - 76,
                               "存档名（小写字母与数字，回车确认，Esc 取消）",
                               rgba(210, 216, 232), 0.9f);
    render::panel(w * 0.5f - 200, h * 0.5f - 42, 400, 46, rgba(12, 12, 18, 235),
                  rgba(140, 122, 92, 220), 6, 2);
    f32 blink = 0.5f + 0.5f * std::sin(t * 8.0f);
    render::text(w * 0.5f - 188, h * 0.5f - 32, name.c_str(), rgba(235, 244, 255), 1.15f);
    f32 tw = render::text_w(name.c_str(), 1.15f);
    render::rect(w * 0.5f - 188 + tw + 2, h * 0.5f - 34, 2, 26,
                 rgba(255, 255, 255, (u8)(120 + 135 * blink)));
    render::text_center_shadow(w * 0.5f, h * 0.5f + 28,
                               "生成 10000x1001 世界约需 1~3 秒",
                               rgba(180, 190, 210), 0.85f);
    render::end();
}

void draw_screen_mods(f32 t)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    draw_title_bg(t * 0.4f);
    render::begin_ui();
    render::panel(w * 0.5f - 380, h * 0.5f - 250, 760, 500,
                  rgba(26, 22, 34, 240), rgba(150, 130, 96, 220), 12, 2);
    render::draw(atlas::U_TITLE_BAR, w * 0.5f - 378, h * 0.5f - 248, 756, 36);
    render::text_center_shadow(w * 0.5f, h * 0.5f - 240, "模组",
                               rgba(255, 226, 140), 1.2f);

    g_btns.clear();
    f32 lx = w * 0.5f - 348, ly = h * 0.5f - 185;
    i32 n = mods::count();
    if(n == 0)
    {
        render::text_center_shadow(w * 0.5f, ly + 30, "没有检测到模组",
                                   rgba(210, 216, 232), 1.05f);
        render::text_center_shadow(w * 0.5f, ly + 62,
                                   "把编译好的 .dll 放进 mods/ 目录后重新启动游戏",
                                   rgba(180, 190, 210), 0.9f);
    }
    for(i32 i = 0; i < n && i < 8; i++)
    {
        mods::ModInfo *m = mods::at(i);
        if(!m) continue;
        f32 by = ly + i * 46;
        push_button(m->enabled ? "已启用" : "已禁用", lx, by, 120, 300 + i);
        render::text(lx + 140, by + 4, m->name,
                     m->enabled ? rgba(200, 245, 205) : rgba(200, 205, 218), 1.0f);
        render::text(lx + 140, by + 26, m->desc[0] ? m->desc : m->file,
                     rgba(160, 168, 186), 0.75f);
        if(!m->valid)
            render::text_right(lx + 736, by + 12, "ABI 不兼容", rgba(235, 120, 110), 0.8f);
    }
    push_button("返回", w * 0.5f - 95, h * 0.5f + 200, 190, 4);
    draw_buttons();
    render::text_center_shadow(w * 0.5f, h * 0.5f + 168,
                               "点击一行切换启用/禁用 · 修改自动保存到 mods.cfg",
                               rgba(200, 208, 226), 0.85f);
    render::end();
}

// ============================================================================
//                                指令
// ============================================================================
void exec_command(const std::string &cmd)
{
    using namespace game;
    char name[64] = {0};
    char a1[64] = {0};
    i32 num = 0;
    i32 got = sscanf(cmd.c_str(), "%63s %63s %d", name, a1, &num);
    if(got <= 0) return;
    const char *cn = (name[0] == '/') ? name + 1 : name;
    Player &p = st().pl;

    if(strcmp(cn, "help") == 0)
        toast("/help /give <物品> <数量> /heal /life <n> /kill /gamemode "
              "/clear /time <n> /tp <x> <y> /summon <名字> [数量]");
    else if(strcmp(cn, "heal") == 0) { p.life = 20; toast("生命已恢复"); }
    else if(strcmp(cn, "life") == 0)
    {
        if(num >= 0 && num <= 20) { p.life = num; toast("生命已设置为 %d", num); }
        else toast("错误：生命必须在 0-20 之间");
    }
    else if(strcmp(cn, "kill") == 0) { start_respawn(); toast("你死了，已重生"); }
    else if(strcmp(cn, "gamemode") == 0)
    {
        world::gamemode = 1 - world::gamemode;
        toast("游戏模式已切换为 %s", world::gamemode == 1 ? "生存" : "创造");
    }
    else if(strcmp(cn, "clear") == 0)
    {
        item::inv_clear();
        armor_update();
        toast("背包已清空");
    }
    else if(strcmp(cn, "give") == 0)
    {
        if(!st().CanGet) { toast("作弊模式已关闭（到设置里打开）"); return; }
        i32 id = -1;
        if(!block_by_name(a1, id)) { toast("错误：未知物品 %s", a1); return; }
        i32 n = (num > 0 && num <= 64) ? num : 1;
        i32 put = item::inv_add(id, n);
        if(put > 0) toast("已获得 %d 个 %s", put, block_of(id).cn);
        else toast("错误：背包已满");
    }
    else if(strcmp(cn, "time") == 0)
    {
        if(num >= 0 && num <= 40000)
        {
            world::now_time = num;
            world::night = (num >= 20000) ? world::NIGHT_NIGHT
                          : (num >= 18000) ? world::NIGHT_DUSK
                                           : world::NIGHT_NOON;
            world::set_light();
            toast("时间已设置为 %d", num);
        }
        else toast("错误：时间必须在 0-40000 之间");
    }
    else if(strcmp(cn, "tp") == 0)
    {
        i32 h2 = 0;
        if(sscanf(cmd.c_str(), "%*s %d %d", &num, &h2) == 2)
        {
            i32 tx = num + world::W / 2;
            i32 ty = world::H - 1 - h2;
            if(tx >= 0 && tx < world::W && ty >= 0 && ty < world::H)
            {
                p.x = tx;
                p.y = ty;
                refresh_hud();
                toast("已传送到 (%d, %d)", num, h2);
            }
            else toast("错误：位置无效");
        }
        else toast("用法：/tp <x> <y>");
    }
    else if(strcmp(cn, "summon") == 0)
    {
        if(!st().CanGet) { toast("作弊模式已关闭"); return; }
        i32 typ = -1;
        if(strcmp(a1, "zombie") == 0 || strcmp(a1, "僵尸") == 0) typ = entity::T_ZOMBIE;
        else if(strcmp(a1, "skeleton") == 0 || strcmp(a1, "骷髅") == 0) typ = entity::T_SKELETON;
        else if(strcmp(a1, "creeper") == 0 || strcmp(a1, "苦力怕") == 0) typ = entity::T_CREEPER;
        else if(strcmp(a1, "spider") == 0 || strcmp(a1, "蜘蛛") == 0) typ = entity::T_SPIDER;
        else if(strcmp(a1, "pigman") == 0 || strcmp(a1, "僵尸猪灵") == 0) typ = entity::T_PIGLIN;
        else if(strcmp(a1, "sheep") == 0 || strcmp(a1, "羊") == 0) typ = entity::T_SHEEP;
        else if(strcmp(a1, "pig") == 0 || strcmp(a1, "猪") == 0) typ = entity::T_PIG;
        else if(strcmp(a1, "wither") == 0 || strcmp(a1, "凋灵") == 0) typ = entity::T_OI;
        else if(strcmp(a1, "worm") == 0 || strcmp(a1, "世界吞噬者") == 0) typ = -2;
        if(typ == -1) { toast("错误：未知实体 %s", a1); return; }
        if(typ == -2)
        {
            entity::spawn_eow(p.x, p.y);
            toast("已召唤世界吞噬者");
            return;
        }
        i32 cnt = (num > 0 && num <= 20) ? num : 1;
        i32 made = 0;
        for(i32 k = 0; k < cnt; k++)
        {
            f64 tx = (f64)((i32)p.x + (world::rnd(11) - 5));
            f64 ty = (f64)((i32)p.y - (world::rnd(5) + 2));
            if(tx < 1 || ty < 1 || tx >= world::W - 1 || ty >= world::H - 1) continue;
            i32 hp = 20, hurt = 4;
            if(typ == entity::T_OI) { hp = 500; hurt = 8; }
            else if(typ == entity::T_CREEPER) hurt = 50;
            else if(typ == entity::T_SPIDER) { hp = 16; hurt = 3; }
            else if(typ == entity::T_PIGLIN) hurt = 6;
            else if(typ == entity::T_SHEEP || typ == entity::T_PIG) { hp = 8; hurt = 0; }
            entity::spawn(typ, hp, hurt, tx, ty);
            made++;
        }
        toast("已召唤 %d 个 %s", made, a1);
    }
    else toast("未知命令：%s（输入 /help 查看帮助）", cn);
}

void on_char(u32 cp)
{
    using namespace game;
    if(st().ui == UI_COMMAND) command_push(cp);
}

}   // namespace

// ============================================================================
//                                主循环
// ============================================================================
static int app_main()
{
    if(!platform::init("2DMC beta1.8 - OpenGL", 1280, 720))
    {
        platform::log("平台初始化失败");
        return 1;
    }
    blocks_init();
    if(!render::init())
    {
        platform::log("渲染初始化失败");
        return 1;
    }
    item::init();
    game::init();
    read_savelist();
    mods::scan();              // 扫描 mods/*.dll
    mods::load_selection();    // 读回上次勾选的模组

    i32 shotFrame = 60;
    if(const char *sf = platform::arg_value("shotframe")) shotFrame = atoi(sf);
    const char *shotPath = platform::arg_value("shot");
    i32 maxFrames = 0;
    if(const char *mf = platform::arg_value("frames")) maxFrames = atoi(mf);
    bool autoWorld = platform::arg_value("world") != nullptr;
    const bool traceMode = platform::arg_value("trace") != nullptr;
    const bool drownMode = platform::arg_value("drown") != nullptr;
    const bool drownFast = platform::arg_value("drownfast") != nullptr;

    i32 screen = SCR_TITLE;
    if(autoWorld) screen = SCR_GAME;
    if(platform::arg_value("mods")) screen = SCR_MODS;   // 截图回归：直接开模组页
    if(traceMode) game::st().special_effect = game::EASE_ON;
    std::string newName;
    f32 t = 0;
    i32 frame = 0;

    if(autoWorld)
    {
        const char *nm = platform::arg_value("name");
        mods::load_enabled();
        game::new_world(nm ? nm : "autotest");
        mods::world_ready();
        platform::log("自动测试世界已生成: 玩家 %.1f,%.1f  相机 %.1f,%.1f  维度 %d",
                      game::st().pl.x, game::st().pl.y, game::st().pl.jx,
                      game::st().pl.jy, world::dim);
    }
    // --load=<名字> 直接读档进游戏（存档往返回归用）
    if(const char *lv = platform::arg_value("load"))
    {
        screen = SCR_GAME;
        mods::load_enabled();
        if(game::load_world(lv))
        {
            mods::world_ready();
            platform::log("读档成功: %s  玩家 %.1f,%.1f  维度 %d  光照中心 %d,%d",
                          lv, game::st().pl.x, game::st().pl.y, world::dim,
                          world::g_light_cx, world::g_light_cy);
            game::refresh_hud();
            for(i32 i = 1; i <= 5; i++)
                if(item::beibao[i].count > 0)
                    platform::log("  背包[%d] %s x%d 耐久 %d/%d", i,
                                  block_of(item::beibao[i].id).cn,
                                  (int)item::beibao[i].count,
                                  item::beibao[i].remain(),
                                  item::beibao[i].max_dur());
        }
        else platform::log("读档失败: %s", lv);
    }
    // --ui=inv/chest/craft/settings/map 打开指定面板（截图回归用）
    if(const char *uv = platform::arg_value("ui"))
    {
        if(strcmp(uv, "inv") == 0) game::ui_open(game::UI_INV);
        else if(strcmp(uv, "craft") == 0) game::ui_open(game::UI_CRAFT);
        else if(strcmp(uv, "settings") == 0) game::ui_open(game::UI_SETTINGS);
        else if(strcmp(uv, "map") == 0) game::ui_open(game::UI_MAP);
        else if(strcmp(uv, "repair") == 0) { game::ui_open(game::UI_CRAFT); game::st().page = 8; }
    }

    while(platform::pump())
    {
        f32 dt = platform::dt();
        t += dt;
        frame++;
        f32 mx = platform::mouse_x(), my = platform::mouse_y();

        if(screen == SCR_TITLE)
        {
            if(platform::key_pressed(platform::K_ESC)) break;
            if(platform::mouse_pressed(platform::MB_LEFT))
                for(const UiButton &b : g_btns)
                    if(button_hot(b, mx, my))
                    {
                        if(b.id == 1) { read_savelist(); screen = SCR_WORLDS; }
                        else if(b.id == 5) screen = SCR_MODS;
                        else if(b.id == 2)
                        {
                            game::shutdown();
                            render::shutdown();
                            platform::shutdown();
                            return 0;
                        }
                    }
            render::begin_frame(rgba(12, 12, 20, 255));
            draw_screen_title(t);
        }
        else if(screen == SCR_WORLDS)
        {
            if(platform::key_pressed(platform::K_ESC)) screen = SCR_TITLE;
            if(platform::mouse_pressed(platform::MB_LEFT))
                for(const UiButton &b : g_btns)
                    if(button_hot(b, mx, my))
                    {
                        if(b.id >= 100 && b.id < 200)
                        {
                            mods::load_enabled();
                            if(game::load_world(g_saves[b.id - 100]))
                            {
                                mods::world_ready();
                                game::refresh_hud();
                                screen = SCR_GAME;
                            }
                            else game::toast("存档损坏或不存在，无法加载");
                        }
                        else if(b.id >= 200 && b.id < 300)
                        {
                            remove_savelist(g_saves[b.id - 200]);
                            read_savelist();
                        }
                        else if(b.id == 3) { newName.clear(); screen = SCR_NEWNAME; }
                        else if(b.id == 4) screen = SCR_TITLE;
                    }
            render::begin_frame(rgba(12, 12, 20, 255));
            draw_screen_worlds(t);
        }
        else if(screen == SCR_NEWNAME)
        {
            if(platform::key_pressed(platform::K_ESC))
            {
                platform::set_char_hook(nullptr);
                screen = SCR_WORLDS;
            }
            else
            {
                if(platform::key_pressed(0x08) && !newName.empty()) newName.pop_back();
                if(platform::key_pressed(platform::K_ENTER) && !newName.empty())
                {
                    platform::set_char_hook(nullptr);
                    mods::load_enabled();
                    game::new_world(newName.c_str());
                    mods::world_ready();
                    add_savelist(newName.c_str());
                    read_savelist();
                    game::refresh_hud();
                    screen = SCR_GAME;
                }
                for(i32 k = platform::K_A; k <= platform::K_Z; k++)
                    if(platform::key_pressed(k) && newName.size() < 48)
                        newName.push_back((char)('a' + (k - platform::K_A)));
                for(i32 k = platform::K_0; k <= platform::K_9; k++)
                    if(platform::key_pressed(k) && newName.size() < 48)
                        newName.push_back((char)('0' + (k - platform::K_0)));
            }
            render::begin_frame(rgba(12, 12, 20, 255));
            draw_screen_newname(t, newName);
        }
        else if(screen == SCR_MODS)
        {
            if(platform::key_pressed(platform::K_ESC)) screen = SCR_TITLE;
            if(platform::mouse_pressed(platform::MB_LEFT))
                for(const UiButton &b : g_btns)
                    if(button_hot(b, mx, my))
                    {
                        if(b.id >= 300 && b.id < 300 + mods::MAX_MODS)
                        {
                            i32 idx = b.id - 300;
                            mods::ModInfo *mi = mods::at(idx);
                            if(mi)
                            {
                                mods::set_enabled(idx, !mi->enabled);
                                mods::save_selection();
                            }
                        }
                        else if(b.id == 4) screen = SCR_TITLE;
                    }
            render::begin_frame(rgba(12, 12, 20, 255));
            draw_screen_mods(t);
        }
        else
        {
            using namespace game;
            // Esc：命令输入 -> 取消；其它面板 -> 关闭；游戏中 -> 暂停/设置。
            // 注意：Esc 只在这里处理一次，下面派发给 ui_key 时要跳过它，
            // 否则「开面板」和「关面板」会在同一帧各执行一次，表现为 Esc 无效。
            if(platform::key_pressed(platform::K_ESC))
            {
                if(st().ui == UI_COMMAND) { platform::set_char_hook(nullptr); ui_close(); }
                else if(ui_is_open()) ui_close();
                else ui_open(UI_SETTINGS);
            }

            if(st().ui == UI_COMMAND)
            {
                // 命令输入模式：字母/数字全部交给字符输入（WM_CHAR），
                // 这里只处理回车执行与退格；Esc 关闭（上面已处理）。
                if(platform::key_pressed(platform::K_ENTER))
                {
                    std::string c = command_text();
                    platform::set_char_hook(nullptr);
                    ui_close();
                    if(!c.empty()) exec_command(c);
                }
                if(platform::key_pressed(0x08)) command_backspace();
            }
            else
            {
                if(platform::key_pressed(platform::K_E))
                {
                    if(st().ui == UI_NONE) ui_open(UI_INV);
                    else if(st().ui == UI_INV || st().ui == UI_CHEST) ui_close();
                }
                if(platform::key_pressed(platform::K_M))
                {
                    if(st().ui == UI_MAP) ui_close();
                    else ui_open(UI_MAP);
                }
                if(platform::key_pressed(platform::K_T))
                {
                    command_clear();
                    ui_open(UI_COMMAND);
                    platform::set_char_hook(on_char);
                }
                if(platform::key_pressed(platform::K_C))   // 打开背包（2x2 合成）；3x3 需右键工作台
                {
                    if(st().ui == UI_NONE) ui_open(UI_INV);
                    else if(st().ui == UI_INV) ui_close();
                }
            }
            if(st().ui != UI_NONE)
            {
                for(i32 k = 0; k < 256; k++)
                    if(k != platform::K_ESC && platform::key_pressed(k)) ui_key(k);
                ui_mouse_move(mx, my);
                if(platform::mouse_pressed(platform::MB_LEFT)) ui_mouse_down(mx, my, 0);
                if(platform::mouse_pressed(platform::MB_RIGHT)) ui_mouse_down(mx, my, 1);
                if(platform::mouse_released(platform::MB_LEFT)) ui_mouse_up(mx, my, 0);
                if(platform::mouse_released(platform::MB_RIGHT)) ui_mouse_up(mx, my, 1);
            }
            f32 wh = platform::wheel();
            if(wh != 0 && st().ui == UI_NONE) on_wheel(wh > 0 ? 1 : -1);

            // 固定 tick 逻辑（暂停菜单打开时冻结世界，HUD/粒子照常）
            if(st().ui != UI_SETTINGS)
            {
                i32 per_frame = (st().setfps == 1) ? 1 : (st().setfps == 2 ? 2 : 3);
                for(i32 i = 0; i < per_frame; i++) { tick_once(); mods::tick(); }
            }
            hud_tick(dt);
            fx::tick(dt);

            // --trace：运动轨迹诊断。强制按住 D + 慢速跳跃，逐帧记录
            // 玩家与相机位置，用来量化「相机抽搐」是否还存在。
            if(traceMode)
            {
                // 注入合成输入：一直向右走，每 40 帧跳一次
                bool wantJump = (frame % 40) >= 20 && (frame % 40) < 26;
                game::force_input(frame > 10 && frame <= 110, wantJump);
                if(frame > 110) game::force_input(false, false);
                if(frame >= 10 && frame <= 110)
                    platform::log("T %d %.4f %.4f %.4f %.4f %.4f %.4f", frame,
                                  st().pl.x, st().pl.y, st().pl.jx, st().pl.jy,
                                  st().pl.spx, st().pl.spy);
            }

            // --drown：把玩家泡在水里，逐帧记录氧气/生命，
            // 用来验证「水里氧气消耗太快 / 血掉太快」
            if(drownMode)
            {
                if(frame == 19)
                {
                    // 一次性准备：头顶到脚下灌成水，氧气与生命拉满
                    i32 px = (i32)st().pl.x;
                    i32 py = (i32)st().pl.y;
                    for(i32 jj = py - 1; jj <= py + 2; jj++)
                        if(px > 1 && px < world::W - 2 && jj > 1 && jj < world::H - 2)
                        {
                            world::s[px][jj] = 19;   // 静态水
                            world::mark_dirty_col(px);
                        }
                    st().pl.oxygen = drownFast ? 8 : 900;
                    st().pl.life = 20;
                    st().pl.wudi = 0;
                    platform::log("D 开始潜水 o2=%d life=%.2f 位置=%d,%d",
                                  st().pl.oxygen, st().pl.life, px, py);
                }
                if(frame > 20)
                {
                    // 把玩家钉在水里（否则空手会浮上水面，测不到消耗）
                    i32 px = (i32)st().pl.x;
                    i32 py = (i32)st().pl.y;
                    for(i32 jj = py - 2; jj <= py + 3; jj++)
                        if(px > 1 && px < world::W - 2 && jj > 1 && jj < world::H - 2)
                        {
                            world::s[px][jj] = 19;
                            world::mark_dirty_col(px);
                        }
                    st().pl.y = (f64)py + 0.2;
                    st().pl.spy = 0;
                    st().pl.jump = 5;
                    if(frame % 60 == 0)
                        platform::log("D %d o2=%d life=%.2f", frame, st().pl.oxygen,
                                      st().pl.life);
                }
            }

            render::begin_frame(rgba(10, 9, 14, 255));
            draw_all(dt);

            // 暂停菜单点了「保存并退出到标题界面」：下一帧回到标题
            if(!st().running)
            {
                st().running = 1;
                st().ui = UI_NONE;
                mods::unload_all();
                screen = SCR_TITLE;
                read_savelist();
            }
        }

        if(shotPath && frame == shotFrame)
        {
            bool ok = render::capture_bmp(shotPath);
            platform::log("截图 %s -> %s (draw calls=%d, quads=%d)",
                          ok ? "成功" : "失败", shotPath, render::last_draw_calls(),
                          render::last_quad_count());
        }
        platform::present();
        platform::end_frame();

        if(shotPath && frame >= shotFrame) break;
        if(maxFrames > 0 && frame >= maxFrames) break;
    }

    game::shutdown();
    render::shutdown();
    platform::shutdown();
    return 0;
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return app_main(); }
#else
int main() { return app_main(); }
#endif








