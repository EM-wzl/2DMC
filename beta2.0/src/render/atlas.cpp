// ===================== 程序化纹理生成 =====================
// 所有方块/实体/UI 贴图都在这里用几何图元画出来，无外部素材依赖。
#include "atlas.h"
#include "../world/tiles.h"
#include <cstring>

namespace atlas {
namespace {

constexpr i32 AW = COLS * CELL;
constexpr i32 AH = ROWS * CELL;
u32 g_px[AW * AH];

// 少写几个 rgba() 的便捷打包
u32 rgb_pack(u8 r, u8 g, u8 b) { return rgba(r, g, b); }

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

    // 源over目标 alpha 混合
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
                for(int ox = 0; ox < thick; ox++) blend(x0 + ox - thick / 2, y0 + oy - thick / 2, c);
            if(x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if(e2 > -dy) { err -= dy; x0 += sx; }
            if(e2 < dx) { err += dx; y0 += sy; }
        }
    }
    // 噪点：在已有像素上叠加减色点，count 为点数
    void specks(u32 c, int count, i32 seed)
    {
        Rng r;
        r.seed((u64)seed * 2654435761u + 12345u);
        for(int i = 0; i < count; i++)
        {
            int x = r.range(CELL), y = r.range(CELL);
            blend(x, y, c);
        }
    }
    // 底部加暗、顶部加亮：让方块有体积感
    void bevel(u32 dark, u32 light)
    {
        for(int y = 0; y < CELL; y++)
            for(int x = 0; x < CELL; x++)
            {
                if((y + 1) % 8 == 0) blend(x, y, dark);
                if(y % 8 == 1) blend(x, y, light);
            }
        for(int x = 0; x < CELL; x++)
        {
            blend(x, 0, light);
            blend(x, CELL - 1, dark);
        }
    }
};

// ---------------- 方块样式生成 ----------------
void gen_rock(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    c.specks(t.accent, 120, 11);
    c.specks(rgba(255, 255, 255, 40), 26, 12);
    c.bevel(rgba(0, 0, 0, 60), rgba(255, 255, 255, 30));
}

void gen_dirt(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    c.specks(t.accent, 150, 21);
    c.specks(tint_of(t.base, 1.25f), 60, 22);
    c.bevel(rgba(0, 0, 0, 45), rgba(255, 255, 255, 22));
}

void gen_brick(Canvas &c, const TileDef &t)
{
    c.clear(t.accent);                       // 灰缝
    const int bw = 15, bh = 7;               // 砖块尺寸
    for(int row = 0; row < 5; row++)
    {
        int y = row * bh;
        int off = (row % 2) ? -bw / 2 : 0;
        for(int x = off; x < CELL; x += bw)
        {
            c.rect(x + 1, y + 1, bw - 2, bh - 2, t.base);
            // 砖块上沿高光、下沿阴影
            c.rect(x + 1, y + 1, bw - 2, 1, tint_of(t.base, 1.22f));
            c.rect(x + 1, y + bh - 2, bw - 2, 1, tint_of(t.base, 0.72f));
        }
    }
    c.bevel(rgba(0, 0, 0, 70), rgba(255, 255, 255, 26));
}

void gen_log(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    for(int x = 0; x < CELL; x += 5)
        c.rect(x, 0, 2, CELL, t.accent);
    for(int x = 0; x < CELL; x += 5)
        c.rect(x + 2, 0, 1, CELL, tint_of(t.base, 1.14f));
    c.specks(t.accent, 40, 31);
    c.bevel(rgba(0, 0, 0, 55), rgba(255, 255, 255, 22));
}

void gen_plank(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    for(int y = 0; y < CELL; y += 8)
    {
        c.rect(0, y, CELL, 1, t.accent);
        c.rect(0, y + 1, CELL, 1, tint_of(t.base, 1.18f));
    }
    for(int y = 0; y < CELL; y += 8)
    {
        int nx = ((y / 8) % 3) * 9 + 7;
        c.rect(nx, y + 3, 2, 4, t.accent);
    }
    c.bevel(rgba(0, 0, 0, 50), rgba(255, 255, 255, 20));
}

void gen_leaves(Canvas &c, const TileDef &t)
{
    c.clear(0);
    Rng r;
    r.seed(77);
    for(int i = 0; i < 26; i++)
    {
        f32 x = r.unit(4, 28), y = r.unit(4, 28), rad = r.unit(3.5f, 6.0f);
        c.disc(x, y, rad, (i % 3 == 0) ? t.accent : t.base);
    }
}

void gen_ore(Canvas &c, const TileDef &t)
{
    gen_rock(c, t);
    Rng r;
    r.seed(55);
    for(int i = 0; i < 5; i++)
    {
        f32 x = r.unit(6, 26), y = r.unit(6, 26), rad = r.unit(3.0f, 4.6f);
        c.disc(x, y, rad, tint_of(t.accent, 0.8f));
        c.disc(x, y, rad - 1.2f, t.accent);
        c.disc(x - rad * 0.3f, y - rad * 0.3f, rad * 0.35f, tint_of(t.accent, 1.45f));
    }
}

void gen_gem_block(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    c.specks(t.accent, 80, 66);
    // 中央菱形宝石
    for(int j = 0; j < 14; j++)
        for(int i = 0; i < 14; i++)
        {
            f32 dx = std::abs(i - 6.5f) / 7.0f, dy = std::abs(j - 6.5f) / 7.0f;
            if(dx + dy <= 1.0f)
            {
                u32 col = (dx + dy < 0.45f) ? tint_of(t.accent, 1.4f) : t.accent;
                c.blend(9 + i, 9 + j, col);
            }
        }
    c.bevel(rgba(0, 0, 0, 55), rgba(255, 255, 255, 40));
}

