#pragma once
// ===================== 平台抽象层 =====================
// 游戏层只通过这里访问窗口/输入/时间，不直接包含 <windows.h>。
// 这样后续换后端（SDL/GLFW）时只改这一层。
#include "../core/types.h"

namespace platform {

// 键码沿用 Win32 虚拟键值，便于与本机消息一一对应，
// 但游戏层只使用这些名字，换平台时只需重新映射数值。
enum Key : i32
{
    K_NONE = 0,
    K_A = 0x41, K_B, K_C, K_D, K_E, K_F, K_G, K_H, K_I, K_J, K_K, K_L, K_M,
    K_N = 0x4E, K_O, K_P, K_Q, K_R, K_S, K_T, K_U, K_V, K_W, K_X, K_Y, K_Z,
    K_0 = 0x30, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
    K_LEFT = 0x25, K_UP = 0x26, K_RIGHT = 0x27, K_DOWN = 0x28,
    K_SPACE = 0x20, K_ENTER = 0x0D, K_ESC = 0x1B, K_TAB = 0x09,
    K_SHIFT = 0x10, K_CTRL = 0x11, K_ALT = 0x12,
    K_F1 = 0x70, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_MINUS = 0xBD, K_PLUS = 0xBB, K_COMMA = 0xBC, K_PERIOD = 0xBE,
};

enum MouseBtn : i32 { MB_LEFT = 0, MB_RIGHT = 1, MB_MIDDLE = 2, MB_COUNT = 3 };

// 创建窗口与 GL 上下文；失败返回 false（错误已打到 stderr）
// title 为 UTF-8（内部转宽字符，避免依赖编译器字符集设置）
bool init(const char *title, i32 w, i32 h);
void shutdown();

// 处理消息；窗口关闭返回 false
bool pump();
void present();          // SwapBuffers
void set_title(const char *title);

i32 width();
i32 height();
bool focused();

// ---- 输入 ----
bool key_down(i32 key);            // 持续按住
bool key_pressed(i32 key);         // 本帧刚按下
bool key_released(i32 key);        // 本帧刚抬起
bool mouse_down(i32 btn);
bool mouse_pressed(i32 btn);
bool mouse_released(i32 btn);
f32 mouse_x();                     // 客户端像素坐标
f32 mouse_y();
f32 wheel();                       // 本帧滚轮增量（正=上滚）
void end_frame();                  // 清空本帧边沿状态（帧尾调用）

// ---- 时间（不受系统时钟调整影响）----
f64 now_seconds();
f32 dt();                          // 距上一帧的秒数，已钳制到 [0,0.1]

// ---- OpenGL 扩展 ----
void *gl_proc(const char *name);
void set_vsync(bool on);

// 图形化构建没有控制台，诊断信息写 debug.log（同时转发到 stderr 便于控制台构建调试）
void log(const char *fmt, ...);

// ---- 命令行（GUI 构建也需要：--shot=xxx.bmp 之类）----
i32 argc();
const char *argv(i32 index);
// 查找 --key=value 形式的参数；找到返回值部分，否则返回 nullptr
const char *arg_value(const char *key);

}   // namespace platform