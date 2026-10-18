// ===================== 程序化纹理生成 =====================
// 所有方块/物品/实体/UI 贴图都在这里用几何图元画出来，无外部素材依赖。
//
// 设计要点（让 32x32 的格子在 16px 显示尺寸下依然好看）：
//   * 每种样式都有「顶亮 / 底暗 / 左右描边」的体积感处理（bevel / edge_outline）
//   * 矿石 = 石底 + 矿脉团簇 + 高光点 + 深色描边，而不是无脑撒点
//   * 工具/护甲统一「斜置主体 + 暗描边 + 亮高光」，七套材质色板换色
//   * 水 / 岩浆 / 传送门带可动画分量（上层按时间做 UV 偏移）
//   * 所有随机都走固定种子的 Rng，保证每次启动纹理完全一致
#include "atlas.h"
#include "png_load.h"
#include "../world/blocks.h"

#include <cstring>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>   // skin_scan 扫描 assets/skins/*.png

namespace atlas {
namespace {

constexpr i32 AW = TEX_W;
constexpr i32 AH = TEX_H;
u32 g_px[AW * AH];

// ---------------- 颜色工具（前置声明，Canvas 内部要用）----------------
u32 mixc(u32 a, u32 b, f32 t);
u32 lighten(u32 c, f32 k);
u32 darken(u32 c, f32 k);
u32 with_a(u32 c, u8 a);
u8  alpha_of(u32 c);

// ---------------- 32x32 画布 ----------------
struct Canvas
{
    u32 px[CELL * CELL];

    void clear(u32 c = 0)
    {
        for(int i = 0; i < CELL * CELL; i++) px[i] = c;
    }
    static bool inb(int x, int y) { return x >= 0 && x < CELL && y >= 0 && y < CELL; }
    u32 get(int x, int y) const { return inb(x, y) ? px[y * CELL + x] : 0; }
    void set(int x, int y, u32 c) { if(inb(x, y)) px[y * CELL + x] = c; }

    // 源 over 目标 alpha 混合
    void blend(int x, int y, u32 c)
    {
        if(!inb(x, y)) return;
        u32 sa = (c >> 24) & 0xFF;
        if(sa == 0) return;
        if(sa == 255) { px[y * CELL + x] = c; return; }
        u32 d = px[y * CELL + x];
        u32 dr = d & 0xFF, dg = (d >> 8) & 0xFF, db = (d >> 16) & 0xFF, da = (d >> 24) & 0xFF;
        u32 sr = c & 0xFF, sg = (c >> 8) & 0xFF, sb = (c >> 16) & 0xFF;
        u32 ra = sa + da * (255 - sa) / 255;
        if(ra == 0) { px[y * CELL + x] = 0; return; }
        u32 inv = da * (255 - sa) / 255;
        u32 rr = (sr * sa + dr * inv) / ra;
        u32 rg = (sg * sa + dg * inv) / ra;
        u32 rb = (sb * sa + db * inv) / ra;
        px[y * CELL + x] = rgba((u8)rr, (u8)rg, (u8)rb, (u8)ra);
    }

