// ===================== UI 面板 =====================
// 旧版用控制台光标排版（Setpos + printf）画面板，坐标是"字符格"。
// 新版本改成真实像素面板 + 鼠标命中测试。
//
// 布局逻辑尽量沿用旧版：
//   旧版 crafting 面板每行一个配方，右侧一行显示"->"
//   旧版背包是 5 列 x 6 行 + 4 个护甲槽
//   旧版箱子 5 列 x 5 行
//
// 所有面板共用一套"槽位网格"抽象：给定格子几何 + 鼠标位置 -> 槽位索引，
// 这样拖拽/交换逻辑只写一份。
#include "game_render.h"
#include "../entity/entity.h"
#include "../item/recipes.h"
#include "../platform/platform.h"
#include "../item/chest.h"
#include "../world/blocks.h"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace game {

// 槽位布局常量（INV_SLOTS / BACKPACK_* / HOTBAR_* / INV_LAST ...）都在 item 命名空间
using namespace item;

// ---------------- 面板几何 ----------------
namespace {

constexpr f32 SLOT = 46.0f;      // 单格像素
constexpr f32 GAP = 4.0f;
constexpr f32 PAD = 14.0f;

// 护甲面板从上到下 = 头 / 胸 / 腿 / 靴，对应旧版 wear[] 的 3 / 1 / 2 / 4
const i32 ARMOR_ORDER[4] = {3, 1, 2, 4};

// 槽位种类
enum SlotRef { SREF_NONE = 0, SREF_INV, SREF_HOT, SREF_ARMOR, SREF_CHEST,
               SREF_CRAFT, SREF_OUT };

// 9 列网格宽度（背包 / 快捷栏共用）
constexpr f32 GRID_W = 9 * SLOT + 8 * GAP;
// 右侧护甲列：标签占位 + 间隔 + 单列
constexpr f32 ARMOR_LABEL_W = 46.0f;
constexpr f32 ARMOR_GAP = 26.0f;
// 物品栏面板宽度（背包 + 护甲列 + 内边距）
constexpr f32 INV_PW = GRID_W + ARMOR_GAP + ARMOR_LABEL_W + SLOT + PAD * 2;

// ★ 网格合成区（MC 式）：合成格 -> 箭头 -> 出口格
constexpr f32 CRAFT_AREA = 110.0f;    // 背包面板顶部合成区高度（含留白）
constexpr f32 CRAFT_Y = 52.0f;        // 背包面板合成格的 y（相对面板顶）
constexpr f32 CT_TOP = 56.0f;         // 合成面板 3x3 的 y（相对面板顶）
constexpr f32 CT_SPACE = 26.0f;       // 合成面板里合成区与背包的间隔
constexpr f32 ARR_W = 44.0f, ARR_H = 24.0f;   // 箭头尺寸

struct Grid
{
    f32 x, y;        // 左上角
    i32 cols, rows;
    f32 slot, gap;
    f32 w() const { return cols * slot + (cols - 1) * gap; }
    f32 h() const { return rows * slot + (rows - 1) * gap; }
    // 返回格子索引（0 基），不在范围内返回 -1
    i32 hit(f32 mx, f32 my) const
    {
        f32 lx = mx - x, ly = my - y;
        if(lx < 0 || ly < 0) return -1;
        i32 c = (i32)(lx / (slot + gap));
        i32 r = (i32)(ly / (slot + gap));
        if(c < 0 || c >= cols || r < 0 || r >= rows) return -1;
        // 落在格子之间的缝隙也算这一格，手感更好
        f32 ox = lx - c * (slot + gap);
        f32 oy = ly - r * (slot + gap);
        if(ox > slot + gap || oy > slot + gap) return -1;
        return r * cols + c;
    }
    void slot_xy(i32 idx, f32 &ox, f32 &oy) const
    {
        i32 c = idx % cols, r = idx / cols;
        ox = x + c * (slot + gap);
        oy = y + r * (slot + gap);
    }
};

// ★ 合成区排布：合成格 -> 箭头 -> 出口格（绘制与命中测试共用同一算法）
void craft_row(const Grid &g, f32 &ax, f32 &ay, Grid &out)
{
    ax = g.x + g.w() + 22;
    ay = g.y + g.h() * 0.5f - ARR_H * 0.5f;
    out = Grid{ax + ARR_W + 22, g.y + (g.h() - SLOT) * 0.5f, 1, 1, SLOT, GAP};
}

// ★ 手上的物品（MC 光标）：脱离槽位、跟着鼠标走。
//   左键点击 = 整叠拿起/放下；右键 = 取半/放一个；左键长按拖动 = 平均分配。
item::Stack g_cursor;
f32 g_cmx = 0, g_cmy = 0;          // 鼠标位置（画光标用）

// 左键按下追踪：松开时区分「点击」与「长按拖动均分」
struct Paint
{
    i32 active = 0;                 // 左键按下中
    i32 moved = 0;                  // 是否拖到非起点格
    i32 spread = 0;                 // 是否已开始平铺（用于补起点格）
    i32 dk = 0, di = 0;             // 起点格（按下时）
    i32 n = 0;                      // 已平铺的格子数（去重）
    i32 kind[64], idx[64];          // 已平铺的格子
};
Paint g_paint;

// 面板打开时把鼠标位置也更新给世界（旧版 movethings 里做）
void sync_mouse_to_world()
{
    // 面板打开时鼠标不再指世界，但保留上次值，避免 mpx/mpy 乱跳
}

}   // namespace

