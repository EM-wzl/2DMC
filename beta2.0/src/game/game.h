#pragma once
// ===================== 游戏层 =====================
// 负责把世界/实体/渲染拼成一个可玩的循环：输入 -> 玩家 -> 怪物 -> 碰撞 -> 渲染。
#include "../core/types.h"
#include "../entity/entity.h"
#include "../world/world.h"

namespace game {

// ---------------- 武器 ----------------
enum WeaponKind : i32 { WK_MELEE = 0, WK_GUN, WK_BOW };

struct WeaponDef
{
    const char *cn;
    i32 itemId;      // 对应方块/物品表 id（用于图标与掉落）
    i32 kind;
    f32 dmg;
    f32 cd;          // 攻击间隔（秒）
    f32 spread;      // 散布（弧度）
    i32 pellets;     // 每次发射弹丸数
    f32 speed;       // 弹速（格/秒）
    i32 energy;      // 能量消耗
    f32 knock;       // 击退
    f32 range;       // 近战范围 / 弹丸最大飞行距离（格）
    i32 pierce;      // 穿透次数
};

const WeaponDef *weapon_of(i32 itemId);
// 该 itemId 是否是武器
bool is_weapon(i32 itemId);

// ---------------- 背包 ----------------
// 与控制台版一致：30 格（前 9 格 = 快捷栏，1-9 键 / 滚轮选中）；
// 同类物品叠加到 64，武器不可叠加、每件独立占一格；格内物品可用鼠标自由排列。
struct InvSlot
{
    i32 id;       // 物品 id（0 = 空）
    i32 count;    // 叠加数量（武器恒为 1）
};

constexpr i32 INV_SLOTS = 30;    // 总格数
constexpr i32 HOT_SLOTS  = 9;    // 快捷栏格数（inv[0..8]）
constexpr i32 INV_COLS = 5, INV_ROWS = 6;   // 背包网格 5 列 x 6 行
constexpr f32 INV_CELL = 64.0f;  // 背包格边长（像素）
constexpr f32 INV_GAP  = 10.0f;  // 背包格间距
constexpr f32 HOT_CELL = 56.0f;  // HUD 快捷栏格边长

// ---------------- 飘字 ----------------
struct FloatText
{
    Vec2 pos;
    f32 vy;
    f32 life, lifeMax;
    f32 value;
    i32 color;
    bool crit;
    i32 kind;        // 0=伤害 1=提示
    char text[24];
};

// ---------------- 玩家 ----------------
struct Player
{
    Vec2 pos;
    Vec2 vel;
    f32 radius = 0.34f;
    f32 speed = 4.40f;              // 格/秒（较旧版明显放慢，留出躲避空间）
    f32 hp = 200, hpMax = 200;
    f32 armor = 120, armorMax = 200;
    f32 energy = 100, energyMax = 100;
    f32 aim = 0;
    f32 fireCd = 0;
    f32 invuln = 0;
    f32 flash = 0;
    f32 dashCd = 0;
    f32 dashT = 0;
    Vec2 dashDir;
    i32 weapon = B_IRON_SWORD;
    i32 coins = 0;
    f32 anim = 0;
    f32 muzzle = 0;
    f32 recoil = 0;
};

// ---------------- 局内统计（成就用）----------------
struct RunStats
{
    i32 kills = 0;
    i32 level = 1;
    i32 roomsCleared = 0;
    i32 damageTaken = 0;
    i32 noHitRooms = 0;
    f32 elapsed = 0;
};

// ---------------- 全局状态 ----------------
struct State
{
    world::Level level;
    Player pl;
    RunStats stats;
    std::vector<FloatText> floats;
    i32 roomDmgStart = 0;    // 进入当前战斗房时的 damageTaken 快照（无伤房判定，不存档）

    i32 currentLevel = 1;
    i32 currentRoomIdx = -1;
    u32 runSeed = 0;         // 本局世界种子：关卡几何由 (种子, 关卡号) 确定性重算
    bool inGame = false;
    bool paused = false;

    // ---- 覆盖层（打开时暂停世界，渲染照常）----
    bool talentOpen = false;  // 关卡开始的天赋三选一
    bool shopOpen = false;    // 商店面板
    bool invOpen = false;     // 背包面板
    i32 talentCount = 0;      // 本局已选天赋次数

