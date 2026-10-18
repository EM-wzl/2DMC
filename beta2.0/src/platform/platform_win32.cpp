// ===================== Win32 + OpenGL 平台实现 =====================
#include "platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// MinGW 默认不定义 UNICODE：不定义时 DefWindowProc 会解析成 ANSI 版本，
// 与 CreateWindowExW 创建的 Unicode 窗口混用会把宽标题按单字节读
// （"2DMC…" 的 UTF-16 是 32 00，读到 00 就截断成 "2"）。
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW
#include <GL/gl.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace platform {
namespace {

HWND  g_hwnd = nullptr;
HDC   g_hdc = nullptr;
HGLRC g_glrc = nullptr;
i32   g_w = 0, g_h = 0;
bool  g_quit = false;
bool  g_focused = false;

bool g_down[256] = {};
bool g_pressed[256] = {};
bool g_released[256] = {};
bool g_mdown[MouseBtn::MB_COUNT] = {};
bool g_mpressed[MouseBtn::MB_COUNT] = {};
f32  g_mx = 0, g_my = 0;
f32  g_wheel = 0;

LARGE_INTEGER g_freq, g_prev;
f32 g_dt = 1.0f / 60.0f;
bool g_have_freq = false;

using PFN_SwapInterval = BOOL(WINAPI *)(int);
PFN_SwapInterval g_swap_interval = nullptr;

// UTF-8 -> UTF-16：不依赖编译器对 L"" 的字符集处理，中文字面量才可靠
std::wstring to_wide(const char *s)
{
    if(!s) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if(n <= 0) return L"";
    std::wstring w;
    w.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    if(!w.empty() && w.back() == 0) w.pop_back();
    return w;
}

void refresh_mouse()
{
    if(!g_hwnd) return;
    POINT p;
    if(!GetCursorPos(&p)) return;
    ScreenToClient(g_hwnd, &p);
    g_mx = (f32)p.x;
    g_my = (f32)p.y;
}

LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg)
    {
    case WM_CLOSE:
        g_quit = true;
        return 0;
    case WM_DESTROY:
        g_quit = true;
        PostQuitMessage(0);
        return 0;
    case WM_SETFOCUS:
        g_focused = true;
        return 0;
    case WM_KILLFOCUS:
        g_focused = false;
        // 失焦时清空按键，避免回来发现键位粘住
        memset(g_down, 0, sizeof(g_down));
        memset(g_mdown, 0, sizeof(g_mdown));
        return 0;
    case WM_SIZE:
        if(wp != SIZE_MINIMIZED)
        {
            g_w = LOWORD(lp);
            g_h = HIWORD(lp);
            if(g_hdc) glViewport(0, 0, g_w, g_h);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;   // 交给 GL 绘制，避免闪白
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        i32 vk = (i32)wp;
        if(vk >= 0 && vk < 256)
        {
            if(!g_down[vk]) g_pressed[vk] = true;
            g_down[vk] = true;
        }
        // 屏蔽 Alt 打开系统菜单 / F10 等，保证 Alt+Enter 之类的组合不抢焦点
        if(msg == WM_SYSKEYDOWN && (wp == VK_MENU || wp == VK_F10))
            return 0;
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        i32 vk = (i32)wp;
        if(vk >= 0 && vk < 256)
        {
            g_down[vk] = false;
            g_released[vk] = true;
        }
        break;
    }
    case WM_MOUSEMOVE:
        g_mx = (f32)(i32)(short)LOWORD(lp);
        g_my = (f32)(i32)(short)HIWORD(lp);
        return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
    {
        i32 idx = MB_LEFT;
        bool down = false;
        if(msg == WM_LBUTTONDOWN) { idx = MB_LEFT;   down = true;  }
        else if(msg == WM_LBUTTONUP) { idx = MB_LEFT;   down = false; }
        else if(msg == WM_RBUTTONDOWN) { idx = MB_RIGHT;  down = true;  }
        else if(msg == WM_RBUTTONUP) { idx = MB_RIGHT;  down = false; }
        else if(msg == WM_MBUTTONDOWN) { idx = MB_MIDDLE; down = true;  }
        else { idx = MB_MIDDLE; down = false; }
        if(down && !g_mdown[idx]) g_mpressed[idx] = true;
        g_mdown[idx] = down;
        refresh_mouse();
        return 0;
    }
    case WM_MOUSEWHEEL:
        g_wheel += (f32)(i32)(short)HIWORD(wp) / (f32)WHEEL_DELTA;
        return 0;
    default:
        break;
    }
    return DefWindowProc(h, msg, wp, lp);
}

}   // namespace