// ============================================================================
//                             通用：画物品格
// ============================================================================
namespace {

void draw_slot_box(f32 x, f32 y, bool hot)
{
    render::draw(hot ? atlas::U_SLOT_HL : atlas::U_SLOT, x, y, SLOT, SLOT);
}

// 拿起即清空源槽（物品进 g_cursor），因此无需再隐藏源槽
bool is_drag_src(i32, i32) { return false; }

void draw_stack_in(const item::Stack &s, f32 x, f32 y, bool hot, bool hide = false)
{
    draw_slot_box(x, y, hot);
    if(hide || s.empty()) return;
    f32 ip = SLOT - 10.0f;
    render::draw(atlas::slot_of_block(s.id), x + 5, y + 5, ip, ip);
    if(s.count > 1)
    {
        char b[8];
        snprintf(b, sizeof(b), "%d", (int)s.count);
        render::text_right(x + SLOT - 4, y + SLOT - 22, b, rgba(255, 255, 255), 0.72f);
    }
    // ★ 耐久条
    i32 mx = s.max_dur();
    if(mx > 0)
    {
        f32 f = (f32)s.remain() / (f32)mx;
        if(f < 0) f = 0;
        if(f > 1) f = 1;
        f32 bw = SLOT - 10.0f, bh = 4.0f;
        f32 bx = x + 5, by = y + SLOT - 9;
        render::rect(bx - 1, by - 1, bw + 2, bh + 2, rgba(0, 0, 0, 200));
        render::draw(atlas::U_DUR_FILL, bx, by, bw * f, bh,
                     tint_of(s.dur_color(), 1.0f, 235));
        if(f < 0.10f)
        {
            f32 bl = 0.5f + 0.5f * std::sin((f32)platform::now_seconds() * 12.0f);
            render::rect_outline(bx - 1, by - 1, bw + 2, bh + 2,
                                 rgba(255, 60, 50, (u8)(120 + 135 * bl)), 1);
        }
    }
}

// 底部统一的操作提示条
void draw_hint(const char *text)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::text_center_shadow(w * 0.5f, h - 30, text, rgba(210, 220, 240), 0.95f);
}

// 手上的物品（g_cursor）跟着鼠标画
void draw_cursor()
{
    if(g_cursor.empty()) return;
    render::draw(atlas::slot_of_block(g_cursor.id), g_cmx - 19, g_cmy - 19, 38, 38,
                 rgba(255, 255, 255, 235));
    if(g_cursor.count > 1)
        render::textf(g_cmx + 7, g_cmy + 4, rgba(255, 255, 255), 0.82f,
                      "%d", (int)g_cursor.count);
}

// 面板外框 + 标题
void draw_frame(f32 x, f32 y, f32 w, f32 h, const char *title)
{
    render::panel(x, y, w, h, rgba(26, 22, 34, 235), rgba(140, 122, 92, 220), 10, 2);
    render::draw(atlas::U_TITLE_BAR, x + 2, y + 2, w - 4, 34);
    render::text(x + 14, y + 8, title, rgba(255, 226, 140), 1.05f);
}

}   // namespace

// ============================================================================
//                        物品格子点击 → 拖拽/交换
// ============================================================================
namespace {

// 前置声明：箱子槽位读写（定义在本节之后）
void chest_get(i32 idx, item::Stack &out);

// 把 src 的 Stack 搬到 dst（同 id 合并、否则交换），与旧版 bb() 语义一致
void merge_or_swap(item::Stack &a, item::Stack &b)
{
    if(a.empty() && b.empty()) return;
    if(b.empty())
    {
        b = a;
        a.reset();
        return;
    }
    if(a.empty()) return;
    if(a.id != b.id)
    {
        item::Stack t = a;
        a = b;
        b = t;
        return;
    }
    // 同 id：耐久物品不合并（各占一格），可堆叠的合并到 64
    if(a.max_dur() > 0 || b.max_dur() > 0) return;
    i32 room = item::MAX_STACK - b.count;
    if(room <= 0) return;
    i32 mv = a.count < room ? a.count : room;
    b.count += (i16)mv;
    a.count -= (i16)mv;
    if(a.count <= 0) a.reset();
}

// ★ 把 item::craft[] 折成 cells[row*size+col]（2x2 只取左上角 0,1,3,4）
void craft_cells(i32 size, i16 *cells)
{
    for(i32 r = 0; r < size; r++)
        for(i32 c = 0; c < size; c++)
        {
            const item::Stack &s = item::craft[r * 3 + c];
            cells[r * size + c] = s.empty() ? 0 : s.id;
        }
}

// 背包能否放下 count 个 type（复制 inv_add 的堆叠规则，扣材料前先检查）
bool inv_room_for(i32 type, i32 count)
{
    if(type <= 0 || type >= BLOCK_COUNT || count <= 0) return false;
    i32 left = count;
    if(block_of(type).max_dur <= 0)
        for(i32 i = INV_FIRST; i <= INV_LAST && left > 0; i++)
            if(item::beibao[i].id == type && item::beibao[i].count > 0 &&
                    item::beibao[i].count < MAX_STACK)
            {
                i32 add = MAX_STACK - item::beibao[i].count;
                if(add > left) add = left;
                left -= add;
            }
    for(i32 i = INV_FIRST; i <= INV_LAST && left > 0; i++)
        if(item::beibao[i].count <= 0)
        {
            i32 add = (block_of(type).max_dur > 0) ? 1 : (left > MAX_STACK ? MAX_STACK : left);
            left -= add;
        }
    return left <= 0;
}

// ★ 出口格点击 = 合成一次：匹配配方 -> 扣材料 -> 产物进背包
void craft_once(i32 size)
{
    if(size < 2) size = 2;
    if(size > 3) size = 3;
    item::craft_size = size;

    i16 cells[9];
    craft_cells(size, cells);
    i16 out = 0, out_n = 0;
    if(!item::grid_result(cells, size, out, out_n)) return;   // 没配方：什么都不做

    // 背包放不下就提示且不扣材料
    if(!inv_room_for(out, out_n))
    {
        toast("背包已满");
        return;
    }
    // 每个非空格子扣 1 个，扣到 0 就清空
    for(i32 r = 0; r < size; r++)
        for(i32 c = 0; c < size; c++)
        {
            item::Stack &s = item::craft[r * 3 + c];
            if(s.empty()) continue;
            s.count--;
            if(s.count <= 0) s.reset();
        }
    item::inv_add(out, out_n);
    toast("合成成功：%s", block_of(out).cn);
}

}   // namespace

