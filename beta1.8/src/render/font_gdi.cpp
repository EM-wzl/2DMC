// ===================== 字体图集实现（Win32 GDI -> GL 纹理）=====================
#include "font.h"
#include "../platform/platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>

#include <unordered_map>

namespace font {
namespace {

constexpr i32 FW = 1024;
constexpr i32 FH = 1024;
constexpr i32 PAD = 1;

u32  g_tex = 0;
HDC  g_dc = nullptr;
HBITMAP g_bmp = nullptr;
u32 *g_bits = nullptr;
HFONT g_font = nullptr;

i32 g_cx = PAD, g_cy = PAD, g_row_h = 0;
f32 g_line_h = 18.0f;
f32 g_space = 5.0f;
bool g_ready = false;

std::unordered_map<u32, Glyph> g_cache;

// 依次尝试可用的中文字体
HFONT make_font()
{
    const wchar_t *faces[] = {
        L"Microsoft YaHei UI", L"Microsoft YaHei", L"SimHei", L"SimSun", L"Segoe UI"
    };
    for(const wchar_t *face : faces)
    {
        HFONT f = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
        if(!f) continue;
        HGDIOBJ old = SelectObject(g_dc, f);
        wchar_t probe = L'\u6C49';   // “汉”
        SIZE sz = {};
        GetTextExtentPoint32W(g_dc, &probe, 1, &sz);
        SelectObject(g_dc, old);
        if(sz.cx > 1 && sz.cy > 1)
        {
            platform::log("字体选用: %ls (汉字 %dx%d)", face, (int)sz.cx, (int)sz.cy);
            return f;
        }
        DeleteObject(f);
    }
    platform::log("警告: 未找到含中文字形的字体，中文将显示为方块");
    return CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

// 在 DIB 上清出一块黑底，供 GDI 绘制字形
void clear_rect(i32 x, i32 y, i32 w, i32 h)
{
    for(i32 j = y; j < y + h && j < FH; j++)
        for(i32 i = x; i < x + w && i < FW; i++)
            g_bits[j * FW + i] = 0;
}

// 黑底白字 -> RGBA(255,255,255,亮度)，让亮度直接当 alpha
void to_alpha_rect(i32 x, i32 y, i32 w, i32 h)
{
    for(i32 j = y; j < y + h && j < FH; j++)
        for(i32 i = x; i < x + w && i < FW; i++)
        {
            u32 p = g_bits[j * FW + i];
            u8 r = (u8)(p & 0xFF), g = (u8)((p >> 8) & 0xFF), b = (u8)((p >> 16) & 0xFF);
            u8 v = r > g ? r : g;
            if(b > v) v = b;
            g_bits[j * FW + i] = ((u32)v << 24) | 0x00FFFFFFu;
        }
}

bool new_row_if_needed(i32 w, i32 h)
{
    if(g_cx + w + PAD >= FW)
    {
        g_cx = PAD;
        g_cy += g_row_h + 1;
        g_row_h = 0;
    }
    if(g_cy + h + PAD >= FH) return false;   // 图集满了
    if(h > g_row_h) g_row_h = h;
    return true;
}

Glyph build_glyph(u32 cp)
{
    Glyph gl = {};
    gl.adv = g_space;
    if(!g_ready || cp == 0 || cp > 0x10FFFF) return gl;

    wchar_t wch[2];
    i32 n = 0;
    if(cp < 0x10000)
    {
        wch[0] = (wchar_t)cp;
        n = 1;
    }
    else
    {
        u32 t = cp - 0x10000;
        wch[0] = (wchar_t)(0xD800 + (t >> 10));
        wch[1] = (wchar_t)(0xDC00 + (t & 0x3FF));
        n = 2;
    }

    SIZE sz = {};
    // 关键：必须先选中我们的字体再量宽度，否则用的是 DC 默认字体，advance 会偏小
    HGDIOBJ prev = SelectObject(g_dc, g_font);
    BOOL okExtent = GetTextExtentPoint32W(g_dc, wch, n, &sz);
    SelectObject(g_dc, prev);
    if(!okExtent) return gl;
    if(sz.cx <= 0 || sz.cy <= 0) return gl;

    i32 gw = sz.cx + 2;      // 留 1px 余量，避免抗锯齿被切
    i32 gh = sz.cy + 2;
    if(gw > 128 || gh > 128) return gl;

    if(!new_row_if_needed(gw, gh)) return gl;

    i32 x = g_cx, y = g_cy;
    clear_rect(x, y, gw, gh);

    HGDIOBJ old = SelectObject(g_dc, g_font);
    i32 oldBk = SetBkMode(g_dc, TRANSPARENT);
    COLORREF oldCol = SetTextColor(g_dc, RGB(255, 255, 255));
    TextOutW(g_dc, x + 1, y + 1, wch, n);
    SetTextColor(g_dc, oldCol);
    SetBkMode(g_dc, oldBk);
    SelectObject(g_dc, old);

    GdiFlush();
    to_alpha_rect(x, y, gw, gh);

    // 只上传新增的字形矩形
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, FW);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, gw, gh,
                    GL_RGBA, GL_UNSIGNED_BYTE, &g_bits[y * FW + x]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    g_cx += gw + 1;

    gl.u0 = (f32)x / FW;
    gl.v0 = (f32)y / FH;
    gl.u1 = (f32)(x + gw) / FW;
    gl.v1 = (f32)(y + gh) / FH;
    gl.w = (f32)gw;
    gl.h = (f32)gh;
    gl.adv = (f32)sz.cx;
    return gl;
}

}   // namespace

bool init()
{
    g_dc = CreateCompatibleDC(nullptr);
    if(!g_dc)
    {
        platform::log("字体: CreateCompatibleDC 失败");
        return false;
    }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = FW;
    bi.bmiHeader.biHeight = -FH;        // 负高度 = 自顶向下，行序与 GL 一致
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    g_bmp = CreateDIBSection(g_dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if(!g_bmp || !bits)
    {
        platform::log("字体: CreateDIBSection 失败");
        return false;
    }
    g_bits = (u32 *)bits;
    SelectObject(g_dc, g_bmp);
    memset(g_bits, 0, (size_t)FW * FH * 4);

    g_font = make_font();

    TEXTMETRICW tm = {};
    HGDIOBJ old = SelectObject(g_dc, g_font);
    GetTextMetricsW(g_dc, &tm);
    SelectObject(g_dc, old);
    g_line_h = (f32)(tm.tmHeight + 2);
    SIZE sp = {};
    wchar_t sw = L' ';
    old = SelectObject(g_dc, g_font);
    GetTextExtentPoint32W(g_dc, &sw, 1, &sp);
    SelectObject(g_dc, old);
    g_space = (f32)(sp.cx > 0 ? sp.cx : 5);

    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FW, FH, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_bits);

    g_ready = true;
    platform::log("字体图集就绪: 行高 %.1f 空白 %.1f", (double)g_line_h, (double)g_space);
    return true;
}

void shutdown()
{
    g_cache.clear();
    if(g_font) { DeleteObject(g_font); g_font = nullptr; }
    if(g_bmp) { DeleteObject(g_bmp); g_bmp = nullptr; }
    if(g_dc) { DeleteDC(g_dc); g_dc = nullptr; }
    if(g_tex) { glDeleteTextures(1, &g_tex); g_tex = 0; }
    g_bits = nullptr;
    g_ready = false;
}

bool ready() { return g_ready; }
u32 texture() { return g_tex; }
f32 line_height() { return g_line_h; }
f32 space_adv() { return g_space; }

const Glyph *get(u32 cp)
{
    auto it = g_cache.find(cp);
    if(it != g_cache.end()) return &it->second;
    Glyph gl = build_glyph(cp);
    auto res = g_cache.emplace(cp, gl);
    return &res.first->second;
}

}   // namespace font