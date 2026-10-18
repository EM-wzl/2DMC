#pragma once
// ===================== 游戏状态与主循环 =====================
// 旧版 beta1.8.cpp 把这些状态散在一堆全局变量里（x/y/life/gongji/...）。
// 新版本收进 State 结构，字段名尽量与旧版一致，方便逐行对照移植。
#include "../core/types.h"
#include "../item/item.h"
#include "../world/world.h"

namespace game {

// 面板
enum UiMode { UI_NONE = 0, UI_INV, UI_CHEST, UI_CRAFT, UI_SETTINGS, UI_MAP, UI_COMMAND };

// 缓动渲染档位（旧版 special_effect）
enum Ease { EASE_OFF = 1, EASE_ON = 2, EASE_STRONG = 3 };

struct Player
{
    f64 x = 1, y = 50;          // 旧版 x / y
    f64 spx = 0, spy = 0;       // 旧版 spx / spy
    f64 life = 20;              // 旧版 life
    f64 jianshang = 1.0;        // 旧版 jianshang（护甲减伤系数）
    f64 playerspeed = 0.5;      // 旧版 playerspeed
    f64 playerjump = 12;        // 旧版 playerjump
    i32 jump = 0;               // 旧版 jump
    i32 fell = 0;               // 旧版 fell
    i32 wudi = 0;               // 旧版 wudi
    i32 oxygen = 900;           // 旧版 oxygen
    i32 drown_ticks = 0;        // 水下氧气递减的分频计数器
    i32 cho = 1;                // 旧版 cho
    i32 gongji = 1;             // 旧版 gongji
    i32 gongjicd = 20;          // 旧版 gongjicd
    i32 playerCD = 0;           // 旧版 playerCD
    i32 hujiazhi = 0;           // 旧版 hujiazhi
    i32 sword = 0;              // 旧版 sword
    i32 issword = 0;            // 旧版 issword
    i32 eat = 0;                // 旧版 eat
    i32 H = 0;                  // 旧版 H（回血计时）
    i32 lx = 0, ly = 0;         // 上一帧格坐标
    i32 gj = 0;                 // 旧版 gj（击退分级）
    // 相机（旧版 jx/jy）
    f64 jx = 1, jy = 50;
    // 鼠标指向的格（旧版 mpx/mpy）
    i32 mpx = 0, mpy = 0;
};

struct State
{
    Player pl;

    // ---- 世界 / 时间 ----
    f64 now_time = 0;           // 与 world::now_time 同步的浮点累加
    f64 tick_f = 0;             // 旧版 tick 的浮点累加（决定逻辑步进）
    f32 dt_scale = 1.0f;        // 帧率档位换算

    // ---- 设置 ----
    i32 CanGet = 1;             // 作弊模式
    i32 savetype = 1;           // 自动保存：1 每3分 / 2 每5分 / 3 每10分 / 4 关
    i32 special_effect = EASE_OFF;
    i32 setfps = 1;             // 1=60fps 2=30fps 3=20fps
    i32 difficulty = 6000;      // 旧版 ed：8000 简单 / 6000 普通 / 4000 困难
    i32 durability_on = 1;      // ★ 耐久度开关
    i32 tile_px = 16;           // 一格屏幕像素（12/16/20/24）
    i32 show_fps = 1;
    i32 sound_on = 1;
    i32 skin_idx = 0;           // ★ 玩家皮肤：assets/skins/ 里的第几张

    // ---- 运行 ----
    i32 running = 1;
    i32 ui = UI_NONE;           // 当前面板
    i32 page = 1;               // 合成页
    i32 color_change = 0;       // 面板里高亮的格子
    i32 open_box = -1;          // 打开中的箱子索引
    i32 slow_update = 0;
    i32 savetick = 0;
    i32 save_interval = 10800;  // 逻辑帧
    i32 l_life = 30;
    i32 selected_slot = 0;      // 面板里拖拽中的格子（旧版 cc）
    i32 drag_from = 0;
    i32 bag = 0;                // 旧版 bag：1 表示面板打开
    // 旧版 l_beibao/l_life 那种"UI 脏标记"
    i32 hud_dirty = 1;

    // ---- 运行时缓存 ----
    i32 bosshp = 0;
    i32 boss2hp = 0;
    i32 minimap_zoom = 2;
    f64 water_bob = 0;

    // ---- 悬浮提示 ----
    char tip[192] = {0};
    f32  tip_life = 0;
};

State &st();

// --trace 诊断用：注入合成输入（等价于一直按住 D / 空格）
void force_input(bool right, bool jump);

// ---- 生命周期 ----
void init();
// 旧版 Start()：死亡重生
void start_respawn();
// 找一块干燥地表出生点（头顶两格空气、脚下固体、不是液体）；返回是否找到
bool find_dry_spawn(i32 from_x, f64 &out_x, f64 &out_y);
// 旧版 shuaxin()：重置 HUD 与光照
void refresh_hud();
// 旧版 gohome()：回到床
void go_home();
// 旧版 Clear()：视野内空气填天空
void clear_view();

// ---- 光照 ----
// 旧版 setlight()：由 world 的通用版驱动，这里只负责传玩家位置
void light_update();
// 旧版 hujia_update()：护甲值/减伤/移速加成
void armor_update();

// ---- 每帧逻辑（一个 tick）----
void tick_once();

// ---- 输入（由 UI 层/主循环调用）----
void select_hotbar(i32 n);
void on_wheel(i32 delta);

// ---- 玩家动作 ----
void mine_at(i32 sx, i32 sy);          // 旧版 wajue
void place_at(i32 sx, i32 sy);         // 旧版 block_
void use_selected(bool right_click);   // 旧版 checkuseitem + 右键使用
void attack_at(i32 sx, i32 sy);        // 旧版玩家点击攻击
f64  mine_progress();                  // 旧版 wj：当前挖掘累积
void mine_reset();

// ---- 方块读写（自动按当前维度选 s / s2，并打脏列标记）----
void p_set_block(i32 x, i32 y, i32 id);
void p_get_block(i32 x, i32 y, i32 &out);

// ★ 耐久度：对当前手持工具造成 dmg 点损耗；返回 true 表示工具碎裂
bool wear_tool(i32 dmg);
// ★ 耐久度：护甲受击损耗（按受伤部位分摊）
void wear_armor_on_hit(f64 dmg);

// ---- 提示 ----
void toast(const char *fmt, ...);
void hud_tick(f32 dt);

// ---- 存档 ----
extern char world_name[64];            // 当前存档名（旧版 zzz）
bool has_world();
void new_world(const char *name, u64 seed = 0);   // seed=0 表示随机
bool load_world(const char *name);
void save_world(bool blocking, bool force_full = false);
void shutdown();

// ---- 主循环入口 ----
// running 变为 0 时返回。返回到标题界面。
void run();

}   // namespace game