// ============================================================================
//                        背包 / 护甲 面板
// ============================================================================
void draw_panel_inventory()
{
    Player &p = st().pl;
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();

    const f32 ph = 46 + CRAFT_AREA + (BACKPACK_ROWS * SLOT + (BACKPACK_ROWS - 1) * GAP) +
                   14 + SLOT + 76;
    const f32 pw = INV_PW;
    f32 px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
    draw_frame(px, py, pw, ph, "物品栏");

    // ★ 2x2 合成格（craft[row*3+col]，只用 0,1,3,4）+ 箭头 + 出口格
    Grid gc = Grid{px + PAD, py + CRAFT_Y, 2, 2, SLOT, GAP};
    for(i32 r = 0; r < 2; r++)
        for(i32 c = 0; c < 2; c++)
        {
            i32 idx = r * 3 + c;
            f32 x, y;
            gc.slot_xy(r * gc.cols + c, x, y);
            draw_stack_in(item::craft[idx], x, y, false, is_drag_src(SREF_CRAFT, idx));
        }
    f32 arx, ary;
    Grid go;
    craft_row(gc, arx, ary, go);
    i16 cells[9];
    craft_cells(2, cells);
    i16 o = 0, on = 0;
    bool ok = item::grid_result(cells, 2, o, on);
    render::draw(atlas::U_ARROW_R, arx, ary, ARR_W, ARR_H,
                 ok ? rgba(150, 230, 160, 240) : rgba(130, 130, 140, 180));
    item::Stack res;
    if(ok) { res.id = o; res.count = on; }
    bool ohit = platform::mouse_x() >= go.x && platform::mouse_x() <= go.x + SLOT &&
                platform::mouse_y() >= go.y && platform::mouse_y() <= go.y + SLOT;
    draw_stack_in(res, go.x, go.y, ohit, false);

    // 背包 3x9（槽位 10..36）
    Grid gb = Grid{px + PAD, py + 46 + CRAFT_AREA, BACKPACK_COLS, BACKPACK_ROWS, SLOT, GAP};
    for(i32 i = 0; i < BACKPACK_COLS * BACKPACK_ROWS; i++)
    {
        f32 x, y;
        gb.slot_xy(i, x, y);
        i32 slot = BACKPACK_FIRST + i;
        draw_stack_in(item::beibao[slot], x, y, st().color_change == slot,
                      is_drag_src(SREF_INV, slot));
    }
    // 快捷栏 1x9（槽位 1..9）
    Grid gh = Grid{px + PAD, py + 46 + CRAFT_AREA + gb.h() + 14, HOTBAR_LAST, 1, SLOT, GAP};
    for(i32 i = 0; i < HOTBAR_LAST; i++)
    {
        f32 x, y;
        gh.slot_xy(i, x, y);
        i32 slot = HOTBAR_FIRST + i;
        draw_stack_in(item::beibao[slot], x, y, p.cho == slot,
                      is_drag_src(SREF_HOT, slot));
    }
    // 护甲 4 格（竖排在右侧）
    f32 ax = px + PAD + gb.w() + ARMOR_GAP;
    f32 ay = py + 46;
    const char *an[4] = {"头", "胸", "腿", "靴"};
    for(i32 i = 0; i < 4; i++)
    {
        render::text(ax, ay + i * (SLOT + GAP) + SLOT * 0.5f - 8, an[i],
                     rgba(200, 210, 230), 0.78f);
        draw_stack_in(item::armor[ARMOR_ORDER[i]], ax + 46, ay + i * (SLOT + GAP),
                      false, is_drag_src(SREF_ARMOR, i));
    }

    // 合成入口
    f32 bx = px + PAD, byy = py + ph - 62;
    bool bhot = platform::mouse_x() >= bx && platform::mouse_x() <= bx + 130 &&
                platform::mouse_y() >= byy && platform::mouse_y() <= byy + 38;
    render::panel(bx, byy, 130, 38, bhot ? rgba(66, 88, 60, 235) : rgba(46, 60, 44, 225),
                  rgba(140, 200, 150, 210), 6, 2);
    render::text_center_shadow(bx + 65, byy + 9, "3x3 需工作台", rgba(220, 245, 220), 0.95f);

    draw_hint(st().durability_on
              ? "左键拖动摆放 · 2x2 合成 · 右键工作台开 3x3 · E/Esc 关闭 · 护甲拖到右侧 · 耐久度：开"
              : "左键拖动摆放 · 2x2 合成 · 右键工作台开 3x3 · E/Esc 关闭 · 耐久度：关");

    // 拖拽中的物品跟随鼠标
    draw_cursor();
}

// ============================================================================
//                            箱子面板
// ============================================================================
void draw_panel_chest()
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();
    if(st().open_box < 0 || st().open_box >= item::box_count())
    {
        draw_panel_inventory();
        return;
    }
    const f32 pw = GRID_W + PAD * 2;
    const f32 ph = 46 + (5 * SLOT + 4 * GAP) + 32 +
                   (BACKPACK_ROWS * SLOT + (BACKPACK_ROWS - 1) * GAP) + 14 + SLOT + 56;
    f32 px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
    draw_frame(px, py, pw, ph, "箱子");

    // 箱内 25 格
    Grid gb = Grid{px + PAD, py + 46, 5, 5, SLOT, GAP};
    item::Box &bx = item::box_at(st().open_box);
    for(i32 i = 0; i < item::BOX_USABLE; i++)
    {
        f32 x, y;
        gb.slot_xy(i, x, y);
        item::Stack tmp;
        tmp.id = (i16)(bx.things[i] % 1000);
        tmp.count = (i16)(bx.things[i] / 1000);
        if(tmp.id < 0 || tmp.id >= BLOCK_COUNT) tmp.reset();
        draw_stack_in(tmp, x, y, false, is_drag_src(SREF_CHEST, i));
    }
    // 背包 3x9 + 快捷栏 1x9
    f32 by = py + 46 + gb.h() + 14;
    render::text(px + PAD, by, "物品栏", rgba(255, 226, 140), 0.85f);
    Grid g = Grid{px + PAD, by + 20, BACKPACK_COLS, BACKPACK_ROWS, SLOT, GAP};
    for(i32 i = 0; i < BACKPACK_COLS * BACKPACK_ROWS; i++)
    {
        f32 x, y;
        g.slot_xy(i, x, y);
        i32 slot = BACKPACK_FIRST + i;
        draw_stack_in(item::beibao[slot], x, y, false, is_drag_src(SREF_INV, slot));
    }
    Grid gh = Grid{px + PAD, by + 20 + g.h() + 14, HOTBAR_LAST, 1, SLOT, GAP};
    for(i32 i = 0; i < HOTBAR_LAST; i++)
    {
        f32 x, y;
        gh.slot_xy(i, x, y);
        i32 slot = HOTBAR_FIRST + i;
        draw_stack_in(item::beibao[slot], x, y, false, is_drag_src(SREF_HOT, slot));
    }
    draw_hint("左键在箱子与背包之间搬运 · E/Esc 关闭");

    draw_cursor();
}

