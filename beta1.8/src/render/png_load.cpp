// ===================== PNG 载入实现（GDI+）=====================
// 用 Windows 自带的 gdiplus.dll 解码 PNG（带 alpha），不需要任何第三方库。
#include "png_load.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>   // MinGW 的 gdiplus.h 依赖 PROPID（在 objidl.h 里）
#include <gdiplus.h>

namespace atlas {

namespace {

ULONG_PTR g_token = 0;
bool g_gdi_inited = false;

bool ensure_gdiplus()
{
    if(g_gdi_inited) return true;
    Gdiplus::GdiplusStartupInput in;
    if(Gdiplus::GdiplusStartup(&g_token, &in, nullptr) != Gdiplus::Ok) return false;
    g_gdi_inited = true;
    return true;
}

// full=false：动画贴图竖向拼帧，只取左上角第一个正方形帧
bool load_png(const char *path, std::vector<u32> &out, i32 &w, i32 &h, bool full)
{
    out.clear();
    w = h = 0;
    if(!path || !path[0] || !ensure_gdiplus()) return false;

    wchar_t wp[MAX_PATH];
    if(!MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, MAX_PATH)) return false;

    using namespace Gdiplus;
    Bitmap *bmp = Bitmap::FromFile(wp, FALSE);
    if(!bmp || bmp->GetLastStatus() != Ok) { delete bmp; return false; }

    const i32 bw = (i32)bmp->GetWidth();
    const i32 bh = (i32)bmp->GetHeight();
    if(bw <= 0 || bh <= 0) { delete bmp; return false; }

    const i32 side = bw < bh ? bw : bh;
    const i32 rw = full ? bw : side, rh = full ? bh : side;
    Gdiplus::Rect rc(0, 0, rw, rh);   // 限定命名空间：windows.h 里也有个 Rect
    BitmapData bd;
    if(bmp->LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok)
    {
        delete bmp;
        return false;
    }

    out.resize((size_t)rw * rh);
    const u8 *base = (const u8 *)bd.Scan0;
    for(i32 y = 0; y < rh; y++)
    {
        const u32 *row = (const u32 *)(base + (size_t)y * bd.Stride);
        for(i32 x = 0; x < rw; x++)
        {
            const u32 v = row[x];                    // GDI+: 0xAARRGGBB
            const u32 a = (v >> 24) & 0xFF;
            const u32 r = (v >> 16) & 0xFF;
            const u32 g = (v >> 8) & 0xFF;
            const u32 b = v & 0xFF;
            out[(size_t)y * rw + x] = (a << 24) | (b << 16) | (g << 8) | r;  // -> 0xAABBGGRR
        }
    }
    bmp->UnlockBits(&bd);
    delete bmp;

    w = rw;
    h = rh;
    return true;
}

}   // namespace

bool load_png_rgba(const char *path, std::vector<u32> &out, i32 &w, i32 &h)
{
    return load_png(path, out, w, h, false);
}

bool load_png_rgba_full(const char *path, std::vector<u32> &out, i32 &w, i32 &h)
{
    return load_png(path, out, w, h, true);
}

}   // namespace atlas
