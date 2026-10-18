// snap.cpp - 自动化测试：启动游戏、模拟鼠标键盘输入、截取控制台画面到文件
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void clickCell(int cx, int cy)   // 游戏命中区坐标（按 8x16 像素网格换算）
{
    HWND h = GetConsoleWindow();
    POINT pt = {cx * 8, cy * 16};
    ClientToScreen(h, &pt);
    SetCursorPos(pt.x, pt.y);
    Sleep(120);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(130);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(300);
}

static void tapKey(WORD vk, int holdMs)
{
    keybd_event(vk, 0, 0, 0);
    Sleep(holdMs);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
    Sleep(120);
}

static void dumpTitle(const char *label, const char *fname)
{
    HWND h = GetConsoleWindow();
    char title[256] = {0};
    GetWindowTextA(h, title, 255);
    FILE *f = fopen(fname, "a");
    if(f)
    {
        fprintf(f, "[%s] title: %s\n", label, title);
        fclose(f);
    }
}

static void dumpScreen(const char *fname)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if(!GetConsoleScreenBufferInfo(hOut, &csbi))
    {
        FILE *f = fopen(fname, "w");
        fprintf(f, "ERR GetConsoleScreenBufferInfo\n");
        fclose(f);
        return;
    }
    COORD size = csbi.dwSize;
    if(size.X <= 0 || size.Y <= 0 || size.X * size.Y > 60000)
    {
        FILE *f = fopen(fname, "w");
        fprintf(f, "ERR size %dx%d\n", size.X, size.Y);
        fclose(f);
        return;
    }
    CHAR_INFO *buf = (CHAR_INFO *)malloc(sizeof(CHAR_INFO) * size.X * size.Y);
    SMALL_RECT r = {0, 0, (SHORT)(size.X - 1), (SHORT)(size.Y - 1)};
    COORD z = {0, 0};
    if(!ReadConsoleOutputA(hOut, buf, size, z, &r))
    {
        FILE *f = fopen(fname, "w");
        fprintf(f, "ERR ReadConsoleOutput\n");
        fclose(f);
        free(buf);
        return;
    }
    FILE *f = fopen(fname, "w");
    fprintf(f, "console %dx%d\n", size.X, size.Y);
    fprintf(f, "=== BACKGROUND (hex: 0=black, F=white) ===\n");
    for(int y = 0; y < size.Y; y++)
    {
        for(int x = 0; x < size.X; x++)
            fprintf(f, "%X", (buf[y * size.X + x].Attributes >> 4) & 0xF);
        fputc('\n', f);
    }
    fprintf(f, "=== TEXT ===\n");
    for(int y = 0; y < size.Y; y++)
    {
        for(int x = 0; x < size.X; x++)
        {
            unsigned char c = (unsigned char)buf[y * size.X + x].Char.AsciiChar;
            fputc((c < 32 || c > 126) ? '.' : c, f);
        }
        fputc('\n', f);
    }
    fclose(f);
    free(buf);
}

int main(int argc, char **argv)
{
    int phase = (argc > 1) ? atoi(argv[1]) : 1;
    FreeConsole();
    AllocConsole();
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX cfi;
    memset(&cfi, 0, sizeof(cfi));
    cfi.cbSize = sizeof(cfi);
    cfi.dwFontSize.X = 8;
    cfi.dwFontSize.Y = 16;
    lstrcpyW(cfi.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(hOut, FALSE, &cfi);
    Sleep(300);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    char cmd[] = "beta2.0.exe";
    if(!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL,
                       "D:\\cpp\\2DMC\\beta", &si, &pi))
    {
        FILE *f = fopen("snap1.txt", "w");
        fprintf(f, "ERR CreateProcess %lu\n", GetLastError());
        fclose(f);
        return 2;
    }
    Sleep(1800);
    SetForegroundWindow(GetConsoleWindow());
    Sleep(300);
    // 主菜单：进入游戏（命中区 p.x∈[48,56] p.y∈[15,17]）
    clickCell(52, 16);
    if(phase == 1)
    {
        // 新建世界（命中区 p.x∈[40,50] p.y∈[27,29]）
        clickCell(45, 28);
        Sleep(500);
        tapKey('T', 60);              // 输入存档名 t
        Sleep(400);
        // 创建世界（命中区 p.x∈[50,60] p.y∈[18,21]）
        clickCell(55, 19);
        Sleep(4000);                  // MAP + enter_level + 渲染
        tapKey('Y', 80);              // 开F3坐标显示
        Sleep(600);
        remove("fps.txt");
        dumpTitle("start", "fps.txt");
        Sleep(1500);
        dumpTitle("start+1.5s", "fps.txt");
        dumpScreen("snap1.txt");      // 出生点画面 + 精确坐标
        // 按S下滑：穿过起始房底门→NPC房→战斗房（触发刷怪+关门）
        tapKey('S', 3200);
        Sleep(2500);                  // 等波次刷怪+怪物追上来
        dumpTitle("battle", "fps.txt");
        dumpScreen("snap2.txt");      // 战斗房画面（关门+怪物）
        // 玩家在屏幕中心(35,21)：环绕点击8邻域攻击贴身怪，验证伤害飘字
        // （0.6s寿命，须在攻击进行中截图）
        for(int cc = 0; cc < 6; cc++)
        {
            clickCell(34, 20); clickCell(35, 20); clickCell(36, 20);
            clickCell(34, 21);                clickCell(36, 21);
            clickCell(34, 22); clickCell(35, 22); clickCell(36, 22);
            Sleep(120);
        }
        dumpScreen("snap2b.txt");     // 攻击进行中：应能看到伤害数字
        tapKey('U', 120);             // 战斗中保存
        Sleep(2500);
        dumpTitle("saved", "fps.txt");
        dumpScreen("snap3.txt");
    }
    else
    {
        // 读档：存档列表条目1（存档名 t，行8-9）
        clickCell(20, 8);
        Sleep(4000);
        dumpScreen("snap4.txt");
        Sleep(1200);
        dumpScreen("snap5.txt");
    }
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