// ============================================================================
//                     合成面板（MC 式 3x3 工作台）
// ============================================================================
void draw_panel_craft()
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();

    const f32 pw = GRID_W + PAD * 2;
    const f32 ph = CT_TOP + (3 * SLOT + 2 * GAP) + CT_SPACE +
                   (BACKPACK_ROWS * SLOT + (BACKPACK_ROWS - 1) * GAP) + 14 + SLOT + 30;
    f32 px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
    draw_frame(px, py, pw, ph, "合成");

    // ★ 3x3 合成格（craft[row*3+col]，9 格全用）
    Grid gc = Grid{px + PAD, py + CT_TOP, 3, 3, SLOT, GAP};
    for(i32 r = 0; r < 3; r++)
        for(i32 c = 0; c < 3; c++)
        {
            i32 idx = r * 3 + c;
            f32 x, y;
            gc.slot_xy(r * gc.cols + c, x, y);
            draw_stack_in(item::craft[idx], x, y, false, is_drag_src(SREF_CRAFT, idx));
        }
    // 箭头 + 出口格（出口格画 grid_result 算出的成品）
    f32 arx, ary;
    Grid go;
    craft_row(gc, arx, ary, go);
    i16 cells[9];
    craft_cells(3, cells);
    i16 o = 0, on = 0;
    bool ok = item::grid_result(cells, 3, o, on);
    render::draw(atlas::U_ARROW_R, arx, ary, ARR_W, ARR_H,
                 ok ? rgba(150, 230, 160, 240) : rgba(130, 130, 140, 180));
    item::Stack res;
    if(ok) { res.id = o; res.count = on; }
    bool ohit = platform::mouse_x() >= go.x && platform::mouse_x() <= go.x + SLOT &&
                platform::mouse_y() >= go.y && platform::mouse_y() <= go.y + SLOT;
    draw_stack_in(res, go.x, go.y, ohit, false);

    // 背包 3x9（槽位 10..36）
    f32 by = py + CT_TOP + gc.h() + CT_SPACE;
    Grid gb = Grid{px + PAD, by, BACKPACK_COLS, BACKPACK_ROWS, SLOT, GAP};
    for(i32 i = 0; i < BACKPACK_COLS * BACKPACK_ROWS; i++)
    {
        f32 x, y;
        gb.slot_xy(i, x, y);
        i32 slot = BACKPACK_FIRST + i;
        draw_stack_in(item::beibao[slot], x, y, st().color_change == slot,
                      is_drag_src(SREF_INV, slot));
    }
    // 快捷栏 1x9（槽位 1..9）
    Grid gh = Grid{px + PAD, by + gb.h() + 14, HOTBAR_LAST, 1, SLOT, GAP};
    for(i32 i = 0; i < HOTBAR_LAST; i++)
    {
        f32 x, y;
        gh.slot_xy(i, x, y);
        i32 slot = HOTBAR_FIRST + i;
        draw_stack_in(item::beibao[slot], x, y, st().pl.cho == slot,
                      is_drag_src(SREF_HOT, slot));
    }
    draw_hint("左键拖动摆放 · 点出口格合成 · E/Esc 关闭");

    // 拖拽中的物品跟随鼠标
    draw_cursor();
}

// ============================================================================
//                            设置面板
// ============================================================================
namespace {

struct SetRow
{
    const char *label;
    const char *value;
    i32 id;
};

void settings_rows(SetRow *rows, i32 &n)
{
    static char buf[9][48];
    n = 0;
    State &s = st();
    snprintf(buf[0], sizeof(buf[0]), "%d 像素/格", s.tile_px);
    rows[n++] = {"渲染缩放", buf[0], 0};
    snprintf(buf[1], sizeof(buf[1]), "%s", s.CanGet ? "开" : "关");
    rows[n++] = {"作弊模式（/give 等）", buf[1], 1};
    snprintf(buf[2], sizeof(buf[2]), "%s", s.durability_on ? "开" : "关");
    rows[n++] = {"★ 耐久度", buf[2], 2};
    snprintf(buf[3], sizeof(buf[3]), "%s",
             s.setfps == 1 ? "60 帧" : (s.setfps == 2 ? "30 帧" : "20 帧"));
    rows[n++] = {"逻辑帧率", buf[3], 3};
    snprintf(buf[4], sizeof(buf[4]), "%s", world::sun ? "关" : "开");
    rows[n++] = {"定格时间", buf[4], 4};
    snprintf(buf[5], sizeof(buf[5]), "%s",
             world::ed == 8000 ? "简单" : (world::ed == 6000 ? "普通" : "困难"));
    rows[n++] = {"难度", buf[5], 5};
    snprintf(buf[6], sizeof(buf[6]), "%s",
             s.special_effect == 1 ? "关" : (s.special_effect == 2 ? "开" : "加强"));
    rows[n++] = {"缓动渲染", buf[6], 6};
    snprintf(buf[7], sizeof(buf[7]), "%s",
             s.savetype == 1 ? "每 3 分钟" : s.savetype == 2 ? "每 5 分钟"
             : s.savetype == 3 ? "每 10 分钟" : "关");
    rows[n++] = {"自动保存", buf[7], 7};
    // ★ 玩家皮肤（assets/skins/*.png），点击切换
    {
        i32 cnt = atlas::skin_count();
        if(cnt <= 0) snprintf(buf[8], sizeof(buf[8]), "无");
        else
        {
            char nm[40];
            snprintf(nm, sizeof(nm), "%s", atlas::skin_name(s.skin_idx));
            size_t L = strlen(nm);                       // 去掉 .png
            if(L > 4 && strcmp(nm + L - 4, ".png") == 0) nm[L - 4] = 0;
            snprintf(buf[8], sizeof(buf[8]), "%s (%d/%d)", nm, s.skin_idx + 1, cnt);
        }
        rows[n++] = {"★ 玩家皮肤", buf[8], 8};
    }
}

}   // namespace

// 暂停菜单的行起始 y / 面板尺寸（绘制与命中测试共用）
constexpr f32 PAUSE_PW = 520, PAUSE_PH = 556;   // 9 行设置 + 两个按钮
constexpr f32 PAUSE_ROW_Y = 96;      // 第一行设置的 y（相对面板）
constexpr f32 PAUSE_ROW_H = 42;

