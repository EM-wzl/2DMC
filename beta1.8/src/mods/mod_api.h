#pragma once
// ===================== 2DMC beta1.8 模组 ABI（v1）=====================
// 写一个模组只需要：
//   1) #include "mod_api.h"（本文件）
//   2) 用 MOD_EXPORT 导出 mod_abi / mod_name（必选），其余按需导出
//   3) 编译成 .dll 放进游戏的 mods/ 目录
//   4) 启动游戏 -> 开始页面「模组」里勾选启用
//
// 编译（MinGW，见 mods/build_mods.bat）：
//   g++ your_mod.cpp -o your_mod.dll -shared -std=c++17 -O2 -static ^
//       -I..\src\mods -finput-charset=UTF-8 -fexec-charset=UTF-8
//
// 约定：
//   * 导出函数必须是 extern "C"，参数/返回值只用 C 基本类型，不要跨 DLL 边界传
//     std::string / std::vector 等（两边运行时不同会崩）。
//   * mod_init 只在「启用」时调用一次；mod_shutdown 在停用/回标题时调用。
//   * mod_world_ready 在世界生成完成或读档完成后调用；mod_tick 每个逻辑帧调用。
//   * 所有可选导出都可以不实现：找不到就不调用。

#define MOD_ABI_VERSION 1

// 实体类型（与引擎 entity::Type 一致）
enum ModEntity
{
    MOD_ENT_ZOMBIE = 1, MOD_ENT_CREEPER = 2, MOD_ENT_SKELETON = 3,
    MOD_ENT_ARROW = 4, MOD_ENT_OI = 5, MOD_ENT_SPIDER = 6, MOD_ENT_PIGLIN = 7,
    MOD_ENT_RECURSIVE = 8, MOD_ENT_WORM_HEAD = 9, MOD_ENT_WORM_BODY = 10,
    MOD_ENT_WORM_TAIL = 11, MOD_ENT_SHEEP = 12, MOD_ENT_PIG = 13,
};

// 引擎提供给模组的宿主 API（函数表，指针在 mod_init 传入，生命周期 = 本次启用期间）
typedef struct ModHostApi
{
    int abi;                    // == MOD_ABI_VERSION，模组应校验
    int world_w, world_h;       // 世界宽高

    void (*log)(const char *msg);           // 写日志（debug.log）
    void (*toast)(const char *msg);         // 屏幕提示

    double (*player_x)(void);               // 玩家坐标
    double (*player_y)(void);

    void (*spawn)(int type, int hp, int hurt, double x, double y);  // 生成实体
    void (*spawn_eow)(double x, double y);  // 召唤世界吞噬者

    void (*set_godeater)(int on);           // 神明吞噬者开关（影响世界吞噬者强度）
    void (*set_oi)(int on);                 // OI 风暴开关

    void  (*set_flag)(const char *key, int val);  // 通用 KV：模组存自己的状态
    int   (*get_flag)(const char *key);
    int   (*rnd)(int n);                    // [0, n) 随机数
} ModHostApi;

#define MOD_EXPORT extern "C" __declspec(dllexport)

// ---- 必选导出 ----
MOD_EXPORT int         mod_abi(void);          // 返回 MOD_ABI_VERSION
MOD_EXPORT const char *mod_name(void);         // 显示名（开始页模组列表里显示）

// ---- 可选导出（不实现则跳过）----
// MOD_EXPORT const char *mod_desc(void);
// MOD_EXPORT void        mod_init(const ModHostApi *api);
// MOD_EXPORT void        mod_shutdown(void);
// MOD_EXPORT void        mod_world_ready(void);
// MOD_EXPORT void        mod_tick(void);
