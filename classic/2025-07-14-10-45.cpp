
#include<bits/stdc++.h>
#include<windows.h>
#include<conio.h>
#include <thread>
#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)
using namespace std;
int s[10001][10001];
int blockwj[101] = {0, 5, 0, 10, 20, 0, 0, 0};
int beibao[100];
bool explored[1001][1001]={0};
int Clocknum = 0, jump = 0, dx, dy, my, tall, ks = 0, mx, fz, cho = 1, key_ = 0, co=0;
double jx = 25, jy = 20, x = 12, y = 55, g = 1, spx = 0, spy = 0, wj = 0;
LARGE_INTEGER frequency;
LARGE_INTEGER lastTime;
int frameCount = 0;
float fps = 0;
void InitFPSCounter() {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
}
void UpdateFPS() {
    frameCount++;
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    float deltaTime = (float)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
    if(deltaTime >= 1.0f) {
        fps = frameCount / deltaTime;
        frameCount = 0;
        lastTime = currentTime;
        char title[256];
        sprintf(title, "我的世界 - FPS: %.2f", fps);
        SetWindowText(GetConsoleWindow(), title);
    }
}
bool SetFileHiddenAndReadOnly(const char* filePath)
{
    DWORD attributes = GetFileAttributes(filePath);
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        cerr << "0\b文件不存在或无法访问" << endl;
        return false;
    }
    // 设置隐藏和只读属性
    attributes |= (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY);
    if (SetFileAttributes(filePath, attributes))
    {
        cout << "0\b文件属性设置成功" << endl;
        return true;
    }
    else
    {
        cerr << "文件属性设置失败" << std::endl;
        return false;
    }
}
POINT p;
CONSOLE_FONT_INFO consoleCurrentFont;
HWND h=GetForegroundWindow();
HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
POINT GetMousePos(){
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(h,&p);
	GetCurrentConsoleFont(hOutput, FALSE, &consoleCurrentFont);
	p.x/=consoleCurrentFont.dwFontSize.X;
	p.y/=consoleCurrentFont.dwFontSize.Y;
	return p;
}
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
}
void PrintProgress(int current, int total)
{
    int progress = (current * 100) / total;
    Setpos(0, 0);
    printf("Progress: [%-50s] %d%%\r", string(progress / 200, '=').c_str(), progress);
    fflush(stdout);
}
void HideCursor()
{
    CONSOLE_CURSOR_INFO cursor_info = {1, 0};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}