void draw_panel_settings()
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();
    const f32 pw = PAUSE_PW, ph = PAUSE_PH;
    f32 px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
    draw_frame(px, py, pw, ph, "暂停 / 设置");

    // 继续游戏
    {
        f32 by = py + 46;
        bool hot = platform::mouse_x() >= px + PAD && platform::mouse_x() <= px + pw - PAD &&
                   platform::mouse_y() >= by && platform::mouse_y() <= by + 38;
        render::panel(px + PAD, by, pw - PAD * 2, 38,
                      hot ? rgba(60, 90, 66, 235) : rgba(46, 70, 52, 225),
                      rgba(140, 210, 150, 220), 6, 2);
        render::text_center_shadow(px + pw * 0.5f, by + 9, "继续游戏（Esc）",
                                   rgba(220, 250, 220), 0.95f);
    }

    SetRow rows[9];
    i32 n = 0;
    settings_rows(rows, n);
    for(i32 i = 0; i < n; i++)
    {
        f32 ry = py + PAUSE_ROW_Y + i * PAUSE_ROW_H;
        bool hot = st().color_change == i + 1;
        render::panel(px + PAD, ry, pw - PAD * 2, 34,
                      hot ? rgba(70, 62, 40, 230) : rgba(38, 34, 48, 200),
                      hot ? rgba(255, 226, 140, 230) : rgba(90, 82, 110, 190), 6, 1);
        render::text(px + PAD + 14, ry + 7, rows[i].label, rgba(220, 226, 240), 0.92f);
        render::text_right(px + pw - PAD - 14, ry + 7, rows[i].value,
                           rgba(255, 226, 140), 0.92f);
        if(platform::mouse_x() >= px + PAD && platform::mouse_x() <= px + pw - PAD &&
                platform::mouse_y() >= ry && platform::mouse_y() <= ry + 34)
            st().color_change = i + 1;
    }
    // 保存并退出
    f32 by = py + ph - 52;
    render::panel(px + PAD, by, pw - PAD * 2, 38, rgba(96, 44, 48, 225),
                  rgba(230, 120, 110, 220), 6, 2);
    render::text_center_shadow(px + pw * 0.5f, by + 9, "保存并退出到标题界面",
                               rgba(255, 226, 200), 0.95f);
    draw_hint("点击一行切换取值 · Esc 返回游戏");
}

// ============================================================================
//                            地图面板
// ============================================================================
void draw_panel_map()
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();
    render::rect(0, 0, w, h, rgba(8, 8, 12, 245));
    render::text_center_shadow(w * 0.5f, 12, "世界地图", rgba(255, 226, 140), 1.4f);

    // 缩放：每格 map_px 像素。缩得太小时按 step 抽样，
    // 否则一屏要画几十万个面片（初始 510k quads，FPS 直接掉到 1）。
    f32 mp = (f32)st().minimap_zoom;
    if(mp < 0.10f) mp = 0.10f;
    if(mp > 8.0f) mp = 8.0f;
    i32 step = 1;
    while(mp * step < 1.2f && step < 64) step++;

    f32 view_w = w / mp, view_h = (h - 90) / mp;
    f32 cx = st().pl.jx, cy = st().pl.jy;
    i32 x0 = (i32)(cx - view_w * 0.5f), x1 = (i32)(cx + view_w * 0.5f);
    i32 y0 = (i32)(cy - view_h * 0.5f), y1 = (i32)(cy + view_h * 0.5f);
    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    if(x1 >= world::W) x1 = world::W - 1;
    if(y1 >= world::H) y1 = world::H - 1;
    x0 = x0 / step * step;
    y0 = y0 / step * step;

    const f32 cellpx = mp * step + 0.6f;
    // 地图区域垂直居中：标题栏 45px，底部提示 45px
    const f32 area_y = 45.0f, area_h = h - 90.0f;
    for(i32 y = y0; y <= y1; y += step)
        for(i32 x = x0; x <= x1; x += step)
        {
            // 抽样：这一块里优先取非空气的一格，比逐格好看
            i16 id = B_AIR;
            for(i32 j = y; j < y + step && j <= y1 && id == B_AIR; j++)
                for(i32 i = x; i < x + step && i <= x1; i++)
                {
                    i16 v = world::s[i][j];
                    if(v != B_AIR && v != B_SKY) { id = v; break; }
                }
            if(id == B_AIR) continue;
            const BlockDef &b = block_of(id);
            f32 sx = w * 0.5f + (f32)(x - cx) * mp;
            f32 sy = area_y + area_h * 0.5f + ((f32)y - cy) * mp;
            render::rect(sx, sy, cellpx, cellpx, b.base);
        }
    // 玩家位置
    f32 px = w * 0.5f, py = area_y + area_h * 0.5f;
    render::draw(atlas::U_MAP_PIN, px - 9, py - 22, 18, 24, rgba(255, 90, 80));
    render::text_center_shadow(w * 0.5f, h - 34,
                               "方向键平移 · +/− 缩放 · M/Esc 关闭",
                               rgba(210, 220, 240), 0.95f);
}

// ============================================================================
//                          命令输入面板
// ============================================================================
namespace {
char g_cmd[128] = {0};
i32  g_cmd_len = 0;
}

const char *command_text() { return g_cmd; }
void command_clear() { g_cmd[0] = 0; g_cmd_len = 0; }
void command_push(u32 cp)
{
    if(cp < 32 || cp > 126) return;
    if(g_cmd_len >= 120) return;
    g_cmd[g_cmd_len++] = (char)cp;
    g_cmd[g_cmd_len] = 0;
}
void command_backspace()
{
    if(g_cmd_len > 0) g_cmd[--g_cmd_len] = 0;
}

void draw_panel_command()
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    render::begin_ui();
    const f32 pw = 640, ph = 200;
    f32 px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
    draw_frame(px, py, pw, ph, "命令");
    render::panel(px + PAD, py + 50, pw - PAD * 2, 42, rgba(12, 12, 18, 235),
                  rgba(140, 122, 92, 220), 6, 2);
    f32 blink = 0.5f + 0.5f * std::sin((f32)platform::now_seconds() * 8.0f);
    render::textf(px + PAD + 12, py + 60, rgba(230, 240, 255), 1.0f, "> %s", g_cmd);
    f32 tw = render::text_w(g_cmd, 1.0f);
    render::rect(px + PAD + 26 + tw, py + 58, 2, 24,
                 rgba(255, 255, 255, (u8)(120 + 135 * blink)));
    render::text(px + PAD, py + 106,
                 "/help /give <物品> <数量> /heal /life <n> /kill /gamemode "
                 "/clear /time <n> /tp <x> <y> /summon <名字> [数量]",
                 rgba(180, 190, 210), 0.82f);
    draw_hint("回车执行 · Esc 取消");
}