bool init(const char *title, i32 w, i32 h)
{
    HINSTANCE inst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"2DMCGameWnd";
    if(!RegisterClassExW(&wc))
    {
        log("RegisterClassEx 失败: %lu", GetLastError());
        return false;
    }

    // 调整窗口尺寸，让客户区正好是 w x h
    RECT rc = {0, 0, w, h};
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    AdjustWindowRect(&rc, style, FALSE);

    std::wstring wtitle = to_wide(title);
    log("标题原始字节: [%s] len=%d -> 宽字符 len=%d", title, (int)strlen(title),
        (int)wtitle.size());
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, wtitle.c_str(), style,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top,
                             nullptr, nullptr, inst, nullptr);
    if(!g_hwnd)
    {
        log("CreateWindowEx 失败: %lu", GetLastError());
        return false;
    }

    g_hdc = GetDC(g_hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if(!pf)
    {
        log("ChoosePixelFormat 失败: %lu", GetLastError());
        return false;
    }
    if(!SetPixelFormat(g_hdc, pf, &pfd))
    {
        log("SetPixelFormat 失败: %lu", GetLastError());
        return false;
    }

    g_glrc = wglCreateContext(g_hdc);
    if(!g_glrc)
    {
        log("wglCreateContext 失败: %lu", GetLastError());
        return false;
    }
    if(!wglMakeCurrent(g_hdc, g_glrc))
    {
        log("wglMakeCurrent 失败: %lu", GetLastError());
        return false;
    }

    g_swap_interval = (PFN_SwapInterval)gl_proc("wglSwapIntervalEXT");
    set_vsync(true);

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_prev);
    g_have_freq = true;

    GetClientRect(g_hwnd, &rc);
    g_w = rc.right - rc.left;
    g_h = rc.bottom - rc.top;
    glViewport(0, 0, g_w, g_h);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    SetForegroundWindow(g_hwnd);
    g_focused = true;

    log("GL_VERSION  = %s", (const char *)glGetString(GL_VERSION));
    log("GL_RENDERER = %s", (const char *)glGetString(GL_RENDERER));
    {
        HDC sdc = GetDC(nullptr);
        if(sdc)
        {
            log("显示刷新率 = %d Hz", (int)GetDeviceCaps(sdc, VREFRESH));
            ReleaseDC(nullptr, sdc);
        }
    }
    log("窗口已创建 %dx%d", g_w, g_h);
    return true;
}

void shutdown()
{
    if(g_glrc)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_glrc);
        g_glrc = nullptr;
    }
    if(g_hdc && g_hwnd)
    {
        ReleaseDC(g_hwnd, g_hdc);
        g_hdc = nullptr;
    }
    if(g_hwnd)
    {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
}

bool pump()
{
    MSG msg;
    while(PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if(msg.message == WM_QUIT) g_quit = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    refresh_mouse();

    if(g_have_freq)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        f32 d = (f32)((f64)(now.QuadPart - g_prev.QuadPart) / (f64)g_freq.QuadPart);
        g_prev = now;
        if(d < 0) d = 0;
        if(d > 0.1f) d = 0.1f;   // 断点/拖动窗口后不要一次跳太多
        g_dt = d;
    }
    return !g_quit;
}

void present()
{
    if(g_hdc) SwapBuffers(g_hdc);
}

void set_title(const char *title)
{
    if(!g_hwnd) return;
    std::wstring w = to_wide(title);
    SetWindowTextW(g_hwnd, w.c_str());
}

