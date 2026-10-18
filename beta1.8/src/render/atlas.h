#pragma once
// ===================== 程序化纹理图集 =====================
// 不依赖任何图片素材：启动时用代码把方块/物品/实体/UI 画进一张 RGBA 图集，
// 再整体上传为一张 GL 纹理，运行时按格子索引取子矩形。
//
// 好处：无外部资源、体积小、改配色只改数据表；所有方块共享一次 draw call。
#include "../core/types.h"

namespace atlas {

constexpr i32 CELL = 32;                   // 单格像素
constexpr i32 COLS = 16;                   // 图集列数
constexpr i32 ROWS = 16;                   // 图集行数
constexpr i32 TEX_W = COLS * CELL;         // 512
constexpr i32 TEX_H = ROWS * CELL;         // 512
constexpr i32 MAX_SLOTS = COLS * ROWS;     // 256 格

// ---- 槽位分配 ----
enum Slot : i32
{
    S_WHITE = 0,                 // 纯白（画纯色矩形用）
    S_BLOCK = 1,                 // 1 .. 102 -> 方块/物品 id 0..101
    S_BLOCK_END = S_BLOCK + 102, // = 103
    S_ENTITY = 112,              // 112 .. -> 实体与特效
    S_UI = 160,                  // 160 .. -> UI 图标与控件
};

// ---- 方块 id -> 槽位 ----
inline i32 slot_of_block(i32 id)
{
    if(id < 0 || id > 101) return S_BLOCK;
    return S_BLOCK + id;
}

// ---- 双图：世界里的矿脉 vs 背包里的物品 ----
// 有几个 id 既是世界里嵌在石头里的矿脉、又是背包里的材料（煤 / 粗铁 /
// 钻石 / 粗金）。背包里要「单独的物品」，世界里要「嵌在石头里的矿石」，
// 一张图顾不了两头，于是给它们各留一个槽位存矿脉版贴图。
// 槽位放在 200 起（UI 结束于 186，186..255 都是空的）。
enum OreSlot : i32
{
    W_ORE_COAL = 200,
    W_ORE_RAW_IRON,
    W_ORE_DIAMOND,
    W_ORE_RAW_GOLD,
    W_ORE_NETHER_GOLD,
    W_ORE_DEBRIS,
    W_ORE_COUNT,
};

// 世界渲染用：矿石类方块返回「嵌在石头里」的槽位，其余仍用物品槽位
i32 slot_of_world_block(i32 id);

// ---- 覆盖层（不是方块，画在方块之上）----
// 旧版用字符色块表示地形，没有「草皮」概念。图形版为了好看，
// 给裸露在空气中的泥土/雪加一条草/雪的顶边，占用 103..110 空槽位。
enum OverlaySlot : i32
{
    O_GRASS_FRINGE = S_BLOCK_END + 0,   // 泥土顶着草
    O_SNOW_FRINGE,                      // 泥土/雪顶着雪
    O_GRASS_FRINGE_L,                   // 左侧悬空（草沿下垂）
    O_GRASS_FRINGE_R,
    O_COUNT_OVERLAY,
};

// ---- 实体贴图槽位 ----
enum EntSlot : i32
{
    E_ARROW_WOOD = S_ENTITY + 0,
    E_ARROW_IRON,
    E_ARROW_EXPLOSIVE,
    E_PLAYER,
    E_PLAYER_ARM,
    E_ZOMBIE,
    E_SKELETON,
    E_CREEPER,
    E_SPIDER,
    E_PIGLIN,
    E_SHEEP,
    E_PIG,
    E_OI,
    E_OI_SHIELDED,
    E_RECURSIVE_BIG,
    E_RECURSIVE_SMALL,
    E_WORM_HEAD,
    E_WORM_BODY,
    E_WORM_TAIL,
    E_WITHER,
    E_SHADOW,
    E_NPC,
    E_HEART_ENT,
    E_COUNT_ENT,
};

// ---- 特效槽位 ----
enum FxSlot : i32
{
    F_SPARK = S_ENTITY + 24,
    F_SMOKE,
    F_FLAME,
    F_BUBBLE,
    F_SPLASH,
    F_DUST,
    F_EXPLOSION,
    F_SHOCKWAVE,
    F_STAR,
    F_GLOW,
    F_CRACK0,      // 破坏进度裂纹 0..9
    F_CRACK1, F_CRACK2, F_CRACK3, F_CRACK4,
    F_CRACK5, F_CRACK6, F_CRACK7, F_CRACK8, F_CRACK9,
    F_COUNT_FX,
};

// ---- UI 槽位 ----
enum UiSlot : i32
{
    U_PANEL = S_UI,
    U_PANEL_DARK,
    U_SLOT,
    U_SLOT_HL,
    U_SLOT_SEL,
    U_CURSOR,
    U_CURSOR_CROSS,
    U_HEART_BG,
    U_HEART_FULL,
    U_HEART_HALF,
    U_ARMOR_ICON,
    U_BUBBLE,
    U_DUR_BG,        // ★ 耐久条底槽
    U_DUR_FILL,      // ★ 耐久条前景（用顶点色染色出绿→红）
    U_BROKEN,        // ★ 碎裂图标
    U_ARROW_L,
    U_ARROW_R,
    U_ARROW_UP,
    U_ARROW_DOWN,
    U_CHECK,
    U_CROSS,
    U_GEAR,
    U_MAP_PIN,
    U_BTN,
    U_BTN_HL,
    U_TITLE_BAR,
    U_COUNT_UI,
};

// ===================== MC 皮肤 =====================
// 64x64 的 MC 皮肤是「展开图」：头/身体/手臂/腿各有自己的贴图块（正面、
// 侧面、顶面……）。整张画出来是一堆错位的小方块，必须按布局把各部位的
// **正面**拼起来才是人物立绘。拼好的立绘是 16x32（宽高比 1:2），
// 存在槽位的**左半边**（右半边留空），绘制时用 draw_sub 取左半边。
// slot 指定写进哪个实体槽位（默认玩家；怪物皮肤复用同一套人形布局）。
void assemble_player_skin(const u32 *skin, i32 sw, i32 sh, i32 slot = E_PLAYER);

// ---- 换肤：assets/skins/*.png ----
void skin_scan();                       // 扫描皮肤目录
i32  skin_count();
const char *skin_name(i32 idx);         // 文件名（不含扩展名）
i32  skin_current();
void skin_set_current(i32 idx);
// 载入第 idx 张皮肤并拼进图集（只改 CPU 像素；
// 还要调 render::upload_atlas() 才会生效到显卡）
bool skin_apply(i32 idx);

// 生成全部纹理像素（CPU 端）
void build();

// 图集像素缓冲（RGBA，行主序，TEX_W x TEX_H）
const u32 *pixels();
i32 pixel_w();
i32 pixel_h();

// 取某槽位的 UV 矩形（已含半像素内缩，避免相邻格渗色）
struct Uv { f32 u0, v0, u1, v1; };
Uv uv_of(i32 slot);

}   // namespace atlas