void SwitchMode(){
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin,&mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, mode);
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
/*void MAP() {
    int total = 10000;
    int current = 0;
    tall = 4900; // 调整初始高度
    vector<thread> threads;
    auto generateChunk = [&](int start, int end) {
        for (int i = start; i < end; i++) {
            if (rand() % 15 == 0) tall++;
            if (rand() % 15 == 1) tall--;
            for (int j = 10000; j >= tall + 1; j--) {
                s[i][j] = 1;
            }
            if (rand() % 15 == 2) {
                int a = tall;
                s[i][tall] = 2;
                tall--;
                s[i][tall] = 2;
                tall--;
                s[i][tall] = 2;
                while (rand() % 2 == 0) {
                    tall--;
                    s[i][tall] = 2;
                }
                tall = a;
            }
            current++;
            //PrintProgress(current, total);
        }
    };
    int chunkSize = 1000;
    for (int i = 0; i < 10000; i += chunkSize) {
        threads.emplace_back(generateChunk, i, min(i + chunkSize, 10000));
    }

    for (auto& t : threads) {
        t.join();
    }
    for (int i = 0; i < 10000; i += 100) {
        for (int j = 0; j < 10000; j += 100) {
            if (rand() % 600 == 0) Make(i, j, 0);
            if (rand() % 3000 == 0) Make_pro(i, j, 0);
            if (rand() % 400 == 0) {
                s[i][j] = 3;
                if (rand() % 4 == 0) s[i + 1][j] = 3;
                if (rand() % 4 == 0) s[i - 1][j] = 3;
                if (rand() % 4 == 0) s[i + 1][j + 1] = 3;
                if (rand() % 4 == 0) s[i - 1][j - 1] = 3;
                if (rand() % 4 == 0) s[i + 1][j - 1] = 3;
                if (rand() % 4 == 0) s[i - 1][j + 1] = 3;
                if (rand() % 4 == 0) s[i][j + 1] = 3;
                if (rand() % 4 == 0) s[i][j - 1] = 3;
            }
            if (rand() % 600 == 0) {
                s[i][j] = 4;
                if (rand() % 5 == 0) s[i + 1][j] = 4;
                if (rand() % 5 == 0) s[i - 1][j] = 4;
                if (rand() % 5 == 0) s[i + 1][j + 1] = 4;
                if (rand() % 5 == 0) s[i - 1][j - 1] = 4;
                if (rand() % 5 == 0) s[i + 1][j - 1] = 4;
                if (rand() % 5 == 0) s[i - 1][j + 1] = 4;
                if (rand() % 5 == 0) s[i][j + 1] = 4;
                if (rand() % 5 == 0) s[i][j - 1] = 4;
            }
        }
    }
}*/
/*void MAP()
{
    int total = 10000;
    int current = 0;
    tall = 4900; // 调整初始高度
    // 生成地图
    for (int i = 0; i < 10000; i += 100)   // 每次处理 100 行
    {
        for (int j = 0; j < 10000; j++)
        {
            if (rand() % 15 == 0) tall++;
            if (rand() % 15 == 1) tall--;
            for (int k = 0; k < 100; k++)   // 处理当前 100 行
            {
                int row = i + k;
                for (int col = 0; col < 10000; col++)
                {
                    if (row >= tall + 1)
                    {
                        s[row][col] = 1;
                    }
                    else if (rand() % 15 == 2)
                    {
                        s[row][col] = 2;
                    }
                }
            }
            current += 100;
            PrintProgress(current, total);
        }
    }
    // 生成特殊块
    for (int i = 0; i < 10000; i += 100)
    {
        for (int j = 0; j < 10000; j += 100)
        {
            if (rand() % 600 == 0) Make(i, j, 0);
            if (rand() % 3000 == 0) Make_pro(i, j, 0);
            if (rand() % 400 == 0)
            {
                s[i][j] = 3;
                if (rand() % 4 == 0) s[i + 1][j] = 3;
                if (rand() % 4 == 0) s[i - 1][j] = 3;
                if (rand() % 4 == 0) s[i + 1][j + 1] = 3;
                if (rand() % 4 == 0) s[i - 1][j - 1] = 3;
                if (rand() % 4 == 0) s[i + 1][j - 1] = 3;
                if (rand() % 4 == 0) s[i - 1][j + 1] = 3;
                if (rand() % 4 == 0) s[i][j + 1] = 3;
                if (rand() % 4 == 0) s[i][j - 1] = 3;
            }
            if (rand() % 600 == 0)
            {
                s[i][j] = 4;
                if (rand() % 5 == 0) s[i + 1][j] = 4;
                if (rand() % 5 == 0) s[i - 1][j] = 4;
                if (rand() % 5 == 0) s[i + 1][j + 1] = 4;
                if (rand() % 5 == 0) s[i - 1][j - 1] = 4;
                if (rand() % 5 == 0) s[i + 1][j - 1] = 4;
                if (rand() % 5 == 0) s[i - 1][j + 1] = 4;
                if (rand() % 5 == 0) s[i][j + 1] = 4;
                if (rand() % 5 == 0) s[i][j - 1] = 4;
            }
        }
    }
}*/
void MAP()
{
	int a;
	int  now=0;
	tall=49;
	for(int i=0;i<=1000;i++){
		if(rand()%15==0)tall++;
		if(rand()%15==1)tall--;
		for(int j=1000;j>=tall+1;j--)
		{
			s[i][j]=1;
		}
		if(rand()%15==2){
		a=tall;
		s[i][tall]=2;
		tall--;
		s[i][tall]=2;
		tall--;
		s[i][tall]=2;
		while(rand()%2==0)
		{
		tall--;
		s[i][tall]=2;
		}
		tall=a;
		}
	}
		Setpos(1,1);
	for(int i=5;i<=995;i++){
		HideCursor();
		printf("\b%.1lf%%                     ",now*1.0/10);
		Setpos(1,1);
		now++;
		for(int j=995;j>=60;j--)
		{
			if(rand()%600==0)Make(i,j,0);
			if(rand()%3000==0)Make_pro(i,j,0);
			if(rand()%400==0){s[i][j]=3;if(rand()%4==0)s[i+1][j]=3;if(rand()%4==0)s[i-1][j]=3;if(rand()%4==0)s[i+1][j+1]=3;if(rand()%4==0)s[i-1][j-1]=3;if(rand()%4==0)s[i+1][j-1]=3;if(rand()%4==0)s[i-1][j+1]=3;if(rand()%4==0)s[i][j+1]=3;if(rand()%4==0)s[i][j-1]=3;}
			if(rand()%600==0){s[i][j]=4;
			if(rand()%5==0)s[i+1][j]=4;
			if(rand()%5==0)s[i-1][j]=4;
			if(rand()%5==0)s[i+1][j+1]=4;
			if(rand()%5==0)s[i-1][j-1]=4;
			if(rand()%5==0)s[i+1][j-1]=4;
			if(rand()%5==0)s[i-1][j+1]=4;
			if(rand()%5==0)s[i][j+1]=4;
			if(rand()%5==0)s[i][j-1]=4;}
		}
	}
}
// 定义函数，打印游戏地图

