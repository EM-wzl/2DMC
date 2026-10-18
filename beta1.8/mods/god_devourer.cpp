// ===================== 模组：神明吞噬者 =====================
// 复刻旧版 beta/beta1.8.cpp 的 mod「godeater」：
//   世界生成末尾执行  if(godeater) make_eow()  —— 召唤世界吞噬者；
//   同时 godeater 让体节数量 38 -> 68、移动速度 x2.5、体节跟随更紧（k 7 -> 3）。
//   这些数值影响在引擎 entity.cpp 里读取 world::godeater，模组只需把它打开。
#include "mod_api.h"

static const ModHostApi *g_api = nullptr;

MOD_EXPORT int mod_abi(void) { return MOD_ABI_VERSION; }
MOD_EXPORT const char *mod_name(void) { return "神明吞噬者"; }
MOD_EXPORT const char *mod_desc(void) { return "世界生成时召唤世界吞噬者：体节更多、更快、更强"; }

MOD_EXPORT void mod_init(const ModHostApi *api) { g_api = api; }
MOD_EXPORT void mod_shutdown(void) { g_api = nullptr; }

MOD_EXPORT void mod_world_ready(void)
{
    if(!g_api) return;
    g_api->set_godeater(1);
    // 旧版 make_eow() 以玩家坐标 (x, y) 为基准召唤
    g_api->spawn_eow(g_api->player_x(), g_api->player_y());
    g_api->toast("神明吞噬者：世界吞噬者苏醒了");
    g_api->log("神明吞噬者已生效：EOW 已召唤");
}
