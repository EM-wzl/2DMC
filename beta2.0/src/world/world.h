#pragma once
// ===================== 关卡世界 =====================
// 新版只保留「当前地牢关卡」这一张网格（旧版 10000x1001 的主世界地形生成链已废弃）：
// 关卡尺寸固定、可确定性重算，所以存档只需保存玩家状态与房间清空进度，
// 不再需要 49MB 的整世界快照。
//
// 房间几何沿用旧版约定，保证与既有设计/经验一致：
//   房间 (L,c) 外墙左上角 = (c*20+1, L*14+1)，含墙 20x14，内部可活动 18x12
//   层间直接相邻（无走廊），门洞在墙行 x0+9 / x0+10 两格
#include "../core/types.h"
#include "../world/tiles.h"

namespace world {

constexpr i32 ROOM_W = 20;      // 含墙宽
constexpr i32 ROOM_H = 14;      // 含墙高
// 网格上限随关卡规模曲线（6+min(10,n) 最多 16 间）放大到 10 列 x 16 层
constexpr i32 MAX_LAYERS = 16;
constexpr i32 MAX_COLS = 10;

// 关卡网格尺寸：最宽 10 间 * 20 = 200，最高 16 层 * 14 = 224，再多留边
constexpr i32 MAX_W = MAX_COLS * ROOM_W + 8;
constexpr i32 MAX_H = MAX_LAYERS * ROOM_H + 8;

enum RoomState : u8
{
    ROOM_LOCKED = 0,   // 未进入
    ROOM_FIGHT = 1,    // 战斗中
    ROOM_CLEAR = 2,    // 已清空
    ROOM_SAFE = 3,     // 安全房（商人房，永不刷怪）
};

struct Room
{
    i32 layer = 0, col = 0;
    i32 x0 = 0, y0 = 0;      // 外墙左上角
    u8  state = ROOM_LOCKED;
    i32 wavesTotal = 0;
    i32 wavesDone = 0;
    i32 aliveLeft = 0;
    f32 spawnCd = 0;          // 下一波倒计时（帧@60 基准；f32 防高刷下截断成 0）
    bool finalRoom = false;
    bool hasChest = false;
    bool shop = false;        // 商店房（安全，含商人与扭蛋机）
    bool sealed = false;      // 战斗房是否已落闸（未落闸不刷怪、不关门）
    i32 depth = 0;            // 距起始房的 BFS 深度
    i32 link[4] = {-1, -1, -1, -1};   // 四向连接的房间下标：0上 1下 2左 3右（-1=无连接）
};

struct Level
{
    i32 index = 0;                       // 第几关（1 起）
    i32 width = 0, height = 0;           // 有效网格尺寸
    std::vector<u16> tiles;              // width*height，存方块 id
    std::vector<u8> light;               // width*height，0..15
    std::vector<Room> rooms;            // 全部房间（含安全房/商店房）
    u32 seed = 0;

    // ---- 基本访问 ----
    bool in_bounds(i32 x, i32 y) const
    {
        return x >= 0 && y >= 0 && x < width && y < height;
    }
    i32 get(i32 x, i32 y) const
    {
        if(!in_bounds(x, y)) return B_WALL;   // 界外按实心处理，防止越界跑出地图
        return tiles[(size_t)y * width + x];
    }
    void set(i32 x, i32 y, i32 id)
    {
        if(!in_bounds(x, y)) return;
        tiles[(size_t)y * width + x] = (u16)id;
    }
    bool solid(i32 x, i32 y) const { return tile_solid(get(x, y)); }
    // 浮点坐标取整查方块
    i32 get_at(f32 fx, f32 fy) const
    {
        return get((i32)std::floor(fx), (i32)std::floor(fy));
    }
    bool solid_at(f32 fx, f32 fy) const { return tile_solid(get_at(fx, fy)); }
    u8 light_at(i32 x, i32 y) const
    {
        if(!in_bounds(x, y)) return 0;
        return light[(size_t)y * width + x];
    }

    // ---- 房间查询 ----
    Room *room(i32 layer, i32 col);
    const Room *room(i32 layer, i32 col) const;
    Room *room_at(f32 wx, f32 wy);        // 世界坐标落在哪个房间（无则 nullptr）
    const Room *room_at(f32 wx, f32 wy) const;
    i32 room_idx(i32 layer, i32 col) const;

    // ---- 门 ----
    // mode: 0=本房所有「已连接」的门全开 1=全关（未连接的方向永远没有门）
    void door_set(i32 layer, i32 col, i32 mode);
    // 改写一对连接房间的门洞（门洞占两格宽、两层墙，两侧同时写）
    void door_both_sides(i32 layer, i32 col, i32 dir, bool open);
    // 圆(x,y,radius) 是否已远离该房间所有门洞区域——落闸关门前的安全检查，
    // 保证门永远不会关在玩家/随从身上（修复"进门被墙卡住"）
    bool door_clear(const Room &r, f32 x, f32 y, f32 radius) const;
};

// 生成第 n 关（确定性：同 seed + 同关卡结果一致）
void generate(Level &lv, i32 n, u32 seed);

// 重新计算光照（地牢为全局亮 + 火把/岩浆局部增强，够用且省 CPU）
void update_light(Level &lv);

}   // namespace world