void gen_water(Canvas &c, const TileDef &t)
{
    c.clear(rgba((u8)(t.base & 0xFF), (u8)((t.base >> 8) & 0xFF), (u8)((t.base >> 16) & 0xFF), 205));
    for(int y = 4; y < CELL; y += 9)
    {
        for(int x = 0; x < CELL; x++)
        {
            int yy = y + (int)(std::sin(x * 0.5f) * 1.8f);
            c.blend(x, yy, rgba((u8)(t.accent & 0xFF), (u8)((t.accent >> 8) & 0xFF),
                                (u8)((t.accent >> 16) & 0xFF), 150));
        }
    }
    for(int i = 0; i < 14; i++)
        c.blend(2 + (i * 7) % 28, 6 + (i * 11) % 22, rgba(255, 255, 255, 70));
}

void gen_lava(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    Rng r;
    r.seed(99);
    for(int i = 0; i < 16; i++)
    {
        f32 x = r.unit(3, 29), y = r.unit(3, 29), rad = r.unit(2.5f, 5.5f);
        c.disc(x, y, rad, rgba((u8)(t.accent & 0xFF), (u8)((t.accent >> 8) & 0xFF),
                              (u8)((t.accent >> 16) & 0xFF), 190));
    }
    // 黑色结壳
    for(int i = 0; i < 26; i++)
        c.blend(r.range(CELL), r.range(CELL), rgba(38, 18, 10, 150));
    c.specks(rgba(255, 236, 170, 120), 24, 101);
}

void gen_ice(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    for(int k = 0; k < 4; k++)
        c.line(2 + k * 8, 2, 2 + k * 8 + 12, 26, rgba(255, 255, 255, 90), 2);
    for(int i = 0; i < 20; i++) c.blend(4 + (i * 13) % 26, 4 + (i * 7) % 26, rgba(255, 255, 255, 150));
    c.bevel(rgba(20, 60, 110, 70), rgba(255, 255, 255, 60));
}

void gen_snow(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    c.specks(t.accent, 70, 121);
    c.specks(rgba(255, 255, 255, 180), 40, 122);
    c.bevel(rgba(150, 170, 200, 60), rgba(255, 255, 255, 90));
}

void gen_glass(Canvas &c, const TileDef &t)
{
    c.clear(0);
    u32 edge = rgba((u8)(t.base & 0xFF), (u8)((t.base >> 8) & 0xFF), (u8)((t.base >> 16) & 0xFF), 200);
    c.frame(1, 1, CELL - 2, CELL - 2, edge, 2);
    c.line(5, 24, 22, 7, rgba(255, 255, 255, 110), 2);
}

void gen_furnace(Canvas &c, const TileDef &t)
{
    gen_rock(c, t);
    c.rect(8, 12, 16, 14, t.accent);
    c.rect(10, 20, 12, 5, rgba(255, 150, 40, 210));
    c.rect(10, 21, 12, 2, rgba(255, 220, 130, 230));
    c.frame(8, 12, 16, 14, tint_of(t.base, 0.7f));
    c.rect(12, 5, 8, 4, tint_of(t.base, 0.78f));
}

void gen_chest(Canvas &c, const TileDef &t)
{
    c.rect(3, 6, 26, 22, t.base);
    c.rect(3, 6, 26, 8, tint_of(t.base, 1.14f));
    c.frame(3, 6, 26, 22, t.accent, 2);
    c.rect(3, 14, 26, 2, t.accent);
    c.rect(14, 12, 4, 7, rgba(240, 208, 120, 255));   // 锁扣
    c.rect(15, 15, 2, 3, rgba(120, 92, 30, 255));
}

void gen_torch(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(14, 14, 4, 17, t.accent);
    c.rect(14, 14, 1, 17, tint_of(t.accent, 1.3f));
    // 火焰
    c.disc(16, 10, 5.5f, rgba(255, 160, 40, 120));
    c.disc(16, 10, 4.0f, t.base);
    c.disc(16, 9, 2.2f, rgba(255, 246, 200, 240));
}

void gen_door(Canvas &c, const TileDef &t)
{
    gen_plank(c, t);
    c.frame(2, 2, CELL - 4, CELL - 4, t.accent, 2);
    c.rect(2, 15, CELL - 4, 2, t.accent);
    c.disc(24, 16, 2.2f, rgba(230, 200, 120, 255));
}

void gen_portal(Canvas &c, const TileDef &t)
{
    c.clear(rgba(18, 8, 30, 255));
    for(int i = 6; i >= 1; i--)
    {
        u32 col = (i % 2) ? t.base : t.accent;
        col = tint_of(col, 0.6f + i * 0.09f, (u8)(70 + i * 18));
        c.ellipse(16, 16, 3.0f + i * 1.8f, 5.0f + i * 2.0f, col);
    }
    c.ellipse(16, 16, 4.0f, 7.0f, rgba(255, 255, 255, 230));
}

void gen_platform(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(0, 6, CELL, 6, t.base);
    c.rect(0, 6, CELL, 2, tint_of(t.base, 1.2f));
    c.rect(0, 11, CELL, 1, t.accent);
    for(int x = 5; x < CELL; x += 10) c.rect(x, 8, 2, 3, t.accent);
}

void gen_machine(Canvas &c, const TileDef &t)
{
    c.clear(t.accent);
    c.frame(2, 2, CELL - 4, CELL - 4, tint_of(t.base, 0.7f), 2);
    c.rect(2, 2, CELL - 4, CELL - 4, t.base);
    c.disc(16, 13, 8.5f, rgba(150, 220, 240, 210));
    c.disc(16, 13, 6.0f, rgba(60, 130, 170, 220));
    c.disc(13, 10, 2.2f, rgba(255, 255, 255, 190));
    c.rect(10, 24, 12, 3, rgba(240, 208, 120, 255));
}