void Print()
{
	co=1;
	Setpos(0,0);
	for(int j=0;j<=30;j++){
	for(int i=0;i<=50;i++)
	{
		dx=int(jx+i-25+0.5);
		dy=int(jy+j-15+0.5);
		if((dx==int(x+0.5)&&int(y+0.5)==dy)){
		if(co!=1)Color(1),co=1;
		printf("{}");
		continue;}
		if(s[dx][dy]==0)
		{
		if(dy<60){
		if(co!=-6)Color(-6),co=-6;
		printf("  ");
		}
		else {
		if(co!=-21)Color(-21),co=-21;
		printf("  ");
		}
		}
		else if(s[dx-1][dy]!=0&&s[dx+1][dy]!=0&&s[dx][dy+1]!=0&&s[dx][dy-1]!=0&&s[dx+1][dy+1]!=0&&s[dx+1][dy-1]!=0&&s[dx-1][dy+1]!=0&&s[dx-1][dy-1]!=0){
		if(co!=-1)Color(-1),co=-1;
		printf("  ");
		}
		else if(s[dx][dy]==1){
		if(co!=-7)Color(-7),co=-7;
		printf("  ");}
		else if(s[dx][dy]==3){
		if(co!=0)Color(0),co=0;
		printf("  ");}
		else if(s[dx][dy]==4){
		if(co!=-5)Color(-5),co=-5;
		printf("  ");}
		else if(s[dx][dy]==2){
		if(co!=-5)Color(-5),co=-5;
		printf("[]");}
		else if(s[dx][dy]==5){
		if(co!=-12)Color(-12),co=-12;
		printf("  ");}
	}
	Setpos(0,j);
}
}
int r=10;
void save()
{
    r = rand() % 10;
    FILE *fp;
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
        fprintf(fp, "%d ", beibao[i]);
    fprintf(fp, "%lf %lf", x , y );
    fclose(fp);
}
bool read()
{
    FILE *fp;
    fp = fopen("save.txt", "r");
    int r;
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
        fscanf(fp, " %d", &beibao[i]);
    }
    fscanf(fp, " %lf %lf", &x, &y);
    fclose(fp);
    return true;
}
void shuaxin()
{
	Setpos(0,0);
	for(int i=0;i<=15000;i++)
	{
		cout<<" ";
	}
}
// 添加全局缓冲区
char screenBuffer[2000][200]; // 根据你的控制台大小调整

// 初始化缓冲区
void InitBuffer() {
    for(int i = 0; i < 2000; i++) {
        for(int j = 0; j < 200; j++) {
            screenBuffer[i][j] = ' ';
        }
    }
}

