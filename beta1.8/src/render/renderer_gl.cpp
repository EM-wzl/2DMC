// ===================== 2D 渲染实现（OpenGL 1.x 兼容路径）=====================
// 一批顶点数组 = 一次 draw call。图集把上千个方块压成 1 个纹理，
// 所以整屏地形 + 实体 + 特效通常只需 1~3 次 draw call。
//
// 顶点色是逐顶点的，因此把「相邻四格的光照」写进四角就能得到平滑光照，
// 不再像旧版字符渲染那样一格一格硬边。
#include "renderer.h"
#include "font.h"
#include "../platform/platform.h"

#include <GL/gl.h>
#include <cstdarg>
#include <cstdio>
#include <vector>
#include <cmath>

namespace render {
namespace {

struct Vtx
{
    f32 x, y, u, v;
    u32 c;
};

u32 g_atlas_tex = 0;
u32 g_cur_tex = 0;
std::vector<Vtx> g_v;
i32 g_calls = 0;
i32 g_quads = 0;

f32 g_sw = 0, g_sh = 0;
f32 g_camx = 0, g_camy = 0, g_zoom = 16.0f;
f32 g_last_tile_px = 16.0f;

struct Clip
{
    f32 x, y, w, h;
};
std::vector<Clip> g_clips;

u32 g_blend_key = 1;   // 当前批次使用的混合状态（1=开 0=关）

// ---------------- 批次 ----------------
void flush()
{
    if(g_v.empty()) return;
    u32 tex = g_cur_tex ? g_cur_tex : g_atlas_tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    if(g_blend_key) glEnable(GL_BLEND);
    else            glDisable(GL_BLEND);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(Vtx), &g_v[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vtx), &g_v[0].u);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vtx), &g_v[0].c);
    glDrawArrays(GL_QUADS, 0, (GLsizei)g_v.size());
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    g_quads += (i32)g_v.size() / 4;
    g_v.clear();
    g_calls++;
}

void use_texture(u32 tex)
{
    if(g_cur_tex != tex)
    {
        flush();
        g_cur_tex = tex;
    }
}

// 轴向矩形，四角同色
void push_quad(f32 x, f32 y, f32 w, f32 h,
               f32 u0, f32 v0, f32 u1, f32 v1, u32 c)
{
    g_v.push_back({x, y, u0, v0, c});
    g_v.push_back({x + w, y, u1, v0, c});
    g_v.push_back({x + w, y + h, u1, v1, c});
    g_v.push_back({x, y + h, u0, v1, c});
}

// 轴向矩形，四角异色（左上/右上/右下/左下）
void push_quad_lit(f32 x, f32 y, f32 w, f32 h,
                   f32 u0, f32 v0, f32 u1, f32 v1,
                   u32 c00, u32 c10, u32 c11, u32 c01)
{
    g_v.push_back({x, y, u0, v0, c00});
    g_v.push_back({x + w, y, u1, v0, c10});
    g_v.push_back({x + w, y + h, u1, v1, c11});
    g_v.push_back({x, y + h, u0, v1, c01});
}

// 绕中心旋转 + 翻转
void push_quad_rot(f32 x, f32 y, f32 w, f32 h,
                   f32 u0, f32 v0, f32 u1, f32 v1, u32 c,
                   f32 rot, bool fx, bool fy)
{
    f32 hw = w * 0.5f, hh = h * 0.5f;
    f32 cx = x + hw, cy = y + hh;
    if(fx) { f32 t = u0; u0 = u1; u1 = t; }
    if(fy) { f32 t = v0; v0 = v1; v1 = t; }
    f32 co = std::cos(rot), si = std::sin(rot);

    f32 lx[4] = {-hw, hw, hw, -hw};
    f32 ly[4] = {-hh, -hh, hh, hh};
    f32 lu[4] = {u0, u1, u1, u0};
    f32 lv[4] = {v0, v0, v1, v1};
    for(int i = 0; i < 4; i++)
    {
        f32 px = lx[i] * co - ly[i] * si + cx;
        f32 py = lx[i] * si + ly[i] * co + cy;
        g_v.push_back({px, py, lu[i], lv[i], c});
    }
}