void gen_bed(Canvas &c, const TileDef &t)
{
    c.rect(2, 6, 28, 21, t.base);
    c.rect(2, 6, 28, 7, tint_of(t.base, 1.15f));
    c.rect(4, 3, 24, 6, t.accent);          // 枕头
    c.frame(2, 6, 28, 21, tint_of(t.base, 0.7f), 1);
}

void gen_wool(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    c.specks(t.accent, 90, 141);
    c.specks(rgba(255, 255, 255, 120), 50, 142);
}

void gen_table(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    c.rect(0, 0, CELL, 12, tint_of(t.base, 0.75f));
    c.specks(t.accent, 60, 151);
    c.rect(0, 12, CELL, 2, tint_of(t.base, 1.25f));
    // 台面工具：锤与砧
    c.rect(6, 4, 8, 3, rgba(210, 210, 218, 255));
    c.rect(8, 2, 4, 6, rgba(150, 150, 158, 255));
    c.rect(18, 5, 4, 4, rgba(90, 90, 96, 255));
}

void gen_netherrack(Canvas &c, const TileDef &t)
{
    c.clear(t.base);
    c.specks(t.accent, 130, 161);
    for(int i = 0; i < 5; i++)
        c.line(3 + i * 6, 2, 3 + i * 6 + 4, 29, rgba(0, 0, 0, 70), 2);
    c.bevel(rgba(0, 0, 0, 60), rgba(255, 255, 255, 22));
}

// ---------------- 物品图标 ----------------
void gen_i_rod(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.line(10, 24, 22, 8, t.accent, 5);
    c.line(10, 24, 22, 8, t.base, 3);
}

void gen_i_pick(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.line(9, 25, 20, 11, t.accent, 5);      // 木柄
    c.line(9, 25, 20, 11, t.base, 3);
    c.line(8, 9, 24, 9, t.base, 3);          // 镐头
    c.line(8, 9, 12, 5, t.base, 3);
    c.line(24, 9, 20, 5, t.base, 3);
}

void gen_i_sword(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.line(9, 24, 12, 20, t.accent, 5);      // 握柄
    c.line(11, 21, 24, 7, t.base, 4);        // 剑身
    c.line(11, 21, 24, 7, tint_of(t.base, 1.4f), 2);
    c.line(8, 17, 16, 24, t.accent, 3);      // 护手
}

void gen_i_axe(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.line(11, 26, 16, 8, t.accent, 4);
    c.rect(13, 5, 12, 11, t.base);
    c.rect(13, 5, 4, 11, tint_of(t.base, 1.3f));
    c.line(13, 5, 25, 10, tint_of(t.base, 0.8f), 1);
}

void gen_i_armor(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(7, 8, 18, 16, t.base);
    c.rect(4, 8, 4, 9, t.base);              // 肩甲
    c.rect(24, 8, 4, 9, t.base);
    c.rect(12, 8, 8, 4, t.accent);           // 领口
    c.rect(7, 8, 18, 2, tint_of(t.base, 1.3f));
    c.rect(14, 14, 4, 8, t.accent);
}

void gen_i_helm(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.ellipse(16, 17, 11, 11, t.base);
    c.rect(5, 17, 22, 8, 0);                  // 削掉下半，做成头盔
    c.rect(8, 16, 16, 5, t.accent);           // 面窗
    c.rect(8, 6, 16, 3, tint_of(t.base, 1.3f));
}

void gen_i_legs(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(7, 6, 18, 7, t.base);
    c.rect(7, 13, 7, 14, t.base);
    c.rect(18, 13, 7, 14, t.base);
    c.rect(7, 6, 18, 2, tint_of(t.base, 1.3f));
    c.rect(9, 24, 5, 3, t.accent);
    c.rect(20, 24, 5, 3, t.accent);
}

void gen_i_boot(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(8, 6, 10, 14, t.base);
    c.rect(8, 18, 18, 8, t.base);
    c.rect(8, 24, 18, 2, t.accent);
    c.rect(8, 6, 10, 2, tint_of(t.base, 1.3f));
}

void gen_i_bow(Canvas &c, const TileDef &t)
{
    c.clear(0);
    for(int j = 4; j < 28; j++)
    {
        int x = 20 - (int)(std::sin((j - 4) * 3.14159f / 24.0f) * 10.0f);
        c.blend(x, j, t.base);
        c.blend(x + 1, j, t.base);
    }
    c.line(20, 4, 20, 27, t.accent, 1);      // 弓弦
}

void gen_i_gun(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(4, 12, 20, 5, t.base);            // 枪管/机匣
    c.rect(4, 11, 20, 1, tint_of(t.base, 1.35f));
    c.rect(4, 18, 8, 3, t.accent);           // 握把
    c.rect(6, 21, 5, 6, t.accent);
    c.rect(20, 15, 3, 4, t.accent);          // 扳机护圈
}

void gen_i_potion(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(13, 4, 6, 5, t.accent);           // 瓶口
    c.rect(11, 3, 10, 2, rgba(150, 120, 80, 255));
    c.ellipse(16, 19, 9.5f, 10.0f, rgba(210, 230, 240, 120));   // 玻璃
    c.ellipse(16, 21, 7.5f, 7.5f, t.base);                      // 液体
    c.disc(13, 17, 2.2f, rgba(255, 255, 255, 170));             // 高光
}

void gen_i_ingot(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(6, 12, 20, 10, t.base);
    c.rect(9, 9, 14, 4, tint_of(t.base, 1.2f));
    c.rect(6, 20, 20, 2, t.accent);
    c.rect(9, 9, 14, 1, rgba(255, 255, 255, 160));
}

