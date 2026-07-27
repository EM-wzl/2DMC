#include<bits/stdc++.h>
#include<windows.h> 
#include<conio.h>
#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)
using namespace std;
int s[1001][1001];
int blockwj[101]={0,50,1,100,200,30,0,0,0,0,-1,0,0};
int beibao[101][2];
int Clocknum=0,jump=0,dx,dy,my,tall,ks=0,mx,fz,cho=1,key_=0,life=20,bx,by,fell=0,co=0,H=0,anx=0,mpx,mpy;
double jx=1,jy=50,x=1,y=50,g=2,spx=0,spy=0,wj=0;
int To_int(float a){return int(a+0.5);}float Abs(float a){return (a>=0)?a:-a;}float Min(float a,float b){return (a<=b)?a:b;}
void Setpos(float x,float y){COORD pos;pos.X=To_int(x)*2+1,pos.Y=To_int(y);SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE),pos);}
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
void HideCursor(){
 	CONSOLE_CURSOR_INFO cursor_info={1,0};
 	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&cursor_info);
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
void Make(int i,int j,int step)
{
	if(i<0||j<0||i>1000||j>1000||step>8)return;
	s[i][j]=0;if(rand()%4==0)Make(i+1,j,step+1);if(rand()%4==0)Make(i-1,j,step+1);if(rand()%4==0)Make(i,j+1,step+1);if(rand()%4==0)Make(i,j-1,step+1);if(rand()%4==0)Make(i+1,j+1,step+1);if(rand()%4==0)Make(i+1,j-1,step+1);if(rand()%4==0)Make(i-1,j+1,step+1);if(rand()%4==0)Make(i-1,j-1,step+1);
}
void Make_pro(int i,int j,int step)
{
	if(i<0||j<0||i>1000||j>1000||step>15)return;
	s[i][j]=0;if(rand()%4==0)Make_pro(i+1,j,step+1);if(rand()%4==0)Make_pro(i-1,j,step+1);if(rand()%4==0)Make_pro(i,j+1,step+1);if(rand()%4==0)Make_pro(i,j-1,step+1);if(rand()%4==0)Make_pro(i+1,j+1,step+1);if(rand()%4==0)Make_pro(i+1,j-1,step+1);if(rand()%4==0)Make_pro(i-1,j+1,step+1);if(rand()%4==0)Make_pro(i-1,j-1,step+1);
}
bool night=0,will_night=0,will_light=0;
int now_time=0;
void time(){
	now_time++;
	if(now_time>=540) will_night=1;
	if(now_time>=720) night=1,will_night=0;
	if(now_time>=1260) will_light=1;
	if(now_time>=1440) night=0,now_time=0,will_light=0;
	Sleep(1);
}
void MAP()
{
	int a,moun=0;
	double now=0;
	tall=49;
	for(int i=0;i<=1000;i++){
		if(rand()%15==0)tall++;
		if(rand()%15==1)tall--;
		if(moun<=0){
		if(rand()%150==0)moun=14;}
		else if(moun>7)tall-=rand()%2+1,moun--;
		else if(moun<=7)tall+=rand()%2+1,moun--;
		if(tall<0)tall=0;
		for(int j=1000;j>=tall+1;j--)
		{
			s[i][j]=1;
			if(j==999)s[i][j]=10;
			if(j<=tall+6)s[i][j]=5;
			if(j<=tall+7&&rand()%2==0)s[i][j]=5;
			if(rand()%50==1)s[i][j]=5;
			
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
	for(int i=5;i<=995;i++){
		HideCursor();
		printf("\b%.1lf%%",now*1.0/10); 
		Setpos(0,-1);
		now++;
		for(int j=995;j>=60;j--)
		{
			if(rand()%600==0)Make(i,j,0);
			if(rand()%3000==0)Make_pro(i,j,0);
			if(rand()%(400-int(j*0.2))==0){s[i][j]=3;if(rand()%4==0)s[i+1][j]=3;if(rand()%4==0)s[i-1][j]=3;if(rand()%4==0)s[i+1][j+1]=3;if(rand()%4==0)s[i-1][j-1]=3;if(rand()%4==0)s[i+1][j-1]=3;if(rand()%4==0)s[i-1][j+1]=3;if(rand()%4==0)s[i][j+1]=3;if(rand()%4==0)s[i][j-1]=3;}
			if(rand()%(600-int(j*0.3))==0){s[i][j]=4;
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
void pr_mouse()
{
	if(s[dx][dy]==0)
	{
	if(dy<60){
	if(co!=-6)Color(-6),co=-6;
	printf("[]");
	}
	else {
	if(co!=0)Color(0),co=0;
	printf("[]");
	}
	}
	else if(s[dx-1][dy]!=0&&s[dx+1][dy]!=0&&s[dx][dy+1]!=0&&s[dx][dy-1]!=0&&s[dx+1][dy+1]!=0&&s[dx+1][dy-1]!=0&&s[dx-1][dy+1]!=0&&s[dx-1][dy-1]!=0){
	if(co!=-1)Color(-1),co=-1;
	printf("[]");
	}
	else if(s[dx][dy]==1){
	if(co!=-7)Color(-7),co=-7;
	printf("[]");}
	else if(s[dx][dy]==3){
	if(co!=0)Color(0),co=0;
	printf("[]");}
	else if(s[dx][dy]==4){
	if(co!=-5)Color(-5),co=-5;
	printf("[]");}
	else if(s[dx][dy]==2){
	if(co!=-5)Color(-5),co=-5;
	printf("{}");}
	else if(s[dx][dy]==5){
	if(co!=-12)Color(-12),co=-12;
	printf("[]");}
	else if(s[dx][dy]==9){
		if(co!=-15)Color(-15),co=-15;
		printf("[]");}
}
void Print()
{
	co=1;
	Setpos(0,0);
	for(int j=0;j<=30;j++){
	for(int i=0;i<=50;i++)
	{
		dx=int(jx+i-25+0.5);
		dy=int(jy+j-15+0.5);
		if(dx==To_int(mpx)&&dy==To_int(mpy)){Color(co);pr_mouse();
		continue;}
		if((dx==int(x+0.5)&&int(y+0.5)==dy)){
		if(co!=1)Color(1),co=1;
		printf("{}");
		continue;}
		if(s[dx][dy]==0)
		{
		if(dy<60){
		if(co!=-6 && !night && !will_light && !will_night)Color(-6),co=-6;
		else if(co!=-23 && night && !will_night && !will_light)Color(-23),co=-23;
		else if(co!=-29 && !night && will_night && !will_light)Color(-29),co=-29;
		else if(co!=-31 && night && will_light && !will_night)Color(-31),co=-31;
		printf("  ");
		}
		else {
		if(co!=-21 && !night)Color(-21),co=-21;
		if(co!=-27 && night)Color(-27),co=-27;
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
		if(co!=-1)Color(-1),co=-1;
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
		else if(s[dx][dy]==9){
		if(co!=-15)Color(-15),co=-15;
		printf("<>");}
	}
	Setpos(0,j);
}
	
}
int r=10;
void save()
{
	r=rand()%10;
	FILE *fp;
	fp=fopen("save.txt","w");
	fprintf(fp,"%d\n",r);
	for(int i=0;i<=1000;i++)
	{
	for(int j=0;j<1001;j++)
	{
		fprintf(fp,"%d ",s[i][j]);
	}
		fprintf(fp,"\n");
	}
	for(int i=0;i<100;i++){
	fprintf(fp,"%d ",beibao[i][0]);
	fprintf(fp,"%d ",beibao[i][1]);}
	fprintf(fp,"%lf %lf %d %d",x,y,life,now_time);
	fclose(fp);
}
bool read()
{
	FILE *fp;
	fp=fopen("save.txt","r");
	fscanf(fp," %d",&r);
	for(int i=0;i<=1000;i++)
	{
	for(int j=0;j<1001;j++)
	{
		fscanf(fp," %d",&s[i][j]);
	}
	}
	for(int i=0;i<100;i++){
	fscanf(fp," %d",&beibao[i][0]);
	fscanf(fp," %d",&beibao[i][1]);
	}
	fscanf(fp," %lf %lf %d %d",&x,&y,&life,&now_time);
	fclose(fp);
	return true;
}
void cls()
{
	HANDLE hConsole=GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if(GetConsoleScreenBufferInfo(hConsole,&csbi))
	{
		DWORD cCharsWrieern;
		DWORD dw=csbi.dwSize.X*csbi.dwSize.Y;
		FillConsoleOutputCharacter(hConsole,' ',dw,{0,0},&cCharsWrieern);
	}
}
int getbeibao(int type)
{for(int i=1;i<=100;i++)if(beibao[i][1]<=0||(beibao[i][0]==type&&beibao[i][1]<64))return i;return 0;}
int getbeibao_2(int type)
{for(int i=1;i<=100;i++)if(beibao[i][1]>0&&beibao[i][0]==type)return i;return 0;}
int getbeibao_3(int type,int nd)
{
	int ls[101][2];
	for(int i=1;i<=100;i++)
	{
		ls[i][0]=beibao[i][0];
		ls[i][1]=beibao[i][1];
	}
	int nw=0,m=nd;
	for(int i=1;i<=100;i++){
	if(beibao[i][1]>0&&beibao[i][0]==type){
	nw+=beibao[i][1];
	if(nw>=nd){
	beibao[i][1]-=m;
	return 1;}
	else m-=beibao[i][1],beibao[i][1]=0;
	}
	}
	for(int i=1;i<=100;i++)
	{
		beibao[i][0]=ls[i][0];
		beibao[i][1]=ls[i][1];
	}
	return 0;
}
void wajue(int sx,int sy)
{
	if(blockwj[s[sx][sy]]>0||s[sx][sy]==2){
	if(beibao[cho][0]==8)wj+=9;
	if(beibao[cho][0]==7)wj+=6;
	else wj+=4;}
	else return;
	if(s[sx][sy]==2){
	if(wj>40){int U=getbeibao(s[sx][sy]);
	if(U!=0)beibao[U][1]++,beibao[U][0]=s[sx][sy],s[sx][sy]=0,wj=0;}
	return;}
	if(wj>blockwj[s[sx][sy]])
	{
	if(!((s[sx][sy]==1||s[sx][sy]==3||s[sx][sy]==4)&&(beibao[cho][0]!=7&&beibao[cho][0]!=8))){
	int U=getbeibao(s[sx][sy]);
	if(U!=0)beibao[U][1]++,beibao[U][0]=s[sx][sy],s[sx][sy]=0,wj=0;}
	}
}
void shuaxin()
{
	Setpos(0,0);
	for(int i=0;i<=15000;i++)
	{
		cout<<" ";
	}
}
void block_(int xxx,int yyy)
{
	if(beibao[cho][0]==6||beibao[cho][0]==7||beibao[cho][0]==8)return;
	if(s[xxx+1][yyy]==0&&s[xxx-1][yyy]==0&&s[xxx][yyy+1]==0&&s[xxx][yyy-1]==0)return;
	if(beibao[cho][1]>0&&s[xxx][yyy]==0)s[xxx][yyy]=beibao[cho][0],beibao[cho][1]--;
}
void Start()
{
	life=20,x=1,y=50;
}
void gohome()
{
	x=1,y=50;
	shuaxin();
}
void y_touch()
{
	if(blockwj[s[int(x+0.5)][int(y+0.5)]]>1||blockwj[s[int(x+0.5)][int(y+0.5)]]==-1){
	if(spy>0){
	if((y-fell)>10)life-=int((y-fell-10)*0.75);
	while(blockwj[s[int(x+0.5)][int(y+0.5)]]>1)y-=0.1;
	fell=y;
	jump=0;
	spy=0;
	}
	if(spy<0){
	while(blockwj[s[int(x+0.5)][int(y+0.5)]]>1)y+=0.1;
	spy=3;
	jump=5;
	}
	}
}
void craft_update()
{
	Setpos(5,5),Color(-7);
	printf("================================================================================");
	for(int i=6;i<=24;i++){
	Setpos(5,i);
	printf("|                                                                              |");}
	Setpos(5,25);
	printf("================================================================================");
	for(int j=10;j<=18;j+=2){
	Setpos(17,j+5);
	for(int i=(j-10)/2*5+1;i<=(j-10)/2*5+5;i++)
	{
		if(beibao[i][1]>0){
		if(beibao[i][0]==1)cout<<beibao[i][1]<<"0\bÔ²Ê¯"<<"   ";
		if(beibao[i][0]==2)cout<<beibao[i][1]<<"0\bÔ­Ä¾"<<"   ";
		if(beibao[i][0]==3)cout<<beibao[i][1]<<"0\bÃºÌ¿"<<"   ";
		if(beibao[i][0]==4)cout<<beibao[i][1]<<"0\b´ÖÌú"<<"   ";
		if(beibao[i][0]==5)cout<<beibao[i][1]<<"0\bÄàÍÁ"<<"   ";
		if(beibao[i][0]==6)cout<<beibao[i][1]<<"0\bÄ¾¹÷"<<"   ";
		if(beibao[i][0]==7)cout<<beibao[i][1]<<"0\bÄ¾¸å"<<"   ";
		if(beibao[i][0]==8)cout<<beibao[i][1]<<"0\bÊ¯¸å"<<"   ";
		if(beibao[i][0]==9)cout<<beibao[i][1]<<"0\bÈÛÂ¯"<<"   ";}
		else cout<<" ÎÞ     ";
	}
	Setpos(7,7);cout<<" ºÏ³ÉÅä·½£º";
	Setpos(7,8);cout<<"  1Ô­Ä¾->2Ä¾¹÷";
	Setpos(7,9);cout<<"  2Ä¾¹÷+3Ô­Ä¾->Ä¾¸å";
	Setpos(7,10);cout<<"  2Ä¾¹÷+5Ô²Ê¯->Ê¯¸å";
	Setpos(7,11);cout<<"  20Ô²Ê¯+5ÃºÌ¿->ÈÛÂ¯";
	}
	
}
void craft_2()
{
	craft_update();
	while(1)
	{
		if(GetAsyncKeyState('E')){
		if(anx==0){anx=1;break;}
		anx=1;}
		else anx=0;
	}
}
void craft_1()
{
	int c_thing=0,cc=0;
	craft_update();
	while(1)
	{
		if(GetAsyncKeyState('E')){
		if(anx==0){anx=1;break;}
		anx=1;}
		else anx=0;
		POINT p=GetMousePos();
		mpx=To_int(p.x/2-26);
    	mpy=To_int(p.y-14);
    	if(!(int((mpx+12)/4+0.5)>5||int((mpx+12)/4+0.5)<1||int(mpy/2*5+0.5)>21||int(mpy/2*5+0.5)<0)){
    	c_thing=int((mpx+12)/4+0.5)+int(mpy/2*5+0.5);
    	if(c_thing<26&&c_thing>0){
    		if(GetAsyncKeyState('Q'))
			{
				cc=c_thing;
				if(GetAsyncKeyState(VK_SHIFT))beibao[cc][1]=0;
				else if(beibao[cc][1]>0) beibao[cc][1]-=1;
				craft_update();
				while(GetAsyncKeyState('Q'))continue;
			}
    		if(KEY_DOWN(VK_LBUTTON)){
			cc=c_thing;
			if(cc%5==0)Setpos(33,(cc/5+13)*2-12),cout<<" & ";
    		else Setpos((cc%5)*4+13,(cc/5+14)*2-12),cout<<" & ";
    		while(KEY_DOWN(VK_LBUTTON))continue;
			while(!(KEY_DOWN(VK_LBUTTON)))continue;
			if(KEY_DOWN(VK_LBUTTON))
			{
			if(cc%5==0)Setpos(33,(cc/5+13)*2-12),cout<<"   ";
    		else Setpos((cc%5)*4+13,(cc/5+14)*2-12),cout<<"   ";
    		POINT p=GetMousePos();
			mpx=To_int(p.x/2-26);
    		mpy=To_int(p.y-14);
    		c_thing=int((mpx+12)/4+0.5)+int(mpy/2*5+0.5);
    		if(!(int((mpx+12)/4+0.5)>5||int((mpx+12)/4+0.5)<1||int(mpy/2*5+0.5)>21||int(mpy/2*5+0.5)<0)){
			if(beibao[cc][0]!=beibao[c_thing][0])swap(beibao[cc],beibao[c_thing]),craft_update();
			else if(beibao[cc][1]+beibao[c_thing][1]<=64)beibao[c_thing][1]+=beibao[cc][1],beibao[cc][1]=0,craft_update();
			else beibao[cc][1]=beibao[cc][1]+beibao[c_thing][1]-64,beibao[c_thing][1]=64,craft_update();
			}
			}
			}
			while(KEY_DOWN(VK_LBUTTON))continue;
			
		}}
    	if(mpy==-5){
    	if (KEY_DOWN(VK_LBUTTON)){
    	Setpos(6,8),cout<<" ->";
    	while(KEY_DOWN(VK_LBUTTON))continue;
		int U=getbeibao_2(2);
    	if(U>0){
		beibao[U][1]--;
		int O=getbeibao(6);
		beibao[O][0]=6;beibao[O][1]+=2;craft_update();
		}
		}
		else Setpos(6,8),cout<<"-> ";
		}
    	else Setpos(6,8),cout<<"   ";
    	if(mpy==-4){
		if (KEY_DOWN(VK_LBUTTON)){
		Setpos(6,9),cout<<" ->";
		while(KEY_DOWN(VK_LBUTTON))continue;
		int U=getbeibao_3(2,3);
		int PP=getbeibao_3(6,2);
    	if(U>0&&PP>0){
		int O=getbeibao(7);
		beibao[O][0]=7;beibao[O][1]+=1;craft_update();
		}
		}
		else Setpos(6,9),cout<<"-> ";
		}
    	else Setpos(6,9),cout<<"   ";
    	if(mpy==-3)
		{
		if (KEY_DOWN(VK_LBUTTON)){
		Setpos(6,10),cout<<" ->";
		while(KEY_DOWN(VK_LBUTTON))continue;
		int U=getbeibao_3(1,5);
		int PP=getbeibao_3(6,2);
    	if(U>0&&PP>0){
		int O=getbeibao(8);
		beibao[O][0]=8;beibao[O][1]+=1;craft_update();
		}
		}
		else Setpos(6,10),cout<<"-> ";
	}
    	else Setpos(6,10),cout<<"   ";
    	if(mpy==-2){
		if (KEY_DOWN(VK_LBUTTON)){
		Setpos(6,11),cout<<" ->";
		while(KEY_DOWN(VK_LBUTTON))continue;
		int U=getbeibao_3(1,20);
		int PP=getbeibao_3(3,5);
    	if(U>0&&PP>0){
		int O=getbeibao(9);
		beibao[O][0]=9;beibao[O][1]+=1;craft_update();
		}
		}
		else Setpos(6,11),cout<<"-> ";
		}
		else  Setpos(6,11),cout<<"   ";
    	Setpos(7,15);
    	cout<<mpx<<"   "<<mpy<<"  "<<mpy/2*5+0.5<<"    ";
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
// ³õÊ¼»¯Ö¡ÂÊ¼ÆÊýÆ÷
void InitFPSCounter()
{
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
}
// ¸üÐÂÖ¡ÂÊ
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
        char title[256];
        sprintf(title, "ÎÒµÄÊÀ½ç - FPS: %.2f", fps);
        SetWindowText(GetConsoleWindow(), title);
    }
}
int main()
{
	try{
		InitFPSCounter();
	srand((unsigned)time(NULL));
	read();
	if(r==10){
	MAP();
	Start();}
	shuaxin();
	fell=y;
	while(1)
	{HideCursor();UpdateFPS();time();
	if(clock()-Clocknum>=10){
	Clocknum=clock();
	mx=0;SwitchMode();
	if(GetAsyncKeyState('E')){
		if(anx==0)anx=1,craft_1();
		anx=1;}
		else anx=0;
	if(GetAsyncKeyState('P'))break;
	if(GetAsyncKeyState('N'))shuaxin();
	if(GetAsyncKeyState('U'))save();
	if(GetAsyncKeyState('B'))gohome();
	spx=0;
	if(GetAsyncKeyState('A')||GetAsyncKeyState(VK_LEFT))spx=-4,mx--;
	if(GetAsyncKeyState('D')||GetAsyncKeyState(VK_RIGHT))spx=4,mx++;
	my=0;
	if(GetAsyncKeyState('W')||GetAsyncKeyState(VK_UP)||GetAsyncKeyState(' '))my++;
	if(GetAsyncKeyState('S')||GetAsyncKeyState(VK_DOWN))my--;
	if(GetAsyncKeyState('1'))cho=1;
	if(GetAsyncKeyState('2'))cho=2;
	if(GetAsyncKeyState('3'))cho=3;
	if(GetAsyncKeyState('4'))cho=4;
	if(GetAsyncKeyState('5'))cho=5;
	if(GetAsyncKeyState('6'))cho=6;
	if(GetAsyncKeyState('7'))cho=7;
	if(GetAsyncKeyState('8'))cho=8;
	if(GetAsyncKeyState('9'))cho=9;
	if(KEY_DOWN(VK_RBUTTON))key_++;
	else key_=0;
	if(key_>0)key_=-3,fz=1;
	else fz=0;
	POINT p=GetMousePos();
    mpx=To_int(jx-26+p.x/2);
    mpy=To_int(jy-14+p.y);
	if (KEY_DOWN(VK_LBUTTON)) {
    		if(abs(mpx-int(x+0.5))<=3 && abs(mpy-int(y+0.5))<=3){
    			wajue(mpx,mpy);
  			}
  			else {
  			if(abs(mpx-int(x+0.5))>abs(mpy-int(y+0.5)))
  			{
  				if(mpx-int(x+0.5)>0){
  				if(blockwj[s[int(x+1.5)][int(y+0.5)]]>0)wajue(int(x+1.5),(y+0.5));
  				else if(blockwj[s[int(x+2.5)][int(y+0.5)]]>0)wajue(int(x+2.5),(y+0.5));
  				else if(blockwj[s[int(x+3.5)][int(y+0.5)]]>0)wajue(int(x+3.5),(y+0.5));
  				}
  				else{
  				if(blockwj[s[int(x-0.5)][int(y+0.5)]]>0)wajue(int(x-0.5),(y+0.5));
  				else if(blockwj[s[int(x-1.5)][int(y+0.5)]]>0)wajue(int(x-1.5),(y+0.5));
  				else if(blockwj[s[int(x-2.5)][int(y+0.5)]]>0)wajue(int(x-2.5),(y+0.5));
  				}
			}
			else {
				if(mpy-int(y+0.5)>0){
  				if(blockwj[s[int(x+0.5)][int(y+1.5)]]>0)wajue(int(x+0.5),(y+1.5));
  				else if(blockwj[s[int(x+0.5)][int(y+2.5)]]>0)wajue(int(x+0.5),(y+2.5));
  				else if(blockwj[s[int(x+0.5)][int(y+3.5)]]>0)wajue(int(x+0.5),(y+3.5));
  				}
  				else{
  				if(blockwj[s[int(x+0.5)][int(y-0.5)]]>0)wajue(int(x+0.5),(y-0.5));
  				else if(blockwj[s[int(x+0.5)][int(y-1.5)]]>0)wajue(int(x+0.5),(y-1.5));
  				else if(blockwj[s[int(x+0.5)][int(y-2.5)]]>0)wajue(int(x+0.5),(y-2.5));
  				}
			}
			  }
		}
	
	if (fz) {
    		if(abs(mpx-int(x+0.5))<=3 && abs(mpy-int(y+0.5))<=3){
    			if(s[mpx][mpy]==9)craft_2();
    			else block_(mpx,mpy);
  			}
  			else {
  			if(abs(mpx-int(x+0.5))>abs(mpy-int(y+0.5)))
  			{
  				if(mpx-int(x+0.5)>0){
  				if(blockwj[s[int(x+3.5)][int(y+0.5)]]==0)block_(int(x+3.5),(y+0.5));
  				else if(blockwj[s[int(x+2.5)][int(y+0.5)]]==0)block_(int(x+2.5),(y+0.5));
  				else if(blockwj[s[int(x+1.5)][int(y+0.5)]]==0)block_(int(x+1.5),(y+0.5));
  				}
  				else{
  				if(blockwj[s[int(x-2.5)][int(y+0.5)]]==0)block_(int(x-2.5),(y+0.5));
  				else if(blockwj[s[int(x-1.5)][int(y+0.5)]]==0)block_(int(x-1.5),(y+0.5));
  				else if(blockwj[s[int(x-0.5)][int(y+0.5)]]==0)block_(int(x-0.5),(y+0.5));
  				}
			}
			else {
				if(mpy-int(y+0.5)>0){
  				if(blockwj[s[int(x+0.5)][int(y+3.5)]]==0)block_(int(x+0.5),(y+3.5));
  				else if(blockwj[s[int(x+0.5)][int(y+2.5)]]==0)block_(int(x+0.5),(y+2.5));
  				else if(blockwj[s[int(x+0.5)][int(y+1.5)]]==0)block_(int(x+0.5),(y+1.5));
  				}
  				else{
  				if(blockwj[s[int(x+0.5)][int(y-2.5)]]>0)block_(int(x+0.5),(y-2.5));
  				else if(blockwj[s[int(x+0.5)][int(y-1.5)]]==0)block_(int(x+0.5),(y-1.5));
  				else if(blockwj[s[int(x+0.5)][int(y-0.5)]]==0)block_(int(x+0.5),(y-0.5));
  				}
			}
			  }
		}
	
	
	/*bx=bool(GetAsyncKeyState('K'))-bool(GetAsyncKeyState('H'));
	by=bool(GetAsyncKeyState('U'))-bool(GetAsyncKeyState('J'));
	if(bx==1){
	if(s[int(x+1.5)][int(y+1.5)]==0&&(blockwj[s[int(x+2.5)][int(y+1.5)]]>0||blockwj[s[int(x+0.5)][int(y+1.5)]]>0||blockwj[s[int(x+1.5)][int(y+0.5)]]>0||blockwj[s[int(x+1.5)][int(y+2.5)]]>0)){
	s[int(x+1.5)][int(y+1.5)]=1;}
	else if(s[int(x+2.5)][int(y+1.5)]==0&&(blockwj[s[int(x+3.5)][int(y+1.5)]]>0||blockwj[s[int(x+1.5)][int(y+1.5)]]>0||blockwj[s[int(x+2.5)][int(y+0.5)]]>0||blockwj[s[int(x+2.5)][int(y+2.5)]]>0)){
	s[int(x+2.5)][int(y+1.5)]=1;}
	else if(s[int(x+3.5)][int(y-0.5)]==0&&(blockwj[s[int(x+4.5)][int(y+1.5)]]>0||blockwj[s[int(x+2.5)][int(y+1.5)]]>0||blockwj[s[int(x+3.5)][int(y+0.5)]]>0||blockwj[s[int(x+3.5)][int(y+2.5)]]>0)){
	s[int(x+3.5)][int(y+1.5)]=1;}}
	else if(bx==-1){
	if(s[int(x-0.5)][int(y+1.5)]==0&&(blockwj[s[int(x+0.5)][int(y+1.5)]]>0||blockwj[s[int(x-1.5)][int(y+1.5)]]>0||blockwj[s[int(x-1.5)][int(y+0.5)]]>0||blockwj[s[int(x-0.5)][int(y+2.5)]]>0)){
	block_(int(x-0.5),int(y+1.5));}
	else if(s[int(x-1.5)][int(y+1.5)]==0&&(blockwj[s[int(x-0.5)][int(y+1.5)]]>0||blockwj[s[int(x-2.5)][int(y+1.5)]]>0||blockwj[s[int(x-2.5)][int(y+0.5)]]>0||blockwj[s[int(x-1.5)][int(y+2.5)]]>0)){
	block_(int(x-1.5),int(y+1.5));}
	else if(s[int(x-2.5)][int(y-0.5)]==0&&(blockwj[s[int(x-1.5)][int(y+1.5)]]>0||blockwj[s[int(x-3.5)][int(y+1.5)]]>0||blockwj[s[int(x-3.5)][int(y+0.5)]]>0||blockwj[s[int(x-2.5)][int(y+2.5)]]>0)){
	block_(int(x-2.5),int(y+1.5));}}
	else if(blockwj[s[int(x+0.5)][int(y+1.5)]]>0||blockwj[s[int(x-0.5)][int(y+0.5)]]>0||blockwj[s[int(x+0.5)][int(y+0.5)]]>0)
	{if(!(blockwj[s[int(x+0.5)][int(y-0.5)]]>0))block_(int(x+0.5),int(y+0.5));}*/
	
	if(my==1&&jump<5)jump++,spy=-12;
	else jump=5;
	/*if(ks==1){
	if(s[int(x+0.5)][int(y+0.5)]==2){
	wj++;
	if(wj>5){
	s[int(x+0.5)][int(y+0.5)]=0,wj=0;
	int ff=1;
	beibao[2]++;
	while(s[int(x+0.5)][int(y+0.5-ff)]==2)s[int(x+0.5)][int(y+0.5-ff)]=0,ff++,beibao[2]++;
	}
	}
	else {
	if(my==-1){
	if(blockwj[s[int(x+0.5)][int(y+1.5)]]>0)wj++;
	if(wj>blockwj[s[int(x+0.5)][int(y+1.5)]])beibao[s[int(x+0.5)][int(y+1.5)]]++,s[int(x+0.5)][int(y+1.5)]=0,wj=0;}
	else if(my==1){
	if(blockwj[s[int(x+0.5)][int(y-0.5)]]>0)wj++;
	if(wj>blockwj[s[int(x+0.5)][int(y-0.5)]])beibao[s[int(x+0.5)][int(y-0.5)]]++,s[int(x+0.5)][int(y-0.5)]=0,wj=0;}
	else if(mx==1){
	if(blockwj[s[int(x+1.5)][int(y+0.5)]]>0)wj++;
	if(wj>blockwj[s[int(x+1.5)][int(y+0.5)]])beibao[s[int(x+1.5)][int(y+0.5)]]++,s[int(x+1.5)][int(y+0.5)]=0,wj=0;}
	else if(mx==-1){
	if(blockwj[s[int(x-0.5)][int(y+0.5)]]>0)wj++;
	if(wj>blockwj[s[int(x-0.5)][int(y+0.5)]])beibao[s[int(x-0.5)][int(y+0.5)]]++,s[int(x-0.5)][int(y+0.5)]=0,wj=0;}
	}}
	else wj=0;*/
	spy+=g;
	spy*=0.9;
	y+=spy*0.05;
	y_touch();
	y+=spy*0.05;
	y_touch();
	x+=spx*0.2;
	if(blockwj[s[int(x+0.5)][int(y+0.5)]]>1){
	if(spx>0)while(blockwj[s[int(x+0.5)][int(y+0.5)]]>1)x-=0.05;
	if(spx<0)while(blockwj[s[int(x+0.5)][int(y+0.5)]]>1)x+=0.05;
	spx=0;
	}
	jx=x;
	jy=y;
	if(jx<25)jx=25;
	if(jy<20)jy=20;
	if(jx>1000)jx=1000;
	if(jy>985)jy=985;
	Print();
	Setpos(1,33);
	Color(0);
	if(life<=0)Start();
	if(life<20)H++;
	if(H>60)life++,H=0;
	cout<<"  ÉúÃü£º"<<life<<"      "<<'\n';  
	for(int i=1;i<=9;i++)
	{
		cout<<i<<".";
		if(beibao[i][1]>0){
		if(beibao[i][0]==1)cout<<" Ô²Ê¯£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==2)cout<<" Ô­Ä¾£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==3)cout<<" ÃºÌ¿£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==4)cout<<" ´ÖÌú£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==5)cout<<" ÄàÍÁ£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==6)cout<<" Ä¾¹÷£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==7)cout<<" Ä¾¸å£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==8)cout<<" Ê¯¸å£º"<<beibao[i][1]<<"  ";
		if(beibao[i][0]==9)cout<<" ÈÛÂ¯£º"<<beibao[i][1]<<"  ";}
		else cout<<" ÎÞ  ";
	}
	cout<<" µ±Ç°Ñ¡Ôñ£º"<<cho<<"        "<<" x:"<<x<<" y:"<<1000-y<<"          "<<mpx<<' '<<mpy<<"                ";}
	}
	save();
	return 0;}
	catch(...)
	{
		cout<<"error";
		exit(1);
	}
}
