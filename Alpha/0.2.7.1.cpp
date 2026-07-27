#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<string>
#include<fstream>
#include<time.h>
#include<cmath>
#include<cstdio>
#include<conio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)
using namespace std;
int s[1001][1001];
int light[1001][1001];
int painting[102][102][2];
char title[256];
int blockwj[101] = { 0, 200, 200, 400, 800, 100, 0, 0, 0, 200, -1, 0, 1200, 0, 0, 0, 5,-1,0, -1, -1 ,-1,-1,12,-1,0,0};
int blocktouch[101] = { 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0 ,1, 0, 0 ,0,0,0,0,0,0};
int blocklv[101] = { 0, 2, 1, 2, 3, 1, 0, 0, 0, 2, 114, 0, 4, 0, 0, 0, 1, 0, 0, 10, 10 ,10,10,15,-1,0,0};
string test[101] = { "0\b空气", "0\b圆石", "0\b原木", "0\b煤炭", "0\b粗铁", "0\b泥土", "0\b木棍", "0\b木稿", "0\b石稿", "0\b熔炉", "0\b基岩", "0\b天空", "0\b钻石", "0\b铁锭", "0\b铁稿", "0\b钻石稿", "0\b火把", "0\b石剑", "0\b石斧"," 0\b静态水19","0\b流动水3/4,","0\b流动水2/4,","0\b流动水1/4,","0\b树叶","0\b回血药水","0\b桶","0\b水桶"};
int beibao[101][2];
bool run = 1;
int bed, ed=8000,sun=1,back_x = 1, back_y = 50, Clocknum = 0, jump = 0, dx, dy, my, tall, ks = 0, mx, fz, cho = 1, key_ = 0, life = 20, bx, by, fell = 0, co = 0, H = 0, anx = 0, mpx, mpy, lx, ly, wjlv = 1, playerCD = 0, tick = 0, setfps = 1, gongji = 1, gongjicd = 20;
double jx = 1, jy = 50, x = 1, y = 50, g = 0.6, spx = 0, spy = 0, wj = 0, FIX = 1;
struct guaiwu
{
    double HP, hurt, gx, gy, spgy, spgx;
    int type, jp, CD;
};
guaiwu shiti[500];
int To_int(float a)
{
    return int(a + 0.5);
}
float Abs(float a)
{
    return (a >= 0) ? a : -a;
}
float Min(float a, float b)
{
    return (a <= b) ? a : b;
}
void Setpos(float x, float y)
{
    COORD pos;
    pos.X = To_int(x) * 2 + 1, pos.Y = To_int(y);
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}
void Color(int a)
{
    if (a == 0 || a == 8 || a == -8) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if (a == -10 || a == 1 || a == -18) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if (a == 2) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN);
    if (a == 3) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE);
    if (a == 4) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED);
    if (a == 5) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
    if (a == 6) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_BLUE);
    if (a == 7) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if (a == -1) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED | BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    if (a == -2) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_INTENSITY | BACKGROUND_GREEN | FOREGROUND_GREEN);
    if (a == -3) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_BLUE);
    if (a == -4) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_INTENSITY | BACKGROUND_RED | FOREGROUND_RED);
    if (a == -5) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN);
    if (a == -6) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_INTENSITY | BACKGROUND_BLUE);
    if (a == -7) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    if (a == -11) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    if (a == -12) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_INTENSITY | BACKGROUND_GREEN);
    if (a == -13) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE);
    if (a == -14) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_INTENSITY | BACKGROUND_RED);
    if (a == -15) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN);
    if (a == -16) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_INTENSITY | BACKGROUND_BLUE);
    if (a == -17) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    if (a == -21) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_GREEN);
    if (a == -23) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if (a == -27) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    if (a == -29) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_BLUE | BACKGROUND_BLUE);
    if (a == -31) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_BLUE | BACKGROUND_BLUE);
}
void HideCursor()
{
    CONSOLE_CURSOR_INFO cursor_info = { 1, 0 };
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}
//POINT p;
//CONSOLE_FONT_INFO consoleCurrentFont;
//HWND h=GetForegroundWindow();
//HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
//POINT GetMousePos(){
//	POINT p;
//	GetCursorPos(&p);
//	ScreenToClient(h,&p);
//	GetCurrentConsoleFont(hOutput, FALSE, &consoleCurrentFont);
//	p.x/=consoleCurrentFont.dwFontSize.X;
//	p.y/=consoleCurrentFont.dwFontSize.Y;
//	return p;
//}
POINT GetMousePos()
{
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(GetConsoleWindow(), &p);
    // MinGW 下无法使用 GetCurrentConsoleFont，使用固定字体大小估算
    const int fontWidth = 8;   // 字体宽度（像素）
    const int fontHeight = 16; // 字体高度（像素）
    p.x /= fontWidth;
    p.y /= fontHeight;
    return p;
}
void Make(int i, int j, int step)
{
    if (i < 0 || j < 0 || i > 1000 || j > 1000 || step > 8)return;
    s[i][j] = 0;
    if (rand() % 4 == 0)Make(i + 1, j, step + 1);
    if (rand() % 4 == 0)Make(i - 1, j, step + 1);
    if (rand() % 4 == 0)Make(i, j + 1, step + 1);
    if (rand() % 4 == 0)Make(i, j - 1, step + 1);
    if (rand() % 4 == 0)Make(i + 1, j + 1, step + 1);
    if (rand() % 4 == 0)Make(i + 1, j - 1, step + 1);
    if (rand() % 4 == 0)Make(i - 1, j + 1, step + 1);
    if (rand() % 4 == 0)Make(i - 1, j - 1, step + 1);
}
void Make_pro(int i, int j, int step)
{
    if (i < 0 || j < 0 || i > 1000 || j > 1000 || step > 15)return;
    s[i][j] = 0;
    if (rand() % 4 == 0)Make_pro(i + 1, j, step + 1);
    if (rand() % 4 == 0)Make_pro(i - 1, j, step + 1);
    if (rand() % 4 == 0)Make_pro(i, j + 1, step + 1);
    if (rand() % 4 == 0)Make_pro(i, j - 1, step + 1);
    if (rand() % 4 == 0)Make_pro(i + 1, j + 1, step + 1);
    if (rand() % 4 == 0)Make_pro(i + 1, j - 1, step + 1);
    if (rand() % 4 == 0)Make_pro(i - 1, j + 1, step + 1);
    if (rand() % 4 == 0)Make_pro(i - 1, j - 1, step + 1);
}
void noodle_cave(int i, int j)
{
	int move_=0;
	int len_=rand()%80+20;
	for(int uu=0;uu<=len_;uu++)
	{
		for(int kk=0;kk<=4;kk++)
		{
		if (i+uu < 0 || j+kk+move_ < 0 || i+uu > 1000 || j+kk+move_ > 1000)return;
		s[i+uu][j+kk+move_] = 0;
		}
		if(rand()%6==0)move_++;
		if(rand()%6==0)move_--;
	}
}
int night = 0, will_night = 0, will_light = 0;
int now_time = 0;
void dfs(int i, int j, int lt, int v)
{
    if (i < 0 || i > 1000 || j < 0 || j > 1000 || lt <= 0) return;
    if (blocktouch[s[i][j]] == true)v = 3;
    else v = 1;
    light[i][j] = lt;
    if (lt - v > light[i + 1][j])dfs(i + 1, j, lt - v, v);
    if (lt - v > light[i - 1][j])dfs(i - 1, j, lt - v, v);
    if (lt - v > light[i][j + 1])dfs(i, j + 1, lt - v, v);
    if (lt - v > light[i][j - 1])dfs(i, j - 1, lt - v, v);
}
void setlight()
{
    memset(light, 0, sizeof(light));
    if (night == 1)
    {
        for (int i = 0; i <= 1000; i++)
        {
            if (s[i][0] == 0 || s[i][0] == 11)light[i][0] = 4;
        }
        for (int i = 0; i <= 1000; i++)
        {
            for (int j = 0; j <= 1000; j++)
            {
                if ((s[i][j] == 0 || s[i][j] == 11) && light[i][j - 1] == 4)
                {
                    light[i][j] = 4;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i <= 1000; i++)
        {
            if (s[i][0] == 0 || s[i][0] == 11)light[i][0] = 20;
        }
        // 修复建议：在访问 light[i][j - 1] 前，确保 j > 0，避免越界访问
        for (int i = 0; i <= 1000; i++)
        {
            for (int j = 0; j <= 1000; j++)
            {
                if (j > 0 && (s[i][j] == 0 || s[i][j] == 11) && light[i][j - 1] == 20)
                {
                    light[i][j] = 20;
                }
            }
        }
    }
    for (int i = 0; i <= 1000; i++)
    {
        for (int j = 0; j <= 1000; j++)
        {
            if ((s[i][j] == 16))
            {
                dfs(i, j, 19, 1);
            }
            else
            {
                if (light[i][j] > 0)dfs(i, j, light[i][j], 1);
            }
        }
    }
}
void sky()
{
    for (int i = 0; i <= 50; i++)
    {
        for (int j = 0; j <= 50; j++)
        {
            if (painting[i][j][0] == 11)
            {
                painting[i][j][0] = 114;
            }
        }
    }
    setlight();
}
void atime(int sun)
{
	if(sun==0) return;
    now_time += 2;
    if (now_time == 18000) night = 2, sky();
    if (now_time == 20000) night = 1, sky();
    if (now_time == 38000) night = 3, sky();
    if (now_time >= 40000) night = 0, now_time = 0, sky();
}
void st(int type, int x, int y, int ra, int ra2)
{
    if (ra == 0)
    {
        s[x][y] = type;
        if (rand() % ra2 == 0)s[x + 1][y] = type;
        if (rand() % ra2 == 0)s[x - 1][y] = type;
        if (rand() % ra2 == 0)s[x + 1][y + 1] = type;
        if (rand() % ra2 == 0)s[x - 1][y - 1] = type;
        if (rand() % ra2 == 0)s[x + 1][y - 1] = type;
        if (rand() % ra2 == 0)s[x - 1][y + 1] = type;
        if (rand() % ra2 == 0)s[x][y + 1] = type;
        if (rand() % ra2 == 0)s[x][y - 1] = type;
    }
}
int water=0;
void MAP()
{
    HideCursor();
    int a, moun = 0;
    double now = 0;
    int wmax=0,wh=0;
    tall = 49;
    for (int i = 0; i <= 1000; i++)
    {
        if (rand() % 15 == 0)tall++;
        if (rand() % 15 == 1)tall--;
        if(water<=0)
        {
        	if (rand() % 120 == 0)water = 10+rand()%15,wmax=water,wh=tall+1;
		}
		else if(water>wmax/2){
			tall+=rand()%2+1;
			water--;
		}
		else {
			tall-=rand()%2+1;
			if(!(tall+1>wh&&water==1))water--;
		}
        if (moun <= 0)
        {
            if (rand() % 150 == 0&&water<=0)moun = 14;
        }
        else if (moun > 7)tall -= rand() % 2 + 1, moun--;
        else if (moun <= 7)tall += rand() % 2 + 1, moun--;
        if (tall < 0)tall = 0;
        for (int j = 1000; j >= tall + 1; j--)
        {
            s[i][j] = 1;
            if (j == 1000)s[i][j] = 10;
            if (j <= tall + 6)s[i][j] = 5;
            if (j <= tall + 7 && rand() % 2 == 0)s[i][j] = 5;
            if (rand() % 50 == 1)s[i][j] = 5;
        }
        if(water>0)
        {
        	for (int j = tall+1; j >= wh; j--)
        {
            s[i][j] = 19;

        }
		}
        if (rand() % 15 == 2&&water<=0)
        {
            a = tall;
            s[i][tall] = 2;
            tall--;
            s[i][tall] = 2;
            tall--;
            s[i][tall] = 2;
            while (rand() % 2 == 0)
            {
                tall--;
                s[i][tall] = 2;
            }
            if(rand()%2==0){s[i-1][tall-1] = 23;s[i][tall-1] = 23;s[i+1][tall-1] = 23;s[i-1][tall] = 23;s[i+1][tall] = 23;s[i-1][tall-2] = 23;s[i][tall-2] = 23;s[i+1][tall-2] = 23;}
            else {s[i-1][tall-1] = 23;s[i-2][tall-1] = 23;s[i][tall-1] = 23;s[i+1][tall-1] = 23;s[i+2][tall-1] = 23;s[i-2][tall] = 23;s[i-1][tall] = 23;s[i+1][tall] = 23;s[i+2][tall] = 23;s[i-2][tall-2] = 23;s[i-1][tall-2] = 23;s[i][tall-2] = 23;s[i+1][tall-2] = 23;s[i+2][tall-2] = 23;s[i-1][tall-3] = 23;s[i][tall-3] = 23;s[i+1][tall-3] = 23;}
            tall = a;
        }
    }
    for (int i = 5; i <= 995; i++)
    {
        Setpos(1, 1);
        printf("\b%.1lf%%", now * 1.0 / 10);
        for (int j = 995; j >= 80; j--)
        {
            st(3, i, j, rand() % (400 - int(j * 0.2)), 2);
            st(4, i, j, rand() % (600 - int(j * 0.3)), 3);
            if (j > 250)st(12, i, j, rand() % (1000 - int(j * 0.5)), 5);
        }
    }
    for (int i = 5; i <= 995; i++)
    {
        Setpos(1, 1);
        printf("\b%.1lf%%", now * 1.0 / 10);
        now++;
        for (int j = 995; j >= 80; j--)
        {
            if (rand() % 600 == 0)Make(i, j, 0);
            if (rand() % 3000 == 0)Make_pro(i, j, 0);
            if (rand()%1600==0)noodle_cave(i,j);
        }
    }
    sky();
}
void Clear()
{
    for (int i = 0; i <= 1000; i++)
    {
        for (int j = 0; j <= 70; j++)
        {
            if (s[i][j] == 0)s[i][j] = 11;
        }
    }
}
void pr_mouse()
{
    if (s[dx][dy] == 11)
    {
        if (co != -6)Color(-6), co = -6;
        printf("[]");
    }
    else if (s[dx][dy] == 0)
    {
        if (co != 0)Color(0), co = 0;
        printf("[]");
    }
    else if (s[dx - 1][dy] != 0 && s[dx + 1][dy] != 0 && s[dx][dy + 1] != 0 && s[dx][dy - 1] != 0 && s[dx + 1][dy + 1] != 0 && s[dx + 1][dy - 1] != 0 && s[dx - 1][dy + 1] != 0 && s[dx - 1][dy - 1] != 0)
    {
        if (co != -1)Color(-1), co = -1;
        printf("[]");
    }
    else if (s[dx][dy] == 1)
    {
        if (co != -7)Color(-7), co = -7;
        printf("[]");
    }
    else if (s[dx][dy] == 3)
    {
        if (co != 0)Color(0), co = 0;
        printf("[]");
    }
    else if (s[dx][dy] == 4)
    {
        if (co != -5)Color(-5), co = -5;
        printf("[]");
    }
    else if (s[dx][dy] == 2)
    {
        if (co != -5)Color(-5), co = -5;
        printf("{}");
    }
    else if (s[dx][dy] == 5)
    {
        if (co != -12)Color(-12), co = -12;
        printf("[]");
    }
    else if (s[dx][dy] == 9)
    {
        if (co != -15)Color(-15), co = -15;
        printf("[]");
    }
    else if (s[dx][dy] == 12)
    {
        if (co != -6)Color(-6), co = -6;
        printf("[]");
    }
    else if (s[dx][dy] == 20)
    {
        if (co != -4)Color(-4), co = -4;
        printf("[]");
    }
}
int jl(int aaaa, int bbbb)
{
    return sqrt((aaaa * aaaa) + (bbbb * bbbb));
}
void Updateblock()
{
    if (s[dx][dy] == 11)
    {
        if (co != -6 && night==0)Color(-6), co = -6;
        else if (co != -23 && night ==1)Color(-23), co = -23;
        else if (co != -29 && night ==2)Color(-29), co = -29;
        else if (co != -31 && night ==3)Color(-31), co = -31;
        printf("  ");
    }
    if (s[dx][dy] == 0)
    {
        if (light[dx][dy] > 0 || abs(jl(dx - To_int(x), dy - To_int(y))) < 4)
        {
            if (co != -1)Color(-1), co = -1;
            printf("  ");
        }
        else
        {
            if (co != 0)Color(0), co = 0;
            printf("  ");
        }
    }
    else if (light[dx][dy] <= 0)
    {
        if (abs(jl(dx - To_int(x), dy - To_int(y))) < 4)
        {
            if (s[dx][dy] == 1)
            {
                if (co != -7)Color(-7), co = -7;
                printf("  ");
            }
            else if (s[dx][dy] == 3)
            {
                if (co != -0)Color(-0), co = -0;
                printf("  ");
            }
            else if (s[dx][dy] == 4)
            {
                if (co != -5)Color(-5), co = -5;
                printf("  ");
            }
            else if (s[dx][dy] == 2)
            {
                if (co != -5)Color(-5), co = -5;
                printf("[]");
            }
            else if (s[dx][dy] == 5)
            {
                if (co != -12)Color(-12), co = -12;
                printf("  ");
            }
            else if (s[dx][dy] == 9)
            {
                if (co != -15)Color(-15), co = -15;
                printf("<>");
            }
            else if (s[dx][dy] == 12)
            {
                if (co != -6)Color(-6), co = -6;
                printf("  ");
            }
            else if (s[dx][dy] == 19||s[dx][dy] == 20||s[dx][dy] == 21||s[dx][dy] == 22)
    		{
        		if (co != -21)Color(-21), co = -21;
        		printf("  ");
    		}
    		else if (s[dx][dy] == 23)
    		{
        		if (co != -12)Color(-12), co = -12;
        		printf("  ");
    		}
            else if (s[dx][dy] == 20) // 床方块
            {
                if (co != -4) Color(-4), co = -4; // 红色背景
                printf("[]");
            }
        }
        else
        {
            if (co != 0)Color(0), co = 0;
            printf("  ");
        }
    }
    else if (s[dx][dy] == 1)
    {
        if (co != -7)Color(-7), co = -7;
        printf("  ");
    }
    else if (s[dx][dy] == 3)
    {
        if (co != -0)Color(-0), co = -0;
        printf("  ");
    }
    else if (s[dx][dy] == 4)
    {
        if (co != -5)Color(-5), co = -5;
        printf("  ");
    }
    else if (s[dx][dy] == 2)
    {
        if (co != -5)Color(-5), co = -5;
        printf("[]");
    }
    else if (s[dx][dy] == 5)
    {
        if (co != -12)Color(-12), co = -12;
        printf("  ");
    }
    else if (s[dx][dy] == 9)
    {
        if (co != -15)Color(-15), co = -15;
        printf("<>");
    }
    else if (s[dx][dy] == 12)
    {
        if (co != -6)Color(-6), co = -6;
        printf("  ");
    }
    else if (s[dx][dy] == 19||s[dx][dy] == 20||s[dx][dy] == 21||s[dx][dy] == 22)
    {
        if (co != -21)Color(-21), co = -21;
        printf("  ");
    }
    else if (s[dx][dy] == 23)
    {
       	if (co != -12)Color(-12), co = -12;
       	printf("  ");
    }
    else if (s[dx][dy] == 16)
    {
        if (co != -1)Color(-1), co = -1;
        if (s[dx][dy + 1] == 0 || s[dx][dy + 1] == 11)
        {
            if (!(s[dx - 1][dy] == 0 || s[dx - 1][dy] == 11))printf("/ ");
            else printf(" \\");
        }
        else printf("I ");
    }
}
bool pr = false;
void Print()
{
    co = 1;
    for (int j = 0; j <= 30; j++)
    {
        for (int i = 0; i <= 50; i++)
        {
            dx = int(jx + i - 25 + 0.5);
            dy = int(jy + j - 15 + 0.5);
            for (int k = 0; k <= 50; k++)
            {
                if (shiti[k].type > 0)
                {
                    if (light[dx][dy] <= 0&&(abs(dx - x) > 4 || abs(dy - y) > 4))continue;
                    if (To_int(shiti[k].gx) == dx && To_int(shiti[k].gy) == dy)
                    {
                        painting[i][j][0] = 17;
                        painting[i][j][1] = 17;
                        Setpos(i, j);
                        Color(1), co = 1;
                        if(shiti[k].type==1)printf("ZZ");
                        else if(shiti[k].type==2){
                        if(shiti[k].CD==0)Color(2), co = 2;
                        else Color(0), co = 0;
						printf("CR");
						}
                        pr = true;
                        break;
                    }
                }
            }
            if (pr)
            {
                pr = 0;
                continue;
            }
            if (dx == int(x + 0.5) && dy == int(y + 0.5))
            {
                painting[i][j][0] = 12;
                painting[i][j][1] = 12;
                Setpos(i, j);
                Color(1), co = 1;
                printf("{}");
            }
            else if (dx == To_int(mpx) && dy == To_int(mpy))
            {
                painting[i][j][0] = 12;
                painting[i][j][1] = 12;
                Setpos(i, j);
                Color(co);
                pr_mouse();
            }
            else if (painting[i][j][0] != s[dx][dy] ||/*(painting[i][j][1]>6)!=(light[dx][dy]>6)||*/(painting[i][j][1] > 0) != (light[dx][dy] > 0) || (abs(dx - x) < 4 && abs(dy - y) < 4))
            {
                painting[i][j][0] = s[dx][dy];
                painting[i][j][1] = light[dx][dy];
                Setpos(i, j);
                Updateblock();
            }
        }
        Setpos(0, j);
    }
}
int r;
void save()
{
    r = rand() % 10;
    FILE* fp;
    fp = fopen("save.txt", "w");
    fprintf(fp, "%d\n", r);
    for (int i = 0; i <= 1000; i++)
    {
        for (int j = 0; j < 1001; j++)
        {
            fprintf(fp, "%d ", s[i][j]);
        }
        fprintf(fp, "\n");
    }
    for (int i = 0; i < 100; i++)
    {
        fprintf(fp, "%d ", beibao[i][0]);
        fprintf(fp, "%d ", beibao[i][1]);
    }
    fprintf(fp, "%lf %lf %d %d %d", x, y,back_x,back_y,life);
    // 保存时间相关变量
    fprintf(fp, "\n%d %d %d %d", now_time, night, will_night, will_light);
    // 保存怪物数据
    for (int i = 0; i < 500; i++)
    {
        fprintf(fp, "\n%lf %lf %lf %lf %lf %lf %d %d %d %d",
                shiti[i].HP, shiti[i].hurt, shiti[i].gx, shiti[i].gy,
                shiti[i].spgy, shiti[i].spgx, shiti[i].type,
                shiti[i].jp, shiti[i].CD,sun);
    }
    fclose(fp);
}
bool read()
{
    FILE* fp;
    fp = fopen("save.txt", "r");
    if (fp == NULL)
    {
        return false;
    }
    fscanf(fp, " %d", &r);
    for (int i = 0; i <= 1000; i++)
    {
        for (int j = 0; j < 1001; j++)
        {
            fscanf(fp, " %d", &s[i][j]);
        }
    }
    for (int i = 0; i < 100; i++)
    {
        fscanf(fp, " %d", &beibao[i][0]);
        fscanf(fp, " %d", &beibao[i][1]);
    }
    fscanf(fp, " %lf %lf %d %d %d", &x, &y,&back_x,&back_y,&life);
    // 读取时间相关变量
    if (fscanf(fp, " %d %d %d %d", &now_time, &night, &will_night, &will_light) != 4)
    {
        // 如果读取失败（旧版存档），初始化时间变量
        now_time = 0;
        night = 0;
        will_night = 0;
        will_light = 0;
    }
    // 读取怪物数据
    for (int i = 0; i < 500; i++)
    {
        if (fscanf(fp, " %lf %lf %lf %lf %lf %lf %d %d %d %d",
                   &shiti[i].HP, &shiti[i].hurt, &shiti[i].gx, &shiti[i].gy,
                   &shiti[i].spgy, &shiti[i].spgx, &shiti[i].type,
                   &shiti[i].jp, &shiti[i].CD,&sun) < 9)
        {
            // 如果读取失败（旧版存档），初始化怪物数据
            shiti[i].type = 0;
            shiti[i].HP = 0;
            shiti[i].hurt = 0;
            shiti[i].gx = 0;
            shiti[i].gy = 0;
            shiti[i].spgx = 0;
            shiti[i].spgy = 0;
            shiti[i].jp = 0;
            shiti[i].CD = 0;
        }
    }
    fclose(fp);
    return true;
}
void Start()
{
    life = 20;
    // 检查重生点是否在床方块内
    if (s[back_x][back_y] == 20)
    {
        // 如果重生点在床方块内，调整位置到上方
        y = back_y - 1;
    }
    else
    {
        y = back_y;
    }
    x = back_x;
    // 重置玩家速度
    spx = 0;
    spy = 0;
    jump = 0;
}
int getbeibao(int type)
{
    for (int i = 1; i <= 100; i++)if (beibao[i][1] <= 0 || (beibao[i][0] == type && beibao[i][1] < 64))return i;
    return 0;
}
int getbeibao_2(int type)
{
    for (int i = 1; i <= 100; i++)if (beibao[i][1] > 0 && beibao[i][0] == type)return i;
    return 0;
}
int ls[101][2];
int getbeibao_3(int type, int nd)
{
    int nw = 0, m = nd;
    for (int i = 1; i <= 100; i++)
    {
        if (beibao[i][1] > 0 && beibao[i][0] == type)
        {
            nw += beibao[i][1];
            if (nw >= nd)
            {
                beibao[i][1] -= m;
                return 1;
            }
            else m -= beibao[i][1], beibao[i][1] = 0;
        }
    }
    return 0;
}
void wajue(int sx, int sy)
{
    if (blockwj[s[sx][sy]] > 0)
    {
        if (beibao[cho][0] == 15)wj += 15;
        if (beibao[cho][0] == 14)wj += 11;
        if (beibao[cho][0] == 8)wj += 8;
        if (beibao[cho][0] == 7)wj += 5;
        else wj += 3;
    }
    else return;
    if (wj > blockwj[s[sx][sy]])
    {
        wjlv = 1;
        if (beibao[cho][0] == 7)wjlv = 2;
        if (beibao[cho][0] == 8)wjlv = 3;
        if (beibao[cho][0] == 14)wjlv = 4;
        if (beibao[cho][0] == 15)wjlv = 5;
        if (wjlv >= blocklv[s[sx][sy]])
        {
            int U = getbeibao(s[sx][sy]);
            if (U != 0)beibao[U][1]++, beibao[U][0] = s[sx][sy], s[sx][sy] = 0, wj = 0;
        }
        else if (wj > blockwj[s[sx][sy]] * 6)s[sx][sy] = 0, wj = 0;
    }
    setlight();
    Clear();
}
void shuaxin()
{
    Setpos(0, 0);
    for (int i = 0; i <= 15000; i++)
    {
        std::cout << " ";
    }
    setlight();
    memset(painting, 0, sizeof(painting));
    Print();
}
void block_(int xxx, int yyy)
{	
    if (beibao[cho][0] == 6 || beibao[cho][0] == 7 || beibao[cho][0] == 8 || beibao[cho][0] == 13 || beibao[cho][0] == 14 || beibao[cho][0] == 15|| beibao[cho][0] == 18 || beibao[cho][0] == 17||beibao[cho][0] == 25)return;
	if ((s[xxx + 1][yyy] == 0 || s[xxx + 1][yyy] == 11||( s[xxx+1][yyy] >= 19&&s[xxx+1][yyy] <= 22)) && (s[xxx - 1][yyy] == 0 || s[xxx - 1][yyy] == 11||( s[xxx-1][yyy] >= 19&&s[xxx-1][yyy] <= 22)) && (s[xxx][yyy + 1] == 0 || s[xxx][yyy + 1] == 11||( s[xxx][yyy+1] >= 19&&s[xxx][yyy+1] <= 22)) && (s[xxx][yyy - 1] == 0 || s[xxx][yyy - 1] == 11||( s[xxx][yyy-1] >= 19&&s[xxx][yyy-1] <= 22)))return;
    if (beibao[cho][0] == 26){if(s[xxx][yyy]==0||s[xxx][yyy]==11){s[xxx][yyy] = 19;beibao[cho][0]=25;setlight();Clear();}return;}
	//if (beibao[cho][1] > 0 && (s[xxx][yyy] == 0 || s[xxx][yyy] == 11|| (s[xxx][yyy] >= 19&&s[xxx][yyy] <= 22)))s[xxx][yyy] = beibao[cho][0], beibao[cho][1]--;
    if (beibao[cho][1] > 0 && (s[xxx][yyy] == 0 || s[xxx][yyy] == 11|| (s[xxx][yyy] >= 19&&s[xxx][yyy] <= 22)))
    {
        s[xxx][yyy] = beibao[cho][0];
        beibao[cho][1]--;
        // 如果是放置床，设置重生点
        if (beibao[cho][0] == 20)
        {
            back_x = xxx;
            back_y = yyy;
            // 保存原来的方块
            bed = s[back_x][back_y];
            s[back_x][back_y] = 20;
        }
    }
    setlight();
    Clear();
}
void gohome()
{
    x = back_x, y = back_y;
    shuaxin();
}
void x_touch()
{
    if (blocktouch[s[int(x + 0.5)][int(y + 0.5)]] == true)
    {
        if (spx > 0)while (blocktouch[s[int(x + 0.5)][int(y + 0.5)]] == true)x -= 0.05;
        if (spx < 0)while (blocktouch[s[int(x + 0.5)][int(y + 0.5)]] == true)x += 0.05;
        spx = 0;
    }
}
void y_touch()
{
    if (blocktouch[s[int(x + 0.5)][int(y + 0.5)]] == true)
    {
    	/*if(s[int(x + 0.5)][int(y + 0.5)]>=19&&s[int(x + 0.5)][int(y + 0.5)]<=22){
		jump=0;
    	spy--;
    	fell=y;
		}*/
		//else{
        if (spy > 0)
        {
            if ((y - fell) > 10)life -= int((y - fell - 10) * 0.75);
            while (blocktouch[s[int(x + 0.5)][int(y + 0.5)]] == true)y -= 0.1;
            fell = y;
            jump = 0;
            spy = 0;
        }
        if (spy < 0)
        {
            while (blocktouch[s[int(x + 0.5)][int(y + 0.5)]] == true)y += 0.1;
            spy = 3;
            jump = 5;
        }//}
    }
}
short getnew[1001][1001];
void block_update()
{
	memset(getnew,0,sizeof(getnew));
	for(int i=0;i<=1000;i++)
	{
		for(int j=0;j<=1000;j++)
	{
		if(s[i][j]==19)
		{
			if(s[i][j+1]==0||s[i][j+1]==11)
			{
				getnew[i][j+1]=2;
			}
			if((s[i-1][j]==0||s[i-1][j]==11)&&s[i][j+1]!=19&&getnew[i][j+1]!=2)
			{
				getnew[i-1][j]=20;
			}
			if((s[i+1][j]==0||s[i+1][j]==11)&&s[i][j+1]!=19&&getnew[i][j+1]!=2)
			{
				getnew[i+1][j]=20;
			}
		}
		if(s[i][j]==20)
		{
			if(s[i][j+1]==0||s[i][j+1]==11)
			{
				getnew[i][j+1]=2;
			}
			if((s[i-1][j]==0||s[i-1][j]==11)&&s[i][j+1]!=19&&getnew[i][j+1]!=2)
			{
				getnew[i-1][j]=21;
			}
			if((s[i+1][j]==0||s[i+1][j]==11)&&s[i][j+1]!=19&&getnew[i][j+1]!=2)
			{
				getnew[i+1][j]=21;
			}
		}
		if(s[i][j]==21)
		{
			
			if(s[i][j+1]==0||s[i][j+1]==11)
			{
				getnew[i][j+1]=2;
			}
			if((s[i-1][j]==0||s[i-1][j]==11)&&s[i][j+1]!=19&&getnew[i][j+1]!=2)
			{
				getnew[i-1][j]=22;
			}
			if((s[i+1][j]==0||s[i+1][j]==11)&&s[i][j+1]!=19&&getnew[i][j+1]!=2)
			{
				getnew[i+1][j]=22;
			}
		}
		if(s[i][j]==22)
		{	
			if(s[i][j+1]==0||s[i][j+1]==11)
			{
				getnew[i][j+1]=2;
			}
		}
	}
	}
	for(int i=0;i<=1000;i++)
	{
		for(int j=0;j<=1000;j++)
	{if(getnew[i][j]>3)s[i][j]=getnew[i][j];
	if(getnew[i][j]==2)s[i][j]=19;
	}}
	Clear();
}
int l;
void make_st(int typ, int hp, int hur, int tx, int ty)
{
    l = 0;
    while (shiti[l].type != 0 && l < 50)l++;
    shiti[l].HP = hp, shiti[l].hurt = hur, shiti[l].type = typ, shiti[l].gx = tx, shiti[l].gy = ty;
    shiti[l].spgx = 0, shiti[l].spgy = 0, shiti[l].jp = 0, shiti[l].CD = 0;
}
void try_make_st()
{
    for (int i = To_int(x) - 100; i <= To_int(x) + 100; i++)
    {
        for (int j = To_int(y) - 60; j <= To_int(y) + 60; j++)
        {
            if (i <= 0 || j <= 0 || i >= 1000 || j >= 1000)continue;
            if (i >= To_int(x) - 26 && j >= To_int(y) - 16 && i <= To_int(x) + 26 && j <= To_int(y) + 16)continue;
            if (s[i][j] == 0 || s[i][j] == 11)
            {
                if (light[i][j] <= 4 && blocktouch[s[i][j + 1]] == 1)if (rand() % ed == 0){
				if(rand()%2==0)make_st(1, 20, 4, i, j);
				else make_st(2, 20, 50, i, j);
				}
            }
        }
    }
}
int dire;
int gj;
int Step;
void st_move()
{
    if (playerCD > 0)playerCD--;
    for (l = 0; l <= 100; l++)
    {
        if (shiti[l].type > 0)
        {
            gj = 0;
            if (KEY_DOWN(VK_LBUTTON) && (abs(mpx - shiti[l].gx) < 1.5 && abs(mpy - shiti[l].gy) < 1.5) && playerCD <= 0)
            {
                if (abs(x - shiti[l].gx) < 4.5 && abs(y - shiti[l].gy) < 4.5)
                {
                    if (spy > 2)shiti[l].HP -= gongji * 5, shiti[l].spgy = -8, gj = 2, playerCD = gongjicd;
                    else shiti[l].HP -= gongji, shiti[l].spgy = -6, gj = 1, playerCD = gongjicd;
                }
            }
            if (shiti[l].HP < 0 || shiti[l].gx < To_int(x) - 100 || shiti[l].gy < To_int(y) - 60 || shiti[l].gx > To_int(x) + 100 || shiti[l].gy > To_int(y) + 60 || shiti[l].gx < 0 || shiti[l].gy < 0 || shiti[l].gx > 1000 || shiti[l].gy > 1000)shiti[l].HP = 0, shiti[l].hurt = 0, shiti[l].type = 0, shiti[l].gx = 0, shiti[l].gy = 0, shiti[l].spgx = 0, shiti[l].spgy = 0;
            if (shiti[l].CD > 0)shiti[l].CD--;
            if (shiti[l].jp == 1)shiti[l].spgy = -8, shiti[l].jp = 2;
            if(s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]>=19&&s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]<=22){
            shiti[l].spgy += g;
            shiti[l].spgy *= 0.75;
            shiti[l].gy += shiti[l].spgy * 0.02;
			}
			else{
            shiti[l].spgy += g;
            shiti[l].gy += shiti[l].spgy * 0.06;}
            if (blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)
            {
                if (shiti[l].spgy >= 0)
                {
                    while (blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)shiti[l].gy -= 0.1;
                    shiti[l].jp = 0;
                    shiti[l].spgy = 0;
                }
                if (shiti[l].spgy < 0)
                {
                    while (blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)shiti[l].gy += 0.1;
                    shiti[l].spgy = 2;
                }
            }
            if (shiti[l].gx > x)shiti[l].spgx -= 0.007, dire = 1;
            else shiti[l].spgx += 0.007, dire = 0;
            if (gj == 1)shiti[l].spgx += ((dire) * 2 - 1) * 0.4;
            else if (gj == 2)shiti[l].spgx += ((dire) * 2 - 1) * 0.6;
            if(s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]>=19&&s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]<=22)shiti[l].gx += shiti[l].spgx*0.5;
            else shiti[l].gx += shiti[l].spgx;
             shiti[l].spgx *= 0.9;
            if (blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)
            {
                Step = 1;
                while (Step < 60)
                {
                    shiti[l].gx -= 0.03 * Step;
                    if (blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == false)break;
                    shiti[l].gx += 0.06 * Step;
                    if (blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == false)break;
                    shiti[l].gx -= 0.03 * Step;
                    Step++;
                }
                if (shiti[l].jp == 0)shiti[l].jp = 1;
                shiti[l].spgx = 0;
            }
            
            if(shiti[l].type==1){
            if (abs(x - shiti[l].gx) < 1 && abs(y - shiti[l].gy) < 1 && shiti[l].CD <= 0)
            {
                shiti[l].CD = 90;
                life -= shiti[l].hurt;
                // 检查玩家是否死亡
                if (life <= 0)
                {
                    Start();life = 20;spx = 0;spy = 0;jump = 0;shuaxin();
                }
            }}
            else if(shiti[l].type==2){
            	if (abs(x - shiti[l].gx) < 6 && abs(y - shiti[l].gy) < 6){
            	if(shiti[l].CD<0)shiti[l].CD--,shiti[l].spgx=0;
            	if(shiti[l].CD<-100){
            		if(jl(abs(x-shiti[l].gx),abs(y-shiti[l].gy))<=8)life -= (8-jl(abs(x-shiti[l].gx),abs(y-shiti[l].gy)))*0.05*shiti[l].hurt;
            		s[int(shiti[l].gx+0.5)][int(shiti[l].gy+0.5)]=0;
					s[int(shiti[l].gx+1.5)][int(shiti[l].gy+0.5)]=0;
            		s[int(shiti[l].gx-0.5)][int(shiti[l].gy+0.5)]=0;
            		s[int(shiti[l].gx+0.5)][int(shiti[l].gy+1.5)]=0;
            		s[int(shiti[l].gx-0.5)][int(shiti[l].gy+1.5)]=0;
            		s[int(shiti[l].gx+1.5)][int(shiti[l].gy+1.5)]=0;
            		s[int(shiti[l].gx+0.5)][int(shiti[l].gy-0.5)]=0;
            		s[int(shiti[l].gx-0.5)][int(shiti[l].gy-0.5)]=0;
            		s[int(shiti[l].gx+1.5)][int(shiti[l].gy-0.5)]=0;
            		s[int(shiti[l].gx+2.5)][int(shiti[l].gy+0.5)]=0;
            		s[int(shiti[l].gx+2.5)][int(shiti[l].gy-0.5)]=0;
            		s[int(shiti[l].gx+2.5)][int(shiti[l].gy+1.5)]=0;
            		
            		s[int(shiti[l].gx-1.5)][int(shiti[l].gy-0.5)]=0;
            		s[int(shiti[l].gx-1.5)][int(shiti[l].gy+0.5)]=0;
            		s[int(shiti[l].gx-1.5)][int(shiti[l].gy+1.5)]=0;
            		
            		s[int(shiti[l].gx+0.5)][int(shiti[l].gy+2.5)]=0;
            		s[int(shiti[l].gx-0.5)][int(shiti[l].gy+2.5)]=0;
            		s[int(shiti[l].gx+1.5)][int(shiti[l].gy+2.5)]=0;
            		
            		s[int(shiti[l].gx+0.5)][int(shiti[l].gy-1.5)]=0;
            		s[int(shiti[l].gx-0.5)][int(shiti[l].gy-1.5)]=0;
            		s[int(shiti[l].gx+1.5)][int(shiti[l].gy-1.5)]=0;
            		Clear();
            		
                	if (life <= 0)
                	{
                    Start();life = 20;spx = 0;spy = 0;jump = 0;shuaxin();
               	 	}
               	 	shiti[l].HP = 0, shiti[l].hurt = 0, shiti[l].type = 0, shiti[l].gx = 0, shiti[l].gy = 0, shiti[l].spgx = 0, shiti[l].spgy = 0;
				}
            	if(abs(x - shiti[l].gx) < 4 && abs(y - shiti[l].gy) < 4){
				if(shiti[l].CD==0)shiti[l].CD--;
            }
			}
			else shiti[l].CD=0;}
        }
    }
}
void craft_update(int num)
{
    Setpos(4, 5), Color(-7);
    printf("==================================================================================");
    for (int i = 6; i <= 24; i++)
    {
        Setpos(4, i);
        printf("|                                                                                |");
    }
    Setpos(4, 25);
    printf("==================================================================================");
    for (int j = 10; j <= 18; j += 2)
    {
        Setpos(17, j + 5);
        for (int i = (j - 10) / 2 * 5 + 1; i <= (j - 10) / 2 * 5 + 5; i++)
        {
            if (beibao[i][1] > 0)
            {
                cout << beibao[i][1] << test[beibao[i][0]] << "   ";
            }
            else cout << " 无     ";
        }
        if (num == 1)
        {
            Setpos(6, 7);
            cout << " 合成配方：";
            Setpos(6, 8);
            cout << "  1原木->2木棍";
            Setpos(6, 9);
            cout << "  2木棍+3原木->木稿";
            Setpos(6, 10);
            cout << "  2木棍+5圆石->石稿";
            Setpos(6, 11);
            cout << "  20圆石+5煤炭->熔炉";
            Setpos(6, 12);
            cout << "  2木棍+3铁锭->铁镐";
            Setpos(6, 13);
            cout << "  2木棍+3钻石->钻石镐";
            Setpos(6, 15);
            cout << "  1木棍+1煤炭->5火把";
            Setpos(6, 16);
            cout << "  1木棍+2圆石->石剑";
            Setpos(6, 17);
            cout << "  2木棍+3原石->石斧";
            Setpos(6, 18);
            cout << "  3铁锭->桶";
        }
        else if (num == 2)
        {
            Setpos(6, 7);
            cout << " 熔炼配方：";
            Setpos(6, 8);
            cout << "  1粗铁+1煤炭->1铁锭";
        }
    }
}
void movethings(int upd)
{
    int c_thing = 0, cc = 0, AN = 0;
    POINT p = GetMousePos();
    mpx = To_int(p.x / 2 - 26);
    mpy = To_int(p.y - 14);
    if (!(int((mpx + 12) / 4 + 0.5) > 5 || int((mpx + 12) / 4 + 0.5) < 1 || int(mpy / 2 * 5 + 0.5) > 21 || int(mpy / 2 * 5 + 0.5) < 0))
    {
        c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
        if (c_thing < 26 && c_thing > 0)
        {
            if (GetAsyncKeyState('Q'))
            {
                cc = c_thing;
                if (GetAsyncKeyState(VK_CONTROL))beibao[cc][1] = 0;
                else if (beibao[cc][1] > 0) beibao[cc][1] -= 1;
                craft_update(1);
                while (GetAsyncKeyState('Q') && AN < 40)AN++, Sleep(1);
            }
            else AN = 0;
            if (KEY_DOWN(VK_LBUTTON))
            {
                cc = c_thing;
                if (cc % 5 == 0)Setpos(33, (cc / 5 + 13) * 2 - 12), cout << " & ";
                else Setpos((cc % 5) * 4 + 13, (cc / 5 + 14) * 2 - 12), cout << " & ";
                while (KEY_DOWN(VK_LBUTTON))continue;
                while (!(KEY_DOWN(VK_LBUTTON)))continue;
                if (KEY_DOWN(VK_LBUTTON))
                {
                    if (cc % 5 == 0)Setpos(33, (cc / 5 + 13) * 2 - 12), cout << "   ";
                    else Setpos((cc % 5) * 4 + 13, (cc / 5 + 14) * 2 - 12), cout << "   ";
                    POINT p = GetMousePos();
                    mpx = To_int(p.x / 2 - 26);
                    mpy = To_int(p.y - 14);
                    c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
                    if (!(int((mpx + 12) / 4 + 0.5) > 5 || int((mpx + 12) / 4 + 0.5) < 1 || int(mpy / 2 * 5 + 0.5) > 21 || int(mpy / 2 * 5 + 0.5) < 0))
                    {
                        if ((cc != c_thing))
                        {
                            if (beibao[cc][0] != beibao[c_thing][0])swap(beibao[cc], beibao[c_thing]), craft_update(upd);
                            else if (beibao[cc][1] + beibao[c_thing][1] <= 64)beibao[c_thing][1] += beibao[cc][1], beibao[cc][1] = 0, craft_update(upd);
                            else beibao[cc][1] = beibao[cc][1] + beibao[c_thing][1] - 64, beibao[c_thing][1] = 64, craft_update(upd);
                        }
                    }
                }
            }
            while (KEY_DOWN(VK_LBUTTON))continue;
        }
    }
}
void SetConsoleToFullscreen()
{
    // 获取控制台窗口句柄
    HWND hwnd = GetConsoleWindow();
    // 设置为全屏
    ShowWindow(hwnd, SW_MAXIMIZE);
}
void zz(int Y, int need1, int type1, int need2, int type2, int get, int sl, int upd)
{
    if (mpy == Y && mpx < -9)
    {
        if (KEY_DOWN(VK_LBUTTON))
        {
            Setpos(5, Y + 13), cout << " ->";
            while (KEY_DOWN(VK_LBUTTON))continue;
            for (int i = 1; i <= 100; i++)
            {
                ls[i][0] = beibao[i][0];
                ls[i][1] = beibao[i][1];
            }
            int U = getbeibao_3(type1, need1);
            int PP = getbeibao_3(type2, need2);
            if (U > 0 && PP > 0)
            {
                int O = getbeibao(get);
                beibao[O][0] = get;
                beibao[O][1] += sl;
                craft_update(upd);
            }
            else
            {
                for (int i = 1; i <= 100; i++)
                {
                    beibao[i][0] = ls[i][0];
                    beibao[i][1] = ls[i][1];
                }
            }
        }
        else Setpos(5, Y + 13), cout << "-> ";
    }
    else Setpos(5, Y + 13), cout << "   ";
}
void zz2(int Y, int need1, int type1, int get, int sl, int upd)
{
    if (mpy == Y && mpx < -9)
    {
        if (KEY_DOWN(VK_LBUTTON))
        {
            Setpos(5, Y + 13), cout << " ->";
            while (KEY_DOWN(VK_LBUTTON))continue;
            for (int i = 1; i <= 100; i++)
            {
                ls[i][0] = beibao[i][0];
                ls[i][1] = beibao[i][1];
            }
            int U = getbeibao_3(type1, need1);
            if (U > 0)
            {
                int O = getbeibao(get);
                beibao[O][0] = get;
                beibao[O][1] += sl;
                craft_update(upd);
            }
            else
            {
                for (int i = 1; i <= 100; i++)
                {
                    beibao[i][0] = ls[i][0];
                    beibao[i][1] = ls[i][1];
                }
            }
        }
        else Setpos(5, Y + 13), cout << "-> ";
    }
    else Setpos(5, Y + 13), cout << "   ";
}
void craft_2()
{
    craft_update(2);
    while (1)
    {
        if (GetAsyncKeyState('E'))
        {
            if (anx == 0)
            {
                anx = 1;
                break;
            }
            anx = 1;
        }
        else anx = 0;
        movethings(2);
        zz(-5, 1, 3, 1, 4, 13, 1, 2);
    }
    setlight();
    memset(painting, 114, sizeof(painting));
}
void craft_1()
{
    craft_update(1);
    while (1)
    {
        if (GetAsyncKeyState('E'))
        {
            if (anx == 0)
            {
                anx = 1;
                break;
            }
            anx = 1;
        }
        else anx = 0;
        movethings(1);
        if (mpy == -5 && mpx < -9)
        {
            if (KEY_DOWN(VK_LBUTTON))
            {
                Setpos(5, 8), cout << " ->";
                while (KEY_DOWN(VK_LBUTTON))continue;
                int U = getbeibao_2(2);
                if (U > 0)
                {
                    beibao[U][1]--;
                    int O = getbeibao(6);
                    beibao[O][0] = 6;
                    beibao[O][1] += 2;
                    craft_update(1);
                }
            }
            else Setpos(5, 8), cout << "-> ";
        }
        else Setpos(5, 8), cout << "   ";
        zz(-4, 3, 2, 2, 6, 7, 1, 1);
        zz(-3, 5, 1, 2, 6, 8, 1, 1);
        zz(-2, 20, 1, 5, 3, 9, 1, 1);
        zz(-1, 3, 13, 2, 6, 14, 1, 1);
        zz(0, 3, 12, 2, 6, 15, 1, 1);
        zz(2, 1, 3, 1, 6, 16, 5, 1);
        zz(3, 1, 6, 2, 1, 17, 1, 1);
        zz(4, 2, 6, 3, 1, 18, 1, 1);
        zz2(5,3 , 13, 25, 1, 1);
    }
}
void SwitchMode()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ~ENABLE_QUICK_EDIT_MODE;
    SetConsoleMode(hStdin, mode);
}
LARGE_INTEGER frequency;
LARGE_INTEGER lastTime;
int frameCount = 0;
float fps = 0;
// 初始化帧率计数器
void InitFPSCounter()
{
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
}
// 更新帧率
void UpdateFPS()
{
    frameCount++;
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    float deltaTime = (float)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
    if (deltaTime >= 1.0f)
    {
        fps = frameCount / deltaTime;
        frameCount = 0;
        lastTime = currentTime;
        sprintf(title, "我的世界 - FPS: %.2f", fps);
        SetWindowTextA(GetConsoleWindow(), title);
    }
}
void update_set()
{
    Setpos(7, 5), Color(-7);
    printf("=========================================================================");
    for (int i = 6; i <= 28; i++)
    {
        Setpos(7, i);
        printf("|                                                                        |");
    }
    Setpos(7, 28);
    printf("==========================================================================");
    Setpos(23, 7);
    cout << " 设置";
    Setpos(20, 14);
    cout << " 帧率设置：";
    if (setfps == 1)cout << " 60帧";
    else if (setfps == 2)cout << " 30帧";
    else if (setfps == 3)cout << " 20帧";
    Setpos(20, 17);
    cout<<" 定格时间： ";
    if(!sun) cout<<" 开";
    else cout<<" 关";
    Setpos(21, 19);
    cout<<" 难度：";
    if(ed==8000) cout<<" 简单";
    else if(ed==4000)cout<<" 中等";
    else cout<<" 困难"; 
    Setpos(20, 21);
    cout << " 保存并退出游戏";
}
void SET()
{
    update_set();
    while (1)
    {
        if (GetAsyncKeyState(VK_ESCAPE))
        {
            if (anx == 0)
            {
                anx = 1;
                break;
            }
            anx = 1;
        }
        else anx = 0;
        POINT p = GetMousePos();
        mpx = To_int(p.x / 2 - 26);
        mpy = To_int(p.y - 14);
        if (mpy == 0 && mpx > -4 && mpx < 4)
        {
            if (KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 14), cout << " ->";
                while (KEY_DOWN(VK_LBUTTON))continue;
                if (setfps == 1)setfps = 2;
                else if(setfps==2)setfps=3;
                else setfps = 1;
                update_set();
            }
            else Setpos(18, 14), cout << "-> ";
        }
        else Setpos(18, 14), cout << "   ";
        if (mpy == 7 && mpx > -4 && mpx < 4)
        {
            if (KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 21), cout << " ->";
                while (KEY_DOWN(VK_LBUTTON))continue;
                save();
                run = false;
                return;
            }
            else Setpos(18, 21), cout << "-> ";
        }
        else Setpos(18, 21), cout << "   ";
        if (mpy == 3 && mpx > -4 && mpx < 4)
        {
            if (KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 17), cout << " ->";
                while (KEY_DOWN(VK_LBUTTON))continue;
                sun=(sun==0?1:0);
                update_set();
            }
            else Setpos(18, 17), cout << "-> ";
        }
        else Setpos(18, 17), cout << "   ";
		if (mpy == 5 && mpx > -4 && mpx < 4)
        {
            if (KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 19), cout << " ->";
                while (KEY_DOWN(VK_LBUTTON))continue;
                ed=(ed==8000?4000:(ed==4000?1000:8000));
                update_set();
            }
            else Setpos(18, 19), cout << "-> ";
        }
        else Setpos(18, 19), cout << "   ";
    }
}
void cls()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        DWORD cCharsWrieern;
        DWORD dw = csbi.dwSize.X * csbi.dwSize.Y;
        FillConsoleOutputCharacter(hConsole, ' ', dw, { 0, 0 }, &cCharsWrieern);
    }
}
bool SetFileReadOnly(const std::string& filePath, bool readOnly)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        std::cerr << "获取文件属性失败: " << GetLastError() << std::endl;
        return false;
    }
    if (readOnly)
    {
        attributes |= FILE_ATTRIBUTE_READONLY;
    }
    else
    {
        attributes &= ~FILE_ATTRIBUTE_READONLY;
    }
    if (!SetFileAttributesA(filePath.c_str(), attributes))
    {
        std::cerr << "设置文件属性失败: " << GetLastError() << std::endl;
        return false;
    }