void gen_i_gem(Canvas &c, const TileDef &t)
{
    c.clear(0);
    for(int j = 0; j < 18; j++)
        for(int i = 0; i < 18; i++)
        {
            f32 dx = std::abs(i - 8.5f) / 9.0f, dy = std::abs(j - 8.5f) / 9.0f;
            if(dx + dy <= 1.0f)
                c.blend(7 + i, 6 + j, (dx + dy < 0.4f) ? tint_of(t.base, 1.5f) : t.base);
        }
    c.frame(11, 10, 4, 4, rgba(255, 255, 255, 200));
}

void gen_i_food(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.ellipse(16, 18, 10.5f, 9.5f, t.base);
    c.ellipse(13, 15, 3.0f, 2.4f, rgba(255, 255, 255, 150));
    c.line(16, 9, 17, 4, t.accent, 3);
    c.disc(21, 6, 3.5f, t.accent);
}

void gen_i_coin(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.disc(16, 16, 10.5f, t.accent);
    c.disc(16, 16, 8.6f, t.base);
    c.disc(16, 16, 4.6f, tint_of(t.base, 1.25f));
    c.disc(13, 13, 2.0f, rgba(255, 255, 255, 170));
}

void gen_i_arrow(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.line(7, 25, 24, 8, t.base, 3);
    c.line(22, 6, 26, 10, tint_of(t.base, 1.4f), 4);
    c.rect(5, 22, 4, 4, t.accent);
    c.rect(4, 25, 3, 3, t.accent);
}

void gen_i_ball(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.disc(16, 16, 9.5f, t.base);
    c.disc(13, 13, 3.2f, tint_of(t.base, 1.45f));
    c.frame(6, 6, 20, 20, t.accent);
}

void gen_i_shield(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(7, 6, 18, 14, t.base);
    for(int j = 0; j < 8; j++)
        c.rect(7 + j, 20 + j, 18 - j * 2, 1, t.base);
    c.rect(15, 8, 3, 14, t.accent);
    c.rect(9, 12, 14, 3, t.accent);
}

void gen_i_bucket(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(8, 10, 16, 15, t.base);
    c.rect(8, 10, 16, 3, t.accent);
    c.frame(8, 10, 16, 15, tint_of(t.base, 0.7f), 1);
    for(int x = 9; x < 24; x += 5) c.rect(x, 13, 1, 11, tint_of(t.base, 0.8f));
    // 提手
    for(int i = 0; i < 16; i++)
        c.blend(8 + i, 10 - (int)(std::sin(i * 3.14159f / 15.0f) * 5.0f), t.accent);
}

void gen_i_scale(Canvas &c, const TileDef &t)
{
    c.clear(0);
    for(int row = 0; row < 4; row++)
        for(int col = 0; col < 3; col++)
        {
            f32 x = 8 + col * 8 + (row % 2) * 4, y = 8 + row * 6;
            c.ellipse(x, y, 4.5f, 3.5f, t.base);
            c.ellipse(x, y - 0.5f, 3.0f, 2.0f, t.accent);
        }
}

void gen_i_amulet(Canvas &c, const TileDef &t)
{
    c.clear(0);
    for(int j = 6; j <= 18; j++)      // 链条
    {
        int dx = (int)(std::sin((j - 6) * 3.14159f / 12.0f) * 8.0f);
        c.blend(16 - dx, j, rgba(210, 210, 220, 220));
        c.blend(16 + dx, j, rgba(210, 210, 220, 220));
    }
    for(int j = 0; j < 10; j++)
        for(int i = 0; i < 10; i++)
        {
            f32 dx = std::abs(i - 4.5f) / 5.0f, dy = std::abs(j - 4.5f) / 5.0f;
            if(dx + dy <= 1.0f) c.blend(11 + i, 18 + j, (dx + dy < 0.4f) ? t.accent : t.base);
        }
}

void gen_i_scrap(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.rect(7, 12, 12, 11, t.base);
    c.rect(13, 8, 9, 9, t.base);
    c.rect(9, 20, 8, 6, t.base);
    c.specks(t.accent, 26, 191);
    c.frame(7, 12, 12, 11, tint_of(t.base, 0.65f), 1);
}

void gen_i_egg(Canvas &c, const TileDef &t)
{
    c.clear(0);
    c.ellipse(16, 19, 9.5f, 11.0f, t.base);
    c.ellipse(16, 12, 6.5f, 7.0f, t.base);
    c.specks(t.accent, 22, 201);
    c.disc(13, 14, 2.6f, rgba(255, 255, 255, 150));
}

// ---------------- 实体 ----------------
// 人形：头 + 躯干 + 双臂 + 双腿
void humanoid(Canvas &c, u32 skin, u32 cloth, u32 limb, bool undead)
{
    c.clear(0);
    // 腿
    c.rect(11, 21, 4, 9, limb);
    c.rect(17, 21, 4, 9, limb);
    // 躯干
    c.rect(10, 12, 12, 10, cloth);
    c.rect(10, 12, 12, 2, tint_of(cloth, 1.25f));
    // 手臂
    c.rect(6, 13, 4, 9, limb);
    c.rect(22, 13, 4, 9, limb);
    if(undead)
    {
        c.rect(6, 13, 4, 9, skin);      // 僵尸前伸的手臂
        c.rect(22, 13, 4, 9, skin);
    }
    // 头
    c.rect(10, 3, 12, 10, skin);
    c.rect(10, 3, 12, 2, tint_of(skin, 1.18f));
    // 眼睛
    u32 eye = undead ? rgba(20, 20, 24, 255) : rgba(250, 250, 250, 255);
    c.rect(12, 7, 3, 3, eye);
    c.rect(18, 7, 3, 3, eye);
    if(undead)
    {
        c.rect(12, 7, 3, 1, rgba(180, 40, 40, 255));
        c.rect(18, 7, 3, 1, rgba(180, 40, 40, 255));
    }
}