    // ---- 背包 ----
    InvSlot inv[INV_SLOTS] = {};  // 0..8 = 快捷栏
    i32 hotSel = 0;               // 当前选中的快捷栏槽
    InvSlot held = {};            // 背包界面里拿在鼠标上的物品
    bool holding = false;

    // 相机（平滑跟随）
    Vec2 cam;
    f32 shake = 0;
    f32 hitStop = 0;         // 命中定格，增强打击感
    f32 msgTimer = 0;
    char msg[128] = {};
    i32 screen = 0;          // 0=标题 1=游戏中 2=开局选武器
    f32 transition = 0;      // 换关淡入
};

State &st();
Player &player();

// ---------------- 生命周期 ----------------
void init();
void new_run();
// offerTalent=true 时关卡开始弹天赋三选一（新开/过关弹，读档/死亡重挑不弹）
void start_level(i32 n, bool offerTalent);
void abandon_run();
// 死亡重挑：满状态重进本关第一个房间（旧版规则）
void restart_level();

// ---- 天赋（旧版规则：生命上限+25 / 护甲上限+50 / 能量上限+50，上限与当前都加）----
void apply_talent(i32 k);         // k: 1/2/3

// ---- 背包 ----
bool inv_add(i32 id, i32 n);      // 入包：同类先叠加（武器不可叠加），放不下返回 false
void inv_select(i32 slot);        // 选中快捷栏槽（0..8）并刷新手持武器
void inv_refresh_weapon();        // p.weapon = 选中槽的武器（空槽 = 拳头 -1）
void inv_wheel(f32 d);           // 滚轮切换快捷栏选中槽
i32  inv_slot_at(i32 mx, i32 my); // 背包界面命中测试，返回槽下标（-1 = 没点到格子）
void inv_click(i32 button);      // 背包界面点击：0=左键拿起/放下，1=右键装备/使用
void inv_close();                // 关闭背包（手上物品放回背包，放不下丢在地上）
// 背包网格左上角像素坐标（渲染与命中测试共用，保证一致）
f32  inv_grid_x0();
f32  inv_grid_y0();

// ---- 商店 / 雇佣兵 ----
void shop_buy(i32 k);             // 商店购买：1=护甲药水15金 2=大治疗20金 3=雇佣兵30金
bool hire_merc();                // 召唤雇佣兵随从（已有则失败）

// ---------------- 开发/验证开关（命令行 --room / --crowd 用）----------------
void warp_to_room(i32 idx);       // 把玩家直接送进第 idx 个房间（复用真实进房逻辑）
void debug_spawn_crowd(i32 n);    // 在玩家附近挤出一群怪，用于验证"不可重叠"
void debug_give_weapon(i32 itemId);   // 在玩家旁边丢一把武器（验证掉落不消失）
void debug_swap_weapon(i32 itemId);  // 直接入包并选中该武器（验证背包装备链）
void debug_add_item(i32 itemId, i32 n);  // 直接入包 n 个物品（--item 验证叠加/背包 UI）

// ---------------- 存档（局内进度）----------------
// 只存玩家状态与进度；关卡几何由 (runSeed, 关卡号) 重算，所以存档很小
bool save_run();
bool load_run();
bool has_save();

// ---------------- 每帧 ----------------
void update(f32 dt);
void render_world();
void render_hud();

// ---------------- 工具 ----------------
void add_float(f32 x, f32 y, f32 value, bool crit);
void add_float_text(f32 x, f32 y, const char *text, i32 color);
void show_msg(const char *fmt, ...);
void damage_player(f32 dmg, f32 fromX, f32 fromY);
void damage_monster(i32 idx, f32 dmg, bool crit, f32 fromX, f32 fromY);
bool point_blocked(const world::Level &lv, f32 x, f32 y);
// 爆炸：范围伤害 + 冲击特效（hurtsPlayer=false 只做特效）
void boom(f32 x, f32 y, f32 radius, f32 dmg, bool hurtsPlayer, i32 skipIdx = -1);
Vec2 camera_mouse_world();
// 整屏渲染（世界 pass + UI pass），供 main 调用
void render_all();

}   // namespace game