// ============================================================================
//                          鼠标 / 键盘事件
// ============================================================================
namespace {

// 面板整体布局（与 draw_* 里的几何保持一致）
struct Layout
{
    f32 px, py, pw, ph;
    Grid inv, hot, armor;
    Grid chest_grid;
    Grid craft, out;     // ★ 网格合成格 + 出口格
};

Layout layout_of(i32 mode)
{
    f32 w = (f32)platform::width(), h = (f32)platform::height();
    Layout L{};
    if(mode == UI_CHEST)
    {
        L.pw = GRID_W + PAD * 2;
        L.ph = 46 + (5 * SLOT + 4 * GAP) + 32 +
               (BACKPACK_ROWS * SLOT + (BACKPACK_ROWS - 1) * GAP) + 14 + SLOT + 56;
        L.px = (w - L.pw) * 0.5f;
        L.py = (h - L.ph) * 0.5f;
        L.chest_grid = Grid{L.px + PAD, L.py + 46, 5, 5, SLOT, GAP};
        f32 by = L.py + 46 + L.chest_grid.h() + 34;
        L.inv = Grid{L.px + PAD, by, BACKPACK_COLS, BACKPACK_ROWS, SLOT, GAP};
        L.hot = Grid{L.px + PAD, by + L.inv.h() + 14, HOTBAR_LAST, 1, SLOT, GAP};
    }
    else if(mode == UI_CRAFT)
    {
        // ★ 合成面板 = MC 工作台：3x3 合成格 + 箭头 + 出口格 + 背包 + 快捷栏
        L.pw = GRID_W + PAD * 2;
        L.ph = CT_TOP + (3 * SLOT + 2 * GAP) + CT_SPACE +
               (BACKPACK_ROWS * SLOT + (BACKPACK_ROWS - 1) * GAP) + 14 + SLOT + 30;
        L.px = (w - L.pw) * 0.5f;
        L.py = (h - L.ph) * 0.5f;
        L.craft = Grid{L.px + PAD, L.py + CT_TOP, 3, 3, SLOT, GAP};
        f32 ax, ay;
        craft_row(L.craft, ax, ay, L.out);
        f32 by = L.py + CT_TOP + L.craft.h() + CT_SPACE;
        L.inv = Grid{L.px + PAD, by, BACKPACK_COLS, BACKPACK_ROWS, SLOT, GAP};
        L.hot = Grid{L.px + PAD, by + L.inv.h() + 14, HOTBAR_LAST, 1, SLOT, GAP};
    }
    else
    {
        // ★ 背包面板：顶部 2x2 合成格 + 箭头 + 出口格，右侧护甲列
        L.ph = 46 + CRAFT_AREA + (BACKPACK_ROWS * SLOT + (BACKPACK_ROWS - 1) * GAP) +
               14 + SLOT + 76;
        L.pw = INV_PW;
        L.px = (w - L.pw) * 0.5f;
        L.py = (h - L.ph) * 0.5f;
        L.craft = Grid{L.px + PAD, L.py + CRAFT_Y, 2, 2, SLOT, GAP};
        f32 ax, ay;
        craft_row(L.craft, ax, ay, L.out);
        L.inv = Grid{L.px + PAD, L.py + 46 + CRAFT_AREA, BACKPACK_COLS, BACKPACK_ROWS,
                     SLOT, GAP};
        L.hot = Grid{L.px + PAD, L.py + 46 + CRAFT_AREA + L.inv.h() + 14, HOTBAR_LAST, 1,
                     SLOT, GAP};
        L.armor = Grid{L.px + PAD + L.inv.w() + ARMOR_GAP + ARMOR_LABEL_W, L.py + 46,
                       1, 4, SLOT, GAP};
    }
    return L;
}

struct Hit { i32 kind; i32 idx; };
inline bool operator==(const Hit &a, const Hit &b) { return a.kind == b.kind && a.idx == b.idx; }

Hit hit_test(Layout &L, f32 mx, f32 my, i32 mode)
{
    const i32 bag_n = BACKPACK_COLS * BACKPACK_ROWS;
    if(mode == UI_CHEST)
    {
        i32 c = L.chest_grid.hit(mx, my);
        if(c >= 0 && c < item::BOX_USABLE) return {SREF_CHEST, c};
        i32 i = L.inv.hit(mx, my);
        if(i >= 0 && i < bag_n) return {SREF_INV, BACKPACK_FIRST + i};
        i32 hh = L.hot.hit(mx, my);
        if(hh >= 0 && hh < HOTBAR_LAST) return {SREF_HOT, HOTBAR_FIRST + hh};
        return {SREF_NONE, 0};
    }
    // ★ 合成格 / 出口格在面板顶部，先于背包/快捷栏判定
    if(L.craft.cols > 0)
    {
        i32 c = L.craft.hit(mx, my);
        if(c >= 0) return {SREF_CRAFT, (c / L.craft.cols) * 3 + (c % L.craft.cols)};
    }
    if(L.out.cols > 0 && L.out.hit(mx, my) >= 0) return {SREF_OUT, 0};
    if(mode == UI_INV)   // 护甲列只在背包面板
    {
        i32 a = L.armor.hit(mx, my);
        if(a >= 0 && a < 4) return {SREF_ARMOR, a};
    }
    i32 h = L.hot.hit(mx, my);
    if(h >= 0 && h < HOTBAR_LAST) return {SREF_HOT, HOTBAR_FIRST + h};
    i32 i = L.inv.hit(mx, my);
    if(i >= 0 && i < bag_n) return {SREF_INV, BACKPACK_FIRST + i};
    return {SREF_NONE, 0};
}

// ---- 槽位统一读写 ----
item::Stack *slot_ptr(const Hit &h)
{
    switch(h.kind)
    {
    case SREF_INV:
    case SREF_HOT:
        return (h.idx >= 0 && h.idx < item::INV_SLOTS) ? &item::beibao[h.idx] : nullptr;
    case SREF_ARMOR:
        return (h.idx >= 0 && h.idx < 4) ? &item::armor[ARMOR_ORDER[h.idx]] : nullptr;
    case SREF_CRAFT:
        return (h.idx >= 0 && h.idx < 9) ? &item::craft[h.idx] : nullptr;
    case SREF_OUT:       // 出口格不是真槽位（成品是算出来的）
        return nullptr;
    default:
        return nullptr;
    }
}

// 箱子的槽位是打包 ushort，需要特殊读写
void chest_get(i32 idx, item::Stack &out)
{
    if(st().open_box < 0 || st().open_box >= item::box_count()) { out.reset(); return; }
    item::Box &b = item::box_at(st().open_box);
    out.id = (i16)(b.things[idx] % 1000);
    out.count = (i16)(b.things[idx] / 1000);
    out.wear = 0;
    out.flag = 0;
    if(out.id < 0 || out.id >= BLOCK_COUNT) out.reset();
}
void chest_set(i32 idx, const item::Stack &s)
{
    if(st().open_box < 0 || st().open_box >= item::box_count()) return;
    item::Box &b = item::box_at(st().open_box);
    if(s.empty()) b.things[idx] = 0;
    else b.things[idx] = (u16)(s.id + s.count * 1000);
}

// ---- 统一槽位读写（含箱子）----
void slot_get(const Hit &h, item::Stack &out)
{
    if(h.kind == SREF_CHEST) chest_get(h.idx, out);
    else if(item::Stack *p = slot_ptr(h)) out = *p;
    else out.reset();
}
void slot_set(const Hit &h, const item::Stack &s)
{
    if(h.kind == SREF_CHEST) chest_set(h.idx, s);
    else if(item::Stack *p = slot_ptr(h)) *p = s;
}

// 护甲槽是否接受该物品（ARMOR_ORDER = 头/胸/腿/靴）
bool armor_accepts(const Hit &to, const item::Stack &s)
{
    if(to.kind != SREF_ARMOR) return true;
    if(to.idx < 0 || to.idx >= 4) return false;
    return block_of(s.id).armor_slot == (u8)ARMOR_ORDER[to.idx];
}

// ★ 拿起整叠到手上（清空源槽）
void take_cursor(const Hit &from)
{
    if(!g_cursor.empty()) return;
    item::Stack s;
    slot_get(from, s);
    if(s.empty()) return;
    g_cursor = s;
    item::Stack blank;
    blank.reset();
    slot_set(from, blank);
}

// ★ 把手上整叠放到目标槽（同 id 合并 / 否则交换）
void place_cursor(const Hit &to)
{
    if(g_cursor.empty()) return;
    if(!armor_accepts(to, g_cursor)) return;
    item::Stack dst;
    slot_get(to, dst);
    merge_or_swap(g_cursor, dst);
    slot_set(to, dst);
    armor_update();
}

// ★ 右键 + 手上空：拿出一半（向上取整），源槽留一半
void right_pick_half(const Hit &h)
{
    if(!g_cursor.empty()) return;
    item::Stack s;
    slot_get(h, s);
    if(s.empty()) return;
    i32 half = (s.count + 1) / 2;
    g_cursor = s;
    g_cursor.count = (i16)half;
    s.count -= (i16)half;
    if(s.count <= 0) s.reset();
    slot_set(h, s);
}

// ★ 右键 + 手上有：往目标格放一个
void right_place_one(const Hit &h)
{
    if(g_cursor.empty()) return;
    item::Stack dst;
    slot_get(h, dst);
    if(dst.empty())
    {
        if(!armor_accepts(h, g_cursor)) return;
        dst = g_cursor;
        dst.count = 1;
    }
    else if(dst.id == g_cursor.id && dst.max_dur() <= 0 && dst.count < item::MAX_STACK)
    {
        dst.count++;
    }
    else return;                       // 不同 id 或已满：不动
    g_cursor.count--;
    if(g_cursor.count <= 0) g_cursor.reset();
    slot_set(h, dst);
    armor_update();
}

// ★ 左键拖动平铺：往一个格子放 1 个（MC 式每格 1 个，物品逐个进格就是动画；已划过的格子去重）
void spread_one(i32 k, i32 i)
{
    for(i32 j = 0; j < g_paint.n; j++)
        if(g_paint.kind[j] == k && g_paint.idx[j] == i) return;
    if(g_paint.n >= 64) return;
    Hit h{k, i};
    item::Stack cur;
    slot_get(h, cur);
    bool ok = cur.empty() ||
              (cur.id == g_cursor.id && cur.max_dur() <= 0 && cur.count < item::MAX_STACK);
    if(ok && g_cursor.count > 0)
    {
        if(cur.empty()) { cur = g_cursor; cur.count = 1; }
        else cur.count++;
        g_cursor.count--;
        if(g_cursor.count <= 0) g_cursor.reset();
        slot_set(h, cur);
    }
    g_paint.kind[g_paint.n] = k;
    g_paint.idx[g_paint.n] = i;
    g_paint.n++;
}

// ★ 关面板：手上的物品塞回背包（整叠搬过去，保留耐久等完整 Stack）
void cursor_return()
{
    if(g_cursor.empty()) return;
    if(g_cursor.max_dur() <= 0)        // 可叠的先并入同 id
        for(i32 i = INV_FIRST; i <= INV_LAST && !g_cursor.empty(); i++)
            if(item::beibao[i].id == g_cursor.id && item::beibao[i].count > 0 &&
                    item::beibao[i].count < item::MAX_STACK)
            {
                i32 room = item::MAX_STACK - item::beibao[i].count;
                i32 mv = g_cursor.count < room ? g_cursor.count : room;
                item::beibao[i].count += (i16)mv;
                g_cursor.count -= (i16)mv;
                if(g_cursor.count <= 0) { g_cursor.reset(); return; }
            }
    for(i32 i = INV_FIRST; i <= INV_LAST && !g_cursor.empty(); i++)   // 再占空槽
        if(item::beibao[i].count <= 0)
        {
            item::beibao[i] = g_cursor;
            g_cursor.reset();
            return;
        }
}

}   // namespace