// 刷新屏幕
void RefreshScreen() {
    COORD pos = {0, 0};
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(hConsole, pos);

    for(int j = 0; j < 50; j++) { // 控制台高度
        for(int i = 0; i < 100; i++) { // 控制台宽度
            putchar(screenBuffer[i][j]);
        }
        putchar('\n');
    }
}
//void DrawMiniMap() {
//    const int mapWidth = 10;
//    const int mapHeight = 7;
//    const int mapX = 70;  // 控制台右侧
//    const int mapY = 2;   // 靠近顶部
//
//    int centerX = int(x + 0.5);
//    int centerY = int(y + 0.5);
//
//    // 只更新可见区域的探索状态
//    for(int i = max(0, centerX - 5); i <= min(1000, centerX + 5); i++) {
//        for(int j = max(0, centerY - 5); j <= min(1000, centerY + 5); j++) {
//            explored[i][j] = true;
//        }
//    }
//
//    // 绘制小地图边框
//    for(int i = mapX; i <= mapX + mapWidth + 1; i++) {
//        screenBuffer[i][mapY] = '-';
//        screenBuffer[i][mapY + mapHeight + 1] = '-';
//    }
//    for(int j = mapY; j <= mapY + mapHeight + 1; j++) {
//        screenBuffer[mapX][j] = '|';
//        screenBuffer[mapX + mapWidth + 1][j] = '|';
//    }
//
//    // 绘制小地图内容
//    for(int i = 0; i < mapWidth; i++) {
//        for(int j = 0; j < mapHeight; j++) {
//            int worldX = centerX - mapWidth/2 + i;
//            int worldY = centerY - mapHeight/2 + j;
//
//            if(worldX == centerX && worldY == centerY) {
//                screenBuffer[mapX + 1 + i][mapY + 1 + j] = '@';  // 玩家位置
//            } else if(explored[worldX][worldY]) {
//                if(s[worldX][worldY] == 0) screenBuffer[mapX + 1 + i][mapY + 1 + j] = ' ';
//                else screenBuffer[mapX + 1 + i][mapY + 1 + j] = '.';  // 简化显示
//            } else {
//                screenBuffer[mapX + 1 + i][mapY + 1 + j] = ' ';  // 未探索区域
//            }
//        }
//    }
//    char coordText[20];
//    sprintf(coordText, "%d,%d", centerX, 1000 - centerY);
//    for(int i = 0; i < strlen(coordText); i++) {
//        screenBuffer[mapX + 1 + i][mapY + mapHeight + 2] = coordText[i];
//    }
//}
void gohome()
{
    x = 12, y = 55;
    shuaxin();
}
int main(){
	memset(explored, 0, sizeof(explored));
	LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    float targetFrameTime = 1.0f / 60.0f;
	Setpos(1,1);
    try
    {
        const char* filePath = "save.txt";
//        SetFileHiddenAndReadOnly(filePath);
        srand((unsigned)time(NULL));
//	cout<<"0\b开始新游戏请按1，读取存档按2";
        /*if(_kbhit())
        {
        	_getch();
        	int jio=_getch();
        	if(jio==49){
        	if(!read())
        	{
        		cout<<"\nerror!!!\n";
        		MAP();
        	}
        	else
        	{
        		MAP();
        	}
        }
        }*/
        //cout<<"\n\n\n\n\n\n\n\n";
        //if(pd())
        //MAP();
        //else if(!read())
        read();
		InitFPSCounter();
        if (r == 10)
            MAP();
        while (1){
        	InitBuffer();
			UpdateFPS();
			Sleep(16);
			SwitchMode();
            HideCursor();
            QueryPerformanceCounter(&end);
        	float frameTime = (end.QuadPart - start.QuadPart) / (float)frequency.QuadPart;
        	if (frameTime < targetFrameTime) {
        	    Sleep((DWORD)((targetFrameTime - frameTime) * 1000));
        	}
            if (clock() - Clocknum >= 10)
            {
                Clocknum = clock();
                mx = 0;
                if (GetAsyncKeyState('P'))break;
                if (GetAsyncKeyState('A'))spx--, mx--;
				if (GetAsyncKeyState('N'))shuaxin();
                if (GetAsyncKeyState('D'))spx++, mx++;
                if (KEY_DOWN(VK_LEFT))spx--, mx--;
                if (KEY_DOWN(VK_RIGHT))spx++, mx++;
                my = 0;
				if (KEY_DOWN(VK_UP))my++;
                if (GetAsyncKeyState('W'))my++;
                if (GetAsyncKeyState('S'))my--;
                if (GetAsyncKeyState('1'))cho = 1;
                if (GetAsyncKeyState('6'))cho = 6;
                if (GetAsyncKeyState('7'))cho = 7;
                if (GetAsyncKeyState('8'))cho = 8;
                if (GetAsyncKeyState('9'))cho = 9;           
		     // 返回出生点
                if (GetAsyncKeyState('B'))gohome();
                if (GetAsyncKeyState('\r') && cho == 1)ks = 1;
                else ks = 0;
                if (GetAsyncKeyState('L'))key_++;
                else key_ = -1;
                if (KEY_DOWN(VK_LBUTTON)) {
    				POINT p=GetMousePos();
    				int mpx=To_int(jx-25+p.x/2);
    				int mpy=To_int(jy-15+p.y);
    				if(abs(mpx-int(x+0.5))<=1 && abs(mpy-int(y+0.5))<=1){
        				beibao[s[mpx][mpy]]++;
        				s[mpx][mpy]=0;
    				}
				}
				printf("\nfps:%.3lf",fps);
                if (key_ > 0)key_ = -5, fz = 1;
                else fz = 0;
                if (fz)
                {
                    if (blockwj[s[int(x + 0.5)][int(y + 1.5)]] > 0 || blockwj[s[int(x + 0.5)][int(y - 0.5)]] > 0 || blockwj[s[int(x - 0.5)][int(y + 0.5)]] > 0 || blockwj[s[int(x + 0.5)][int(y + 0.5)]] > 0)
                        if (cho == 8 && beibao[1] > 0)s[int(x + 0.5)][int(y + 0.5)] = 1, beibao[1]--;
                    if (cho == 7 && beibao[2] > 0)s[int(x + 0.5)][int(y + 0.5)] = 2, beibao[2]--;
                    if (cho == 6 && beibao[4] > 0)s[int(x + 0.5)][int(y + 0.5)] = 2, beibao[4]--;
                    if (cho == 9 && beibao[3] > 0)s[int(x + 0.5)][int(y + 0.5)] = 3, beibao[3]--;
                }
                if (my == 1 && jump < 5)jump++, spy = -12;
                else jump = 5;
                if (ks == 1)
                {
                    if (s[int(x + 0.5)][int(y + 0.5)] == 2)
                    {
                        wj++;
                        if (wj > 5)
                        {
                            s[int(x + 0.5)][int(y + 0.5)] = 0, wj = 0;
                            int ff = 1;
                            beibao[2]++;
                            while (s[int(x + 0.5)][int(y + 0.5 - ff)] == 2)s[int(x + 0.5)][int(y + 0.5 - ff)] = 0, ff++, beibao[2]++;
                        }
                    }

                    if (my == -1)
                    {
                        if (blockwj[s[int(x + 0.5)][int(y + 1.5)]] > 0)wj++;
                        if (wj > blockwj[s[int(x + 0.5)][int(y + 1.5)]])beibao[s[int(x + 0.5)][int(y + 1.5)]]++, s[int(x + 0.5)][int(y + 1.5)] = 0, wj = 0;
                    }
                    else if (my == 1)
                    {
                        if (blockwj[s[int(x + 0.5)][int(y - 0.5)]] > 0)wj++;
                        if (wj > blockwj[s[int(x + 0.5)][int(y - 0.5)]])beibao[s[int(x + 0.5)][int(y - 0.5)]]++, s[int(x + 0.5)][int(y - 0.5)] = 0, wj = 0;
                    }
                    else if (mx == 1)
                    {
                        if (blockwj[s[int(x + 1.5)][int(y + 0.5)]] > 0)wj++;
                        if (wj > blockwj[s[int(x + 1.5)][int(y + 0.5)]])beibao[s[int(x + 1.5)][int(y + 0.5)]]++, s[int(x + 1.5)][int(y + 0.5)] = 0, wj = 0;
                    }
                    else if (mx == -1)
                    {
                        if (blockwj[s[int(x - 0.5)][int(y + 0.5)]] > 0)wj++;
                        if (wj > blockwj[s[int(x - 0.5)][int(y + 0.5)]])beibao[s[int(x - 0.5)][int(y + 1.5)]]++, s[int(x - 0.5)][int(y + 0.5)] = 0, wj = 0;
                    }
                }
                else wj = 0;
                spy += g;
                spx *= 0.8;
                spy *= 0.9;
                y+=spy*0.15;
	if(blockwj[s[int(x+0.5)][int(y+0.5)]]>0){
	if(spy>=0){
	while(blockwj[s[int(x+0.5)][int(y+0.5)]]>0)y-=0.1;
	jump=0;
	spy=0;
	}
	if(spy<0){
	while(blockwj[s[int(x+0.5)][int(y+0.5)]]>0)y+=0.1;
	spy=3;
	jump=5;
	}
	}
                x += spx * 0.2;
                if (blockwj[s[int(x + 0.5)][int(y + 0.5)]] > 0)
                {
                    if (spx > 0)while (blockwj[s[int(x + 0.5)][int(y + 0.5)]] > 0)x -= 0.1;
                    if (spx < 0)while (blockwj[s[int(x + 0.5)][int(y + 0.5)]] > 0)x += 0.1;
                    spx = 0;
                }
                jx = x;
                jy = y;
                if (jx < 25)jx = 25;
                if (jy < 20)jy = 20;
                if (jx > 1000)jx = 1000;
                if (jy > 985)jy = 985;
                Print();
                //Setpos(110, 1);
//                DrawMiniMap();
                Setpos(1, 33);
                Color(0);
                cout << " 1.木稿" << ' ' << " 6.铁矿石：" << beibao[4] << ' ' << " 7.木材：" << beibao[2] << ' ' << " 8.石头：" << beibao[1] << ' ' << " 9.煤炭：" << beibao[3] << ' ';
                cout << " 当前选择：" << cho << "        " << " x:" << x << " y:" << 1000 - y << "      ";
            }
        }
        save();
//        SetFileHiddenAndReadOnly(filePath);
        return 0;
    }
    catch (...)
    {
        cout << "error";
        exit(1);
    }
}