    // ---- 基本图元 ----
    void rect(int x, int y, int w, int h, u32 c)
    {
        for(int j = y; j < y + h; j++)
            for(int i = x; i < x + w; i++) blend(i, j, c);
    }
    void frame(int x, int y, int w, int h, u32 c, int t = 1)
    {
        rect(x, y, w, t, c);
        rect(x, y + h - t, w, t, c);
        rect(x, y, t, h, c);
        rect(x + w - t, y, t, h, c);
    }
    void disc(f32 cx, f32 cy, f32 r, u32 c)
    {
        if(r <= 0) return;
        for(int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++)
            for(int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++)
            {
                f32 dx = i + 0.5f - cx, dy = j + 0.5f - cy;
                if(dx * dx + dy * dy <= r * r) blend(i, j, c);
            }
    }
    void ellipse(f32 cx, f32 cy, f32 rx, f32 ry, u32 c)
    {
        if(rx < 0.01f) rx = 0.01f;
        if(ry < 0.01f) ry = 0.01f;
        for(int j = (int)(cy - ry - 1); j <= (int)(cy + ry + 1); j++)
            for(int i = (int)(cx - rx - 1); i <= (int)(cx + rx + 1); i++)
            {
                f32 dx = (i + 0.5f - cx) / rx, dy = (j + 0.5f - cy) / ry;
                if(dx * dx + dy * dy <= 1.0f) blend(i, j, c);
            }
    }
    void line(int x0, int y0, int x1, int y1, u32 c, int thick = 2)
    {
        int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;
        int guard = 0;
        while(guard++ < 512)
        {
            for(int oy = 0; oy < thick; oy++)
                for(int ox = 0; ox < thick; ox++)
                    blend(x0 + ox - thick / 2, y0 + oy - thick / 2, c);
            if(x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if(e2 > -dy) { err -= dy; x0 += sx; }
            if(e2 < dx) { err += dx; y0 += sy; }
        }
    }

    // ---- 光照明暗 ----
    void shade_rect(f32 k, int x0, int y0, int w, int h)
    {
        for(int j = y0; j < y0 + h; j++)
            for(int i = x0; i < x0 + w; i++)
            {
                if(!inb(i, j)) continue;
                u32 p = px[j * CELL + i];
                u32 a = (p >> 24) & 0xFF;
                if(a == 0) continue;
                f32 r = (p & 0xFF) * k, g = ((p >> 8) & 0xFF) * k, b = ((p >> 16) & 0xFF) * k;
                if(r > 255) r = 255;
                if(g > 255) g = 255;
                if(b > 255) b = 255;
                px[j * CELL + i] = rgba((u8)r, (u8)g, (u8)b, (u8)a);
            }
    }
    void shade_f(f32 k) { shade_rect(k, 0, 0, CELL, CELL); }

    // 顶亮底暗 + 左右微差：方块之间的体积感靠这个
    void bevel(f32 light_k, f32 dark_k)
    {
        for(int i = 0; i < CELL; i++)
        {
            for(int d = 0; d < 3; d++)
            {
                f32 k = 1.0f + (light_k - 1.0f) * (1.0f - d / 3.0f);
                shade_rect(k, i, d, 1, 1);
                f32 kd = 1.0f + (dark_k - 1.0f) * (1.0f - d / 3.0f);
                shade_rect(kd, i, CELL - 1 - d, 1, 1);
            }
        }
        for(int j = 0; j < CELL; j++)
        {
            shade_rect(1.0f + (light_k - 1.0f) * 0.45f, 0, j, 1, 1);
            shade_rect(1.0f + (dark_k - 1.0f) * 0.45f, CELL - 1, j, 1, 1);
        }
    }

    // 四周 1px 深色描边（让相邻方块能分辨）
    void edge_outline(u32 c)
    {
        for(int i = 0; i < CELL; i++)
        {
            blend(i, 0, c);
            blend(i, CELL - 1, c);
            blend(0, i, c);
            blend(CELL - 1, i, c);
        }
    }
    void outline(u32 c, int x, int y, int w, int h)
    {
        for(int i = x; i < x + w; i++) { blend(i, y, c); blend(i, y + h - 1, c); }
        for(int j = y; j < y + h; j++) { blend(x, j, c); blend(x + w - 1, j, c); }
    }

    // ---- 噪声 ----
    void specks(u32 c, int count, u32 seed)
    {
        Rng r;
        r.seed((u64)seed * 2654435761u + 12345u);
        for(int i = 0; i < count; i++) blend(r.range(CELL), r.range(CELL), c);
    }
    // 块状斑驳：比逐像素点更像岩石/矿脉
    void blotches(u32 c, int count, int size_min, int size_max, u32 seed)
    {
        Rng r;
        r.seed((u64)seed * 2246822519u + 9871u);
        for(int i = 0; i < count; i++)
        {
            int x = r.range(CELL), y = r.range(CELL);
            int w = r.range(size_min, size_max), h = r.range(size_min, size_max);
            rect(x, y, w, h, c);
        }
    }
    // 低频云噪声：双线性插值的 8x8 控制点
    void cloud_noise(f32 amount, u32 seed, u32 c_light, u32 c_dark)
    {
        Rng r;
        r.seed((u64)seed * 668265263u + 4321u);
        const int G = 8;
        f32 ctrl[G + 1][G + 1];
        for(int j = 0; j <= G; j++)
            for(int i = 0; i <= G; i++) ctrl[i][j] = r.unit();
        for(int y = 0; y < CELL; y++)
            for(int x = 0; x < CELL; x++)
            {
                f32 fx = (f32)x / CELL * G, fy = (f32)y / CELL * G;
                int x0 = (int)fx, y0 = (int)fy;
                if(x0 >= G) x0 = G - 1;
                if(y0 >= G) y0 = G - 1;
                f32 tx = fx - x0, ty = fy - y0;
                tx = tx * tx * (3 - 2 * tx);
                ty = ty * ty * (3 - 2 * ty);
                f32 v = ctrl[x0][y0] * (1 - tx) * (1 - ty) +
                        ctrl[x0 + 1][y0] * tx * (1 - ty) +
                        ctrl[x0][y0 + 1] * (1 - tx) * ty +
                        ctrl[x0 + 1][y0 + 1] * tx * ty;
                f32 d = v - 0.5f;
                if(d > 0)      blend(x, y, with_a(c_light, (u8)(d * 2 * amount * 255)));
                else if(d < 0) blend(x, y, with_a(c_dark, (u8)(-d * 2 * amount * 255)));
            }
    }
};

// ---------------- 颜色工具 ----------------
u32 mixc(u32 a, u32 b, f32 t)
{
    if(t < 0) t = 0;
    if(t > 1) t = 1;
    int ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    int br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF;
    int ba = (b >> 24) & 0xFF;
    return rgba((u8)(ar + (br - ar) * t), (u8)(ag + (bg - ag) * t),
                (u8)(ab + (bb - ab) * t), (u8)(aa + (ba - aa) * t));
}
u32 lighten(u32 c, f32 k) { return mixc(c, rgba(255, 255, 255, alpha_of(c)), k); }
u32 darken(u32 c, f32 k)  { return mixc(c, rgba(0, 0, 0, alpha_of(c)), k); }
u32 with_a(u32 c, u8 a)   { return (c & 0x00FFFFFFu) | ((u32)a << 24); }
u8  alpha_of(u32 c)       { return (u8)((c >> 24) & 0xFF); }

// ============================================================================
//                            方块贴图
// ============================================================================

void gen_rock(Canvas &c, u32 base, u32 dark, u32 lite, u32 seed)
{
    c.clear(base);
    c.cloud_noise(0.30f, seed, lite, dark);
    c.blotches(with_a(dark, 90), 22, 2, 5, seed + 1);
    c.blotches(with_a(lite, 70), 16, 1, 3, seed + 2);
    c.bevel(1.14f, 0.74f);
    c.edge_outline(with_a(darken(base, 0.45f), 190));
}

void gen_dirt(Canvas &c, u32 base, u32 accent, u32 seed)
{
    c.clear(base);
    // 对比度拉高：地面是画面里面积最大的东西，太平会显得很「塑料」
    c.cloud_noise(0.42f, seed, lighten(base, 0.42f), darken(accent, 0.48f));
    c.blotches(with_a(darken(accent, 0.35f), 170), 18, 1, 4, seed + 3);
    c.blotches(with_a(lighten(base, 0.50f), 150), 16, 1, 3, seed + 4);
    // 细颗粒：逐像素小点，近距离看有质感
    for(int k = 0; k < 160; k++)
    {
        Rng r;
        r.seed((u64)(seed + (u32)k) * 2246822519u);
        int x = r.range(CELL), y = r.range(CELL);
        c.blend(x, y, with_a(r.range(2) ? darken(accent, 0.45f) : lighten(base, 0.55f),
                             (u8)r.range(60, 150)));
    }
    c.bevel(1.12f, 0.74f);
    c.edge_outline(with_a(darken(accent, 0.55f), 200));
}

// 表层覆盖（雪 / 草）
void gen_grass(Canvas &c, u32 top, u32 soil, u32 seed, bool snow)
{
    gen_dirt(c, soil, darken(soil, 0.25f), seed);
    int th = snow ? 8 : 6;
    for(int x = 0; x < CELL; x++)
    {
        Rng r;
        r.seed((u64)(seed + (u32)x) * 2246822519u);
        int h = th;
        if(!snow) h += r.range(3) - 1;
        else      h += r.range(2);
        for(int y = 0; y < h; y++)
        {
            f32 t = (f32)y / (f32)h;
            u32 col = mixc(lighten(top, 0.18f * (1 - t)), darken(top, 0.12f * t), 0.5f);
            if(r.range(100) < 18) col = darken(col, 0.14f);
            c.set(x, y, col);
        }
        c.blend(x, h, mixc(top, soil, 0.65f));
        if(!snow && r.range(100) < 40) c.blend(x, h + 1, mixc(top, soil, 0.30f));
    }
    c.bevel(1.10f, 0.80f);
    c.edge_outline(with_a(darken(soil, 0.50f), 150));
}

void gen_log(Canvas &c, u32 bark, u32 dark, u32 seed)
{
    c.clear(bark);
    Rng r;
    r.seed((u64)seed * 668265263u + 77u);
    // 竖向树皮纹
    for(int x = 0; x < CELL; x++)
    {
        int v = r.range(3);
        u32 col = bark;
        if(v == 0) col = lighten(bark, 0.16f);
        if(v == 2) col = darken(bark, 0.18f);
        for(int y = 0; y < CELL; y++)
        {
            u32 p = col;
            if(r.range(100) < 12) p = darken(bark, 0.30f);
            c.set(x, y, p);
        }
    }
    // 树皮裂缝
    for(int k = 0; k < 5; k++)
    {
        int x = r.range(3, CELL - 5);
        int y0 = r.range(CELL), len = r.range(6, 16);
        for(int y = y0; y < y0 + len && y < CELL; y++)
            c.blend(x + ((y - y0) / 6), y, with_a(darken(dark, 0.30f), 170));
    }
    // 上下端面年轮
    for(int j = 0; j < 5; j++)
        for(int i = 0; i < CELL; i++)
        {
            f32 t = j / 5.0f;
            u32 ring = mixc(lighten(bark, 0.30f), dark, t * 0.7f);
            if(((i / 3) + j) % 3 == 0) ring = darken(ring, 0.10f);
            c.blend(i, j, ring);
            c.blend(i, CELL - 1 - j, darken(ring, 0.12f));
        }
    for(int rr = 2; rr <= 7; rr += 2)
        for(int a = 0; a < 64; a++)
        {
            f32 an = a * 3.14159f * 2 / 64;
            c.blend((int)(15.5f + std::cos(an) * rr), (int)(15.5f + std::sin(an) * rr),
                    with_a(darken(bark, 0.20f), 60));
        }
    c.bevel(1.12f, 0.78f);
    c.edge_outline(with_a(darken(bark, 0.50f), 180));
}

void gen_plank(Canvas &c, u32 base, u32 dark, u32 seed, bool grid)
{
    c.clear(base);
    Rng r;
    r.seed((u64)seed * 2654435761u + 31337u);
    const int PH = 8;
    for(int y = 0; y < CELL; y++)
        for(int x = 0; x < CELL; x++)
        {
            int row = y / PH;
            u32 col = (row % 2 == 0) ? lighten(base, 0.08f) : darken(base, 0.06f);
            int grain = (x * 7 + row * 13) % 23;
            if(grain == 0) col = darken(col, 0.13f);
            if(grain == 5) col = lighten(col, 0.10f);
            if(r.range(100) < 6) col = darken(col, 0.18f);
            c.set(x, y, col);
        }
    for(int y = PH; y < CELL; y += PH)
        for(int x = 0; x < CELL; x++) c.blend(x, y, with_a(dark, 210));
    if(grid)
    {
        for(int row = 0; row < CELL / PH; row++)
        {
            int x = (row % 2 == 0) ? 10 : 22;
            for(int y = row * PH + 1; y < row * PH + PH; y++)
                c.blend(x, y, with_a(dark, 170));
            int nx = (row % 2 == 0) ? 5 : 17;
            c.set(nx, row * PH + 2, lighten(base, 0.45f));
            c.set(nx + 1, row * PH + 2, darken(base, 0.25f));
        }
    }
    c.bevel(1.10f, 0.80f);
    c.edge_outline(with_a(dark, 200));
}

void gen_leaves(Canvas &c, u32 base, u32 dark, u32 seed)
{
    c.clear(0);
    Rng r;
    r.seed((u64)seed * 2246822519u + 555u);
    for(int k = 0; k < 26; k++)
    {
        f32 cx = r.unit(2, CELL - 2), cy = r.unit(2, CELL - 2);
        f32 rad = r.unit(3.0f, 6.5f);
        u32 col = base;
        int p = r.range(100);
        if(p < 28) col = lighten(base, 0.22f);
        else if(p < 55) col = darken(base, 0.20f);
        c.disc(cx, cy, rad, col);
    }
    for(int k = 0; k < 30; k++)
        c.blend(r.range(CELL), r.range(CELL), with_a(darken(dark, 0.35f), 150));
    for(int k = 0; k < 14; k++)
        c.set(r.range(1, CELL - 1), r.range(1, CELL - 1), 0);
    // 左上受光
    for(int y = 0; y < CELL; y++)
        for(int x = 0; x < CELL; x++)
        {
            if(!alpha_of(c.get(x, y))) continue;
            f32 t = (x + y) / (f32)(CELL * 2);
            f32 k = 0.30f - t * 0.60f;
            if(k > 0) c.blend(x, y, rgba(255, 255, 255, (u8)(k * 255)));
        }
    c.shade_f(0.94f);
}

void gen_ore(Canvas &c, u32 stone, u32 stone_dark, u32 ore, u32 ore_lite, u32 seed)
{
    gen_rock(c, stone, stone_dark, lighten(stone, 0.30f), seed);
    Rng r;
    r.seed((u64)seed * 668265263u + 909u);
    for(int k = 0; k < 4; k++)
    {
        int bx = r.range(4, CELL - 9), by = r.range(4, CELL - 9);
        int n = r.range(3, 6);
        for(int q = 0; q < n; q++)
        {
            int x = bx + r.range(0, 6), y = by + r.range(0, 6);
            int w = r.range(2, 4), h = r.range(2, 4);
            c.rect(x, y, w, h, ore);
            c.rect(x, y, w, 1, ore_lite);
            c.rect(x, y, 1, h, lighten(ore, 0.18f));
            c.rect(x + w - 1, y, 1, h, darken(ore, 0.32f));
            c.rect(x, y + h - 1, w, 1, darken(ore, 0.42f));
        }
    }
    for(int k = 0; k < 10; k++)
    {
        int x = r.range(CELL), y = r.range(CELL);
        if(c.get(x, y) != stone) c.blend(x, y, with_a(lighten(ore_lite, 0.5f), 200));
    }
    c.bevel(1.10f, 0.80f);
    c.edge_outline(with_a(stone_dark, 190));
}

void gen_gem(Canvas &c, u32 base, u32 lite, u32 seed)
{
    c.clear(darken(base, 0.42f));
    c.cloud_noise(0.18f, seed, lighten(base, 0.20f), darken(base, 0.30f));
    Rng r;
    r.seed((u64)seed * 2654435761u + 1717u);
    for(int k = 0; k < 7; k++)
    {
        int cx = r.range(6, CELL - 6), cy = r.range(6, CELL - 6);
        int s = r.range(3, 6);
        for(int dy = -s; dy <= s; dy++)
            for(int dx = -s; dx <= s; dx++)
            {
                if(std::abs(dx) + std::abs(dy) > s) continue;
                u32 col = base;
                if(dx - dy < -s / 2) col = lite;
                if(dx + dy > s) col = darken(base, 0.28f);
                c.blend(cx + dx, cy + dy, col);
            }
    }
    c.rect(11, 9, 3, 2, with_a(rgba(255, 255, 255), 210));
    c.rect(19, 17, 2, 2, with_a(rgba(255, 255, 255), 150));
    c.bevel(1.16f, 0.76f);
    c.edge_outline(with_a(darken(base, 0.55f), 200));
}

void gen_water(Canvas &c, u32 base, u32 foam, u32 seed)
{
    c.clear(base);
    c.cloud_noise(0.16f, seed, lighten(base, 0.25f), darken(base, 0.22f));
    for(int k = 0; k < 2; k++)
    {
        f32 ph = k * 3.1f;
        int yb = 9 + k * 12;
        for(int x = 0; x < CELL; x++)
        {
            int y = yb + (int)(std::sin(x * 0.42f + ph) * 2.2f);
            c.blend(x, y, with_a(foam, 120));
            c.blend(x, y + 1, with_a(foam, 60));
        }
    }
    for(int x = 0; x < CELL; x++)
    {
        c.blend(x, 2, with_a(foam, 90));
        c.blend(x, 3, with_a(foam, 40));
    }
    // 液体整体略透明
    for(int i = 0; i < CELL * CELL; i++)
    {
        u8 a = alpha_of(c.px[i]);
        if(a) c.px[i] = with_a(c.px[i], (u8)(a * 0.88f));
    }
}

void gen_lava(Canvas &c, u32 base, u32 hot, u32 seed)
{
    c.clear(darken(base, 0.30f));
    c.cloud_noise(0.42f, seed, hot, darken(base, 0.55f));
    Rng r;
    r.seed((u64)seed * 2246822519u + 2468u);
    for(int k = 0; k < 7; k++)
    {
        f32 y = r.unit(2, CELL - 2);
        f32 amp = r.unit(1.0f, 2.5f);
        f32 ph = r.unit(0, 6.28f);
        for(int x = 0; x < CELL; x++)
        {
            int yy = (int)(y + std::sin(x * 0.35f + ph) * amp);
            c.blend(x, yy, with_a(hot, 210));
            c.blend(x, yy + 1, with_a(mixc(hot, base, 0.5f), 150));
        }
    }
    for(int k = 0; k < 16; k++)
        c.disc((f32)r.range(CELL), (f32)r.range(CELL), r.unit(1.0f, 2.2f),
               with_a(lighten(hot, 0.35f), 190));
    for(int k = 0; k < 12; k++)
        c.blotches(with_a(darken(base, 0.62f), 130), 1, 2, 5, seed + (u32)k);
    c.edge_outline(with_a(darken(base, 0.62f), 200));
}

void gen_ice(Canvas &c, u32 base, u32 lite, u32 seed)
{
    c.clear(base);
    c.cloud_noise(0.20f, seed, lite, darken(base, 0.20f));
    Rng r;
    r.seed((u64)seed * 668265263u + 1357u);
    for(int k = 0; k < 6; k++)
    {
        int x = r.range(CELL), y = r.range(CELL);
        int len = r.range(6, 16);
        int dx = r.range(2) ? 1 : -1;
        for(int q = 0; q < len; q++)
        {
            c.blend(x + q * dx, y + q, with_a(lite, 130));
            c.blend(x + q * dx + 1, y + q, with_a(rgba(255, 255, 255), 60));
        }
    }
    c.rect(4, 4, 7, 1, with_a(rgba(255, 255, 255), 190));
    c.rect(4, 4, 1, 6, with_a(rgba(255, 255, 255), 150));
    c.bevel(1.18f, 0.86f);
    c.edge_outline(with_a(darken(base, 0.35f), 170));
}

void gen_furnace(Canvas &c, u32 stone, u32 dark, u32 seed)
{
    gen_rock(c, stone, dark, lighten(stone, 0.28f), seed);
    c.frame(1, 1, CELL - 2, CELL - 2, with_a(darken(stone, 0.35f), 150), 1);
    const int mx = 9, my = 13, mw = 14, mh = 13;
    c.rect(mx - 1, my - 1, mw + 2, mh + 2, darken(stone, 0.50f));
    for(int y = 0; y < mh; y++)
    {
        f32 t = 1.0f - (f32)y / mh;
        u32 col = mixc(rgba(90, 24, 10), rgba(255, 170, 60), t);
        for(int x = 0; x < mw; x++)
        {
            f32 fx = std::abs(x - mw * 0.5f) / (mw * 0.5f);
            c.set(mx + x, my + y, mixc(darken(col, 0.55f), col, 1.0f - fx * fx));
        }
    }
    for(int k = 0; k < 3; k++)
    {
        int fx = mx + 3 + k * 4;
        for(int y = 0; y < 5; y++)
            c.blend(fx, my + mh - 1 - y, with_a(rgba(255, 226, 130), (u8)(200 - y * 30)));
    }
    c.rect(11, 4, 10, 5, darken(stone, 0.42f));
    c.rect(11, 4, 10, 1, lighten(stone, 0.30f));
    c.bevel(1.12f, 0.80f);
    c.edge_outline(with_a(dark, 200));
}

void gen_chest(Canvas &c, u32 wood, u32 metal, u32 seed)
{
    c.clear(0);
    const int x0 = 3, y0 = 7, w = 26, h = 21;
    c.rect(x0, y0, w, h, wood);
    Rng r;
    r.seed((u64)seed * 2654435761u + 8642u);
    for(int y = y0; y < y0 + h; y++)
        for(int x = x0; x < x0 + w; x++)
        {
            int g = (x * 5 + y * 3) % 17;
            if(g == 0) c.blend(x, y, with_a(darken(wood, 0.18f), 120));
            if(r.range(100) < 8) c.blend(x, y, with_a(lighten(wood, 0.16f), 100));
        }
    // 箱盖
    c.rect(x0, y0 - 3, w, 4, lighten(wood, 0.16f));
    c.rect(x0, y0 + 1, w, 1, darken(wood, 0.45f));
    // 铁包边
    c.rect(x0, y0 - 3, w, 1, metal);
    c.rect(x0, y0 + h - 1, w, 1, metal);
    c.rect(x0, y0 - 3, 1, h + 3, metal);
    c.rect(x0 + w - 1, y0 - 3, 1, h + 3, metal);
    c.rect(x0 + 5, y0 - 2, 2, 24, with_a(darken(metal, 0.25f), 200));
    c.rect(x0 + w - 7, y0 - 2, 2, 24, with_a(darken(metal, 0.25f), 200));
    // 锁扣
    c.rect(14, y0, 5, 6, metal);
    c.rect(14, y0, 5, 1, lighten(metal, 0.40f));
    c.rect(16, y0 + 2, 1, 3, darken(metal, 0.55f));
    c.edge_outline(with_a(rgba(0, 0, 0, 180), 180));
}

void gen_torch(Canvas &c, u32 flame, u32 wood)
{
    c.clear(0);
    c.rect(14, 14, 4, 18, wood);
    c.rect(14, 14, 1, 18, lighten(wood, 0.30f));
    c.rect(17, 14, 1, 18, darken(wood, 0.35f));
    for(int y = 14; y < 32; y += 4) c.rect(14, y, 4, 1, with_a(darken(wood, 0.40f), 160));
    struct Layer { f32 cx, cy, rx, ry; u32 col; };
    const Layer L[3] = {
        {16.0f, 11.0f, 6.5f, 8.5f, with_a(rgba(226, 96, 32), 235)},
        {16.0f, 12.0f, 4.5f, 6.0f, with_a(rgba(255, 158, 44), 245)},
        {16.0f, 13.5f, 2.6f, 3.6f, with_a(rgba(255, 238, 170), 255)},
    };
    for(const Layer &l : L) c.ellipse(l.cx, l.cy, l.rx, l.ry, l.col);
    c.line(16, 3, 16, 8, with_a(rgba(255, 190, 90), 200), 2);
    c.blend(16, 2, with_a(rgba(255, 226, 150), 180));
    // 光晕（用方块本身的 accent 色）
    for(int k = 0; k < 3; k++)
        c.disc(16.0f, 11.0f, 9.0f + k * 2.5f, with_a(flame, (u8)(26 - k * 8)));
}

void gen_portal(Canvas &c, u32 base, u32 glow, u32 seed)
{
    c.clear(with_a(base, 235));
    for(int y = 0; y < CELL; y++)
        for(int x = 0; x < CELL; x++)
        {
            f32 dx = x - 15.5f, dy = y - 15.5f;
            f32 ang = std::atan2(dy, dx);
            f32 rad = std::sqrt(dx * dx + dy * dy);
            f32 sw = std::sin(ang * 2.0f + rad * 0.55f + (f32)(seed % 7));
            f32 t = (sw + 1.0f) * 0.5f;
            c.blend(x, y, with_a(mixc(darken(base, 0.35f), lighten(glow, 0.10f), t * 0.85f),
                                 210));
        }
    Rng r;
    r.seed((u64)seed * 2246822519u + 4242u);
    for(int k = 0; k < 26; k++)
        c.blend(r.range(CELL), r.range(CELL),
                with_a(rgba(240, 210, 255), (u8)r.range(90, 220)));
    c.frame(0, 0, CELL, CELL, with_a(glow, 170), 1);
    c.frame(1, 1, CELL - 2, CELL - 2, with_a(glow, 70), 1);
}

void gen_bed(Canvas &c, u32 sheet, u32 pillow)
{
    c.clear(0);
    c.rect(3, 22, 4, 8, rgba(112, 76, 44));
    c.rect(25, 22, 4, 8, rgba(112, 76, 44));
    c.rect(2, 15, 28, 9, rgba(126, 86, 50));
    c.rect(2, 15, 28, 1, rgba(160, 116, 70));
    c.rect(3, 8, 26, 9, sheet);
    for(int x = 3; x < 29; x++) c.blend(x, 8, with_a(lighten(sheet, 0.30f), 200));
    for(int x = 3; x < 29; x += 4) c.rect(x, 9, 2, 8, with_a(darken(sheet, 0.16f), 150));
    c.rect(4, 5, 9, 6, pillow);
    c.rect(4, 5, 9, 1, rgba(255, 255, 255));
    c.rect(4, 10, 9, 1, with_a(darken(pillow, 0.25f), 220));
    c.edge_outline(with_a(rgba(0, 0, 0, 150), 150));
}

void gen_wool(Canvas &c, u32 base, u32 seed)
{
    c.clear(base);
    Rng r;
    r.seed((u64)seed * 2654435761u + 5150u);
    for(int k = 0; k < 90; k++)
    {
        int x = r.range(CELL), y = r.range(CELL);
        u32 col = (k % 3 == 0) ? lighten(base, 0.16f) : darken(base, 0.12f);
        c.blend(x, y, col);
        if(r.range(2)) c.blend(x + 1, y, with_a(col, 120));
        if(r.range(2)) c.blend(x, y + 1, with_a(col, 100));
    }
    for(int y = 0; y < CELL; y += 4)
        for(int x = 0; x < CELL; x++)
            c.blend(x, y + ((x / 4) % 2), with_a(darken(base, 0.08f), 90));
    c.bevel(1.08f, 0.90f);
}

void gen_table(Canvas &c, u32 metal, u32 wood)
{
    c.clear(0);
    c.rect(1, 8, 30, 8, metal);
    c.rect(1, 8, 30, 2, lighten(metal, 0.28f));
    c.rect(1, 14, 30, 2, darken(metal, 0.32f));
    for(int x = 3; x < 29; x += 6) c.rect(x, 9, 1, 6, with_a(darken(metal, 0.40f), 150));
    c.rect(3, 16, 5, 14, wood);
    c.rect(24, 16, 5, 14, wood);
    c.rect(3, 16, 1, 14, lighten(wood, 0.25f));
    c.rect(28, 16, 1, 14, darken(wood, 0.30f));
    // 台面工具剪影
    c.line(9, 5, 20, 5, rgba(70, 70, 76), 4);
    c.line(14, 2, 14, 8, rgba(126, 86, 50), 4);
    c.blend(24, 4, with_a(rgba(255, 226, 150), 200));
    c.blend(26, 6, with_a(rgba(255, 190, 90), 160));
    c.edge_outline(with_a(rgba(0, 0, 0, 160), 160));
}

void gen_netherrack(Canvas &c, u32 base, u32 dark, u32 seed)
{
    c.clear(base);
    c.cloud_noise(0.34f, seed, lighten(base, 0.26f), darken(base, 0.44f));
    Rng r;
    r.seed((u64)seed * 668265263u + 777u);
    for(int k = 0; k < 26; k++)
    {
        int x = r.range(2, CELL - 3), y = r.range(2, CELL - 3);
        c.disc((f32)x, (f32)y, r.unit(1.0f, 2.6f), with_a(darken(base, 0.62f), 210));
        c.blend(x, y - 1, with_a(lighten(base, 0.20f), 120));
    }
    c.bevel(1.10f, 0.74f);
    c.edge_outline(with_a(dark, 200));
}

void gen_ebon(Canvas &c, u32 base, u32 vein, u32 seed, bool wall)
{
    c.clear(base);
    c.cloud_noise(0.24f, seed, lighten(base, 0.22f), darken(base, 0.42f));
    Rng r;
    r.seed((u64)seed * 2246822519u + 999u);
    for(int k = 0; k < 7; k++)
    {
        int x = r.range(CELL), y = r.range(CELL);
        int len = r.range(5, 14);
        int dx = r.range(3) - 1, dy = r.range(3) - 1;
        if(dx == 0 && dy == 0) dx = 1;
        for(int q = 0; q < len; q++)
        {
            int px = x + q * dx, py = y + q * dy;
            c.blend(px, py, with_a(vein, 190));
            c.blend(px + 1, py, with_a(vein, 70));
        }
    }
    c.blotches(with_a(darken(base, 0.35f), 120), 14, 2, 5, seed + 3);
    if(wall)
    {
        for(int y = 0; y < CELL; y += 8)
        {
            for(int x = 0; x < CELL; x++) c.blend(x, y, with_a(darken(base, 0.55f), 170));
            for(int x = ((y / 8) % 2) * 8; x < CELL; x += 16)
                for(int j = 1; j < 8; j++) c.blend(x, y + j, with_a(darken(base, 0.50f), 140));
        }
    }
    c.bevel(1.12f, 0.76f);
    c.edge_outline(with_a(darken(base, 0.50f), 190));
}

void gen_debris(Canvas &c, u32 base, u32 metal, u32 seed)
{
    c.clear(base);
    c.cloud_noise(0.28f, seed, lighten(base, 0.22f), darken(base, 0.36f));
    Rng r;
    r.seed((u64)seed * 2654435761u + 3131u);
    for(int k = 0; k < 9; k++)
    {
        int x = r.range(3, CELL - 7), y = r.range(3, CELL - 7);
        int w = r.range(3, 6), h = r.range(2, 5);
        c.rect(x, y, w, h, metal);
        c.rect(x, y, w, 1, lighten(metal, 0.35f));
        c.rect(x, y + h - 1, w, 1, darken(metal, 0.40f));
        c.rect(x + w - 1, y, 1, h, darken(metal, 0.30f));
    }
    c.bevel(1.10f, 0.78f);
    c.edge_outline(with_a(darken(base, 0.50f), 190));
}

// 部分高度的液体：只占下方 tap/4 的高度
void gen_fluid_part(Canvas &c, u32 base, u32 foam, u32 seed, int tap)
{
    c.clear(0);
    int h = CELL * tap / 4;
    if(h < 4) h = 4;
    if(h > CELL) h = CELL;
    const int y0 = CELL - h;
    for(int y = y0; y < CELL; y++)
        for(int x = 0; x < CELL; x++)
        {
            f32 t = (f32)(y - y0) / h;
            c.set(x, y, mixc(lighten(base, 0.18f), darken(base, 0.14f), t));
        }
    for(int x = 0; x < CELL; x++)
    {
        int y = y0 + (int)(std::sin(x * 0.45f + (f32)(seed % 11)) * 1.4f);
        c.blend(x, y, with_a(foam, 170));
        c.blend(x, y + 1, with_a(foam, 80));
    }
}

// ============================================================================
//                            物品图标
// ============================================================================
// 统一构图：斜置木柄从 (9,24) 指向 (21,12)，暗描边 + 亮高光
void icon_handle(Canvas &c, u32 wood)
{
    c.line(9, 24, 21, 12, darken(wood, 0.55f), 5);
    c.line(9, 24, 21, 12, wood, 3);
    c.line(10, 23, 20, 13, lighten(wood, 0.28f), 1);
}

void gen_i_stick(Canvas &c, u32 wood)
{
    c.clear(0);
    icon_handle(c, wood);
    c.rect(13, 19, 2, 2, with_a(darken(wood, 0.40f), 200));
    c.rect(17, 15, 2, 2, with_a(darken(wood, 0.40f), 180));
}

void gen_i_pick(Canvas &c, u32 metal, u32 wood, u32 accent)
{
    c.clear(0);
    icon_handle(c, wood);
    const f32 cx = 16.0f, cy = 15.0f, r = 12.0f;
    for(int pass = 0; pass < 3; pass++)
    {
        f32 rr = (pass == 0) ? r : (pass == 1 ? r : r - 1.4f);
        f32 rad = (pass == 0) ? 2.6f : (pass == 1 ? 1.9f : 1.0f);
        u32 col = (pass == 0) ? darken(metal, 0.50f)
                : (pass == 1) ? metal : accent;
        for(int a = 0; a < 48; a++)
        {
            f32 an = -2.45f + a * 0.030f;
            c.disc(cx + std::cos(an) * rr, cy + std::sin(an) * rr, rad,
                   (pass == 2) ? with_a(col, 190) : col);
        }
    }
    c.rect(13, 15, 7, 2, rgba(96, 66, 40));
}

void gen_i_axe(Canvas &c, u32 metal, u32 wood, u32 accent)
{
    c.clear(0);
    icon_handle(c, wood);
    for(int y = 4; y <= 18; y++)
    {
        f32 t = (y - 4) / 14.0f;
        int xr = 26 - (int)(std::sin(t * 3.14159f) * 4.0f);
        int xl = 15 + (int)(t * 3.0f);
        for(int x = xl; x <= xr; x++) c.set(x, y, metal);
        c.set(xr, y, accent);
        if(xr - 1 >= 0) c.set(xr - 1, y, lighten(metal, 0.20f));
        c.blend(xl - 1, y, with_a(darken(metal, 0.50f), 200));
    }
    c.line(15, 5, 15, 18, with_a(darken(metal, 0.50f), 180), 1);
    c.rect(13, 13, 7, 2, rgba(96, 66, 40));
}

void gen_i_sword(Canvas &c, u32 metal, u32 accent, u32 grip)
{
    c.clear(0);
    const int bx = 15;
    for(int y = 3; y <= 20; y++)
    {
        int w = 3;
        if(y < 6) w = 1;
        else if(y < 8) w = 2;
        c.rect(bx - w / 2, y, w, 1, metal);
        c.set(bx - w / 2, y, lighten(metal, 0.30f));
        c.set(bx - w / 2 + w - 1, y, darken(metal, 0.35f));
    }
    c.rect(bx, 8, 1, 12, with_a(accent, 170));
    c.rect(9, 21, 14, 3, darken(metal, 0.45f));
    c.rect(9, 21, 14, 1, lighten(metal, 0.25f));
    c.rect(14, 24, 4, 6, grip);
    c.rect(14, 24, 1, 6, lighten(grip, 0.30f));
    c.disc(16.0f, 30.0f, 2.2f, darken(metal, 0.30f));
}

void gen_i_helm(Canvas &c, u32 mat)
{
    c.clear(0);
    c.ellipse(16.0f, 15.0f, 11.0f, 11.0f, mat);
    c.rect(5, 15, 22, 9, mat);
    c.rect(15, 3, 3, 4, lighten(mat, 0.40f));
    c.rect(9, 14, 14, 6, with_a(rgba(18, 18, 24), 225));
    c.rect(9, 14, 14, 1, darken(mat, 0.50f));
    c.rect(4, 16, 3, 8, darken(mat, 0.25f));
    c.rect(25, 16, 3, 8, darken(mat, 0.25f));
    c.ellipse(11.0f, 10.0f, 4.0f, 2.6f, with_a(rgba(255, 255, 255), 120));
    c.outline(darken(mat, 0.55f), 4, 15, 24, 10);
}

void gen_i_chest(Canvas &c, u32 mat)
{
    c.clear(0);
    c.rect(8, 8, 16, 18, mat);
    c.ellipse(7.0f, 9.0f, 5.0f, 5.0f, lighten(mat, 0.12f));
    c.ellipse(25.0f, 9.0f, 5.0f, 5.0f, darken(mat, 0.10f));
    c.rect(13, 5, 6, 4, with_a(rgba(18, 18, 24), 220));
    c.rect(15, 10, 2, 16, with_a(darken(mat, 0.28f), 180));
    for(int y = 12; y <= 24; y += 5)
    {
        c.set(10, y, lighten(mat, 0.45f));
        c.set(21, y, darken(mat, 0.30f));
    }
    c.rect(8, 22, 16, 4, darken(mat, 0.18f));
    c.rect(9, 9, 3, 5, with_a(rgba(255, 255, 255), 90));
    c.outline(darken(mat, 0.55f), 8, 8, 16, 18);
}

void gen_i_legs(Canvas &c, u32 mat)
{
    c.clear(0);
    c.rect(7, 7, 18, 6, mat);
    c.rect(7, 7, 18, 1, lighten(mat, 0.30f));
    c.rect(8, 13, 7, 15, mat);
    c.rect(17, 13, 7, 15, mat);
    c.rect(8, 13, 1, 15, lighten(mat, 0.22f));
    c.rect(17, 13, 1, 15, lighten(mat, 0.10f));
    c.rect(14, 13, 1, 15, with_a(darken(mat, 0.50f), 180));
    c.rect(23, 13, 1, 15, darken(mat, 0.35f));
    c.rect(8, 22, 7, 2, darken(mat, 0.25f));
    c.rect(17, 22, 7, 2, darken(mat, 0.25f));
    c.rect(13, 9, 6, 3, lighten(mat, 0.42f));
    c.outline(darken(mat, 0.55f), 7, 7, 18, 6);
    c.outline(darken(mat, 0.55f), 8, 13, 7, 15);
    c.outline(darken(mat, 0.55f), 17, 13, 7, 15);
}

void gen_i_boot(Canvas &c, u32 mat)
{
    c.clear(0);
    c.rect(9, 6, 10, 12, mat);
    c.rect(9, 6, 10, 1, lighten(mat, 0.32f));
    c.rect(9, 16, 16, 7, mat);
    c.rect(9, 22, 16, 2, darken(mat, 0.45f));
    c.rect(22, 16, 4, 7, darken(mat, 0.15f));
    for(int y = 9; y <= 14; y += 3) c.rect(10, y, 8, 1, with_a(darken(mat, 0.35f), 190));
    c.rect(10, 7, 3, 9, with_a(rgba(255, 255, 255), 80));
    c.outline(darken(mat, 0.55f), 9, 6, 10, 12);
    c.outline(darken(mat, 0.55f), 9, 16, 16, 8);
}

void gen_i_bow(Canvas &c, u32 wood, u32 string_col, u32 accent)
{
    c.clear(0);
    const f32 cx = 8.0f, cy = 15.5f, r = 12.0f;
    for(int a = 0; a <= 60; a++)
    {
        f32 an = -1.15f + a * (2.30f / 60.0f);
        c.disc(cx + std::cos(an) * r, cy + std::sin(an) * r, 2.2f, darken(wood, 0.50f));
    }
    for(int a = 0; a <= 60; a++)
    {
        f32 an = -1.15f + a * (2.30f / 60.0f);
        c.disc(cx + std::cos(an) * r, cy + std::sin(an) * r, 1.5f, wood);
    }
    c.line(21, 5, 21, 26, with_a(string_col, 210), 1);
    c.rect(16, 13, 4, 6, rgba(96, 66, 40));
    c.disc(21.0f, 5.0f, 1.8f, accent);
    c.disc(21.0f, 26.0f, 1.8f, accent);
    c.line(4, 12, 12, 22, with_a(lighten(wood, 0.35f), 90), 1);
}

void gen_i_arrow(Canvas &c, u32 shaft, u32 head)
{
    c.clear(0);
    c.line(6, 26, 24, 8, darken(shaft, 0.45f), 3);
    c.line(6, 26, 24, 8, shaft, 1);
    for(int i = 0; i < 7; i++) c.rect(24 - i, 4 + i, 2 * i + 2, 1, head);
    c.rect(24, 4, 2, 2, lighten(head, 0.40f));
    for(int i = 0; i < 6; i++)
    {
        c.rect(5 + i, 23 - i, 4, 1, with_a(rgba(238, 238, 238), (u8)(220 - i * 20)));
        c.rect(7 + i, 25 - i, 4, 1, with_a(rgba(210, 210, 210), (u8)(200 - i * 20)));
    }
    c.line(6, 27, 24, 9, with_a(darken(head, 0.45f), 120), 1);
}

void gen_i_shield(Canvas &c, u32 wood, u32 metal)
{
    c.clear(0);
    for(int y = 5; y < 28; y++)
    {
        f32 t = (y - 5) / 23.0f;
        int w = (int)(22 - t * t * 12);
        int x0 = 16 - w / 2;
        c.rect(x0, y, w, 1, y < 12 ? lighten(wood, 0.10f) : darken(wood, 0.06f));
    }
    for(int y = 7; y < 27; y += 5)
        for(int x = 5; x < 27; x++) c.blend(x, y, with_a(darken(wood, 0.28f), 130));
    for(int y = 5; y < 28; y++)
    {
        f32 t = (y - 5) / 23.0f;
        int w = (int)(22 - t * t * 12);
        int x0 = 16 - w / 2;
        c.set(x0, y, metal);
        c.set(x0 + 1, y, darken(metal, 0.20f));
        c.set(x0 + w - 1, y, darken(metal, 0.35f));
        c.set(x0 + w - 2, y, darken(metal, 0.15f));
    }
    c.rect(6, 5, 20, 2, metal);
    c.disc(16.0f, 16.0f, 4.2f, darken(metal, 0.25f));
    c.disc(16.0f, 16.0f, 2.8f, metal);
    c.rect(15, 12, 2, 8, lighten(metal, 0.40f));
}

void gen_i_bucket(Canvas &c, u32 metal, u32 liquid, bool empty)
{
    c.clear(0);
    for(int y = 9; y < 28; y++)
    {
        f32 t = (y - 9) / 19.0f;
        int w = (int)(16 - t * 3);
        int x0 = 16 - w / 2;
        c.rect(x0, y, w, 1, metal);
        c.set(x0, y, lighten(metal, 0.35f));
        c.set(x0 + w - 1, y, darken(metal, 0.40f));
    }
    c.rect(7, 8, 18, 2, lighten(metal, 0.20f));
    c.rect(7, 7, 18, 1, darken(metal, 0.30f));
    for(int y = 14; y < 26; y += 6) c.rect(8, y, 16, 1, with_a(darken(metal, 0.45f), 190));
    for(int a = 0; a <= 24; a++)
    {
        f32 an = 3.14159f + a * (3.14159f / 24);
        c.blend((int)(16 + std::cos(an) * 9.0f), (int)(9 + std::sin(an) * 5.0f),
                darken(metal, 0.50f));
    }
    if(!empty)
    {
        for(int y = 11; y < 27; y++)
        {
            f32 t = (y - 11) / 16.0f;
            int w = (int)(14 - t * 3);
            int x0 = 16 - w / 2;
            c.rect(x0, y, w, 1, y == 11 ? lighten(liquid, 0.30f) : liquid);
        }
    }
    c.outline(darken(metal, 0.50f), 7, 8, 18, 20);
}

void gen_i_ingot(Canvas &c, u32 metal, u32 shine, bool scrap)
{
    c.clear(0);
    for(int y = 12; y < 24; y++)
    {
        f32 t = (y - 12) / 12.0f;
        int w = (int)(18 - t * 5);
        int x0 = 16 - w / 2;
        c.rect(x0, y, w, 1, metal);
        c.set(x0, y, lighten(metal, 0.30f));
        c.set(x0 + w - 1, y, darken(metal, 0.38f));
    }
    c.rect(7, 11, 18, 2, lighten(metal, 0.25f));
    c.rect(7, 11, 18, 1, shine);
    if(scrap)
    {
        Rng r;
        r.seed(0x5C4Au);
        for(int k = 0; k < 40; k++)
            c.blend(r.range(8, 24), r.range(11, 24),
                    with_a(rgba(60, 48, 44), (u8)r.range(60, 160)));
        for(int k = 0; k < 5; k++)
            c.rect(r.range(8, 22), r.range(12, 22), 2, 1, with_a(rgba(30, 24, 22), 200));
    }
    c.outline(darken(metal, 0.50f), 7, 11, 18, 13);
}

void gen_i_gem(Canvas &c, u32 gem, u32 shine)
{
    c.clear(0);
    const int cx = 16, cy = 16, r = 10;
    for(int y = -r; y <= r; y++)
        for(int x = -r; x <= r; x++)
        {
            if(std::abs(x) + std::abs(y) > r) continue;
            u32 col = gem;
            if(x - y < -r / 2) col = lighten(gem, 0.35f);
            if(x + y > r / 2) col = darken(gem, 0.26f);
            c.set(cx + x, cy + y, col);
        }
    for(int y = -3; y <= 3; y++)
        for(int x = -3; x <= 3; x++)
            if(std::abs(x) + std::abs(y) <= 3) c.set(cx + x, cy + y, lighten(gem, 0.40f));
    c.rect(cx - 2, cy - 2, 2, 2, shine);
    // 描边
    for(int y = -r; y <= r; y++)
        for(int x = -r; x <= r; x++)
            if(std::abs(x) + std::abs(y) == r) c.set(cx + x, cy + y, darken(gem, 0.45f));
}

void gen_i_nugget(Canvas &c, u32 metal, u32 shine)
{
    c.clear(0);
    Rng r;
    r.seed(0x9E37u);
    for(int k = 0; k < 9; k++)
    {
        f32 x = r.unit(8, 24), y = r.unit(9, 25);
        f32 s = r.unit(1.6f, 3.3f);
        c.disc(x, y, s + 0.9f, darken(metal, 0.45f));
        c.disc(x, y, s, metal);
        c.blend((int)(x - s * 0.3f), (int)(y - s * 0.3f), shine);
    }
}

void gen_i_string(Canvas &c, u32 col)
{
    c.clear(0);
    for(int a = 0; a < 150; a++)
    {
        f32 t = a / 150.0f * 6.283f;
        f32 rr = 8.0f + std::sin(t * 3.0f) * 3.0f;
        int x = (int)(16 + std::cos(t) * rr);
        int y = (int)(16 + std::sin(t) * rr * 0.95f);
        c.blend(x, y, col);
        c.blend(x + 1, y, with_a(col, 150));
    }
    c.disc(16.0f, 16.0f, 3.0f, with_a(darken(col, 0.25f), 200));
}

void gen_i_powder(Canvas &c, u32 col, u32 dark)
{
    c.clear(0);
    Rng r;
    r.seed(0x1F2Eu);
    for(int y = 0; y < CELL; y++)
        for(int x = 0; x < CELL; x++)
        {
            f32 dx = (x - 16) / 11.0f, dy = (y - 22) / 9.0f;
            if(dx * dx + dy * dy > 1.0f) continue;
            f32 t = (y - 13) / 18.0f;
            c.set(x, y, mixc(dark, col, 1.0f - t * 0.5f));
        }
    for(int k = 0; k < 90; k++)
    {
        int x = 16 + r.range(-9, 9), y = 15 + r.range(-3, 11);
        if(alpha_of(c.get(x, y)))
            c.blend(x, y, with_a(r.range(2) ? dark : rgba(255, 255, 255),
                                 (u8)r.range(40, 130)));
    }
    for(int k = 0; k < 16; k++)
        c.blend(16 + r.range(-12, 12), 24 + r.range(-10, 3), with_a(col, 200));
}

void gen_i_scale(Canvas &c, u32 base, u32 shine)
{
    c.clear(0);
    struct S { f32 x, y; };
    const S ss[3] = {{10, 19}, {21, 19}, {15.5f, 10}};
    for(const S &s : ss)
    {
        c.ellipse(s.x, s.y, 6.5f, 5.5f, darken(base, 0.55f));
        c.ellipse(s.x, s.y, 5.5f, 4.5f, base);
        c.ellipse(s.x - 1.5f, s.y - 1.5f, 1.8f, 1.4f, with_a(shine, 180));
        for(int k = 0; k < 4; k++)
            c.line((int)(s.x - 3 + k * 2), (int)(s.y - 3), (int)(s.x - 3 + k * 2),
                   (int)(s.y + 3), with_a(darken(base, 0.30f), 110), 1);
    }
}

void gen_i_amulet(Canvas &c, u32 base, u32 shine)
{
    c.clear(0);
    for(int a = 0; a < 40; a++)
    {
        f32 an = 3.14159f + a * (3.14159f / 39);
        int x = (int)(16 + std::cos(an) * 11.0f);
        int y = (int)(9 + std::sin(an) * 7.0f);
        c.blend(x, y, rgba(198, 198, 206));
        c.blend(x, y + 1, with_a(rgba(140, 140, 148), 170));
    }
    c.disc(16.0f, 20.0f, 8.5f, darken(base, 0.55f));
    c.disc(16.0f, 20.0f, 7.0f, base);
    c.disc(16.0f, 20.0f, 4.0f, darken(base, 0.22f));
    c.disc(16.0f, 20.0f, 2.2f, shine);
    for(int k = 0; k < 4; k++)
    {
        f32 an = k * 1.5708f + 0.7854f;
        c.line((int)(16 + std::cos(an) * 7), (int)(20 + std::sin(an) * 7),
               (int)(16 + std::cos(an) * 11), (int)(20 + std::sin(an) * 11),
               lighten(base, 0.35f), 2);
    }
}

// 通用「核心/杂项」图标（OI 核心、递归）
void gen_i_core(Canvas &c, u32 a, u32 b)
{
    c.clear(0);
    c.disc(16.0f, 16.0f, 12.0f, darken(a, 0.55f));
    c.disc(16.0f, 16.0f, 10.5f, a);
    c.disc(16.0f, 16.0f, 6.0f, lighten(a, 0.25f));
    c.disc(16.0f, 16.0f, 3.2f, b);
    for(int k = 0; k < 8; k++)
    {
        f32 an = k * 0.7854f;
        c.line((int)(16 + std::cos(an) * 11), (int)(16 + std::sin(an) * 11),
               (int)(16 + std::cos(an) * 14), (int)(16 + std::sin(an) * 14),
               with_a(b, 200), 2);
    }
    c.disc(12.0f, 12.0f, 2.5f, with_a(rgba(255, 255, 255), 200));
}

void gen_potion(Canvas &c, u32 liquid, u32 shine)
{
    c.clear(0);
    c.ellipse(16.0f, 21.0f, 9.0f, 9.0f, with_a(rgba(220, 232, 240), 120));
    for(int y = 16; y < 30; y++)
        for(int x = 6; x < 27; x++)
        {
            f32 dx = (x - 16) / 8.2f, dy = (y - 21) / 8.2f;
            if(dx * dx + dy * dy > 1.0f) continue;
            if(y < 18) continue;
            c.set(x, y, (y == 18) ? lighten(liquid, 0.30f) : liquid);
        }
    c.rect(13, 6, 6, 8, with_a(rgba(226, 238, 246), 150));
    c.rect(11, 4, 10, 3, rgba(140, 100, 60));
    c.rect(11, 4, 10, 1, rgba(180, 140, 90));
    c.ellipse(12.0f, 19.0f, 2.2f, 4.0f, with_a(rgba(255, 255, 255), 140));
    c.blend(19, 17, with_a(shine, 200));
    for(int y = 12; y < 31; y++)
    {
        f32 q = 1.0f - std::pow((y - 21) / 9.0f, 2.0f);
        int w = (int)(std::sqrt(q > 0 ? q : 0.0f) * 9.0f);
        c.blend(16 - w, y, with_a(rgba(60, 70, 84), 170));
        c.blend(16 + w, y, with_a(rgba(40, 50, 60), 190));
    }
}

void gen_food(Canvas &c, u32 base, u32 accent, u32 seed, int kind)
{
    c.clear(0);
    Rng r;
    r.seed((u64)seed * 2246822519u + 606u);
    switch(kind)
    {
    case 0:   // 苹果
        c.disc(16.0f, 19.0f, 10.0f, darken(base, 0.50f));
        c.disc(16.0f, 19.0f, 9.0f, base);
        c.ellipse(12.5f, 15.5f, 3.2f, 2.2f, with_a(rgba(255, 255, 255), 110));
        c.line(16, 10, 17, 4, rgba(110, 74, 42), 2);
        c.ellipse(21.0f, 6.0f, 4.0f, 2.4f, accent);
        break;
    case 1:   // 生肉
        for(int y = 8; y < 27; y++)
            for(int x = 6; x < 27; x++)
            {
                f32 dx = (x - 16) / 11.0f, dy = (y - 17) / 10.0f;
                f32 w = 1.0f + std::sin(x * 0.5f + y * 0.3f) * 0.10f;
                if(dx * dx + dy * dy > w) continue;
                c.set(x, y, base);
            }
        for(int k = 0; k < 5; k++)
            c.line(9 + k * 4, 11 + (k % 2) * 2, 13 + k * 4, 24 - (k % 2) * 3,
                   with_a(accent, 190), 2);
        c.ellipse(12.0f, 13.0f, 3.0f, 2.0f, with_a(rgba(255, 255, 255), 80));
        break;
    case 2:   // 金苹果
        c.disc(16.0f, 19.0f, 11.0f, with_a(lighten(base, 0.20f), 90));
        c.disc(16.0f, 19.0f, 10.0f, darken(base, 0.42f));
        c.disc(16.0f, 19.0f, 9.0f, base);
        c.ellipse(12.5f, 15.5f, 3.4f, 2.4f, accent);
        c.line(16, 10, 17, 4, rgba(150, 110, 40), 2);
        c.ellipse(21.0f, 6.0f, 4.0f, 2.4f, rgba(120, 190, 90));
        for(int k = 0; k < 4; k++)
            c.blend(8 + r.range(18), 12 + r.range(16), with_a(rgba(255, 255, 255), 220));
        break;
    case 3:   // 腐肉
        for(int y = 8; y < 28; y++)
            for(int x = 5; x < 28; x++)
            {
                f32 dx = (x - 16) / 12.0f, dy = (y - 18) / 10.0f;
                f32 w = 1.0f + std::sin(x * 0.6f + y * 0.4f) * 0.16f;
                if(dx * dx + dy * dy > w) continue;
                c.set(x, y, base);
            }
        for(int k = 0; k < 26; k++)
            c.blend(7 + r.range(19), 10 + r.range(17),
                    with_a(r.range(2) ? darken(base, 0.35f) : rgba(110, 150, 70),
                           (u8)r.range(100, 200)));
        break;
    default:  // 蠕虫诱饵
        for(int pass = 0; pass < 2; pass++)
        {
            for(int a = 0; a < 200; a++)
            {
                f32 t = a / 200.0f;
                f32 an = t * 9.0f;
                f32 x = 16 + std::cos(an) * (3.0f + t * 8.0f);
                f32 y = 6 + t * 20.0f;
                c.disc(x, y, pass ? 2.2f : 3.2f,
                       pass ? base : darken(base, 0.45f));
            }
        }
        for(int k = 0; k < 10; k++)
            c.blend(16 + r.range(-9, 9), 8 + r.range(18), with_a(accent, 170));
        break;
    }
}

// 顶边覆盖层：草皮 / 雪皮。只画上方 8px，下面透明，
// 这样叠在泥土方块上不会盖住泥土本身的纹理。
void gen_fringe(Canvas &c, u32 top, u32 top_dark, int h, bool ragged, u32 seed)
{
    c.clear(0);
    Rng r;
    r.seed((u64)seed * 668265263u + 313u);
    for(int x = 0; x < CELL; x++)
    {
        int hh = h;
        if(ragged) hh += r.range(3) - 1;
        for(int y = 0; y < hh; y++)
        {
            f32 t = (f32)y / (f32)hh;
            u32 col = mixc(lighten(top, 0.22f), darken(top_dark, 0.06f * t), t * 0.75f);
            if(r.range(100) < 16) col = darken(col, 0.14f);
            if(r.range(100) < 10) col = lighten(col, 0.14f);
            c.blend(x, y, col);
        }
        // 底边渐隐，避免出现一条硬线
        c.blend(x, hh, with_a(darken(top_dark, 0.1f), 150));
        if(ragged && r.range(100) < 45)
            c.blend(x, hh + 1, with_a(darken(top_dark, 0.2f), 90));
    }
    // 顶面高光
    for(int x = 0; x < CELL; x++) c.blend(x, 0, with_a(lighten(top, 0.45f), 170));
}

// ---- 写入图集 ----
void put(i32 slot, const Canvas &c)
{
    if(slot < 0 || slot >= MAX_SLOTS) return;
    i32 sx = (slot % COLS) * CELL;
    i32 sy = (slot / COLS) * CELL;
    for(i32 y = 0; y < CELL; y++)
        memcpy(&g_px[(sy + y) * AW + sx], &c.px[y * CELL], CELL * 4);
}

void put_solid(i32 slot, u32 col)
{
    Canvas c;
    c.clear(col);
    put(slot, c);
}

void draw_block_tile(i32 id, Canvas &c)
{
    const BlockDef &b = block_of(id);
    const u32 base = b.base;
    const u32 acc = b.accent;
    const u32 seed = 1000u + (u32)id * 7u;

    switch(b.style)
    {
    case TS_EMPTY:
        c.clear(0);
        break;
    case TS_ROCK:
        if(id == B_BEDROCK)
        {
            gen_rock(c, base, darken(base, 0.50f), lighten(base, 0.45f), seed);
            c.blotches(with_a(rgba(20, 20, 24, 200), 0), 30, 2, 6, seed + 9);
            c.blotches(rgba(20, 20, 24, 200), 30, 2, 6, seed + 9);
        }
        else if(id == B_OBSIDIAN)
            gen_rock(c, base, darken(base, 0.55f), rgba(126, 84, 190), seed);
        else
            gen_rock(c, base, acc, lighten(base, 0.30f), seed);
        break;
    case TS_DIRT:
        gen_dirt(c, base, acc, seed);
        break;
    case TS_GRASS:
        gen_grass(c, base, acc, seed, id == B_SNOW);
        break;
    case TS_LOG:
        gen_log(c, base, acc, seed);
        break;
    case TS_PLANK:
        gen_plank(c, base, darken(base, 0.42f), seed, true);
        break;
    case TS_LEAVES:
        gen_leaves(c, base, acc, seed);
        break;
    case TS_ORE:
    {
        u32 ore_lite = lighten(acc, 0.35f);
        if(id == B_COAL)             ore_lite = rgba(96, 96, 102);
        else if(id == B_RAW_IRON)    ore_lite = rgba(226, 180, 150);
        else if(id == B_RAW_GOLD)    ore_lite = rgba(255, 240, 170);
        else if(id == B_NETHER_GOLD) ore_lite = rgba(255, 232, 150);
        gen_ore(c, rgba(126, 126, 130), rgba(86, 86, 90), base, ore_lite, seed);
        break;
    }
    case TS_GEM:
        gen_gem(c, base, rgba(255, 255, 255), seed);
        break;
    case TS_WATER:
        gen_water(c, base, acc, seed);
        break;
    case TS_FLUID_PART:
        if(id <= 22) gen_fluid_part(c, base, acc, seed, 22 - id + 1);
        else         gen_fluid_part(c, base, acc, seed, 34 - id + 1);
        break;
    case TS_LAVA:
        gen_lava(c, base, acc, seed);
        break;
    case TS_ICE:
        gen_ice(c, base, acc, seed);
        break;
    case TS_SNOW:
        gen_grass(c, base, darken(base, 0.30f), seed, true);
        break;
    case TS_FURNACE:
        gen_furnace(c, base, acc, seed);
        break;
    case TS_CHEST:
        gen_chest(c, base, acc, seed);
        break;
    case TS_TORCH:
        gen_torch(c, acc, base);
        break;
    case TS_PORTAL:
        gen_portal(c, base, acc, seed);
        break;
    case TS_BED:
        gen_bed(c, base, acc);
        break;
    case TS_WOOL:
        gen_wool(c, base, seed);
        break;
    case TS_TABLE:
        gen_table(c, base, acc);
        break;
    case TS_NETHERRACK:
        gen_netherrack(c, base, acc, seed);
        break;
    case TS_EBON:
        gen_ebon(c, base, acc, seed, id == B_EBONWALL);
        break;
    case TS_DEBRIS:
        gen_debris(c, base, acc, seed);
        break;
    case TS_POTION:
        gen_potion(c, base, acc);
        break;
    case TS_FOOD:
        if(id == B_APPLE)             gen_food(c, base, acc, seed, 0);
        else if(id == B_ROTTEN_FLESH) gen_food(c, base, acc, seed, 3);
        else if(id == B_GOLDEN_APPLE) gen_food(c, base, acc, seed, 2);
        else if(id == B_WORM_FOOD)    gen_food(c, base, acc, seed, 4);
        else                          gen_food(c, base, acc, seed, 1);
        break;
    case TS_MISC:
        gen_i_core(c, base, acc);
        break;

    // ---- 物品图标 ----
    case TS_I_STICK:  gen_i_stick(c, base); break;
    case TS_I_PICK:   gen_i_pick(c, base, rgba(150, 108, 62), lighten(base, 0.50f)); break;
    case TS_I_AXE:    gen_i_axe(c, base, rgba(150, 108, 62), lighten(base, 0.50f)); break;
    case TS_I_SWORD:  gen_i_sword(c, base, acc, rgba(126, 86, 50)); break;
    case TS_I_HELM:   gen_i_helm(c, base); break;
    case TS_I_CHEST:  gen_i_chest(c, base); break;
    case TS_I_LEGS:   gen_i_legs(c, base); break;
    case TS_I_BOOT:   gen_i_boot(c, base); break;
    case TS_I_BOW:    gen_i_bow(c, base, rgba(238, 238, 238), acc); break;
    case TS_I_ARROW:  gen_i_arrow(c, base, acc); break;
    case TS_I_SHIELD: gen_i_shield(c, base, acc); break;
    case TS_I_BUCKET: gen_i_bucket(c, base, acc, id == B_BUCKET); break;
    case TS_I_INGOT:
        gen_i_ingot(c, base, lighten(base, 0.45f), id == B_NETHERITE_SCRAP);
        break;
    case TS_I_NUGGET: gen_i_nugget(c, base, rgba(255, 255, 255)); break;
    case TS_I_GEM:    gen_i_gem(c, base, rgba(255, 255, 255, 230)); break;
    case TS_I_STRING: gen_i_string(c, base); break;
    case TS_I_POWDER: gen_i_powder(c, base, acc); break;
    case TS_I_SCALE:  gen_i_scale(c, base, rgba(230, 200, 255)); break;
    case TS_I_AMULET: gen_i_amulet(c, base, rgba(255, 255, 255)); break;
    default:          gen_i_core(c, base, acc); break;
    }
}

// ============================================================================
//                            实体贴图
// ============================================================================
// 人形模板：kind 0 僵尸 1 玩家 2 骷髅 3 苦力怕 4 蜘蛛 5 猪/猪灵
void humanoid(Canvas &c, u32 skin, u32 cloth, u32 cloth2, int kind)
{
    c.clear(0);
    const u32 out = rgba(24, 20, 22, 220);
    switch(kind)
    {
    case 0:
    case 1:
        c.rect(10, 22, 5, 9, cloth2);
        c.rect(17, 22, 5, 9, cloth2);
        c.outline(out, 10, 22, 5, 9);
        c.outline(out, 17, 22, 5, 9);
        c.rect(9, 12, 14, 11, cloth);
        c.rect(9, 12, 14, 1, lighten(cloth, 0.25f));
        c.outline(out, 9, 12, 14, 11);
        c.rect(4, 12, 5, 11, kind == 0 ? cloth : skin);
        c.rect(23, 12, 5, 11, kind == 0 ? cloth : skin);
        c.outline(out, 4, 12, 5, 11);
        c.outline(out, 23, 12, 5, 11);
        c.rect(10, 3, 12, 9, skin);
        c.rect(10, 3, 12, 1, lighten(skin, 0.20f));
        c.outline(out, 10, 3, 12, 9);
        if(kind == 0)
        {
            c.rect(12, 6, 3, 2, rgba(30, 60, 30));
            c.rect(18, 6, 3, 2, rgba(30, 60, 30));
            c.rect(12, 6, 1, 1, rgba(120, 200, 110));
            c.rect(18, 6, 1, 1, rgba(120, 200, 110));
            c.rect(13, 10, 6, 1, rgba(20, 40, 20));
        }
        else
        {
            c.rect(12, 6, 3, 3, rgba(30, 40, 60));
            c.rect(18, 6, 3, 3, rgba(30, 40, 60));
            c.rect(12, 6, 1, 1, rgba(200, 220, 255));
            c.rect(18, 6, 1, 1, rgba(200, 220, 255));
        }
        break;
    case 2:   // 骷髅
        c.rect(11, 22, 3, 9, skin);
        c.rect(18, 22, 3, 9, skin);
        c.outline(out, 11, 22, 3, 9);
        c.outline(out, 18, 22, 3, 9);
        c.rect(12, 13, 8, 9, skin);
        for(int y = 14; y < 22; y += 2) c.rect(12, y, 8, 1, darken(skin, 0.35f));
        c.outline(out, 12, 13, 8, 9);
        c.rect(5, 13, 3, 10, skin);
        c.rect(24, 13, 3, 10, skin);
        c.outline(out, 5, 13, 3, 10);
        c.outline(out, 24, 13, 3, 10);
        c.rect(10, 3, 12, 10, skin);
        c.outline(out, 10, 3, 12, 10);
        c.rect(11, 7, 4, 4, rgba(12, 12, 16));
        c.rect(17, 7, 4, 4, rgba(12, 12, 16));
        c.rect(15, 11, 2, 3, rgba(12, 12, 16));
        c.rect(13, 12, 6, 1, rgba(12, 12, 16));
        break;
    case 3:   // 苦力怕
        c.rect(8, 20, 4, 11, cloth);
        c.rect(14, 20, 4, 11, cloth);
        c.rect(20, 20, 4, 11, cloth);
        c.rect(9, 9, 14, 12, cloth);
        c.rect(9, 9, 14, 1, lighten(cloth, 0.30f));
        c.rect(10, 2, 12, 8, cloth);
        c.outline(out, 10, 2, 12, 8);
        c.outline(out, 9, 9, 14, 12);
        c.rect(12, 4, 3, 3, rgba(10, 14, 10));
        c.rect(17, 4, 3, 3, rgba(10, 14, 10));
        c.rect(14, 7, 4, 2, rgba(10, 14, 10));
        c.rect(13, 9, 2, 1, rgba(10, 14, 10));
        c.rect(17, 9, 2, 1, rgba(10, 14, 10));
        c.blotches(with_a(darken(cloth, 0.30f), 120), 14, 2, 4, 31);
        break;
    case 4:   // 蜘蛛
        for(int k = 0; k < 4; k++)
        {
            int y = 14 + k * 4;
            c.line(16, 18, 3 - k, y - 4, darken(cloth, 0.30f), 2);
            c.line(16, 18, 29 + k, y - 4, darken(cloth, 0.30f), 2);
        }
        c.ellipse(16.0f, 20.0f, 9.0f, 8.0f, cloth);
        c.ellipse(16.0f, 11.0f, 6.0f, 5.0f, lighten(cloth, 0.12f));
        c.outline(out, 10, 6, 12, 22);
        c.rect(11, 9, 3, 3, rgba(200, 40, 40));
        c.rect(18, 9, 3, 3, rgba(200, 40, 40));
        c.rect(14, 12, 2, 2, rgba(240, 90, 90));
        c.rect(17, 12, 2, 2, rgba(240, 90, 90));
        break;
    default:  // 猪 / 猪灵（横向四足）
        c.rect(6, 16, 20, 10, cloth);
        c.rect(6, 16, 20, 1, lighten(cloth, 0.25f));
        c.rect(9, 25, 4, 6, darken(cloth, 0.20f));
        c.rect(20, 25, 4, 6, darken(cloth, 0.20f));
        c.rect(22, 10, 8, 8, cloth);
        c.outline(out, 22, 10, 8, 8);
        c.rect(28, 13, 4, 4, darken(cloth, 0.15f));
        c.rect(29, 14, 1, 2, rgba(20, 14, 14));
        c.rect(25, 11, 4, 3, rgba(220, 220, 220));
        c.rect(24, 13, 2, 2, rgba(15, 15, 20));
        c.outline(out, 6, 16, 20, 10);
        break;
    }
}

void gen_oi(Canvas &c, bool shielded)
{
    c.clear(0);
    const u32 core = rgba(110, 200, 255);
    const u32 dark = rgba(30, 60, 110);
    c.disc(16.0f, 16.0f, 15.0f, with_a(dark, 200));
    c.disc(16.0f, 16.0f, 13.5f, with_a(core, 210));
    for(int k = 0; k < 8; k++)
    {
        f32 an = k * 0.7854f;
        c.line((int)(16 + std::cos(an) * 9), (int)(16 + std::sin(an) * 9),
               (int)(16 + std::cos(an) * 15), (int)(16 + std::sin(an) * 15),
               with_a(rgba(200, 240, 255), 220), 2);
    }
    c.disc(16.0f, 16.0f, 7.5f, dark);
    c.disc(16.0f, 16.0f, 5.5f, rgba(255, 255, 255));
    c.disc(16.0f, 16.0f, 3.0f, rgba(120, 220, 255));
    c.disc(13.0f, 13.0f, 2.0f, rgba(255, 255, 255));
    if(shielded)
        for(int a = 0; a < 64; a++)
        {
            f32 an = a * 0.0982f;
            c.blend((int)(16 + std::cos(an) * 15.5f), (int)(16 + std::sin(an) * 15.5f),
                    with_a(rgba(255, 220, 120), 230));
        }
}

void gen_recursive(Canvas &c, bool big)
{
    c.clear(0);
    const u32 base = big ? rgba(176, 84, 224) : rgba(210, 140, 244);
    const u32 dark = rgba(64, 24, 96);
    const f32 r = big ? 14.0f : 8.5f;
    for(int k = 6; k >= 1; k--)
    {
        f32 rr = r * k / 6.0f;
        c.disc(16.0f, 16.0f, rr, (k % 2) ? base : dark);
    }
    c.disc(16.0f, 16.0f, r * 0.25f, rgba(255, 240, 255));
    for(int a = 0; a < 90; a++)
    {
        f32 an = a * 0.0698f;
        c.disc(16 + std::cos(an) * (r + 1.0f), 16 + std::sin(an) * (r + 1.0f), 1.1f,
               with_a(rgba(240, 200, 255), 200));
    }
}

void gen_worm(Canvas &c, int part, u32 base, u32 dark)
{
    c.clear(0);
    const u32 out = rgba(20, 12, 26, 220);
    if(part == 0)
    {
        c.ellipse(15.0f, 16.0f, 13.0f, 12.0f, dark);
        c.ellipse(15.0f, 16.0f, 12.0f, 11.0f, base);
        c.outline(out, 3, 5, 25, 22);
        c.ellipse(13.0f, 17.0f, 8.0f, 7.0f, rgba(40, 12, 30));
        c.ellipse(13.0f, 17.0f, 6.0f, 5.0f, rgba(120, 20, 50));
        for(int k = 0; k < 7; k++)
        {
            f32 an = 3.14159f + k * 0.45f;
            c.line((int)(13 + std::cos(an) * 6), (int)(17 + std::sin(an) * 5),
                   (int)(13 + std::cos(an) * 8), (int)(17 + std::sin(an) * 7),
                   rgba(240, 240, 230), 2);
        }
        c.disc(22.0f, 10.0f, 3.0f, rgba(255, 60, 60));
        c.disc(22.0f, 10.0f, 1.4f, rgba(255, 230, 230));
    }
    else if(part == 1)
    {
        c.ellipse(16.0f, 16.0f, 12.0f, 13.0f, dark);
        c.ellipse(16.0f, 16.0f, 10.5f, 11.5f, base);
        for(int y = 6; y < 27; y += 5)
            c.rect(6, y, 20, 1, with_a(darken(base, 0.35f), 200));
        c.outline(out, 5, 4, 22, 24);
        c.ellipse(12.0f, 11.0f, 3.0f, 2.0f, with_a(rgba(255, 255, 255), 70));
    }
    else
    {
        c.ellipse(16.0f, 18.0f, 11.0f, 11.0f, dark);
        c.ellipse(16.0f, 18.0f, 9.5f, 9.5f, base);
        c.line(16, 27, 16, 9, with_a(darken(base, 0.40f), 180), 2);
        c.outline(out, 6, 8, 20, 20);
    }
}

void gen_wither(Canvas &c)
{
    c.clear(0);
    const u32 bone = rgba(196, 196, 190);
    const u32 dark = rgba(52, 48, 52);
    c.rect(9, 3, 8, 9, bone);
    c.rect(13, 1, 8, 9, lighten(bone, 0.10f));
    c.rect(18, 4, 8, 9, bone);
    c.outline(dark, 9, 3, 8, 9);
    c.outline(dark, 13, 1, 8, 9);
    c.outline(dark, 18, 4, 8, 9);
    c.rect(11, 6, 2, 2, rgba(255, 60, 40));
    c.rect(15, 4, 2, 2, rgba(255, 60, 40));
    c.rect(21, 7, 2, 2, rgba(255, 60, 40));
    c.rect(11, 12, 10, 14, dark);
    for(int y = 13; y < 26; y += 3) c.rect(9, y, 14, 2, bone);
    c.outline(dark, 9, 12, 14, 14);
    c.rect(4, 12, 6, 5, bone);
    c.rect(22, 12, 6, 5, bone);
}

void gen_shadow_ent(Canvas &c)
{
    c.clear(0);
    for(int k = 0; k < 30; k++)
    {
        Rng r;
        r.seed(0xA11CEu + (u64)k * 7919u);
        c.disc(r.unit(10, 22), r.unit(8, 26), r.unit(4, 9),
               with_a(rgba(24, 12, 40), (u8)r.range(90, 180)));
    }
    c.ellipse(16.0f, 17.0f, 11.0f, 13.0f, rgba(30, 16, 52));
    c.outline(rgba(12, 6, 24, 220), 5, 4, 22, 26);
    c.rect(11, 13, 3, 3, rgba(190, 120, 255));
    c.rect(18, 13, 3, 3, rgba(190, 120, 255));
    c.rect(11, 16, 10, 1, rgba(120, 60, 180));
}

void gen_player_arm(Canvas &c, u32 skin, u32 sleeve)
{
    c.clear(0);
    c.rect(3, 12, 26, 9, sleeve);
    c.rect(3, 12, 26, 1, lighten(sleeve, 0.25f));
    c.rect(24, 12, 6, 9, skin);
    c.outline(rgba(24, 20, 22, 210), 3, 12, 26, 9);
    c.rect(6, 15, 14, 2, with_a(darken(sleeve, 0.30f), 160));
}

// ---- 特效 ----
void gen_spark(Canvas &c)
{
    c.clear(0);
    c.disc(16.0f, 16.0f, 4.0f, with_a(rgba(255, 240, 190), 120));
    c.disc(16.0f, 16.0f, 2.2f, rgba(255, 250, 225));
    for(int k = 0; k < 4; k++)
    {
        f32 an = k * 1.5708f;
        c.line((int)(16 + std::cos(an) * 3), (int)(16 + std::sin(an) * 3),
               (int)(16 + std::cos(an) * 9), (int)(16 + std::sin(an) * 9),
               with_a(rgba(255, 230, 160), 150), 1);
    }
}

void gen_smoke(Canvas &c)
{
    c.clear(0);
    Rng r;
    r.seed(0x5A0Cu);
    for(int k = 0; k < 14; k++)
        c.disc(r.unit(9, 23), r.unit(9, 23), r.unit(3.5f, 7.0f),
               with_a(rgba(150, 150, 156), (u8)r.range(40, 110)));
}

void gen_flame(Canvas &c)
{
    c.clear(0);
    c.ellipse(16.0f, 19.0f, 7.0f, 10.0f, with_a(rgba(226, 90, 26), 200));
    c.ellipse(16.0f, 20.0f, 5.0f, 7.0f, with_a(rgba(255, 158, 44), 220));
    c.ellipse(16.0f, 21.0f, 3.0f, 4.5f, with_a(rgba(255, 238, 170), 240));
    c.line(16, 5, 16, 12, with_a(rgba(255, 180, 80), 150), 2);
}

void gen_bubble(Canvas &c)
{
    c.clear(0);
    for(int a = 0; a < 128; a++)
    {
        f32 an = a * 0.0491f;
        c.disc(16 + std::cos(an) * 13.0f, 16 + std::sin(an) * 13.0f, 1.9f,
               rgba(255, 255, 255));
    }
    c.disc(11.0f, 11.0f, 2.4f, with_a(rgba(255, 255, 255), 190));
    c.disc(16.0f, 16.0f, 10.0f, with_a(rgba(190, 230, 255), 55));
}

void gen_splash(Canvas &c)
{
    c.clear(0);
    c.ellipse(16.0f, 22.0f, 12.0f, 5.0f, with_a(rgba(120, 190, 255), 150));
    for(int k = 0; k < 6; k++)
    {
        f32 an = -0.5f - k * 0.42f;
        c.line(16, 22, (int)(16 + std::cos(an) * 13), (int)(22 + std::sin(an) * 13),
               with_a(rgba(180, 224, 255), 190), 2);
    }
}

void gen_dust(Canvas &c)
{
    c.clear(0);
    Rng r;
    r.seed(0xD057u);
    for(int k = 0; k < 22; k++)
        c.disc(r.unit(6, 26), r.unit(6, 26), r.unit(1.2f, 3.0f),
               with_a(rgba(210, 190, 160), (u8)r.range(50, 140)));
}

void gen_explosion(Canvas &c)
{
    c.clear(0);
    for(int k = 5; k >= 1; k--)
    {
        u32 col = (k > 3) ? with_a(rgba(255, 120, 30), 130)
                : (k > 1) ? with_a(rgba(255, 190, 60), 180)
                          : with_a(rgba(255, 250, 210), 230);
        c.disc(16.0f, 16.0f, k * 3.0f, col);
    }
    for(int k = 0; k < 12; k++)
    {
        f32 an = k * 0.5236f;
        c.disc(16 + std::cos(an) * 13, 16 + std::sin(an) * 13, 2.4f,
               with_a(rgba(255, 170, 70), 150));
    }
}

void gen_shockwave(Canvas &c)
{
    c.clear(0);
    for(int a = 0; a < 160; a++)
    {
        f32 an = a * 0.0393f;
        u32 col = with_a(rgba(255, 255, 255), (u8)(150 + 90 * std::sin(an * 7)));
        c.disc(16 + std::cos(an) * 14.0f, 16 + std::sin(an) * 14.0f, 1.6f, col);
    }
}

void gen_star(Canvas &c)
{
    c.clear(0);
    c.disc(16.0f, 16.0f, 3.0f, rgba(255, 250, 220));
    for(int k = 0; k < 4; k++)
    {
        f32 an = k * 1.5708f + 0.7854f;
        c.line((int)(16 + std::cos(an) * 3), (int)(16 + std::sin(an) * 3),
               (int)(16 + std::cos(an) * 13), (int)(16 + std::sin(an) * 13),
               with_a(rgba(255, 246, 200), 200), 2);
    }
}

void gen_glow(Canvas &c)
{
    c.clear(0);
    for(int r = 15; r >= 1; r--)
    {
        f32 t = 1.0f - r / 16.0f;
        c.disc(16.0f, 16.0f, (f32)r, with_a(rgba(255, 255, 255), (u8)(t * t * 200)));
    }
}

void gen_crack(Canvas &c, int level)
{
    c.clear(0);
    if(level <= 0) return;
    Rng r;
    r.seed(0xC4ACu + (u64)level * 131u);
    const u32 col = with_a(rgba(10, 10, 12), (u8)(90 + level * 15));
    for(int k = 0; k < 1 + level; k++)
    {
        int x = r.range(CELL), y = r.range(CELL);
        int len = 4 + level * 2;
        int dx = r.range(3) - 1, dy = r.range(2);
        if(dx == 0) dx = 1;
        for(int q = 0; q < len; q++)
        {
            c.blend(x + q * dx, y + q * dy, col);
            if(q % 3 == 0) c.blend(x + q * dx + 1, y + q * dy, with_a(col, 120));
        }
    }
    if(level >= 5)
        for(int k = 0; k < level - 2; k++)
        {
            f32 an = r.unit(0, 6.283f);
            c.line((int)(16 + std::cos(an) * 4), (int)(16 + std::sin(an) * 4),
                   (int)(16 + std::cos(an) * 15), (int)(16 + std::sin(an) * 15),
                   with_a(col, 140), 1);
        }
}

void gen_arrow_fx(Canvas &c, u32 shaft, u32 head)
{
    c.clear(0);
    c.line(4, 28, 26, 6, darken(shaft, 0.50f), 3);
    c.line(4, 28, 26, 6, shaft, 1);
    for(int i = 0; i < 8; i++) c.rect(25 - i, 4 + i, 2 * i + 2, 1, head);
    c.rect(25, 4, 2, 2, lighten(head, 0.45f));
    for(int i = 0; i < 7; i++)
    {
        c.rect(3 + i, 25 - i, 4, 1, with_a(rgba(238, 238, 238), (u8)(220 - i * 20)));
        c.rect(5 + i, 27 - i, 4, 1, with_a(rgba(210, 210, 210), (u8)(200 - i * 20)));
    }
}

// ---- UI ----
void gen_panel_tex(Canvas &c, u32 fill, u32 seed)
{
    c.clear(0);
    c.rect(0, 0, CELL, CELL, fill);
    Rng r;
    r.seed((u64)seed * 668265263u + 4242u);
    for(int k = 0; k < 70; k++)
        c.blend(r.range(CELL), r.range(CELL),
                with_a(r.range(2) ? rgba(255, 255, 255) : rgba(0, 0, 0),
                       (u8)r.range(6, 22)));
    // 左上角 1/4 圆之外抠掉（圆角半径 = CELL）
    for(int y = 0; y < CELL; y++)
        for(int x = 0; x < CELL; x++)
        {
            f32 d = std::sqrt((f32)(x * x + y * y));
            if(d > CELL - 0.5f) c.set(x, y, with_a(c.get(x, y), 0));
        }
}

void gen_slot(Canvas &c, u32 fill, u32 border, bool highlight)
{
    c.clear(0);
    c.rect(0, 0, CELL, CELL, fill);
    c.rect(0, 0, CELL, 2, darken(fill, 0.35f));
    c.rect(0, 0, 2, CELL, darken(fill, 0.25f));
    c.rect(0, CELL - 2, CELL, 2, lighten(fill, 0.18f));
    c.rect(CELL - 2, 0, 2, CELL, lighten(fill, 0.12f));
    if(highlight) c.rect(2, 2, CELL - 4, CELL - 4, with_a(rgba(255, 255, 255), 40));
    c.frame(0, 0, CELL, CELL, border, 1);
}

void gen_cursor(Canvas &c, bool cross)
{
    c.clear(0);
    if(cross)
    {
        c.rect(14, 2, 4, 12, rgba(255, 255, 255));
        c.rect(14, 18, 4, 12, rgba(255, 255, 255));
        c.rect(2, 14, 12, 4, rgba(255, 255, 255));
        c.rect(18, 14, 12, 4, rgba(255, 255, 255));
        c.rect(15, 15, 2, 2, rgba(255, 80, 80));
        c.outline(rgba(0, 0, 0, 200), 14, 2, 4, 12);
        c.outline(rgba(0, 0, 0, 200), 14, 18, 4, 12);
        c.outline(rgba(0, 0, 0, 200), 2, 14, 12, 4);
        c.outline(rgba(0, 0, 0, 200), 18, 14, 12, 4);
    }
    else
    {
        for(int y = 0; y < 22; y++)
        {
            int w = 1 + (y < 12 ? y / 2 : (22 - y) / 3);
            for(int x = 0; x < w; x++) c.set(x, y, rgba(255, 255, 255));
            c.set(w, y, rgba(20, 20, 24));
        }
        c.line(0, 0, 0, 21, rgba(20, 20, 24));
    }
}

void gen_heart(Canvas &c, u32 col, bool bg, bool half)
{
    c.clear(0);
    static const char *H[9] = {
        "..##.##..",
        ".#######.",
        "#########",
        "#########",
        "#########",
        ".#######.",
        "..#####..",
        "...###...",
        "....#....",
    };
    const u32 body = bg ? rgba(30, 26, 28) : col;
    const u32 shine = bg ? rgba(50, 44, 46) : lighten(col, 0.55f);
    for(int y = 0; y < 9; y++)
        for(int x = 0; x < 9; x++)
        {
            if(H[y][x] != '#') continue;
            u32 cc = body;
            if(half && x >= 5) cc = bg ? rgba(30, 26, 28) : darken(col, 0.60f);
            if(x == 2 && y == 1) cc = shine;
            c.rect(2 + x * 3, 1 + y * 3, 3, 3, cc);
        }
}

void gen_armor_icon(Canvas &c)
{
    c.clear(0);
    c.rect(8, 8, 16, 14, rgba(220, 228, 240));
    c.ellipse(7.0f, 9.0f, 5.0f, 4.0f, rgba(220, 228, 240));
    c.ellipse(25.0f, 9.0f, 5.0f, 4.0f, rgba(220, 228, 240));
    c.rect(14, 11, 4, 9, rgba(120, 130, 150));
    c.outline(rgba(40, 46, 58), 8, 8, 16, 14);
}

void gen_dur_bar(Canvas &c, bool fill)
{
    c.clear(0);
    if(fill)
    {
        c.rect(0, 0, CELL, CELL, rgba(255, 255, 255));
        c.rect(0, 0, CELL, 2, rgba(255, 255, 255));
        c.rect(0, CELL * 2 / 3, CELL, CELL / 3, rgba(196, 196, 196));
    }
    else
    {
        c.rect(0, 0, CELL, CELL, rgba(18, 16, 20));
        c.rect(0, 0, CELL, CELL / 3, rgba(8, 8, 10));
        c.rect(0, CELL - 2, CELL, 2, rgba(60, 58, 64));
    }
}

void gen_broken(Canvas &c)
{
    c.clear(0);
    c.line(6, 26, 16, 16, rgba(120, 84, 48), 3);
    c.line(20, 12, 28, 5, rgba(120, 84, 48), 3);
    c.line(16, 16, 18, 14, with_a(rgba(255, 90, 70), 220), 2);
    c.line(18, 12, 20, 14, with_a(rgba(255, 90, 70), 220), 2);
    c.rect(20, 4, 8, 3, rgba(160, 160, 166));
    c.rect(22, 6, 4, 2, rgba(160, 160, 166));
    c.rect(9, 27, 3, 2, rgba(150, 150, 156));
    c.rect(24, 27, 2, 2, rgba(150, 150, 156));
}

void gen_arrow_ui(Canvas &c, int dir)
{
    c.clear(0);
    for(int i = 0; i < 12; i++)
    {
        int w = 12 - i;
        // i=0 是尾部(宽 w=12)，i=11 是尖端(窄 w=1)。让尖端朝向 dir 指示的方向，并整体居中。
        switch(dir)
        {
        case 0: c.rect(21 - i, 16 - w / 2, 1, w, rgba(240, 240, 245)); break;   // 左 ◁
        case 1: c.rect(10 + i, 16 - w / 2, 1, w, rgba(240, 240, 245)); break;   // 右 ▷
        case 2: c.rect(16 - w / 2, 21 - i, w, 1, rgba(240, 240, 245)); break;   // 上 △
        default: c.rect(16 - w / 2, 10 + i, w, 1, rgba(240, 240, 245)); break;  // 下 ▽
        }
    }
}

void gen_check(Canvas &c, bool cross)
{
    c.clear(0);
    if(cross)
    {
        c.line(8, 8, 24, 24, rgba(232, 90, 80), 4);
        c.line(24, 8, 8, 24, rgba(232, 90, 80), 4);
    }
    else
    {
        c.line(7, 16, 14, 24, rgba(120, 226, 130), 4);
        c.line(14, 24, 26, 7, rgba(120, 226, 130), 4);
    }
}

void gen_gear(Canvas &c)
{
    c.clear(0);
    c.disc(16.0f, 16.0f, 10.0f, rgba(220, 226, 236));
    for(int k = 0; k < 8; k++)
    {
        f32 an = k * 0.7854f;
        c.rect((int)(16 + std::cos(an) * 12) - 2, (int)(16 + std::sin(an) * 12) - 2, 4, 4,
               rgba(220, 226, 236));
    }
    c.disc(16.0f, 16.0f, 4.0f, with_a(rgba(0, 0, 0), 0));
    for(int a = 0; a < 40; a++)
    {
        f32 an = a * 0.157f;
        c.disc(16 + std::cos(an) * 4.0f, 16 + std::sin(an) * 4.0f, 1.2f,
               rgba(30, 30, 36));
    }
}

void gen_map_pin(Canvas &c)
{
    c.clear(0);
    c.disc(16.0f, 12.0f, 8.0f, rgba(240, 80, 70));
    c.disc(16.0f, 12.0f, 4.0f, rgba(255, 220, 210));
    c.line(16, 18, 16, 29, rgba(240, 80, 70), 3);
}

void gen_button(Canvas &c, u32 top, u32 bottom, bool hl)
{
    c.clear(0);
    for(int y = 0; y < CELL; y++)
    {
        u32 col = mixc(top, bottom, y / (f32)CELL);
        if(hl) col = lighten(col, 0.18f);
        c.rect(0, y, CELL, 1, col);
    }
    c.rect(0, 0, CELL, 2, with_a(rgba(255, 255, 255), hl ? 110 : 70));
    c.rect(0, CELL - 2, CELL, 2, with_a(rgba(0, 0, 0), 90));
}

void gen_title_bar(Canvas &c)
{
    c.clear(0);
    for(int y = 0; y < CELL; y++)
        c.rect(0, y, CELL, 1, mixc(rgba(46, 40, 66), rgba(24, 20, 36), y / (f32)CELL));
    c.rect(0, 0, CELL, 1, with_a(rgba(255, 226, 140), 150));
    c.rect(0, CELL - 1, CELL, 1, with_a(rgba(0, 0, 0), 160));
}

}   // namespace

// MC 里树叶 / 水这类贴图是**灰度图**，靠生物群系色在地图上染色；
// 直接拿来会发白，所以这里按 id 手动乘一层颜色。
u32 tile_tint(i32 id)
{
    switch(id)
    {
    case B_LEAF:  return rgba( 88, 186,  68);   // 树叶 -> 绿
    case B_WATER: case B_FLOW3: case B_FLOW2: case B_FLOW1:
                  return rgba( 70, 132, 234);   // 水 -> 蓝
    default:      return rgba(255, 255, 255);   // 不染色
    }
}

// 把 assets/tiles/<file> 画进指定槽位。找不到文件就返回 false，保留原贴图。
// tint_id 用于给树叶/水这类灰度图染色（传 -1 表示不染色）。
bool blit_file(i32 slot, const char *file, i32 tint_id)
{
    char path[256];
    snprintf(path, sizeof(path), "assets/tiles/%s", file);
    std::vector<u32> src;
    i32 w = 0, h = 0;
    if(!load_png_rgba(path, src, w, h) || w <= 0 || h <= 0) return false;

    const u32 tint = tile_tint(tint_id);
    const bool do_tint = tint != rgba(255, 255, 255);
    const u32 tr = tint & 0xFF, tg = (tint >> 8) & 0xFF, tb = (tint >> 16) & 0xFF;

    Canvas c;
    c.clear(0);
    // 树叶贴图有大量镂空像素（MC 是靠镂空 + 群系色），2D 里会透出天空发白，
    // 所以先铺一层不透明的深绿底，再把纹理叠上去。
    if(tint_id == B_LEAF) c.clear(lighten(tint, 0.18f));
    for(i32 y = 0; y < CELL; y++)
        for(i32 x = 0; x < CELL; x++)
        {
            // 最近邻放大，保持像素画锐利（16x16 -> 32x32 正好 x2）
            i32 sx = x * w / CELL, sy = y * h / CELL;
            u32 px = src[(size_t)sy * w + sx];
            u32 a = (px >> 24) & 0xFF;
            if(do_tint && a)
            {
                u32 r = (px & 0xFF) * tr / 255;
                u32 g = ((px >> 8) & 0xFF) * tg / 255;
                u32 b = ((px >> 16) & 0xFF) * tb / 255;
                px = (a << 24) | (b << 16) | (g << 8) | r;
            }
            c.blend(x, y, px);
        }
    put(slot, c);
    return true;
}

// 物品/方块贴图：<英文名>.png
bool blit_external(i32 id)
{
    char file[128];
    snprintf(file, sizeof(file), "%s.png", block_of(id).name);
    return blit_file(slot_of_block(id), file, id);
}

// ---- 双图：矿脉版贴图（世界里用）----
struct OreTex { i32 id; i32 slot; const char *file; };
const OreTex ORE_TEX[] = {
    { B_COAL,            W_ORE_COAL,        "ore_coal.png" },
    { B_RAW_IRON,        W_ORE_RAW_IRON,    "ore_raw_iron.png" },
    { B_DIAMOND,         W_ORE_DIAMOND,     "ore_diamond.png" },
    { B_RAW_GOLD,        W_ORE_RAW_GOLD,    "ore_raw_gold.png" },
    { B_NETHER_GOLD,     W_ORE_NETHER_GOLD, "ore_nether_gold.png" },
    { B_ANCIENT_DEBRIS,  W_ORE_DEBRIS,      "ore_debris.png" },
};
constexpr i32 ORE_TEX_N = (i32)(sizeof(ORE_TEX) / sizeof(ORE_TEX[0]));

i32 slot_of_world_block(i32 id)
{
    for(i32 k = 0; k < ORE_TEX_N; k++)
        if(ORE_TEX[k].id == id) return ORE_TEX[k].slot;
    return slot_of_block(id);
}

// ============================================================================
//                        MC 皮肤解析与拼装
// ============================================================================
// MC 皮肤（64x64）是「展开图」：每个部位（头/身体/手臂/腿）都是一组
// 小方块，分别贴在立方体的六个面上。整张画出来就是一堆错位的方块，
// 必须按下面的坐标把各部位的**正面**抠出来、按人物结构拼起来。
//
// 拼好的立绘是 16x32（宽高比 1:2）：
//     [ 头 8x8     ]          x=4..12  y=0..8
//   [臂][身体 8x12][臂]        y=8..20   臂 x=0..4 / 12..16
//     [右腿][左腿  ]          y=20..32  腿 x=4..12
// 放进槽位的**左半边**（右半边留空），绘制时用 draw_sub 取左半边。
namespace {
std::vector<std::string> g_skins;    // assets/skins/ 下的文件名
i32 g_skin_cur = 0;

// 把皮肤里 (sx,sy,w,h) 的一块抠到立绘的 (dx,dy)
void skin_blit(Canvas &c, const u32 *skin, i32 sw, i32 sh,
               i32 dx, i32 dy, i32 sx, i32 sy, i32 w, i32 h)
{
    for(i32 y = 0; y < h; y++)
        for(i32 x = 0; x < w; x++)
        {
            i32 fx = sx + x, fy = sy + y;
            if(fx < 0 || fx >= sw || fy < 0 || fy >= sh) continue;
            c.set(dx + x, dy + y, skin[(size_t)fy * sw + fx]);
        }
}
}   // namespace

void assemble_player_skin(const u32 *skin, i32 sw, i32 sh, i32 slot)
{
    if(!skin || sw < 64 || sh < 32) return;

    // 各部位「正面」在皮肤里的位置（实测自官方皮肤，非凭记忆的坐标）：
    //   头 (8,8) 8x8  身体 (20,20) 8x12  右臂 (44,20) 4x12  右腿 (4,20) 4x12
    //   左臂 (36,52) 4x12   左腿 (20,52) 4x12   ← 64x64 里左腿区块在 (16,48)
    // 旧版 64x32 皮肤没有独立的左臂/左腿，直接复用右侧那块
    const bool legacy = (sh < 64);
    const i32 LARM_X = legacy ? 44 : 36, LARM_Y = legacy ? 20 : 52;
    const i32 LLEG_X = legacy ?  4 : 20, LLEG_Y = legacy ? 20 : 52;

    Canvas c;
    c.clear(0);
    //                    立绘位置    皮肤里的正面块坐标
    skin_blit(c, skin, sw, sh,  4,  0,   8,  8, 8, 8);    // 头
    skin_blit(c, skin, sw, sh,  4,  8,  20, 20, 8, 12);   // 身体
    skin_blit(c, skin, sw, sh,  0,  8,  44, 20, 4, 12);   // 右臂（屏幕左）
    skin_blit(c, skin, sw, sh, 12,  8, LARM_X, LARM_Y, 4, 12);   // 左臂（屏幕右）
    skin_blit(c, skin, sw, sh,  4, 20,   4, 20, 4, 12);   // 右腿（屏幕左）
    skin_blit(c, skin, sw, sh,  8, 20, LLEG_X, LLEG_Y, 4, 12);   // 左腿（屏幕右）

    put(slot, c);
}

// ---- 换肤：扫描 assets/skins/*.png ----
void skin_scan()
{
    g_skins.clear();
    WIN32_FIND_DATAA fd;
    HANDLE fh = FindFirstFileA("assets\\skins\\*.png", &fd);
    if(fh != INVALID_HANDLE_VALUE)
    {
        do
        {
            if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            g_skins.push_back(fd.cFileName);
        } while(FindNextFileA(fh, &fd));
        FindClose(fh);
    }
    if(g_skin_cur >= (i32)g_skins.size()) g_skin_cur = 0;
    fprintf(stderr, "[skin] 发现皮肤 %d 张\n", (int)g_skins.size());
}

i32 skin_count() { return (i32)g_skins.size(); }

const char *skin_name(i32 idx)
{
    if(idx < 0 || idx >= (i32)g_skins.size()) return "";
    return g_skins[idx].c_str();
}

i32 skin_current() { return g_skin_cur; }
void skin_set_current(i32 idx) { g_skin_cur = idx; }

bool skin_apply(i32 idx)
{
    if(idx < 0 || idx >= (i32)g_skins.size()) return false;
    char path[256];
    snprintf(path, sizeof(path), "assets/skins/%s", g_skins[idx].c_str());
    std::vector<u32> buf;
    i32 w = 0, h = 0;
    if(!load_png_rgba(path, buf, w, h) || buf.empty()) return false;
    assemble_player_skin(buf.data(), w, h);
    g_skin_cur = idx;
    fprintf(stderr, "[skin] 应用皮肤 #%d %s (%dx%d)\n", idx, g_skins[idx].c_str(), w, h);
    return true;
}

// ============================================================================
//                        MC 原版怪物皮肤
// ============================================================================
// 官方怪物贴图也是展开图（僵尸/猪灵 64x64，骷髅/苦力怕/蜘蛛 64x32），
// 放在工程外的官方资源目录里。人形怪与玩家皮肤同布局，直接用
// assemble_player_skin 拼立绘；苦力怕/蜘蛛不是人形，按各自的 UV 抠部件。
// 找不到贴图就保留 build() 里先画好的程序化贴图。
namespace {
// 皮肤里 (sx,sy,w,h) 的一块「正面」缩放拷到立绘 (dx,dy,dw,dh)
struct SkinPart { i32 sx, sy, w, h, dx, dy, dw, dh; };

void skin_blit_scaled(Canvas &c, const u32 *skin, i32 sw, i32 sh, const SkinPart &p)
{
    for(i32 y = 0; y < p.dh; y++)
        for(i32 x = 0; x < p.dw; x++)
        {
            i32 fx = p.sx + x * p.w / p.dw, fy = p.sy + y * p.h / p.dh;
            if(fx < 0 || fx >= sw || fy < 0 || fy >= sh) continue;
            c.set(p.dx + x, p.dy + y, skin[(size_t)fy * sw + fx]);
        }
}

void assemble_parts(i32 slot, const u32 *skin, i32 sw, i32 sh,
                    const SkinPart *parts, i32 n)
{
    Canvas c;
    c.clear(0);
    for(i32 k = 0; k < n; k++) skin_blit_scaled(c, skin, sw, sh, parts[k]);
    put(slot, c);
}

// 官方资源目录（在工程外）：相对路径够不到就退回绝对路径
const char *MC_ENTITY_DIR[] = {
    "../Default-Java-java-latest/assets/minecraft/textures/entity",
    "D:/cpp/2DMC/Default-Java-java-latest/assets/minecraft/textures/entity",
};

bool load_mc_skin(const char *rel, std::vector<u32> &buf, i32 &w, i32 &h)
{
    char path[512];
    for(i32 k = 0; k < 2; k++)
    {
        snprintf(path, sizeof(path), "%s/%s", MC_ENTITY_DIR[k], rel);
        if(load_png_rgba_full(path, buf, w, h) && !buf.empty()) return true;
    }
    return false;
}

// 把 5 种怪物的官方皮肤拼成 16x32 立绘写进各自槽位，返回成功个数
i32 mc_mob_skins()
{
    std::vector<u32> buf;
    i32 w = 0, h = 0, ok = 0;

    // 人形怪：布局与玩家皮肤一致（64x64 有独立左臂/左腿，64x32 走旧版复用）
    const struct { i32 slot; const char *file; } HUM[] = {
        { E_ZOMBIE,   "zombie/zombie.png" },
        { E_SKELETON, "skeleton/skeleton.png" },
        { E_PIGLIN,   "piglin/zombified_piglin.png" },
    };
    for(const auto &m : HUM)
        if(load_mc_skin(m.file, buf, w, h))
        {
            assemble_player_skin(buf.data(), w, h, m.slot);
            ok++;
        }

    // 苦力怕：头/身体与人形同坐标、没有手臂；腿只有 6 行，拉伸到 12 行才够到脚点
    if(load_mc_skin("creeper/creeper.png", buf, w, h))
    {
        const SkinPart parts[] = {
            {  8,  8, 8,  8,  4,  0, 8,  8 },    // 头
            { 20, 20, 8, 12,  4,  8, 8, 12 },    // 身体
            {  4, 20, 4,  6,  4, 20, 4, 12 },    // 右腿（屏幕左）
            {  4, 20, 4,  6,  8, 20, 4, 12 },    // 左腿（屏幕右）
        };
        assemble_parts(E_CREEPER, buf.data(), w, h, parts, 4);
        ok++;
    }

    // 蜘蛛：头（红眼）/颈/腹部 + 两侧各 4 条腿，最下面一条够到脚点
    if(load_mc_skin("spider/spider.png", buf, w, h))
    {
        const SkinPart parts[] = {
            { 40, 12,  8, 8,  4,  0,  8,  8 },   // 头
            {  6,  6,  6, 6,  5,  8,  6,  6 },   // 颈
            { 12, 24, 10, 8,  3, 14, 10, 12 },   // 腹部
            // 8 条腿共用一块腿贴图，左右各 4 条
            { 20,  2,  3, 2,  0,  6,  3,  5 },
            { 20,  2,  3, 2, 13,  6,  3,  5 },
            { 20,  2,  3, 2,  0, 13,  3,  5 },
            { 20,  2,  3, 2, 13, 13,  3,  5 },
            { 20,  2,  3, 2,  0, 20,  3,  5 },
            { 20,  2,  3, 2, 13, 20,  3,  5 },
            { 20,  2,  3, 2,  0, 27,  3,  5 },
            { 20,  2,  3, 2, 13, 27,  3,  5 },
        };
        assemble_parts(E_SPIDER, buf.data(), w, h, parts,
                       (i32)(sizeof(parts) / sizeof(parts[0])));
        ok++;
    }
    return ok;
}
}   // namespace

void build()
{
    memset(g_px, 0, sizeof(g_px));

    put_solid(S_WHITE, rgba(255, 255, 255));

    for(i32 id = 0; id < BLOCK_COUNT; id++)
    {
        Canvas c;
        draw_block_tile(id, c);
        put(slot_of_block(id), c);
    }

    // ---- 外部贴图优先：assets/tiles/<英文名>.png 存在就覆盖 ----
    {
        i32 ext = 0;
        for(i32 id = 0; id < BLOCK_COUNT; id++)
            if(blit_external(id)) ext++;
        fprintf(stderr, "[atlas] 外部贴图已载入 %d / %d\n", ext, (int)BLOCK_COUNT);
    }

    // ---- 双图：矿脉版（世界里「嵌在石头里」的那一张）----
    // 先画程序化底图兜底，外部贴图缺失时这个槽位不至于是空的。
    {
        i32 o = 0;
        for(i32 k = 0; k < ORE_TEX_N; k++)
        {
            Canvas c;
            draw_block_tile(ORE_TEX[k].id, c);
            put(ORE_TEX[k].slot, c);
            if(blit_file(ORE_TEX[k].slot, ORE_TEX[k].file, ORE_TEX[k].id)) o++;
        }
        fprintf(stderr, "[atlas] 矿脉双图已载入 %d / %d\n", o, (int)ORE_TEX_N);
    }

    // ---- 实体 ----
    { Canvas c; humanoid(c, rgba(96, 142, 96), rgba(48, 84, 132), rgba(38, 62, 104), 0); put(E_ZOMBIE, c); }
    { Canvas c; humanoid(c, rgba(232, 196, 156), rgba(70, 132, 200), rgba(52, 76, 140), 1); put(E_PLAYER, c); }
    { Canvas c; humanoid(c, rgba(222, 222, 216), rgba(200, 200, 194), rgba(180, 180, 174), 2); put(E_SKELETON, c); }
    { Canvas c; humanoid(c, rgba(108, 180, 96), rgba(108, 180, 96), rgba(96, 160, 86), 3); put(E_CREEPER, c); }
    { Canvas c; humanoid(c, rgba(60, 44, 44), rgba(66, 48, 48), rgba(48, 36, 36), 4); put(E_SPIDER, c); }
    { Canvas c; humanoid(c, rgba(232, 168, 148), rgba(96, 152, 104), rgba(70, 110, 78), 5); put(E_PIGLIN, c); }
    { Canvas c; humanoid(c, rgba(226, 226, 222), rgba(226, 226, 222), rgba(196, 196, 192), 5); put(E_SHEEP, c); }
    { Canvas c; humanoid(c, rgba(240, 176, 176), rgba(240, 176, 176), rgba(214, 146, 146), 5); put(E_PIG, c); }
    { Canvas c; gen_oi(c, false);        put(E_OI, c); }
    { Canvas c; gen_oi(c, true);         put(E_OI_SHIELDED, c); }
    { Canvas c; gen_recursive(c, true);  put(E_RECURSIVE_BIG, c); }
    { Canvas c; gen_recursive(c, false); put(E_RECURSIVE_SMALL, c); }
    { Canvas c; gen_worm(c, 0, rgba(150, 60, 96), rgba(78, 26, 52)); put(E_WORM_HEAD, c); }
    { Canvas c; gen_worm(c, 1, rgba(150, 60, 96), rgba(78, 26, 52)); put(E_WORM_BODY, c); }
    { Canvas c; gen_worm(c, 2, rgba(150, 60, 96), rgba(78, 26, 52)); put(E_WORM_TAIL, c); }
    { Canvas c; gen_wither(c);      put(E_WITHER, c); }
    { Canvas c; gen_shadow_ent(c);  put(E_SHADOW, c); }
    { Canvas c; humanoid(c, rgba(228, 190, 150), rgba(96, 62, 140), rgba(58, 40, 90), 1); put(E_NPC, c); }
    { Canvas c; gen_heart(c, rgba(226, 60, 70), false, false); put(E_HEART_ENT, c); }
    { Canvas c; gen_arrow_fx(c, rgba(150, 108, 62), rgba(196, 196, 202)); put(E_ARROW_WOOD, c); }
    { Canvas c; gen_arrow_fx(c, rgba(180, 180, 186), rgba(230, 230, 236)); put(E_ARROW_IRON, c); }
    { Canvas c; gen_arrow_fx(c, rgba(200, 120, 60), rgba(255, 190, 70));   put(E_ARROW_EXPLOSIVE, c); }
    { Canvas c; gen_player_arm(c, rgba(232, 196, 156), rgba(70, 132, 200)); put(E_PLAYER_ARM, c); }

    // 玩家立绘：从 assets/skins/*.png 取第一张，按 MC 皮肤布局拼装。
    // 一张都没有就保留上面的程序化小人。人物画 2 格高，见 draw_player_only()。
    skin_scan();
    if(skin_count() > 0) skin_apply(0);

    // MC 官方怪物皮肤：拼成 16x32 立绘覆盖上面的程序化怪（缺文件的怪保留原样）。
    // 怪物同样画 2 格高，见 game_render 的 draw_entities()。
    fprintf(stderr, "[atlas] MC 怪物皮肤已载入 %d / 5\n", mc_mob_skins());

    // ---- 特效 ----
    { Canvas c; gen_spark(c);     put(F_SPARK, c); }
    { Canvas c; gen_smoke(c);     put(F_SMOKE, c); }
    { Canvas c; gen_flame(c);     put(F_FLAME, c); }
    { Canvas c; gen_bubble(c);    put(F_BUBBLE, c); }
    { Canvas c; gen_splash(c);    put(F_SPLASH, c); }
    { Canvas c; gen_dust(c);      put(F_DUST, c); }
    { Canvas c; gen_explosion(c); put(F_EXPLOSION, c); }
    { Canvas c; gen_shockwave(c); put(F_SHOCKWAVE, c); }
    { Canvas c; gen_star(c);      put(F_STAR, c); }
    { Canvas c; gen_glow(c);      put(F_GLOW, c); }
    for(int lv = 0; lv <= 9; lv++)
    {
        Canvas c;
        gen_crack(c, lv);
        put(F_CRACK0 + lv, c);
    }

    // ---- UI ----
    { Canvas c; gen_panel_tex(c, rgba(58, 50, 74), 0x9A11u); put(U_PANEL, c); }
    { Canvas c; gen_panel_tex(c, rgba(26, 22, 34), 0xDA2Cu); put(U_PANEL_DARK, c); }
    { Canvas c; gen_slot(c, rgba(74, 66, 88), rgba(20, 18, 26), false);    put(U_SLOT, c); }
    { Canvas c; gen_slot(c, rgba(96, 86, 112), rgba(20, 18, 26), true);    put(U_SLOT_HL, c); }
    { Canvas c; gen_slot(c, rgba(120, 104, 60), rgba(255, 226, 140), true); put(U_SLOT_SEL, c); }
    { Canvas c; gen_cursor(c, false); put(U_CURSOR, c); }
    { Canvas c; gen_cursor(c, true);  put(U_CURSOR_CROSS, c); }
    { Canvas c; gen_heart(c, rgba(226, 60, 70), true, false);  put(U_HEART_BG, c); }
    { Canvas c; gen_heart(c, rgba(226, 60, 70), false, false); put(U_HEART_FULL, c); }
    { Canvas c; gen_heart(c, rgba(226, 60, 70), false, true);  put(U_HEART_HALF, c); }
    { Canvas c; gen_armor_icon(c);     put(U_ARMOR_ICON, c); }
    { Canvas c; gen_bubble(c);         put(U_BUBBLE, c); }
    { Canvas c; gen_dur_bar(c, false); put(U_DUR_BG, c); }
    { Canvas c; gen_dur_bar(c, true);  put(U_DUR_FILL, c); }
    { Canvas c; gen_broken(c);         put(U_BROKEN, c); }
    { Canvas c; gen_arrow_ui(c, 0);    put(U_ARROW_L, c); }
    { Canvas c; gen_arrow_ui(c, 1);    put(U_ARROW_R, c); }
    { Canvas c; gen_arrow_ui(c, 2);    put(U_ARROW_UP, c); }
    { Canvas c; gen_arrow_ui(c, 3);    put(U_ARROW_DOWN, c); }
    { Canvas c; gen_check(c, false);   put(U_CHECK, c); }
    { Canvas c; gen_check(c, true);    put(U_CROSS, c); }
    { Canvas c; gen_gear(c);           put(U_GEAR, c); }
    { Canvas c; gen_map_pin(c);        put(U_MAP_PIN, c); }
    { Canvas c; gen_button(c, rgba(96, 86, 112), rgba(56, 48, 70), false); put(U_BTN, c); }
    { Canvas c; gen_button(c, rgba(126, 112, 148), rgba(76, 64, 94), true);  put(U_BTN_HL, c); }
    { Canvas c; gen_title_bar(c);      put(U_TITLE_BAR, c); }

    // ---- 顶边覆盖层（草皮 / 雪皮）----
    // 泥土/雪裸露在空气中时叠一条顶边，看起来才像「地表」而不是一坨土。
    { Canvas c; gen_fringe(c, rgba(96, 168, 74), rgba(58, 118, 48), 7, true, 11); put(O_GRASS_FRINGE, c); }
    { Canvas c; gen_fringe(c, rgba(240, 246, 254), rgba(196, 214, 234), 8, false, 12); put(O_SNOW_FRINGE, c); }
    { Canvas c; gen_fringe(c, rgba(96, 168, 74), rgba(58, 118, 48), 6, true, 13); put(O_GRASS_FRINGE_L, c); }
    { Canvas c; gen_fringe(c, rgba(96, 168, 74), rgba(58, 118, 48), 6, true, 14); put(O_GRASS_FRINGE_R, c); }
}

const u32 *pixels() { return g_px; }
i32 pixel_w() { return AW; }
i32 pixel_h() { return AH; }

Uv uv_of(i32 slot)
{
    if(slot < 0 || slot >= MAX_SLOTS) slot = S_WHITE;
    const f32 sx = (f32)((slot % COLS) * CELL);
    const f32 sy = (f32)((slot / COLS) * CELL);
    const f32 e = 0.25f;   // 半像素内缩，避免采到相邻格
    Uv uv;
    uv.u0 = (sx + e) / TEX_W;
    uv.v0 = (sy + e) / TEX_H;
    uv.u1 = (sx + CELL - e) / TEX_W;
    uv.v1 = (sy + CELL - e) / TEX_H;
    return uv;
}

}   // namespace atlas