// ---------------- 裁剪 ----------------
void apply_clip()
{
    if(g_clips.empty())
    {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    f32 x0 = g_clips[0].x, y0 = g_clips[0].y;
    f32 x1 = g_clips[0].x + g_clips[0].w, y1 = g_clips[0].y + g_clips[0].h;
    for(size_t i = 1; i < g_clips.size(); i++)
    {
        x0 = std::max(x0, g_clips[i].x);
        y0 = std::max(y0, g_clips[i].y);
        x1 = std::min(x1, g_clips[i].x + g_clips[i].w);
        y1 = std::min(y1, g_clips[i].y + g_clips[i].h);
    }
    if(x1 < x0) x1 = x0;
    if(y1 < y0) y1 = y0;
    i32 sx = (i32)x0;
    i32 sy = (i32)(g_sh - y1);
    i32 sw = (i32)(x1 - x0);
    i32 sh = (i32)(y1 - y0);
    if(sw < 0) sw = 0;
    if(sh < 0) sh = 0;
    glEnable(GL_SCISSOR_TEST);
    glScissor(sx, sy, sw, sh);
}

// ---------------- UTF-8 ----------------
u32 next_cp(const char *&s)
{
    u8 c = (u8)*s;
    if(c == 0) return 0;
    if(c < 0x80) { s++; return c; }
    i32 n = 0;
    u32 cp = 0;
    if((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1Fu; }
    else if((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0Fu; }
    else if((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07u; }
    else { s++; return 0xFFFD; }
    for(i32 i = 0; i < n; i++)
    {
        u8 cc = (u8)s[1 + i];
        if((cc & 0xC0) != 0x80) { s++; return 0xFFFD; }
        cp = (cp << 6) | (cc & 0x3Fu);
    }
    s += n + 1;
    return cp;
}

// 白点 UV（画纯色矩形用）
void white_uv(f32 &u, f32 &v)
{
    atlas::Uv uv = atlas::uv_of(atlas::S_WHITE);
    u = (uv.u0 + uv.u1) * 0.5f;
    v = (uv.v0 + uv.v1) * 0.5f;
}

}   // namespace

// ---------------- 生命周期 ----------------
bool init()
{
    atlas::build();

    glGenTextures(1, &g_atlas_tex);
    glBindTexture(GL_TEXTURE_2D, g_atlas_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas::pixel_w(), atlas::pixel_h(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, atlas::pixels());

    if(!font::init())
        platform::log("字体初始化失败，文本将不可见");

    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);

    platform::log("渲染初始化完成: 图集 %dx%d 共 %d 格", atlas::pixel_w(),
                  atlas::pixel_h(), atlas::MAX_SLOTS);
    return true;
}

void upload_atlas()
{
    if(!g_atlas_tex) return;
    glBindTexture(GL_TEXTURE_2D, g_atlas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas::pixel_w(), atlas::pixel_h(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, atlas::pixels());
}

void shutdown()
{
    g_v.clear();
    font::shutdown();
    if(g_atlas_tex) { glDeleteTextures(1, &g_atlas_tex); g_atlas_tex = 0; }
}

f32 tile_px() { return g_last_tile_px; }

// ---------------- 坐标系 ----------------
void begin_frame(Color clearColor)
{
    g_sw = (f32)platform::width();
    g_sh = (f32)platform::height();
    g_calls = 0;
    g_quads = 0;
    set_clear_color(clearColor);
    glClear(GL_COLOR_BUFFER_BIT);
}

void begin_world(f32 centerX, f32 centerY, f32 tilePx)
{
    g_sw = (f32)platform::width();
    g_sh = (f32)platform::height();
    g_camx = centerX;
    g_camy = centerY;
    g_zoom = tilePx > 0.5f ? tilePx : 0.5f;
    g_last_tile_px = g_zoom;

    flush();
    f32 halfW = g_sw * 0.5f / g_zoom;
    f32 halfH = g_sh * 0.5f / g_zoom;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // y 轴翻转：世界 y 向下，屏幕 y 也向下
    glOrtho(centerX - halfW, centerX + halfW, centerY + halfH, centerY - halfH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    g_clips.clear();
    apply_clip();
}

void begin_ui()
{
    g_sw = (f32)platform::width();
    g_sh = (f32)platform::height();
    flush();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_sw, g_sh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    g_clips.clear();
    apply_clip();
}

// ---------------- 图元 ----------------
void draw(i32 slot, f32 x, f32 y, f32 w, f32 h, Color tint)
{
    atlas::Uv uv = atlas::uv_of(slot);
    use_texture(g_atlas_tex);
    push_quad(x, y, w, h, uv.u0, uv.v0, uv.u1, uv.v1, tint);
}

void draw_ex(i32 slot, f32 x, f32 y, f32 w, f32 h, Color tint,
             f32 rot, bool flipX, bool flipY)
{
    atlas::Uv uv = atlas::uv_of(slot);
    use_texture(g_atlas_tex);
    push_quad_rot(x, y, w, h, uv.u0, uv.v0, uv.u1, uv.v1, tint, rot, flipX, flipY);
}

void draw_lit(i32 slot, f32 x, f32 y, f32 w, f32 h,
              Color c00, Color c10, Color c11, Color c01)
{
    atlas::Uv uv = atlas::uv_of(slot);
    use_texture(g_atlas_tex);
    push_quad_lit(x, y, w, h, uv.u0, uv.v0, uv.u1, uv.v1, c00, c10, c11, c01);
}

void draw_sub(i32 slot, f32 x, f32 y, f32 w, f32 h, f32 su0, f32 sv0, f32 su1,
              f32 sv1, Color tint)
{
    atlas::Uv uv = atlas::uv_of(slot);
    f32 u0 = uv.u0 + (uv.u1 - uv.u0) * su0;
    f32 u1 = uv.u0 + (uv.u1 - uv.u0) * su1;
    f32 v0 = uv.v0 + (uv.v1 - uv.v0) * sv0;
    f32 v1 = uv.v0 + (uv.v1 - uv.v0) * sv1;
    use_texture(g_atlas_tex);
    push_quad(x, y, w, h, u0, v0, u1, v1, tint);
}

void rect(f32 x, f32 y, f32 w, f32 h, Color c)
{
    f32 u, v;
    white_uv(u, v);
    use_texture(g_atlas_tex);
    push_quad(x, y, w, h, u, v, u, v, c);
}

void rect_lit(f32 x, f32 y, f32 w, f32 h, Color c00, Color c10, Color c11, Color c01)
{
    f32 u, v;
    white_uv(u, v);
    use_texture(g_atlas_tex);
    push_quad_lit(x, y, w, h, u, v, u, v, c00, c10, c11, c01);
}

void rect_outline(f32 x, f32 y, f32 w, f32 h, Color c, f32 t)
{
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c);
    rect(x + w - t, y, t, h, c);
}

void line(f32 x0, f32 y0, f32 x1, f32 y1, Color c, f32 thickness)
{
    f32 dx = x1 - x0, dy = y1 - y0;
    f32 len = std::sqrt(dx * dx + dy * dy);
    if(len < 0.001f) return;
    f32 nx = -dy / len * thickness * 0.5f;
    f32 ny = dx / len * thickness * 0.5f;
    f32 u, v;
    white_uv(u, v);
    use_texture(g_atlas_tex);
    g_v.push_back({x0 + nx, y0 + ny, u, v, c});
    g_v.push_back({x1 + nx, y1 + ny, u, v, c});
    g_v.push_back({x1 - nx, y1 - ny, u, v, c});
    g_v.push_back({x0 - nx, y0 - ny, u, v, c});
}

void circle(f32 cx, f32 cy, f32 r, Color c, i32 segs)
{
    (void)segs;
    draw(atlas::F_GLOW, cx - r, cy - r, r * 2, r * 2, c);
}

void ring(f32 cx, f32 cy, f32 r, f32 thickness, Color c, i32 segs)
{
    (void)segs;
    (void)thickness;
    draw(atlas::U_BUBBLE, cx - r, cy - r, r * 2, r * 2, c);
}

void panel(f32 x, f32 y, f32 w, f32 h, Color fill, Color border, f32 r,
           f32 border_w)
{
    if(r < 0.5f) r = 0.5f;
    if(r * 2 > w) r = w * 0.5f;
    if(r * 2 > h) r = h * 0.5f;
    atlas::Uv uv = atlas::uv_of(atlas::U_PANEL);
    f32 u0 = uv.u0, v0 = uv.v0, u1 = uv.u1, v1 = uv.v1;
    f32 mu, mv;
    white_uv(mu, mv);
    use_texture(g_atlas_tex);

    // 中部与四条边
    push_quad(x + r, y + r, w - 2 * r, h - 2 * r, mu, mv, mu, mv, fill);
    push_quad(x + r, y, w - 2 * r, r, mu, mv, mu, mv, fill);
    push_quad(x + r, y + h - r, w - 2 * r, r, mu, mv, mu, mv, fill);
    push_quad(x, y + r, r, h - 2 * r, mu, mv, mu, mv, fill);
    push_quad(x + w - r, y + r, r, h - 2 * r, mu, mv, mu, mv, fill);

    // 四角：取圆角贴图的四分之一当遮罩填充
    struct Corner { f32 dx, dy, su0, sv0, su1, sv1; };
    const Corner cs[4] = {
        {0,       0,       0.0f, 0.0f, 0.5f, 0.5f},
        {w - r,   0,       0.5f, 0.0f, 1.0f, 0.5f},
        {0,       h - r,   0.0f, 0.5f, 0.5f, 1.0f},
        {w - r,   h - r,   0.5f, 0.5f, 1.0f, 1.0f},
    };
    for(const Corner &c : cs)
    {
        f32 a0 = u0 + (u1 - u0) * c.su0, a1 = u0 + (u1 - u0) * c.su1;
        f32 b0 = v0 + (v1 - v0) * c.sv0, b1 = v0 + (v1 - v0) * c.sv1;
        push_quad(x + c.dx, y + c.dy, r, r, a0, b0, a1, b1, fill);
    }

    if(border_w > 0.01f)
        rect_outline(x + 1, y + 1, w - 2, h - 2, border, border_w);
}

void push_clip(f32 x, f32 y, f32 w, f32 h)
{
    flush();
    g_clips.push_back({x, y, w, h});
    apply_clip();
}

void pop_clip()
{
    flush();
    if(!g_clips.empty()) g_clips.pop_back();
    apply_clip();
}

// ---------------- 文本 ----------------
void text(f32 x, f32 y, const char *utf8, Color c, f32 scale)
{
    if(!utf8 || !font::ready()) return;
    if(scale <= 0.01f) scale = 0.01f;
    use_texture(font::texture());
    f32 penX = x;
    f32 penY = y;
    const char *s = utf8;
    while(true)
    {
        u32 cp = next_cp(s);
        if(cp == 0) break;
        if(cp == '\n')
        {
            penX = x;
            penY += font::line_height() * scale;
            continue;
        }
        if(cp == '\r') continue;
        if(cp == '\t') { penX += font::space_adv() * 4 * scale; continue; }
        if(cp == ' ')  { penX += font::space_adv() * scale; continue; }
        const font::Glyph *g = font::get(cp);
        if(!g) { penX += font::space_adv() * scale; continue; }
        if(g->w > 0.01f && g->h > 0.01f)
            push_quad(penX, penY, g->w * scale, g->h * scale,
                      g->u0, g->v0, g->u1, g->v1, c);
        penX += g->adv * scale;
    }
}

void text_shadow(f32 x, f32 y, const char *utf8, Color c, f32 scale)
{
    Color sh = rgba(0, 0, 0, (u8)((c >> 24) * 0.75f));
    text(x + scale, y + scale, utf8, sh, scale);
    text(x, y, utf8, c, scale);
}

void text_center_shadow(f32 cx, f32 y, const char *utf8, Color c, f32 scale)
{
    text_shadow(cx - text_w(utf8, scale) * 0.5f, y, utf8, c, scale);
}

f32 text_w(const char *utf8, f32 scale)
{
    if(!utf8 || !font::ready()) return 0;
    f32 w = 0, best = 0;
    const char *s = utf8;
    while(true)
    {
        u32 cp = next_cp(s);
        if(cp == 0) break;
        if(cp == '\n') { best = std::max(best, w); w = 0; continue; }
        if(cp == '\r') continue;
        if(cp == '\t') { w += font::space_adv() * 4 * scale; continue; }
        if(cp == ' ')  { w += font::space_adv() * scale; continue; }
        const font::Glyph *g = font::get(cp);
        w += (g ? g->adv : font::space_adv()) * scale;
    }
    return std::max(best, w);
}

f32 text_h(f32 scale)
{
    return font::ready() ? font::line_height() * scale : 16.0f * scale;
}

void textf(f32 x, f32 y, Color c, f32 scale, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    text(x, y, buf, c, scale);
}

void text_center(f32 cx, f32 y, const char *utf8, Color c, f32 scale)
{
    text(cx - text_w(utf8, scale) * 0.5f, y, utf8, c, scale);
}

void text_right(f32 rx, f32 y, const char *utf8, Color c, f32 scale)
{
    text(rx - text_w(utf8, scale), y, utf8, c, scale);
}

// ---------------- 帧收尾 ----------------
void end()
{
    flush();
}

void set_clear_color(Color c)
{
    glClearColor(((c >> 0) & 0xFF) / 255.0f,
                 ((c >> 8) & 0xFF) / 255.0f,
                 ((c >> 16) & 0xFF) / 255.0f,
                 ((c >> 24) & 0xFF) / 255.0f);
}

i32 last_draw_calls() { return g_calls; }
i32 last_quad_count() { return g_quads; }

void set_blend(bool on)
{
    u32 k = on ? 1u : 0u;
    if(g_blend_key == k) return;
    flush();          // 先把上一批用旧状态提交，避免状态串批
    g_blend_key = k;
}

// BMP 文件头/信息头（显式声明，避免把 windows.h 拉进渲染层）
#pragma pack(push, 1)
struct BmpFileHeader
{
    u16 type;
    u32 size;
    u16 res1, res2;
    u32 offBits;
};
struct BmpInfoHeader
{
    u32 size;
    i32 w, h;
    u16 planes, bpp;
    u32 compression;
    u32 sizeImage;
    i32 xppm, yppm;
    u32 clrUsed, clrImportant;
};
#pragma pack(pop)

bool capture_bmp(const char *path)
{
    flush();
    i32 w = platform::width();
    i32 h = platform::height();
    if(w <= 0 || h <= 0) return false;

    std::vector<u8> buf((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

    // glReadPixels 行序自底向上，正好是 BMP 的原生行序；只需把 RGBA 换成 BGRA
    for(size_t i = 0; i < buf.size(); i += 4)
    {
        u8 r = buf[i], b = buf[i + 2];
        buf[i] = b;
        buf[i + 2] = r;
        buf[i + 3] = 255;
    }

    FILE *f = fopen(path, "wb");
    if(!f) return false;

    const u32 rowBytes = (u32)w * 4;
    const u32 dataSize = rowBytes * (u32)h;

    BmpFileHeader fh = {};
    fh.type = 0x4D42;
    fh.offBits = (u32)(sizeof(BmpFileHeader) + sizeof(BmpInfoHeader));
    fh.size = fh.offBits + dataSize;

    BmpInfoHeader ih = {};
    ih.size = sizeof(BmpInfoHeader);
    ih.w = w;
    ih.h = h;
    ih.planes = 1;
    ih.bpp = 32;
    ih.compression = 0;
    ih.sizeImage = dataSize;

    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    fwrite(buf.data(), 1, dataSize, f);
    fclose(f);
    return true;
}

}   // namespace render