void gen_zombie(Canvas &c) { humanoid(c, rgb_pack(94, 158, 84), rgb_pack(64, 96, 62), rgb_pack(78, 128, 72), true); }
void gen_skeleton(Canvas &c) { humanoid(c, rgb_pack(226, 228, 224), rgb_pack(198, 200, 196), rgb_pack(214, 216, 212), false); }
void gen_piglin(Canvas &c) { humanoid(c, rgb_pack(226, 152, 130), rgb_pack(140, 108, 62), rgb_pack(200, 132, 112), true); }

void gen_creeper(Canvas &c)
{
    c.clear(0);
    c.rect(11, 3, 10, 10, rgb_pack(74, 176, 72));
    c.rect(11, 12, 10, 3, rgb_pack(58, 150, 58));
    c.rect(8, 15, 16, 12, rgb_pack(66, 162, 66));
    c.rect(8, 15, 16, 2, rgb_pack(88, 190, 84));
    c.rect(6, 18, 2, 7, rgb_pack(58, 144, 58));
    c.rect(24, 18, 2, 7, rgb_pack(58, 144, 58));
    // 经典面孔
    c.rect(12, 5, 3, 3, rgb_pack(20, 40, 20));
    c.rect(17, 5, 3, 3, rgb_pack(20, 40, 20));
    c.rect(14, 8, 4, 4, rgb_pack(20, 40, 20));
    c.rect(15, 12, 2, 6, rgb_pack(20, 40, 20));
    c.rect(12, 12, 2, 4, rgb_pack(20, 40, 20));
    c.rect(18, 12, 2, 4, rgb_pack(20, 40, 20));
}

void gen_spider(Canvas &c)
{
    c.clear(0);
    // 八条腿
    for(int s = -1; s <= 1; s += 2)
        for(int k = 0; k < 4; k++)
            c.line(16, 18, 16 + s * (7 + k * 3), 10 + k * 5, rgb_pack(52, 40, 34), 2);
    c.ellipse(16, 20, 10.0f, 8.0f, rgb_pack(66, 52, 44));      // 腹部
    c.ellipse(16, 12, 8.0f, 7.0f, rgb_pack(82, 64, 54));       // 头胸
    c.rect(9, 9, 3, 3, rgb_pack(220, 40, 40));                 // 红眼
    c.rect(13, 8, 3, 3, rgb_pack(220, 40, 40));
    c.rect(18, 8, 3, 3, rgb_pack(220, 40, 40));
    c.rect(22, 9, 3, 3, rgb_pack(220, 40, 40));
}

void gen_sheep(Canvas &c)
{
    c.clear(0);
    c.rect(8, 12, 4, 8, rgb_pack(180, 180, 184));
    c.rect(20, 12, 4, 8, rgb_pack(180, 180, 184));
    c.ellipse(16, 15, 11.0f, 8.5f, rgb_pack(238, 238, 234));   // 羊毛
    c.specks(rgb_pack(206, 206, 202), 40, 301);
    c.rect(22, 7, 7, 7, rgb_pack(226, 220, 214));              // 头
    c.rect(24, 9, 2, 2, rgb_pack(30, 30, 34));
    c.rect(27, 11, 2, 3, rgb_pack(198, 192, 186));
}

void gen_pig(Canvas &c)
{
    c.clear(0);
    c.rect(9, 17, 4, 6, rgb_pack(206, 128, 132));
    c.rect(20, 17, 4, 6, rgb_pack(206, 128, 132));
    c.ellipse(16, 17, 11.0f, 8.0f, rgb_pack(238, 152, 152));
    c.rect(20, 9, 9, 8, rgb_pack(230, 140, 140));
    c.rect(25, 12, 5, 5, rgb_pack(198, 108, 108));             // 鼻子
    c.rect(26, 13, 1, 3, rgb_pack(120, 60, 60));
    c.rect(29, 13, 1, 3, rgb_pack(120, 60, 60));
    c.rect(23, 11, 2, 2, rgb_pack(30, 24, 24));
}

void gen_player(Canvas &c)
{
    c.clear(0);
    c.rect(11, 21, 4, 9, rgb_pack(52, 68, 130));
    c.rect(17, 21, 4, 9, rgb_pack(52, 68, 130));
    c.rect(10, 12, 12, 10, rgb_pack(70, 110, 210));            // 蓝甲
    c.rect(10, 12, 12, 2, rgb_pack(110, 150, 240));
    c.rect(6, 13, 4, 9, rgb_pack(72, 116, 220));
    c.rect(22, 13, 4, 9, rgb_pack(72, 116, 220));
    c.rect(10, 3, 12, 10, rgb_pack(238, 196, 158));            // 头
    c.rect(10, 3, 12, 3, rgb_pack(70, 52, 40));                // 头发
    c.rect(12, 7, 3, 3, rgb_pack(30, 30, 40));
    c.rect(18, 7, 3, 3, rgb_pack(30, 30, 40));
}

void gen_boss(Canvas &c)
{
    c.clear(0);
    // 飞行 Boss：暗紫色球体 + 巨眼 + 触须
    for(int i = 0; i < 6; i++)
        c.ellipse(16, 16, 15.0f - i * 2.0f, 15.0f - i * 2.0f,
                  rgb_pack((u8)(40 + i * 6), (u8)(20 + i * 4), (u8)(60 + i * 8)));
    c.ellipse(16, 16, 9.0f, 9.0f, rgb_pack(120, 40, 160));
    c.ellipse(16, 15, 6.5f, 6.5f, rgb_pack(250, 240, 210));    // 眼白
    c.ellipse(16, 15, 3.0f, 4.0f, rgb_pack(180, 30, 60));      // 瞳孔
    c.disc(14, 12, 1.6f, rgb_pack(255, 255, 255));
    // 触须
    for(int s = -1; s <= 1; s += 2)
        for(int k = 0; k < 3; k++)
            c.line(16 + s * 3, 24, 16 + s * (7 + k * 4), 30, rgb_pack(84, 30, 110), 2);
}

