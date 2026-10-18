// ===================== 模组加载器实现（Win32 DLL 插件）=====================
#include "mod_loader.h"
#include "mod_api.h"

#include "../platform/platform.h"
#include "../world/world.h"
#include "../entity/entity.h"
#include "../game/game.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace mods {

namespace {

ModInfo g_mods[MAX_MODS];
i32 g_n = 0;

using FnAbi        = int (*)(void);
using FnName       = const char *(*)(void);
using FnDesc       = const char *(*)(void);
using FnInit       = void (*)(const ModHostApi *);
using FnVoid       = void (*)(void);

std::unordered_map<std::string, i32> g_flags;

// ---------------- 宿主 API 实现 ----------------
void h_log(const char *m)   { platform::log("[mod] %s", m ? m : ""); }
void h_toast(const char *m) { game::toast("%s", m ? m : ""); }
double h_px() { return game::st().pl.x; }
double h_py() { return game::st().pl.y; }
void h_spawn(int type, int hp, int hurt, double x, double y)
{
    entity::spawn(type, (f64)hp, (f64)hurt, x, y);
}
void h_spawn_eow(double x, double y) { entity::spawn_eow(x, y); }
void h_set_godeater(int on) { world::godeater = on ? 1 : 0; }
void h_set_oi(int on)       { world::oi = on ? 1 : 0; }
void h_set_flag(const char *k, int v) { if(k) g_flags[k] = v; }
int  h_get_flag(const char *k)
{
    std::unordered_map<std::string, i32>::const_iterator it = g_flags.find(k ? k : "");
    return it == g_flags.end() ? 0 : it->second;
}
int  h_rnd(int n) { return n > 0 ? (i32)world::rnd(n) : 0; }

ModHostApi g_api;
bool g_api_ready = false;

void build_api()
{
    if(g_api_ready) return;
    g_api.abi = MOD_ABI_VERSION;
    g_api.world_w = world::W;
    g_api.world_h = world::H;
    g_api.log = h_log;
    g_api.toast = h_toast;
    g_api.player_x = h_px;
    g_api.player_y = h_py;
    g_api.spawn = h_spawn;
    g_api.spawn_eow = h_spawn_eow;
    g_api.set_godeater = h_set_godeater;
    g_api.set_oi = h_set_oi;
    g_api.set_flag = h_set_flag;
    g_api.get_flag = h_get_flag;
    g_api.rnd = h_rnd;
    g_api_ready = true;
}

// 解析一个模组的导出表（句柄已加载）
void resolve_entry(ModInfo &m, HMODULE hm)
{
    m.fn_init = (void *)GetProcAddress(hm, "mod_init");
    m.fn_shutdown = (void *)GetProcAddress(hm, "mod_shutdown");
    m.fn_world_ready = (void *)GetProcAddress(hm, "mod_world_ready");
    m.fn_tick = (void *)GetProcAddress(hm, "mod_tick");
}

}   // namespace

i32 count() { return g_n; }
ModInfo *at(i32 i) { return (i >= 0 && i < g_n) ? &g_mods[i] : nullptr; }
bool any_loaded()
{
    for(i32 i = 0; i < g_n; i++)
        if(g_mods[i].loaded) return true;
    return false;
}

void unload_all()
{
    for(i32 i = 0; i < g_n; i++)
    {
        ModInfo &m = g_mods[i];
        if(m.loaded && m.handle)
        {
            if(m.fn_shutdown) ((FnVoid)m.fn_shutdown)();
            FreeLibrary((HMODULE)m.handle);
        }
        m.handle = nullptr;
        m.fn_init = m.fn_shutdown = m.fn_world_ready = m.fn_tick = nullptr;
        m.loaded = false;
    }
}