i32 width() { return g_w; }
i32 height() { return g_h; }
bool focused() { return g_focused; }

bool key_down(i32 k) { return k >= 0 && k < 256 ? g_down[k] : false; }
bool key_pressed(i32 k) { return k >= 0 && k < 256 ? g_pressed[k] : false; }
bool key_released(i32 k) { return k >= 0 && k < 256 ? g_released[k] : false; }
bool mouse_down(i32 b) { return b >= 0 && b < MB_COUNT ? g_mdown[b] : false; }
bool mouse_pressed(i32 b) { return b >= 0 && b < MB_COUNT ? g_mpressed[b] : false; }
f32 mouse_x() { return g_mx; }
f32 mouse_y() { return g_my; }
f32 wheel() { return g_wheel; }

void end_frame()
{
    memset(g_pressed, 0, sizeof(g_pressed));
    memset(g_released, 0, sizeof(g_released));
    memset(g_mpressed, 0, sizeof(g_mpressed));
    g_wheel = 0;
}

f64 now_seconds()
{
    if(!g_have_freq) return 0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (f64)now.QuadPart / (f64)g_freq.QuadPart;
}

f32 dt() { return g_dt; }

void *gl_proc(const char *name)
{
    void *p = (void *)wglGetProcAddress(name);
    if(!p)
    {
        static HMODULE gl = LoadLibraryA("opengl32.dll");
        if(gl) p = (void *)GetProcAddress(gl, name);
    }
    return p;
}

void set_vsync(bool on)
{
    if(g_swap_interval) g_swap_interval(on ? 1 : 0);
}

void log(const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    static FILE *f = nullptr;
    if(!f)
    {
        f = fopen("debug.log", "w");
    }
    if(f)
    {
        fprintf(f, "%s\n", buf);
        fflush(f);
    }
    // GUI 构建（-mwindows）没有控制台，往 stderr 写是无效句柄，跳过
    static int haveErr = -1;
    if(haveErr < 0)
    {
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        haveErr = (h && h != INVALID_HANDLE_VALUE) ? 1 : 0;
    }
    if(haveErr) fprintf(stderr, "%s\n", buf);
}

// ---------------- 命令行 ----------------
namespace {
char **g_argv = nullptr;
i32 g_argc = 0;
bool g_argv_tried = false;

void build_argv()
{
    if(g_argv_tried) return;
    g_argv_tried = true;
    int n = 0;
    LPWSTR *w = CommandLineToArgvW(GetCommandLineW(), &n);
    if(!w || n <= 0) return;
    g_argv = (char **)calloc((size_t)n + 1, sizeof(char *));
    if(!g_argv)
    {
        LocalFree(w);
        return;
    }
    for(int i = 0; i < n; i++)
    {
        int need = WideCharToMultiByte(CP_UTF8, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
        if(need <= 0) continue;
        char *s = (char *)malloc((size_t)need);
        if(!s) continue;
        WideCharToMultiByte(CP_UTF8, 0, w[i], -1, s, need, nullptr, nullptr);
        g_argv[g_argc++] = s;
    }
    LocalFree(w);
}
}   // namespace

i32 argc()
{
    build_argv();
    return g_argc;
}

const char *argv(i32 index)
{
    build_argv();
    if(index < 0 || index >= g_argc) return "";
    return g_argv[index];
}

const char *arg_value(const char *key)
{
    build_argv();
    size_t klen = strlen(key);
    for(i32 i = 0; i < g_argc; i++)
    {
        const char *a = g_argv[i];
        if(a[0] != '-' || a[1] != '-') continue;
        const char *body = a + 2;
        // 支持 --key=value 与裸开关 --key（裸开关返回空串，仍为真值）
        if(strncmp(body, key, klen) == 0 && body[klen] == '=')
            return body + klen + 1;
        if(strncmp(body, key, klen) == 0 && body[klen] == '\0')
            return body + klen;
    }
    return nullptr;
}

}   // namespace platform