void gen_worm_head(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 14.0f, rgb_pack(76, 26, 96));
    c.disc(16, 16, 11.5f, rgb_pack(112, 44, 138));
    c.disc(16, 16, 7.5f, rgb_pack(38, 12, 48));                // 巨口
    for(int i = 0; i < 8; i++)
    {
        f32 a = i * 3.14159f / 4.0f;
        c.disc(16 + std::cos(a) * 8.5f, 16 + std::sin(a) * 8.5f, 1.9f, rgb_pack(240, 232, 214));
    }
}

void gen_worm_body(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 13.5f, rgb_pack(66, 22, 84));
    c.disc(16, 16, 10.0f, rgb_pack(98, 38, 124));
    c.disc(16, 16, 5.5f, rgb_pack(58, 20, 74));
    c.specks(rgb_pack(150, 90, 180), 26, 401);
}

void gen_worm_tail(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 11.0f, rgb_pack(66, 22, 84));
    c.disc(16, 16, 7.5f, rgb_pack(98, 38, 124));
    for(int k = 0; k < 3; k++)
        c.line(16, 22, 16 + (k % 2 ? 6 : -6), 30, rgb_pack(112, 44, 138), 3);
}

void gen_oi(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 13.0f, rgb_pack(60, 60, 70));
    c.disc(16, 16, 10.0f, rgb_pack(210, 210, 218));
    c.disc(16, 16, 6.5f, rgb_pack(90, 200, 240));
    c.disc(13, 13, 3.0f, rgba(255, 255, 255, 190));
}

void gen_recursive(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 13.0f, rgb_pack(30, 90, 70));
    c.disc(16, 16, 9.5f, rgb_pack(60, 200, 140));
    for(int i = 0; i < 5; i++)
        c.ellipse(16, 16, 9.0f - i * 1.6f, 9.0f - i * 1.6f,
                  (i % 2) ? rgb_pack(20, 70, 56) : rgb_pack(96, 236, 176));
}

void gen_arrow(Canvas &c, u32 shaft, u32 head)
{
    c.clear(0);
    c.line(6, 26, 25, 7, shaft, 2);
    c.line(23, 4, 27, 8, head, 3);
    c.rect(4, 24, 4, 4, rgb_pack(230, 230, 236));
}

void gen_bullet(Canvas &c)
{
    c.clear(0);
    c.ellipse(16, 16, 4.0f, 7.0f, rgb_pack(246, 214, 110));
    c.ellipse(16, 14, 2.2f, 4.0f, rgb_pack(255, 252, 220));
    c.disc(16, 22, 2.0f, rgba(255, 200, 80, 140));
}

void gen_coin_pick(Canvas &c)
{
    gen_i_coin(c, tile_of(B_COIN));
}

void gen_spark(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 9.0f, rgba(255, 220, 140, 90));
    c.disc(16, 16, 5.0f, rgba(255, 240, 200, 190));
    c.disc(16, 16, 2.2f, rgba(255, 255, 255, 240));
}

void gen_explosion(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 15.0f, rgba(255, 120, 30, 90));
    c.disc(16, 16, 11.0f, rgba(255, 176, 60, 150));
    c.disc(16, 16, 7.0f, rgba(255, 228, 140, 210));
    c.disc(16, 16, 3.5f, rgba(255, 255, 240, 240));
}

void gen_muzzle(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 14.0f, rgba(255, 200, 90, 70));
    c.disc(16, 16, 8.5f, rgba(255, 236, 160, 170));
    c.disc(16, 16, 4.0f, rgba(255, 255, 255, 230));
}

void gen_shadow(Canvas &c)
{
    c.clear(0);
    c.ellipse(16, 16, 14.0f, 9.0f, rgba(0, 0, 0, 120));
    c.ellipse(16, 16, 10.0f, 6.0f, rgba(0, 0, 0, 70));
}

void gen_heart(Canvas &c)
{
    c.clear(0);
    c.disc(11, 12, 6.0f, rgb_pack(226, 52, 66));
    c.disc(21, 12, 6.0f, rgb_pack(226, 52, 66));
    for(int j = 0; j < 12; j++)
        c.rect(11 - j / 2, 14 + j, 22 + j / 2 - (11 - j / 2), 1, rgb_pack(226, 52, 66));
    c.disc(12, 10, 2.4f, rgba(255, 200, 206, 200));
}

void gen_skull(Canvas &c)
{
    c.clear(0);
    c.ellipse(16, 14, 11.0f, 10.0f, rgb_pack(238, 238, 232));
    c.rect(11, 20, 10, 7, rgb_pack(226, 226, 218));
    c.ellipse(12, 13, 3.4f, 3.8f, rgb_pack(30, 30, 36));
    c.ellipse(20, 13, 3.4f, 3.8f, rgb_pack(30, 30, 36));
    c.rect(15, 18, 3, 3, rgb_pack(30, 30, 36));
    for(int i = 0; i < 3; i++) c.rect(12 + i * 4, 21, 2, 5, rgb_pack(120, 118, 112));
}

// ---------------- UI ----------------
void gen_bar_bg(Canvas &c)
{
    c.clear(rgba(16, 18, 24, 210));
    c.frame(0, 0, CELL, CELL, rgba(0, 0, 0, 160), 2);
}

void gen_bar_hp(Canvas &c)
{
    c.clear(0);
    c.rect(0, 0, CELL, CELL, rgb_pack(214, 52, 62));
    c.rect(0, 0, CELL, 4, rgb_pack(246, 120, 126));
}