bool ui_is_open() { return st().ui != UI_NONE; }

void ui_open(i32 mode)
{
    st().ui = mode;
    st().bag = 1;
    g_paint = Paint{};
    // ★ 网格大小跟着面板走：背包 2x2 / 合成面板 3x3
    item::craft_size = (mode == UI_CRAFT) ? 3 : 2;
}
void ui_close()
{
    st().ui = UI_NONE;
    st().bag = 0;
    st().color_change = 0;
    g_paint = Paint{};
    cursor_return();         // ★ 关面板：手上的物品塞回背包
    item::craft_clear();     // ★ 关面板：合成格里的物品塞回背包
}

bool ui_mouse_move(f32 mx, f32 my)
{
    g_cmx = mx;
    g_cmy = my;
    // 左键按住拖动 + 手上有物品：MC 式平铺——划过空/同 id 格子，每格放 1 个（物品逐个进格）
    if(g_paint.active && platform::mouse_down(platform::MB_LEFT) && !g_cursor.empty())
    {
        Layout L = layout_of(st().ui);
        Hit h = hit_test(L, mx, my, st().ui);
        if(h.kind != SREF_NONE && h.kind != SREF_OUT)
        {
            if(!(h.kind == g_paint.dk && h.idx == g_paint.di)) g_paint.moved = 1;
            if(g_paint.moved)
            {
                if(!g_paint.spread)          // 确认拖动后补上起点格
                {
                    g_paint.spread = 1;
                    spread_one(g_paint.dk, g_paint.di);
                }
                spread_one(h.kind, h.idx);
            }
        }
    }
    return st().ui != UI_NONE;
}

