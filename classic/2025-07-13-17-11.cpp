#include<bits/stdc++.h>
#include<windows.h> 
#include<conio.h>
using namespace std;
int s[1001][1001];
int blockwj[101]={0,5,0,10,20,0,0,0,0,0,-1,0,0,};
int beibao[100];
int Clocknum=0,jump=0,dx,dy,my,tall,ks=0,mx,fz,cho=1,key_=0,life=20,bx,by,fell=0;
double jx=1,jy=50,x=1,y=50,g=2,spx=0,spy=0,wj=0;
int To_int(float a){return int(a+0.5);}float Abs(float a){return (a>=0)?a:-a;}float Min(float a,float b){return (a<=b)?a:b;}
void Setpos(float x,float y){COORD pos;pos.X=To_int(x)*2+1,pos.Y=To_int(y);SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE),pos);}
void Color(int a){if(a==0||a==8||a==-8) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
if(a==-10||a==1||a==-18) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE);if(a==2) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN);if(a==3) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_RED|FOREGROUND_BLUE);if(a==4) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_RED);if(a==5) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_RED|FOREGROUND_GREEN);if(a==6) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_BLUE);if(a==7) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
if(a==-1) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_RED|BACKGROUND_INTENSITY|BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE);if(a==-2) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),BACKGROUND_INTENSITY|BACKGROUND_GREEN|FOREGROUND_GREEN);if(a==-3) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),BACKGROUND_INTENSITY|BACKGROUND_RED|BACKGROUND_BLUE|FOREGROUND_RED|FOREGROUND_BLUE);if(a==-4) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),BACKGROUND_INTENSITY|BACKGROUND_RED|FOREGROUND_RED);if(a==-5) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),BACKGROUND_INTENSITY|BACKGROUND_RED|BACKGROUND_GREEN|FOREGROUND_RED|FOREGROUND_GREEN);if(a==-6) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),BACKGROUND_INTENSITY|BACKGROUND_BLUE);if(a==-7) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE);
if(a==-11) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|BACKGROUND_INTENSITY|BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE);if(a==-12) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|BACKGROUND_INTENSITY|BACKGROUND_GREEN);if(a==-13) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|BACKGROUND_INTENSITY|BACKGROUND_RED|BACKGROUND_BLUE);if(a==-14) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|BACKGROUND_INTENSITY|BACKGROUND_RED);if(a==-15) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|BACKGROUND_INTENSITY|BACKGROUND_RED|BACKGROUND_GREEN);if(a==-16) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|BACKGROUND_INTENSITY|BACKGROUND_BLUE);if(a==-17) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),FOREGROUND_INTENSITY|FOREGROUND_GREEN|FOREGROUND_BLUE|BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE);
}
void HideCursor(){
 	CONSOLE_CURSOR_INFO cursor_info={1,0};
 	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&cursor_info);
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
void MAP()
{
	int a;
	double now=0;
	tall=49;
	for(int i=0;i<=1000;i++){
		if(rand()%15==0)tall++;
		if(rand()%15==1)tall--;
		for(int j=1000;j>=tall+1;j--)
		{
			if(j==999)s[i][j]=10;
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
	for(int i=5;i<=995;i++){
		HideCursor();
		printf("\b%.1lf%%",now*1.0/10); 
		Setpos(0,-1);
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
void Print()
{
	for(int i=0;i<=50;i++){
	for(int j=0;j<=30;j++)
	{
		dx=int(jx+i-25+0.5);
		dy=int(jy+j-15+0.5);
		Setpos(i,j);
		if((dx==int(x+0.5)&&int(y+0.5)==dy))Color(1),printf("1 ");
		if(s[dx][dy]==0)
		{
		if(dy<60)Color(-6),printf("  ");
		else Color(0),printf("  ");
		}
		else if(s[dx-1][dy]!=0&&s[dx+1][dy]!=0&&s[dx][dy+1]!=0&&s[dx][dy-1]!=0&&s[dx+1][dy+1]!=0&&s[dx+1][dy-1]!=0&&s[dx-1][dy+1]!=0&&s[dx-1][dy-1]!=0){
		Color(-1),printf("  ");
		continue;}
		else if(s[dx][dy]==1)Color(-7),printf("O ");
		else if(s[dx][dy]==3)Color(-1),printf("# ");
		else if(s[dx][dy]==4)Color(-5),printf("* ");
		else if(s[dx][dy]==2)Color(-5),printf("[]");
	}
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
	for(int i=0;i<100;i++)
	fprintf(fp,"%d ",beibao[i]);
	fprintf(fp,"%lf %lf",x,y);
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
	fscanf(fp," %d",&beibao[i]);
	}
	fscanf(fp," %lf %lf",&x,&y);
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
	if(cho==8&&beibao[1]>0)s[xxx][yyy]=1,beibao[1]--;
	if(cho==7&&beibao[2]>0)s[xxx][yyy]=2,beibao[2]--;
	if(cho==6&&beibao[4]>0)s[xxx][yyy]=4,beibao[4]--;
	if(cho==9&&beibao[3]>0)s[xxx][yyy]=3,beibao[3]--;}
void Start()
{
	life=20,x=1,y=50;
}

void gohome()
{
	x=12,y=55;
	shuaxin();
}
int main()
{
	try{
	srand((unsigned)time(NULL));
	read();
	if(r==10){
	MAP();
	Start();}
	shuaxin();
	fell=y;
	while(1)
	{HideCursor();
	if(clock()-Clocknum>=15){
	Clocknum=clock();
	mx=0;
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
	if(GetAsyncKeyState('6'))cho=6;
	if(GetAsyncKeyState('7'))cho=7;
	if(GetAsyncKeyState('8'))cho=8;
	if(GetAsyncKeyState('9'))cho=9;
	if(GetAsyncKeyState('\r')&&cho==1)ks=1;
	else ks=0;
	if(GetAsyncKeyState('L'))key_++;
	else key_=-2;
	if(key_>0)key_=-2,fz=1;
	else fz=0;
	if(fz){
	bx=bool(GetAsyncKeyState('K'))-bool(GetAsyncKeyState('H'));
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
	{if(!(blockwj[s[int(x+0.5)][int(y-0.5)]]>0))block_(int(x+0.5),int(y+0.5));}
	}
	if(my==1&&jump<5)jump++,spy=-12;
	else jump=5;
	if(ks==1){
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
	else wj=0;
	spy+=g;
	spy*=0.9;
	y+=spy*0.1;
	if(blockwj[s[int(x+0.5)][int(y+0.5)]]>0||blockwj[s[int(x+0.5)][int(y+0.5)]]==-1){
		if((y-fell)>8)life-=int(y-fell-8);
	if(spy>=0){
	while(blockwj[s[int(x+0.5)][int(y+0.5)]]>0)y-=0.1;
	fell=y;
	jump=0;
	spy=0;
	}
	if(spy<0){
	while(blockwj[s[int(x+0.5)][int(y+0.5)]]>0)y+=0.1;
	spy=3;
	jump=5;
	}
	}
	x+=spx*0.2;
	if(blockwj[s[int(x+0.5)][int(y+0.5)]]>0){
	if(spx>0)while(blockwj[s[int(x+0.5)][int(y+0.5)]]>0)x-=0.1;
	if(spx<0)while(blockwj[s[int(x+0.5)][int(y+0.5)]]>0)x+=0.1;
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
	if(life<0)Start();
	cout<<"  ÉúÃü£º"<<life<<"      "<<'\n';  
	cout<<" 1.Ä¾¸å"<<' '<<" 6.´ÖÌú£º"<<beibao[4]<<' '<<" 7.Ô­Ä¾£º"<<beibao[2]<<' '<<" 8.Ô²Ê¯£º"<<beibao[1]<<' '<<" 9.ÃºÌ¿£º"<<beibao[3]<<' ';
	cout<<" µ±Ç°Ñ¡Ôñ£º"<<cho<<"        "<<" x:"<<x<<" y:"<<1000-y<<"                        ";}
	}
	save();
	return 0;}
	catch(...)
	{
		cout<<"error";
		exit(1);
	}
}

