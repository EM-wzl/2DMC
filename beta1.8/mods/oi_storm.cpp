// ===================== 模组：OI风暴 =====================
// 复刻旧版 beta/beta1.8.cpp 的 mod「oi」：
//   世界生成时执行  make_st(5, 500, 8, WORLD_WIDTH/2, 100)
//   —— 在世界中央召唤一只 OI（凋灵）。
#include "mod_api.h"

static const ModHostApi *g_api = nullptr;

MOD_EXPORT int mod_abi(void) { return MOD_ABI_VERSION; }
MOD_EXPORT const char *mod_name(void) { return "OI风暴"; }
MOD_EXPORT const char *mod_desc(void) { return "世界生成时在中央召唤一只 OI（凋灵）"; }

MOD_EXPORT void mod_init(const ModHostApi *api) { g_api = api; }
MOD_EXPORT void mod_shutdown(void) { g_api = nullptr; }

MOD_EXPORT void mod_world_ready(void)
{
    if(!g_api) return;
    g_api->set_oi(1);
    // 旧版：make_st(5, 500, 8, WORLD_WIDTH / 2, 100)
    g_api->spawn(MOD_ENT_OI, 500, 8, g_api->world_w / 2.0, 100.0);
    g_api->toast("OI风暴：世界中央出现了 OI");
    g_api->log("OI风暴已生效：OI 已生成");
}