#else
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0)
    {
        perror("获取文件状态失败");
        return false;
    }
    mode_t newMode;
    if (readOnly)
    {
        newMode = fileStat.st_mode & ~(S_IWUSR | S_IWGRP | S_IWOTH);
    }
    else
    {
        newMode = fileStat.st_mode | S_IWUSR;
    }
    if (chmod(filePath.c_str(), newMode) != 0)
    {
        perror("修改文件权限失败");
        return false;
    }
#endif
    return true;
}
void SetWindowSize(int cols, int lines)
{
    HWND console = GetConsoleWindow();
    RECT r;
    GetWindowRect(console, &r);
    MoveWindow(console, r.left, r.top, cols * 8, lines * 16, TRUE);
}
int main()
{
    try
    {
        SetConsoleToFullscreen();
        //SetWindowSize(300, 500);
        SetFileReadOnly("save.txt", false);
        HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
        if (hInput == INVALID_HANDLE_VALUE)
        {
            std::cerr << "无法获取输入句柄\n";
            return 1;
        }
        DWORD mode;
        if (!GetConsoleMode(hInput, &mode))
        {
            std::cerr << "GetConsoleMode 失败\n";
            return 1;
        }
        // 禁用 ENABLE_QUICK_EDIT_MODE（快速编辑模式）
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        hInput = GetStdHandle(STD_INPUT_HANDLE);
        if (hInput == INVALID_HANDLE_VALUE)
        {
            std::cerr << "无法获取输入句柄\n";
            return 1;
        }
        if (!GetConsoleMode(hInput, &mode))
        {
            std::cerr << "GetConsoleMode 失败\n";
            return 1;
        }
        // 禁用 ENABLE_QUICK_EDIT_MODE（快速编辑模式）
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        HWND hwnd = GetConsoleWindow();
        if (hwnd)
        {
            HMENU hMenu = GetSystemMenu(hwnd, FALSE);  // 获取系统菜单
            if (hMenu)
            {
                RemoveMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);       // 可选：移除“关闭”
                RemoveMenu(hMenu, 0, MF_BYPOSITION);             // 移除整个菜单
            }
        }
        SetConsoleMode(hInput, mode);
        InitFPSCounter();
        srand((unsigned)time(NULL));
        if (!read())
        {
            MAP();
            Start();
        }
        fell = y;
        HideCursor();
        jx = x, jy = y;
        if (jx < 25)jx = 25;
        if (jy < 20)jy = 20;
        if (jx > 1000)jx = 1000;
        if (jy > 985)jy = 985;
        Clear();
        setlight();
        memset(painting, 114, sizeof(painting));
        Print();
        run = 1;
        while (1)
        {
            if (clock() - Clocknum >= 15 * setfps)
            {
                keybd_event(VK_MENU, 0, 0, 0);                // 按下
                keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);  // 释放
                UpdateFPS();
                HideCursor();
                Clocknum = clock();
                for (int S = 1; S <= setfps; S++)
                {
                    tick++;
                    if (tick > 20)tick = 0, try_make_st();
                    atime(sun);
                    mx = 0;
                    SwitchMode();
                    gongji = 1;
                    gongjicd = 20;
                    if (beibao[cho][0] == 17)gongji = 5, gongjicd = 37.5;
                    if (beibao[cho][0] == 18)gongji = 9, gongjicd = 75;
                    st_move();
                    if (GetAsyncKeyState(VK_ESCAPE))
                    {
                        if (anx == 0)
                        {
                            anx = 1, SET();
                            if (run == 0)return 0;
                            setlight();
                            memset(painting, 114, sizeof(painting));
                            Print();
                        }
                        anx = 1;
                    }
                    else anx = 0;
                    if (GetAsyncKeyState('O') && GetAsyncKeyState('T'))
                    {
                        memset(s, 0, sizeof(s));
                        for(int AAAA=0;AAAA<=101;AAAA++)
                        {
                        	shiti[AAAA].CD=0,shiti[AAAA].HP=0,shiti[AAAA].spgx=0,shiti[AAAA].spgy=0,shiti[AAAA].gx=0,shiti[AAAA].gy=0,shiti[AAAA].hurt=0,shiti[AAAA].type=0,shiti[AAAA].jp=0;
						}
                        cls();
                        shuaxin();
                        MAP();
                        cls();
                        now_time=0;
                        night=0;
                        Clear();
                        memset(beibao, 0, sizeof(beibao));
                        back_x = 10, back_y = 25;
                        life = 20;
                        Start();
                    }
                    if (GetAsyncKeyState(VK_NUMPAD0))
                    {
                        s[back_x][back_y] = bed;
                        back_x = x;
                        back_y = y;
                        bed = s[back_x][back_y];
                        s[back_x][back_y] = 25;
                        Print();
                    }
                    if (GetAsyncKeyState('E'))
                    {
                        if (anx == 0)
                        {
                            anx = 1, craft_1();
                            setlight();
                            memset(painting, 114, sizeof(painting));
                            Print();
                        }
                        anx = 1;
                    }
                    else anx = 0;
                    if (GetAsyncKeyState('N'))shuaxin();
                    if (GetAsyncKeyState('U'))save();
                    if (GetAsyncKeyState('B'))gohome();
                    spx = 0;
                    if (GetAsyncKeyState('A') || GetAsyncKeyState(VK_LEFT))spx = -3, mx--;
                    if (GetAsyncKeyState('D') || GetAsyncKeyState(VK_RIGHT))spx = 3, mx++;
                    my = 0;
                    if (GetAsyncKeyState('W') || GetAsyncKeyState(VK_UP) || GetAsyncKeyState(' '))my++;
                    if (GetAsyncKeyState('S') || GetAsyncKeyState(VK_DOWN))my--;
                    if (GetAsyncKeyState('1'))cho = 1;
                    if (GetAsyncKeyState('2'))cho = 2;
                    if (GetAsyncKeyState('3'))cho = 3;
                    if (GetAsyncKeyState('4'))cho = 4;
                    if (GetAsyncKeyState('5'))cho = 5;
                    if (GetAsyncKeyState('6'))cho = 6;
                    if (GetAsyncKeyState('7'))cho = 7;
                    if (GetAsyncKeyState('8'))cho = 8;
                    if (GetAsyncKeyState('9'))cho = 9;
                    if(KEY_DOWN(VK_RBUTTON) && beibao[cho][0]==24){
						life+=((life+5>20)?(20-life):5);
						Sleep(100);
						beibao[cho][1]--;
					}
                    else if (KEY_DOWN(VK_RBUTTON) && beibao[cho][0]!=24)key_++;
                    else key_ = 0;
                    if (key_ > 0)key_ = -10, fz = 1;
                    else fz = 0;
                    POINT p = GetMousePos();
                    mpx = To_int(jx - 26 + p.x / 2);
                    mpy = To_int(jy - 14 + p.y);
                    if (KEY_DOWN(VK_LBUTTON))
                    {
                        if (abs(mpx - int(x + 0.5)) <= 3 && abs(mpy - int(y + 0.5)) <= 3)
                        {
                            wajue(mpx, mpy);
                        }
                        else
                        {
                            if (abs(mpx - int(x + 0.5)) > abs(mpy - int(y + 0.5)))
                            {
                                if (mpx - int(x + 0.5) > 0)
                                {
                                    if (blockwj[s[int(x + 1.5)][int(y + 0.5)]] > 0)wajue(int(x + 1.5), (y + 0.5));
                                    else if (blockwj[s[int(x + 2.5)][int(y + 0.5)]] > 0)wajue(int(x + 2.5), (y + 0.5));
                                    else if (blockwj[s[int(x + 3.5)][int(y + 0.5)]] > 0)wajue(int(x + 3.5), (y + 0.5));
                                }
                                else
                                {
                                    if (blockwj[s[int(x - 0.5)][int(y + 0.5)]] > 0)wajue(int(x - 0.5), (y + 0.5));
                                    else if (blockwj[s[int(x - 1.5)][int(y + 0.5)]] > 0)wajue(int(x - 1.5), (y + 0.5));
                                    else if (blockwj[s[int(x - 2.5)][int(y + 0.5)]] > 0)wajue(int(x - 2.5), (y + 0.5));
                                }
                            }
                            else
                            {
                                if (mpy - int(y + 0.5) > 0)
                                {
                                    if (blockwj[s[int(x + 0.5)][int(y + 1.5)]] > 0)wajue(int(x + 0.5), (y + 1.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y + 2.5)]] > 0)wajue(int(x + 0.5), (y + 2.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y + 3.5)]] > 0)wajue(int(x + 0.5), (y + 3.5));
                                }
                                else
                                {
                                    if (blockwj[s[int(x + 0.5)][int(y - 0.5)]] > 0)wajue(int(x + 0.5), (y - 0.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y - 1.5)]] > 0)wajue(int(x + 0.5), (y - 1.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y - 2.5)]] > 0)wajue(int(x + 0.5), (y - 2.5));
                                }
                            }
                        }
                    }
                    if (fz)
                    {
                        if (abs(mpx - int(x + 0.5)) <= 3 && abs(mpy - int(y + 0.5)) <= 3)
                        {
                            if (s[mpx][mpy] == 9)craft_2();
                            else if(beibao[cho][0]==25)
							{
								if(s[mpx][mpy]==19){
								beibao[cho][0]=26;
								s[mpx][mpy]=0;
								Clear();
								}
							}
							else block_(mpx, mpy);
                        }
                        else
                        {
                            if (abs(mpx - int(x + 0.5)) > abs(mpy - int(y + 0.5)))
                            {
                                if (mpx - int(x + 0.5) > 0)
                                {
                                    if (blockwj[s[int(x + 3.5)][int(y + 0.5)]] == 0)block_(int(x + 3.5), (y + 0.5));
                                    else if (blockwj[s[int(x + 2.5)][int(y + 0.5)]] == 0)block_(int(x + 2.5), (y + 0.5));
                                    else if (blockwj[s[int(x + 1.5)][int(y + 0.5)]] == 0)block_(int(x + 1.5), (y + 0.5));
                                }
                                else
                                {
                                    if (blockwj[s[int(x - 2.5)][int(y + 0.5)]] == 0)block_(int(x - 2.5), (y + 0.5));
                                    else if (blockwj[s[int(x - 1.5)][int(y + 0.5)]] == 0)block_(int(x - 1.5), (y + 0.5));
                                    else if (blockwj[s[int(x - 0.5)][int(y + 0.5)]] == 0)block_(int(x - 0.5), (y + 0.5));
                                }
                            }
                            else
                            {
                                if (mpy - int(y + 0.5) > 0)
                                {
                                    if (blockwj[s[int(x + 0.5)][int(y + 3.5)]] == 0)block_(int(x + 0.5), (y + 3.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y + 2.5)]] == 0)block_(int(x + 0.5), (y + 2.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y + 1.5)]] == 0)block_(int(x + 0.5), (y + 1.5));
                                }
                                else
                                {
                                    if (blockwj[s[int(x + 0.5)][int(y - 2.5)]] > 0)block_(int(x + 0.5), (y - 2.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y - 1.5)]] == 0)block_(int(x + 0.5), (y - 1.5));
                                    else if (blockwj[s[int(x + 0.5)][int(y - 0.5)]] == 0)block_(int(x + 0.5), (y - 0.5));
                                }
                            }
                        }
                    }
                    if(s[int(x + 0.5)][int(y + 0.5)]>=19&&s[int(x + 0.5)][int(y + 0.5)]<=22){
                    fell=y;
					if (my == 1)spy = -12;
                    if (GetAsyncKeyState(VK_SHIFT))spy = 10;
                    spy += g;
                    spy *= 0.75;
                    y += spy * 0.02;
                    y_touch();
                    y += spy * 0.02;
                    y_touch();
                    x += spx * 0.05;
                    x_touch();
					}
					else{
                    if (my == 1 && jump < 5)jump++, spy = -10;
                    else jump = 5;
                    spy += g;
                    spy *= 0.9;
                    y += spy * 0.04;
                    y_touch();
                    y += spy * 0.04;
                    y_touch();
                    x += spx * 0.08;
                    x_touch();}
                    
                    if (x < 0)x = 0;
                    if (y < 0)y = 0;
                    if (x > 1000)x = 1000;
                    if (y > 1000)gohome();
                    jx = x;
                    jy = y;
                    if (jx < 25)jx = 25;
                    if (jy < 15)jy = 15;
                    if (jx > 1000)jx = 1000;
                    if (jy > 985)jy = 985;
                }
                if (tick % 10 == 0)block_update();
                Print();
                Setpos(1, 33);
                Color(0);
                if (life <= 0)Start();
                if (life < 20)H++;
                if (H > 300)life++, H = 0;
                cout << "  生命：" << life << "      " << '\n';
                if (tick % 5 == 0)
                {
                    for (int i = 1; i <= 9; i++)
                    {
                        cout << "  ";
                        if (beibao[i][1] > 0)
                        {
                            cout << beibao[i][1] << test[beibao[i][0]] << "   ";
                        }
                        else cout << " 无  ";
                    }
                }
                Setpos(1, 35);
                cout << " 当前选择：" << cho << "        " << " x:" << x << " y:" << 1000 - y << "          " << mpx << ' ' << mpy <<"   "<<now_time<< "                ";
            }
        }
        save();
        SetFileReadOnly("save.txt", true);
        return 0;
    }
    catch (...)
    {
        cout << "error";
        exit(1);
        SetFileReadOnly("save.txt", true);
    }
}