void gen_bar_armor(Canvas &c)
{
    c.clear(0);
    c.rect(0, 0, CELL, CELL, rgb_pack(72, 176, 210));
    c.rect(0, 0, CELL, 4, rgb_pack(150, 226, 246));
}

void gen_bar_energy(Canvas &c)
{
    c.clear(0);
    c.rect(0, 0, CELL, CELL, rgb_pack(86, 128, 232));
    c.rect(0, 0, CELL, 4, rgb_pack(160, 190, 255));
}

void gen_panel(Canvas &c)
{
    c.clear(0);
    u32 body = rgba(24, 26, 34, 232);
    u32 edge = rgba(140, 150, 172, 210);
    c.rect(0, 0, CELL, CELL, body);
    c.frame(0, 0, CELL, CELL, edge, 2);
    c.frame(3, 3, CELL - 6, CELL - 6, rgba(255, 255, 255, 26), 1);
}

void gen_slot(Canvas &c)
{
    c.clear(0);
    c.rect(0, 0, CELL, CELL, rgba(30, 32, 42, 226));
    c.frame(0, 0, CELL, CELL, rgba(120, 130, 152, 200), 2);
    c.frame(2, 2, CELL - 4, CELL - 4, rgba(255, 255, 255, 20), 1);
}

void gen_cursor(Canvas &c)
{
    c.clear(0);
    // 十字准星
    c.rect(14, 2, 4, 28, rgba(255, 255, 255, 200));
    c.rect(2, 14, 28, 4, rgba(255, 255, 255, 200));
    c.rect(15, 4, 2, 24, rgb_pack(255, 92, 92));
    c.rect(4, 15, 24, 2, rgb_pack(255, 92, 92));
}

void gen_arrow_up(Canvas &c)
{
    c.clear(0);
    for(int j = 0; j < 14; j++)
        c.rect(16 - j, 8 + j, j * 2, 1, rgba(240, 240, 240, 220));
}

void gen_arrow_down(Canvas &c)
{
    c.clear(0);
    for(int j = 0; j < 14; j++)
        c.rect(16 - (13 - j), 8 + j, (13 - j) * 2, 1, rgba(240, 240, 240, 220));
}

void gen_star(Canvas &c)
{
    c.clear(0);
    for(int i = 0; i < 5; i++)
    {
        f32 a = -3.14159f / 2 + i * 2 * 3.14159f / 5;
        c.line(16, 16, 16 + std::cos(a) * 13, 16 + std::sin(a) * 13, rgb_pack(250, 220, 110), 3);
        f32 b = a + 3.14159f / 5;
        c.line(16, 16, 16 + std::cos(b) * 13, 16 + std::sin(b) * 13, rgb_pack(250, 220, 110), 3);
    }
    c.disc(16, 16, 3.0f, rgba(255, 255, 255, 210));
}

void gen_key(Canvas &c)
{
    c.clear(0);
    c.disc(12, 12, 6.5f, rgb_pack(238, 208, 110));
    c.disc(12, 12, 3.0f, rgba(0, 0, 0, 0));
    c.rect(15, 14, 4, 4, rgb_pack(238, 208, 110));
    c.rect(18, 18, 3, 8, rgb_pack(238, 208, 110));
    c.rect(21, 20, 4, 3, rgb_pack(238, 208, 110));
    c.rect(21, 24, 4, 3, rgb_pack(238, 208, 110));
}

void gen_circle(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 15.5f, rgba(255, 255, 255, 255));
}

void gen_ring(Canvas &c)
{
    c.clear(0);
    c.disc(16, 16, 15.5f, rgba(255, 255, 255, 255));
    // 挖空内圈：blend 对 alpha=0 是空操作，必须直接置 0
    for(int j = 0; j < CELL; j++)
        for(int i = 0; i < CELL; i++)
        {
            f32 dx = i + 0.5f - 16, dy = j + 0.5f - 16;
            if(dx * dx + dy * dy <= 11.5f * 11.5f) c.set(i, j, 0);
        }
}

// ---------------- 槽位写入 ----------------
void put(i32 slot, const Canvas &c)
{
    if(slot < 0 || slot >= MAX_SLOTS) return;
    i32 sx = (slot % COLS) * CELL;
    i32 sy = (slot / COLS) * CELL;
    for(int y = 0; y < CELL; y++)
        memcpy(&g_px[(sy + y) * AW + sx], &c.px[y * CELL], CELL * 4);
}

void put_solid(i32 slot, Color col)
{
    Canvas c;
    c.clear(col);
    put(slot, c);
}

}   // namespace

