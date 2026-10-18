#pragma once
// ===================== 游戏渲染（世界 + HUD + 面板）=====================
// 旧版把渲染和逻辑揉在 Print()/Updateblock()/boxprint()/craft_update() 里。
// 新版本拆开：本模块只负责画，状态都从 game::st() / world / entity 读。
#include "game.h"
#include "../render/atlas.h"
#include "../render/renderer.h"

namespace game {

// 每帧调用一次：天空 + 地形 + 实体 + 玩家 + 粒子 + HUD + 当前面板
void draw_all(f32 dt);

// 世界渲染（不含 UI）
void draw_world();

// HUD（血条 / 护甲 / 氧气 / 快捷栏 / Boss 血条 / 提示）
void draw_hud();

// ---- 面板（ui.cpp / ui_inv.cpp 实现）----
void draw_panel_inventory();
void draw_panel_chest();
void draw_panel_craft();
void draw_panel_settings();
void draw_panel_map();
void draw_panel_command();

// ---- 命令输入（ui.cpp）----
const char *command_text();
void command_clear();
void command_push(u32 cp);
void command_backspace();

// ---- UI 命中测试与点击（由主循环转发鼠标事件）----
// 返回 true 表示事件被面板吃掉（不再传给世界）
bool ui_mouse_down(f32 mx, f32 my, i32 button);
bool ui_mouse_up(f32 mx, f32 my, i32 button);
bool ui_mouse_move(f32 mx, f32 my);
bool ui_key(i32 key);

// 面板打开/关闭
void ui_open(i32 mode);
void ui_close();
bool ui_is_open();

// ---- 粒子 ----
namespace fx {
void spawn_dust(f32 x, f32 y, i32 block_id, i32 count);
void spawn_explosion(f32 x, f32 y, f32 radius);
void spawn_splash(f32 x, f32 y);
void spawn_flame(f32 x, f32 y);
void spawn_text(f32 x, f32 y, const char *text, u32 color);   // 飘字
void tick(f32 dt);
void draw();                 // 粒子：在 begin_world 坐标系里画
void draw_float_text();      // 飘字：在 begin_ui 坐标系里画（在 HUD 之前调用）
void clear();
}

// ---- 破坏进度裂纹（世界坐标）----
void set_crack(i32 x, i32 y, i32 level);
void clear_crack();

}   // namespace game

