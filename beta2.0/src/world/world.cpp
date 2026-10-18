// ===================== 关卡世界实现 =====================
#include "world.h"

namespace world {

i32 Level::room_idx(i32 layer, i32 col) const
{
    for(size_t i = 0; i < rooms.size(); i++)
        if(rooms[i].layer == layer && rooms[i].col == col) return (i32)i;
    return -1;
}

Room *Level::room(i32 layer, i32 col)
{
    i32 i = room_idx(layer, col);
    return i < 0 ? nullptr : &rooms[(size_t)i];
}

const Room *Level::room(i32 layer, i32 col) const
{
    i32 i = room_idx(layer, col);
    return i < 0 ? nullptr : &rooms[(size_t)i];
}

Room *Level::room_at(f32 wx, f32 wy)
{
    i32 ix = (i32)std::floor(wx), iy = (i32)std::floor(wy);
    for(size_t i = 0; i < rooms.size(); i++)
    {
        const Room &r = rooms[i];
        if(ix >= r.x0 && ix < r.x0 + ROOM_W && iy >= r.y0 && iy < r.y0 + ROOM_H)
            return &rooms[i];
    }
    return nullptr;
}

const Room *Level::room_at(f32 wx, f32 wy) const
{
    i32 ix = (i32)std::floor(wx), iy = (i32)std::floor(wy);
    for(size_t i = 0; i < rooms.size(); i++)
    {
        const Room &r = rooms[i];
        if(ix >= r.x0 && ix < r.x0 + ROOM_W && iy >= r.y0 && iy < r.y0 + ROOM_H)
            return &rooms[i];
    }
    return nullptr;
}

namespace {

// 两层房间之间的竖直门：上门底墙(y0+13) 与 下门顶墙(y0+14) 的同两列
void open_door_v(Level &lv, Room &above, Room &below, bool open)
{
    i32 v = open ? B_AIR : B_WALL;
    lv.set(above.x0 + 9, above.y0 + ROOM_H - 1, v);
    lv.set(above.x0 + 10, above.y0 + ROOM_H - 1, v);
    lv.set(below.x0 + 9, below.y0, v);
    lv.set(below.x0 + 10, below.y0, v);
}

void open_door_h(Level &lv, Room &left, Room &right, bool open)
{
    i32 v = open ? B_AIR : B_WALL;
    lv.set(left.x0 + ROOM_W - 1, left.y0 + 6, v);
    lv.set(left.x0 + ROOM_W - 1, left.y0 + 7, v);
    lv.set(right.x0, right.y0 + 6, v);
    lv.set(right.x0, right.y0 + 7, v);
}

void carve_room(Level &lv, Room &r)
{
    i32 x0 = r.x0, y0 = r.y0;
    // 内部清空
    for(i32 y = y0 + 1; y < y0 + ROOM_H - 1; y++)
        for(i32 x = x0 + 1; x < x0 + ROOM_W - 1; x++)
            lv.set(x, y, B_AIR);
    // 四角火把（装饰 + 局部光源）
    lv.set(x0 + 2, y0 + 2, B_TORCH);
    lv.set(x0 + ROOM_W - 3, y0 + 2, B_TORCH);
    lv.set(x0 + 2, y0 + ROOM_H - 3, B_TORCH);
    lv.set(x0 + ROOM_W - 3, y0 + ROOM_H - 3, B_TORCH);
}

// dir 的反方向（0上↔1下 2左↔3右）
inline i32 opposite_dir(i32 d) { return d == 0 ? 1 : (d == 1 ? 0 : (d == 2 ? 3 : 2)); }

}   // namespace

void Level::door_set(i32 layer, i32 col, i32 mode)
{
    i32 idx = room_idx(layer, col);
    if(idx < 0) return;
    Room &r = rooms[(size_t)idx];
    for(i32 d = 0; d < 4; d++)
    {
        if(r.link[d] < 0) continue;         // 未连接的方向根本没有门
        door_both_sides(layer, col, d, mode == 0);
    }
}

void Level::door_both_sides(i32 layer, i32 col, i32 dir, bool open)
{
    i32 idx = room_idx(layer, col);
    if(idx < 0 || dir < 0 || dir > 3) return;
    Room &r = rooms[(size_t)idx];
    i32 j = r.link[dir];
    if(j < 0 || j >= (i32)rooms.size()) return;
    Room &nb = rooms[(size_t)j];
    if(dir == 0)      open_door_v(*this, nb, r, open);   // 上邻
    else if(dir == 1) open_door_v(*this, r, nb, open);   // 下邻
    else if(dir == 2) open_door_h(*this, nb, r, open);   // 左邻
    else              open_door_h(*this, r, nb, open);   // 右邻
}

bool Level::door_clear(const Room &r, f32 x, f32 y, f32 radius) const
{
    // 门洞区域覆盖两侧墙各 1 格、门两格宽：
    //   竖直门: 列 [x0+9, x0+11) × 行 [y0-1, y0+1)（上）/ [y0+13, y0+15)（下）
    //   水平门: 列 [x0-1, x0+1)（左）/ [x0+19, x0+21)（右）× 行 [y0+6, y0+8)
    for(i32 d = 0; d < 4; d++)
    {
        if(r.link[d] < 0) continue;
        f32 ax0, ay0, ax1, ay1;
        if(d == 0)
        {
            ax0 = (f32)(r.x0 + 9);  ax1 = (f32)(r.x0 + 11);
            ay0 = (f32)(r.y0 - 1);  ay1 = (f32)(r.y0 + 1);
        }
        else if(d == 1)
        {
            ax0 = (f32)(r.x0 + 9);  ax1 = (f32)(r.x0 + 11);
            ay0 = (f32)(r.y0 + ROOM_H - 1);  ay1 = (f32)(r.y0 + ROOM_H + 1);
        }
        else if(d == 2)
        {
            ax0 = (f32)(r.x0 - 1);  ax1 = (f32)(r.x0 + 1);
            ay0 = (f32)(r.y0 + 6);  ay1 = (f32)(r.y0 + 8);
        }
        else
        {
            ax0 = (f32)(r.x0 + ROOM_W - 1);  ax1 = (f32)(r.x0 + ROOM_W + 1);
            ay0 = (f32)(r.y0 + 6);  ay1 = (f32)(r.y0 + 8);
        }
        f32 px = clampf(x, ax0, ax1), py = clampf(y, ay0, ay1);
        f32 dx = x - px, dy = y - py;
        if(dx * dx + dy * dy < radius * radius) return false;
    }
    return true;
}

void generate(Level &lv, i32 n, u32 seed)
{
    if(n < 1) n = 1;
    lv.index = n;
    lv.seed = seed;
    lv.width = MAX_W;
    lv.height = MAX_H;
    lv.tiles.assign((size_t)lv.width * lv.height, (u16)B_WALL);
    lv.light.assign((size_t)lv.width * lv.height, (u8)0);

    Rng rng;
    rng.seed(((u64)seed << 16) ^ ((u64)(u32)n * 2654435761ull) ^ 0x9E3779B9ull);

    // ---- 1) 随机生长出一棵房间树（分叉路线，不再是一条链）----
    // 从起始房开始随机蔓延：往下权重最高，左右次之，偶尔往上。
    // 已有房间旁再撞到已占格子时，有几率补一条「环路」连接。
    // 第 1 关 7 间、逐关 +1、上限 16 间（网格 10x16，高关尝试次数放宽保证放满）
    i32 want = 6 + std::min(10, n);
    static bool used[MAX_LAYERS][MAX_COLS];
    for(i32 L = 0; L < MAX_LAYERS; L++)
        for(i32 c = 0; c < MAX_COLS; c++) used[L][c] = false;
    lv.rooms.clear();

    auto add_room = [&](i32 L, i32 c) -> i32
    {
        Room r;
        r.layer = L;
        r.col = c;
        r.x0 = c * ROOM_W + 2;
        r.y0 = L * ROOM_H + 2;
        lv.rooms.push_back(r);
        used[L][c] = true;
        return (i32)lv.rooms.size() - 1;
    };

    i32 start = add_room(0, MAX_COLS / 2);
    i32 extraLinks = 0;
    i32 maxTries = 600 + n * 400;       // 关卡越大越难找到空格，尝试次数随之放宽
    for(i32 tries = 0; (i32)lv.rooms.size() < want && tries < maxTries; tries++)
    {
        i32 si = rng.range((i32)lv.rooms.size());
        i32 roll = rng.range(10);
        i32 d = (roll < 5) ? 1 : (roll < 7 ? 3 : (roll < 9 ? 2 : 0));  // 下5 右2 左2 上1
        i32 nl = lv.rooms[(size_t)si].layer + (d == 1 ? 1 : (d == 0 ? -1 : 0));
        i32 nc = lv.rooms[(size_t)si].col + (d == 3 ? 1 : (d == 2 ? -1 : 0));
        if(nl < 0 || nl >= MAX_LAYERS || nc < 0 || nc >= MAX_COLS) continue;
        if(used[nl][nc])
        {
            // 已有房间：有几率补一条额外连接，形成环路/捷径
            if(extraLinks < 2 && rng.chance(20))
            {
                i32 ti = lv.room_idx(nl, nc);
                if(ti >= 0 && lv.rooms[(size_t)si].link[d] < 0)
                {
                    lv.rooms[(size_t)si].link[d] = ti;
                    lv.rooms[(size_t)ti].link[opposite_dir(d)] = si;
                    extraLinks++;
                }
            }
            continue;
        }
        i32 ti = add_room(nl, nc);
        lv.rooms[(size_t)si].link[d] = ti;
        lv.rooms[(size_t)ti].link[opposite_dir(d)] = si;
    }

    // ---- 2) BFS 深度：选 Boss 房（最深处）与商店房（深度居中）----
    std::vector<i32> depth(lv.rooms.size(), -1);
    std::vector<i32> bfsq;
    depth[(size_t)start] = 0;
    bfsq.push_back(start);
    for(size_t qi = 0; qi < bfsq.size(); qi++)
    {
        i32 cur = bfsq[qi];
        for(i32 d = 0; d < 4; d++)
        {
            i32 nb = lv.rooms[(size_t)cur].link[d];
            if(nb < 0 || depth[(size_t)nb] >= 0) continue;
            depth[(size_t)nb] = depth[(size_t)cur] + 1;
            bfsq.push_back(nb);
        }
    }
    i32 maxDepth = 0, boss = start;
    for(size_t i = 0; i < lv.rooms.size(); i++)
    {
        lv.rooms[i].depth = depth[i] < 0 ? 0 : depth[i];
        if(depth[i] > maxDepth) { maxDepth = depth[i]; boss = (i32)i; }
    }
    lv.rooms[(size_t)boss].finalRoom = true;
    lv.rooms[(size_t)start].state = ROOM_SAFE;

    // 商店房：深度最接近中点的普通房（不放起始房/Boss 房）
    i32 shopIdx = -1, bestDiff = 1 << 30;
    for(size_t i = 0; i < lv.rooms.size(); i++)
    {
        if((i32)i == start || (i32)i == boss) continue;
        i32 diff = lv.rooms[i].depth * 2 - maxDepth;
        if(diff < 0) diff = -diff;
        if(diff < bestDiff) { bestDiff = diff; shopIdx = (i32)i; }
    }
    if(shopIdx >= 0)
    {
        lv.rooms[(size_t)shopIdx].shop = true;
        lv.rooms[(size_t)shopIdx].state = ROOM_SAFE;
    }

    // ---- 3) 挖空房间，打开「已连接」的门（其余方向永远是实墙）----
    for(Room &r : lv.rooms) carve_room(lv, r);
    for(size_t i = 0; i < lv.rooms.size(); i++)
        for(i32 d = 0; d < 4; d++)
        {
            i32 j = lv.rooms[i].link[d];
            if(j < 0) continue;
            if(d == 1)      open_door_v(lv, lv.rooms[i], lv.rooms[(size_t)j], true);
            else if(d == 0) open_door_v(lv, lv.rooms[(size_t)j], lv.rooms[i], true);
            else if(d == 3) open_door_h(lv, lv.rooms[i], lv.rooms[(size_t)j], true);
            else            open_door_h(lv, lv.rooms[(size_t)j], lv.rooms[i], true);
        }

    // ---- 4) 宝箱与设施 ----
    // 起始房：补给箱
    {
        Room &s = lv.rooms[(size_t)start];
        lv.set(s.x0 + ROOM_W - 4, s.y0 + ROOM_H / 2, B_CHEST);
        s.hasChest = true;
    }
    // Boss 房：大宝箱
    {
        Room &b = lv.rooms[(size_t)boss];
        lv.set(b.x0 + ROOM_W / 2 + 4, b.y0 + 3, B_CHEST);
        b.hasChest = true;
    }
    // 商店房：扭蛋机
    if(shopIdx >= 0)
    {
        Room &sh = lv.rooms[(size_t)shopIdx];
        lv.set(sh.x0 + ROOM_W - 5, sh.y0 + ROOM_H / 2, B_GACHA);
    }
    // 其余房间：叶子房 65% / 路中房 30% 放小宝箱
    for(size_t i = 0; i < lv.rooms.size(); i++)
    {
        if((i32)i == start || (i32)i == boss || (i32)i == shopIdx) continue;
        i32 deg = 0;
        for(i32 d = 0; d < 4; d++) if(lv.rooms[i].link[d] >= 0) deg++;
        if(!rng.chance(deg <= 1 ? 65 : 30)) continue;
        lv.set(lv.rooms[i].x0 + 3, lv.rooms[i].y0 + 3, B_CHEST);
        lv.rooms[i].hasChest = true;
    }

    update_light(lv);
}

void update_light(Level &lv)
{
    // 地牢全局亮：房间内部与门洞给 15，墙体给 7 以产生体积感，
    // 火把/岩浆周围额外提亮到 15 并向外衰减 1 格。
    for(i32 y = 0; y < lv.height; y++)
        for(i32 x = 0; x < lv.width; x++)
        {
            i32 id = lv.get(x, y);
            u8 v;
            if(id == B_AIR || id == B_TORCH || id == B_CHEST || id == B_GACHA
                    || id == B_LEVEL_PORTAL)
                v = 15;
            else if(id == B_WALL || id == B_EBONWALL)
                v = 7;
            else
                v = 11;
            lv.light[(size_t)y * lv.width + x] = v;
        }

    // 火把/岩浆的局部高光
    for(i32 y = 0; y < lv.height; y++)
        for(i32 x = 0; x < lv.width; x++)
        {
            i32 id = lv.get(x, y);
            if(id != B_TORCH && id != B_LAVA) continue;
            for(i32 dy = -2; dy <= 2; dy++)
                for(i32 dx = -2; dx <= 2; dx++)
                {
                    i32 nx = x + dx, ny = y + dy;
                    if(!lv.in_bounds(nx, ny)) continue;
                    if(std::abs(dx) + std::abs(dy) > 2) continue;
                    lv.light[(size_t)ny * lv.width + nx] = 15;
                }
        }
}

}   // namespace world