void build()
{
    memset(g_px, 0, sizeof(g_px));

    // 纯白格：画纯色矩形/UI 底条用
    put_solid(S_WHITE, rgba(255, 255, 255, 255));

    // ---- 方块 ----
    for(i32 id = 0; id < 150; id++)
    {
        const TileDef &t = g_tiles[id];
        Canvas c;
        switch(t.style)
        {
        case TS_EMPTY: c.clear(0); break;
        case TS_ROCK: gen_rock(c, t); break;
        case TS_DIRT: gen_dirt(c, t); break;
        case TS_BRICK: gen_brick(c, t); break;
        case TS_LOG: gen_log(c, t); break;
        case TS_PLANK: gen_plank(c, t); break;
        case TS_LEAVES: gen_leaves(c, t); break;
        case TS_ORE: gen_ore(c, t); break;
        case TS_GEM_BLOCK: gen_gem_block(c, t); break;
        case TS_WATER: gen_water(c, t); break;
        case TS_LAVA: gen_lava(c, t); break;
        case TS_ICE: gen_ice(c, t); break;
        case TS_SNOW: gen_snow(c, t); break;
        case TS_GLASS: gen_glass(c, t); break;
        case TS_FURNACE: gen_furnace(c, t); break;
        case TS_CHEST: gen_chest(c, t); break;
        case TS_TORCH: gen_torch(c, t); break;
        case TS_DOOR: gen_door(c, t); break;
        case TS_PORTAL: gen_portal(c, t); break;
        case TS_PLATFORM: gen_platform(c, t); break;
        case TS_MACHINE: gen_machine(c, t); break;
        case TS_BED: gen_bed(c, t); break;
        case TS_WOOL: gen_wool(c, t); break;
        case TS_TABLE: gen_table(c, t); break;
        case TS_NETHERRACK: gen_netherrack(c, t); break;
        case TS_I_ROD: gen_i_rod(c, t); break;
        case TS_I_PICK: gen_i_pick(c, t); break;
        case TS_I_SWORD: gen_i_sword(c, t); break;
        case TS_I_AXE: gen_i_axe(c, t); break;
        case TS_I_ARMOR: gen_i_armor(c, t); break;
        case TS_I_HELM: gen_i_helm(c, t); break;
        case TS_I_LEGS: gen_i_legs(c, t); break;
        case TS_I_BOOT: gen_i_boot(c, t); break;
        case TS_I_BOW: gen_i_bow(c, t); break;
        case TS_I_GUN: gen_i_gun(c, t); break;
        case TS_I_POTION: gen_i_potion(c, t); break;
        case TS_I_INGOT: gen_i_ingot(c, t); break;
        case TS_I_GEM: gen_i_gem(c, t); break;
        case TS_I_FOOD: gen_i_food(c, t); break;
        case TS_I_COIN: gen_i_coin(c, t); break;
        case TS_I_ARROW: gen_i_arrow(c, t); break;
        case TS_I_BALL: gen_i_ball(c, t); break;
        case TS_I_SHIELD: gen_i_shield(c, t); break;
        case TS_I_BUCKET: gen_i_bucket(c, t); break;
        case TS_I_SCALE: gen_i_scale(c, t); break;
        case TS_I_AMULET: gen_i_amulet(c, t); break;
        case TS_I_SCRAP: gen_i_scrap(c, t); break;
        case TS_I_EGG: gen_i_egg(c, t); break;
        default: gen_rock(c, t); break;
        }
        put(slot_of_block(id), c);
    }

    // ---- 实体与特效 ----
    {
        Canvas c;
        gen_arrow(c, rgb_pack(150, 110, 60), rgb_pack(226, 226, 232)); put(E_ARROW, c);
        gen_arrow(c, rgb_pack(206, 206, 212), rgb_pack(150, 110, 60)); put(E_ARROW_IRON, c);
        gen_arrow(c, rgb_pack(226, 96, 40), rgb_pack(255, 210, 120)); put(E_ARROW_FIRE, c);
        gen_bullet(c); put(E_BULLET, c);
        gen_zombie(c); put(E_ZOMBIE, c);
        gen_skeleton(c); put(E_SKELETON, c);
        gen_creeper(c); put(E_CREEPER, c);
        gen_spider(c); put(E_SPIDER, c);
        gen_piglin(c); put(E_PIGLIN, c);
        gen_oi(c); put(E_OI, c);
        gen_recursive(c); put(E_RECURSIVE, c);
        gen_worm_head(c); put(E_WORM_HEAD, c);
        gen_worm_body(c); put(E_WORM_BODY, c);
        gen_worm_tail(c); put(E_WORM_TAIL, c);
        gen_sheep(c); put(E_SHEEP, c);
        gen_pig(c); put(E_PIG, c);
        gen_player(c); put(E_PLAYER, c);
        gen_boss(c); put(E_BOSS, c);
        gen_coin_pick(c); put(E_COIN, c);
        gen_spark(c); put(E_SPARK, c);
        gen_explosion(c); put(E_EXPLOSION, c);
        gen_muzzle(c); put(E_MUZZLE, c);
        gen_shadow(c); put(E_SHADOW, c);
        gen_heart(c); put(E_HEART, c);
        gen_skull(c); put(E_SKULL, c);
    }

    // ---- UI ----
    {
        Canvas c;
        gen_bar_bg(c); put(U_BAR_BG, c);
        gen_bar_hp(c); put(U_BAR_HP, c);
        gen_bar_armor(c); put(U_BAR_ARMOR, c);
        gen_bar_energy(c); put(U_BAR_ENERGY, c);
        gen_panel(c); put(U_PANEL, c);
        gen_slot(c); put(U_SLOT, c);
        gen_cursor(c); put(U_CURSOR, c);
        gen_arrow_up(c); put(U_ARROW_UP, c);
        gen_arrow_down(c); put(U_ARROW_DOWN, c);
        gen_star(c); put(U_STAR, c);
        gen_key(c); put(U_KEY, c);
        gen_circle(c); put(U_CIRCLE, c);
        gen_ring(c); put(U_RING, c);
    }
}

const u32 *pixels() { return g_px; }
i32 pixel_w() { return AW; }
i32 pixel_h() { return AH; }

Uv uv_of(i32 slot)
{
    if(slot < 0) slot = 0;
    if(slot >= MAX_SLOTS) slot = MAX_SLOTS - 1;
    i32 cx = (slot % COLS) * CELL;
    i32 cy = (slot / COLS) * CELL;
    // 半像素内缩，避免线性/最近采样时取到相邻格
    const f32 half = 0.25f;
    Uv r;
    r.u0 = (cx + half) / (f32)AW;
    r.v0 = (cy + half) / (f32)AH;
    r.u1 = (cx + CELL - half) / (f32)AW;
    r.v1 = (cy + CELL - half) / (f32)AH;
    return r;
}

}   // namespace atlas