bool ui_mouse_down(f32 mx, f32 my, i32 button)
{
    State &s = st();
    if(s.ui == UI_NONE) return false;

    // 设置面板：点击一行切换
    if(s.ui == UI_SETTINGS)
    {
        f32 w = (f32)platform::width(), h = (f32)platform::height();
        const f32 pw = PAUSE_PW, ph = PAUSE_PH;
        f32 px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
        // 继续游戏
        if(mx >= px + PAD && mx <= px + pw - PAD &&
                my >= py + 46 && my <= py + 84)
        {
            ui_close();
            return true;
        }
        for(i32 i = 0; i < 9; i++)
        {
            f32 ry = py + PAUSE_ROW_Y + i * PAUSE_ROW_H;
            if(mx >= px + PAD && mx <= px + pw - PAD && my >= ry && my <= ry + 34)
            {
                switch(i)
                {
                case 0: s.tile_px = (s.tile_px >= 24) ? 12 : s.tile_px + 4; break;
                case 1: s.CanGet = !s.CanGet; break;
                case 2: s.durability_on = !s.durability_on;
                        toast(s.durability_on ? "耐久度：开" : "耐久度：关"); break;
                case 3: s.setfps = (s.setfps >= 3) ? 1 : s.setfps + 1; break;
                case 4: world::sun = !world::sun; break;
                case 5: world::ed = (world::ed == 8000) ? 6000
                                      : (world::ed == 6000 ? 4000 : 8000); break;
                case 6: s.special_effect = (s.special_effect >= 3) ? 1
                                                                    : s.special_effect + 1; break;
                case 7: s.savetype = (s.savetype >= 4) ? 1 : s.savetype + 1; break;
                case 8:   // ★ 换皮肤：换一张并立刻重传图集
                {
                    i32 cnt = atlas::skin_count();
                    if(cnt > 0)
                    {
                        i32 nx = (s.skin_idx + 1) % cnt;
                        if(atlas::skin_apply(nx))
                        {
                            s.skin_idx = nx;
                            render::upload_atlas();
                            toast("皮肤：%s", atlas::skin_name(nx));
                        }
                    }
                    else toast("assets/skins 里没有皮肤文件");
                    break;
                }
                }
                return true;
            }
        }
        f32 by = py + ph - 52;
        if(mx >= px + PAD && mx <= px + pw - PAD && my >= by && my <= by + 38)
        {
            save_world(true);
            s.running = 0;      // 回标题界面
            return true;
        }
        return true;
    }

    if(s.ui == UI_MAP || s.ui == UI_COMMAND) return true;

    // 物品栏左下角的「合成」按钮
    if(s.ui == UI_INV)
    {
        Layout LB = layout_of(s.ui);
        f32 bx = LB.px + PAD, byy = LB.py + LB.ph - 62;
        if(mx >= bx && mx <= bx + 130 && my >= byy && my <= byy + 38)
        {
            toast("3x3 合成需右键工作台");
            return true;
        }
    }

    // 背包 / 箱子 / 合成：MC 式拖拽交互
    Layout L = layout_of(s.ui);
    Hit h = hit_test(L, mx, my, s.ui);

    // ★ 出口格：点一下合成一次（左/右键都算，不能被拿起）
    if(h.kind == SREF_OUT)
    {
        craft_once(s.ui == UI_CRAFT ? 3 : 2);
        return true;
    }

    if(button == 1)          // 右键：取半 / 放一个
    {
        if(h.kind == SREF_NONE) return true;
        if(g_cursor.empty()) right_pick_half(h);
        else right_place_one(h);
        return true;
    }

    // 左键：记录起点，动作在 up 时做（区分点击 / 拖动平铺）
    g_paint.active = 1;
    g_paint.moved = 0;
    g_paint.spread = 0;
    g_paint.n = 0;
    g_paint.dk = h.kind;
    g_paint.di = h.idx;
    return true;
}

bool ui_mouse_up(f32 mx, f32 my, i32 button)
{
    State &s = st();
    if(s.ui == UI_NONE) return false;
    if(button == 1) return true;          // 右键已在按下时处理
    if(!g_paint.active) return true;
    g_paint.active = 0;

    if(g_paint.moved)                     // 拖动平铺完成（物品已逐个放入）
    {
        g_paint.n = 0;
        return true;
    }

    // 普通点击：拿起 / 放下整叠
    g_paint.n = 0;
    Layout L = layout_of(s.ui);
    Hit h = hit_test(L, mx, my, s.ui);
    if(h.kind == SREF_NONE || h.kind == SREF_OUT) return true;
    if(g_cursor.empty()) take_cursor(h);
    else place_cursor(h);
    return true;
}

// 注意：Esc 不在这里处理。Esc 由主循环统一处理（打开/关闭面板），
// 否则「按 Esc 打开面板」和「ui_key 收到同一个 Esc 立刻关闭」会在同一帧
// 各执行一次，表现为 Esc 完全没反应。
bool ui_key(i32 key)
{
    State &s = st();
    if(s.ui == UI_NONE) return false;
    if(s.ui == UI_MAP)
    {
        if(key == platform::K_MINUS && s.minimap_zoom > 0.15f) s.minimap_zoom *= 0.7f;
        if(key == platform::K_PLUS && s.minimap_zoom < 8.0f) s.minimap_zoom *= 1.4f;
        return true;
    }
    return true;
}

}   // namespace game








