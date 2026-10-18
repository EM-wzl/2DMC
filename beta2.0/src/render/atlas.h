#pragma once
// ===================== 程序化纹理图集 =====================
// 不依赖任何图片素材：启动时用代码把方块与实体画进一张 RGBA 图集，
// 再整体上传为一张 GL 纹理，运行时按格子索引取子矩形。
// 好处是无外部资源、体积小、改配色只改数据表。
#include "../core/types.h"

namespace atlas {

constexpr i32 CELL = 32;          // 单格像素
constexpr i32 COLS = 16;          // 图集列数
constexpr i32 ROWS = 32;          // 图集行数
constexpr i32 MAX_SLOTS = COLS * ROWS;   // 512 格

// 槽位分配：与 block id 直接对应，便于查表和调试
enum Slot : i32
{
    S_WHITE = 0,            // 纯白（画纯色矩形用）
    S_TILE = 1,             // 1 .. 150  -> 方块 id 0..149
    S_TILE_END = S_TILE + 150,
    S_ENT = 256,            // 256 .. -> 实体/特效
    S_UI = 384,             // 384 .. -> UI 图标
};

// 方块 id -> 槽位
inline i32 slot_of_block(i32 id) { return S_TILE + (id < 0 ? 0 : (id > 149 ? 0 : id)); }

// 实体贴图槽位
enum EntSlot : i32
{
    E_ARROW = S_ENT + 0,
    E_ARROW_IRON,
    E_ARROW_FIRE,
    E_BULLET,
    E_ZOMBIE,
    E_SKELETON,
    E_CREEPER,
    E_SPIDER,
    E_PIGLIN,
    E_OI,
    E_RECURSIVE,
    E_WORM_HEAD,
    E_WORM_BODY,
    E_WORM_TAIL,
    E_SHEEP,
    E_PIG,
    E_PLAYER,
    E_BOSS,
    E_COIN,
    E_SPARK,
    E_EXPLOSION,
    E_MUZZLE,
    E_SHADOW,
    E_HEART,
    E_SKULL,
    E_COUNT,
};

// UI 图标槽位
enum UiSlot : i32
{
    U_BAR_BG = S_UI + 0,
    U_BAR_HP,
    U_BAR_ARMOR,
    U_BAR_ENERGY,
    U_PANEL,
    U_SLOT,
    U_CURSOR,
    U_ARROW_UP,
    U_ARROW_DOWN,
    U_STAR,
    U_KEY,
    U_CIRCLE,
    U_RING,
    U_COUNT,
};

// 生成全部纹理像素（CPU 端）
void build();

// 图集像素缓冲（RGBA，行主序，COLS*CELL x ROWS*CELL）
const u32 *pixels();
i32 pixel_w();
i32 pixel_h();

// 取某槽位的 UV 矩形（已含半像素内缩，避免相邻格渗色）
struct Uv { f32 u0, v0, u1, v1; };
Uv uv_of(i32 slot);

}   // namespace atlas