void scan()
{
    unload_all();
    for(i32 i = 0; i < g_n; i++) g_mods[i] = ModInfo{};
    g_n = 0;

    WIN32_FIND_DATAA fd;
    HANDLE fh = FindFirstFileA("mods\\*.dll", &fd);
    if(fh == INVALID_HANDLE_VALUE)
    {
        platform::log("[mod] 未找到 mods/ 目录或目录为空");
        return;
    }
    do
    {
        if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if(g_n >= MAX_MODS) break;

        ModInfo &m = g_mods[g_n];
        snprintf(m.file, sizeof(m.file), "%s", fd.cFileName);
        m.valid = false;

        char path[256];
        snprintf(path, sizeof(path), "mods\\%s", fd.cFileName);
        HMODULE hm = LoadLibraryA(path);
        if(!hm)
        {
            platform::log("[mod] 加载失败: %s (err %lu)", fd.cFileName,
                          (unsigned long)GetLastError());
            g_n++;
            continue;
        }
        FnAbi fabi = (FnAbi)GetProcAddress(hm, "mod_abi");
        FnName fname = (FnName)GetProcAddress(hm, "mod_name");
        FnDesc fdesc = (FnDesc)GetProcAddress(hm, "mod_desc");
        if(fabi && fname && fabi() == MOD_ABI_VERSION)
        {
            const char *nm = fname();
            snprintf(m.name, sizeof(m.name), "%s", nm ? nm : fd.cFileName);
            const char *de = fdesc ? fdesc() : nullptr;
            snprintf(m.desc, sizeof(m.desc), "%s", de ? de : "");
            m.valid = true;
            platform::log("[mod] 发现模组: %s (%s)", m.name, m.file);
        }
        else
        {
            snprintf(m.name, sizeof(m.name), "%s", fd.cFileName);
            platform::log("[mod] 忽略 %s：缺少 mod_abi/mod_name 或 ABI 不匹配",
                          fd.cFileName);
        }
        FreeLibrary(hm);
        g_n++;
    } while(FindNextFileA(fh, &fd));
    FindClose(fh);
}

void set_enabled(i32 i, bool on)
{
    if(i >= 0 && i < g_n) g_mods[i].enabled = on;
}

void load_selection()
{
    FILE *fp = fopen("mods.cfg", "r");
    if(!fp) return;
    char line[128];
    while(fgets(line, sizeof(line), fp))
    {
        char *p = line;
        while(*p == ' ' || *p == '\t') p++;
        i32 len = (i32)strlen(p);
        while(len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' ||
                          p[len - 1] == ' ')) p[--len] = 0;
        if(len == 0 || p[0] == '#') continue;
        for(i32 i = 0; i < g_n; i++)
            if(strcmp(g_mods[i].file, p) == 0) g_mods[i].enabled = true;
    }
    fclose(fp);
}

void save_selection()
{
    FILE *fp = fopen("mods.cfg", "w");
    if(!fp) return;
    fprintf(fp, "# 启用的模组（每行一个 dll 文件名）\n");
    for(i32 i = 0; i < g_n; i++)
        if(g_mods[i].enabled) fprintf(fp, "%s\n", g_mods[i].file);
    fclose(fp);
}

void load_enabled()
{
    build_api();
    unload_all();
    for(i32 i = 0; i < g_n; i++)
    {
        ModInfo &m = g_mods[i];
        if(!m.enabled || !m.valid) continue;
        char path[256];
        snprintf(path, sizeof(path), "mods\\%s", m.file);
        HMODULE hm = LoadLibraryA(path);
        if(!hm) { platform::log("[mod] 启用失败: %s", m.file); continue; }
        m.handle = (void *)hm;
        resolve_entry(m, hm);
        m.loaded = true;
        if(m.fn_init) ((FnInit)m.fn_init)(&g_api);
        platform::log("[mod] 已启用: %s", m.name);
    }
}

void world_ready()
{
    for(i32 i = 0; i < g_n; i++)
        if(g_mods[i].loaded && g_mods[i].fn_world_ready)
            ((FnVoid)g_mods[i].fn_world_ready)();
}

void tick()
{
    for(i32 i = 0; i < g_n; i++)
        if(g_mods[i].loaded && g_mods[i].fn_tick)
            ((FnVoid)g_mods[i].fn_tick)();
}

void flag_set(const char *key, i32 val) { h_set_flag(key, val); }
i32  flag_get(const char *key) { return h_get_flag(key); }

}   // namespace mods
