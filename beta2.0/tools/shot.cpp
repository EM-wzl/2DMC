// ===================== 窗口截图工具 =====================
// 用法: shot.exe <标题子串> <输出.bmp>
// 从桌面 DC 抓取目标窗口可见区域（GL 窗口用窗口 DC 会抓到黑屏，
// 必须走桌面 DC + CAPTUREBLT）后写成 32 位 BMP。
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const char *g_needle = nullptr;
static HWND g_found = nullptr;

static void describe(HWND w)
{
    wchar_t buf[512] = {};
    int n = GetWindowTextW(w, buf, 512);
    wchar_t cls[256] = {};
    GetClassNameW(w, cls, 256);
    char nc[512] = {};
    WideCharToMultiByte(CP_UTF8, 0, cls, -1, nc, sizeof(nc), nullptr, nullptr);
    // 标题以码点形式打印，避开控制台编码干扰
    printf("hwnd=%p vis=%d class=[%s] titleChars=%d codes:", (void *)w,
           IsWindowVisible(w) ? 1 : 0, nc, n);
    for(int i = 0; i < n && i < 24; i++) printf(" %04X", (unsigned)buf[i]);
    printf("\n");
}

static BOOL CALLBACK enum_cb(HWND w, LPARAM)
{
    if(!IsWindowVisible(w)) return TRUE;
    wchar_t buf[512] = {};
    GetWindowTextW(w, buf, 512);
    if(!buf[0]) return TRUE;
    char nb[1024] = {};
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, nb, sizeof(nb), nullptr, nullptr);
    if(strstr(nb, g_needle))
    {
        g_found = w;
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK list_cb(HWND w, LPARAM)
{
    if(IsWindowVisible(w)) describe(w);
    return TRUE;
}

int main(int argc, char **argv)
{
    if(argc >= 2 && strcmp(argv[1], "--list") == 0)
    {
        EnumWindows(list_cb, 0);
        return 0;
    }
    if(argc < 3)
    {
        printf("usage: shot.exe <title-substr> <out.bmp>\n");
        printf("       shot.exe --list\n");
        return 2;
    }
    g_needle = argv[1];
    const char *out = argv[2];

    for(int i = 0; i < 40 && !g_found; i++)
    {
        EnumWindows(enum_cb, 0);
        if(!g_found) Sleep(250);
    }
    if(!g_found)
    {
        printf("window not found: %s\n", g_needle);
        return 3;
    }

    RECT r = {};
    GetWindowRect(g_found, &r);
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    if(w <= 0 || h <= 0)
    {
        printf("bad window rect\n");
        return 4;
    }

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;      // 自顶向下
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if(!bmp || !bits)
    {
        printf("CreateDIBSection failed\n");
        return 5;
    }
    HGDIOBJ old = SelectObject(mem, bmp);

    if(!BitBlt(mem, 0, 0, w, h, screen, r.left, r.top, SRCCOPY | CAPTUREBLT))
    {
        printf("BitBlt failed: %lu\n", GetLastError());
        return 6;
    }
    GdiFlush();

    // 写 BMP（行按 4 字节对齐，32bpp 天然对齐）
    int rowBytes = w * 4;
    int dataSize = rowBytes * h;
    BITMAPFILEHEADER fh = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + dataSize;
    BITMAPINFOHEADER ih = bi.bmiHeader;

    FILE *f = fopen(out, "wb");
    if(!f)
    {
        printf("cannot open %s\n", out);
        return 7;
    }
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    // BMP 要求自底向上；我们抓的是自顶向下，需要逐行倒序写出
    unsigned char *p = (unsigned char *)bits;
    for(int y = h - 1; y >= 0; y--)
        fwrite(p + (size_t)y * rowBytes, 1, rowBytes, f);
    fclose(f);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);

    printf("saved %s (%dx%d)\n", out, w, h);
    return 0;
}