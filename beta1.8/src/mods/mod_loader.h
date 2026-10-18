#pragma once
// ===================== 模组加载器 =====================
// 扫描 mods/*.dll，读取每个模组导出的名字/描述/ABI，按开始页里的勾选加载。
// 勾选结果持久化在 mods.cfg（每行一个启用的 dll 文件名）。
#include "../core/types.h"

namespace mods {

constexpr i32 MAX_MODS = 32;

struct ModInfo
{
    char file[80];      // mods/ 下的 dll 文件名
    char name[64];      // mod_name()
    char desc[160];     // mod_desc()（没导出则空）
    bool valid;         // ABI 匹配且能被加载
    bool enabled;       // 开始页里是否勾选
    bool loaded;        // 是否已载入（handle != null）
    void *handle;       // HMODULE
    void *fn_init, *fn_shutdown, *fn_world_ready, *fn_tick;   // 已解析的导出
};

// 扫描 mods/ 目录（可重复调用；会释放之前的句柄）
void scan();
i32  count();
ModInfo *at(i32 i);
void set_enabled(i32 i, bool on);

// 读/写 mods.cfg（scan 之后调用 load_selection）
void load_selection();
void save_selection();

// 载入所有已勾选且有效的模组（进世界前调用；可重复调用，会先卸载）
void load_enabled();
// 卸载全部已载入模组（回标题时调用）
void unload_all();
// 广播：世界生成完成 / 读档完成
void world_ready();
// 广播：每逻辑帧
void tick();
// 是否有任何已载入模组
bool any_loaded();

// 通用 KV（模组私有状态）
void flag_set(const char *key, i32 val);
i32  flag_get(const char *key);

}   // namespace mods
