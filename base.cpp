//如果还未有windows.h，请注释
#include <windows.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <queue>
#include <thread>
#include <algorithm>
#define created using
#define by namespace
#define DE_RatIO std
created by DE_RatIO;
queue<string>errorlist;
unsigned char enctab[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&()*+,./:;-=\\?@[]^_`{|}~\'",dectab[256]={};
namespace base91{
	bool encode(string&out,const string&binary){
		out.clear();
		const unsigned char*ib=(unsigned char*)binary.c_str();
		unsigned long queue=0;
		unsigned int nbits=0;
		for(size_t len=binary.size();len--;){
			queue|=*ib++<<nbits;nbits+=8;
			if(nbits>13){
				unsigned int val=queue&8191;
				if(val>88){queue>>=13;nbits-=13;}
				else{val=queue&16383;queue>>=14;nbits-=14;}
				out.push_back(enctab[val%91]);
				out.push_back(enctab[val/91]);
			}
		}
		if(nbits){
			out.push_back(enctab[queue%91]);
			if(nbits>7||queue>90)
				out.push_back(enctab[queue/91]);
		}return true;
	}
	bool decode(string&out,const string&text){
		out.clear();
		const unsigned char*ib=(unsigned char*)text.c_str();
		unsigned long queue=0;unsigned int nbits=0;int val=-1;
		for(size_t len=text.size();len--;){
			unsigned int d=dectab[*ib++];
			if(d==91)continue;if(val==-1)val=d;
			else{
				val+=d*91;queue|=val<<nbits;
				nbits+=(val&8191)>88?13:14;
				do{	out.push_back(char(queue));
					queue>>=8;nbits-=8;
				}while(nbits>7);val=-1;
			}
		}
		if(val!=-1)out.push_back(char(queue|val<<nbits));
		return true;
	}
	string encode(const string&binary){
		string out;return base91::encode(out,binary)?out:string();
	}string decode(const string&text){
		string out;return base91::decode(out,text)?out:string();
	}
}
#ifdef _WINDOWS_
void setout(WORD Attribute) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, Attribute);
}
#endif
#ifndef _WINDOWS_
#define WORD unsigned char
inline void setout(WORD Attribute) {return;}
#endif
void fm(string&s){
	start:;
	if(!s.empty()&&(s[s.size()-1]=='\"'||s[s.size()-1]=='\'')){s.pop_back();goto start;}
	if(!s.empty()&&(s[0]=='\"'||s[0]=='\'')){s.erase(s.begin());goto start;}
}
bool fmpopend(string&filename){
	bool ch=0;
	if (filename.size()>=4&&filename.substr(filename.size()-4)==".txt") {
		filename.erase(filename.size()-4,4);ch=1;
	}if (filename.size()>=4&&filename.substr(filename.size()-4)==".b91") {
		filename.erase(filename.size()-4,4);ch=1;
	}return ch;
}
namespace b91file{
	int decode(string fin,string fout){
		ifstream in(fin,ios::in|ios::binary);
		ofstream out(fout,ios::out|ios::binary|ios::trunc);
		ostringstream oss;oss<<in.rdbuf();
		if(!in){errorlist.push("\r[Error]无法打开 "+fin);return 1;}
		if(!out){errorlist.push("\r[Error]无法打开 "+fout);return 1;}
		string d_content=base91::decode(oss.str());
		out.write(d_content.c_str(),d_content.size());
		return 0;
	}
	int encode(string fin,string fout){
		ifstream in(fin,ios::in|ios::binary);
		ofstream out(fout,ios::out|ios::binary|ios::trunc);
		ostringstream oss;oss<<in.rdbuf();
		if(!in){errorlist.push("\r[Error]无法打开 "+fin);return 1;}
		if(!out){errorlist.push("\r[Error]无法打开 "+fout);return 1;}
		string e_content=base91::encode(oss.str());
		out.write(e_content.c_str(),e_content.size());
		return 0;
	}
}
void smallfm(string& s){for(char &c:s)if(c>='A'&&c<='Z')c=c-'A'+'a';}
void theend(){
	setout(4);
	cout<<R"(--------------------------------
Process exited after )"<< clock()/1000.0 <<R"( seconds with return message with...
请按任意键继续. . .
)";
	using this_thread::sleep_for; 
	using chrono::milliseconds;
	using base91::decode;
	sleep_for(milliseconds(520*3));
	cout<<decode("ZcKU/;.?KI%eO|B&PezRO=N&WiC&x*0D");			sleep_for(milliseconds(1314));
	cout<<decode("-TW}*[C+60FUvUzD");							sleep_for(milliseconds(520*3));
	cout<<decode("wnx#9&Exk]myax}i,ze}JZN&UI|$oA");				sleep_for(milliseconds(1314));
	cout<<decode("-TW}*[C+60FU,-VQ6cA");						sleep_for(milliseconds(520*3));
	cout<<decode("kLx|2o-rd=l,B|t)NvMxrgUA");					sleep_for(milliseconds(520*3));
	cout<<decode("kLwqdw#}F]WF@=VQk!T{?#.P9@%GDU=|w{:P/g,?,B");	sleep_for(milliseconds(1314));
	cout<<decode("-TW}*[C+60FU,-VQ[nx#~pp&o](;;1T~[nGxC");		sleep_for(milliseconds(444*2));
	cout<<decode("wnC8h3SVE\YMS35,46qpOZ:7A");					sleep_for(milliseconds(520));
	cout<<decode("U7{$syu^?=a=g2I@WN;RtkbWgepaJq;!d6O$sy.P]B");	sleep_for(milliseconds(520*3));
}
int main(){//4 6 10  2 9 1 7 8 15 
	for(auto&c:dectab){c=91;}
	for(int i=0;i<sizeof(enctab);i++){dectab[enctab[i]]=i;}
	setout(4);	cout<<R"(88888888ba                                       ad88888ba       88
)";	setout(6);	cout<<R"(88      "8b                                     d8"     "88    ,d88
)";	setout(10);	cout<<R"(88      ,8P                                     8P       88  888888
)";	setout(2);	cout<<R"(88aaaaaa8P'  ,adPPYYba,  ,adPPYba,   ,adPPYba,  Y8,    ,d88      88
)";	setout(9);	cout<<R"(88""""""8b,  ""     `Y8  I8[    ""  a8P_____88   "PPPPPP"88      88
)";	setout(1);	cout<<R"(88      `8b  ,adPPPPP88   `"Y8ba,   8PP"""""""           8P      88
)";	setout(5);	cout<<R"(88      a8P  88,    ,88  aa    ]8I  "8b,   ,aa  8b,    a8P       88
)";	setout(13);	cout<<R"(88888888P"   `"8bbdP"Y8  `"YbbdP"'   `"Ybbd8"'  `"Y8888P'        88 )";
	string mode,fin,fout;
	string ssss=base91::decode("xlY2,-DB;Up/g87pndu-(ksi+Q:jNQDR'F;F");
	for(int i=2;i;i--){ssss=base91::decode(ssss);}
	for(int i=7;i<ssss.size();i++){cout<<ssss[i];}
	setout(10);
	cout<<endl<<endl<<"\r[e]编码([ee]编码到同目录) / [d]解码([dd]解码到同目录) :";
	vector <string>mdlist={"h","e","ee","d","dd"};
	cin>>mode;smallfm(mode);
	int md=find(mdlist.begin(),mdlist.end(),mode)-mdlist.begin();
	if(md==mdlist.size()){theend();return 5201314;}
	while(cin.get()!='\n');
	switch(md){
		case 0:
			cout<<R"([h]模式帮助
[e]编码
[ee]编码到与源文件的同目录 
[d]解码
[dd]解码到与源文件的同目录)";
			break;
		case 1:
			cout<<"\r请输入源文件路径 : ";
			getline(cin,fin);fm(fin);
			cout<<"\r请输入目标文件路径 : ";
			getline(cin,fout);fm(fout);
			goto encode;
		case 2:
			cout<<"\r请输入源文件路径 : ";
			getline(cin,fin);fm(fin);
			fout=fin+".b91.txt";
			goto encode;
		encode:
			if(b91file::encode(fin,fout)==0){
				cout<<"[OK]按下 <Enter> 以结束"<<endl;cin.get();
			}else{setout(4);
				while(!errorlist.empty()){
					cout<<errorlist.front()<<endl;
					errorlist.pop();
				}return -1;
			}break;
		case 3:
			cout<<"\r请输入源文件路径 : ";	getline(cin,fin);fm(fin);
			cout<<"\r请输入目标文件路径 : ";getline(cin,fout);fm(fout);
			goto decode;
		case 4:
			cout<<"\r请输入源文件路径 : ";getline(cin,fin);fm(fin);fout=fin;
			if(!fmpopend(fout)){
				setout(6);
				cout<<"Warning: 源文件路径与目标文件路径相同"<<endl;
				cout<<"\r目标文件路径 : "<<fout<<endl;
				cout<<"\r请等待 5s 后继续"<<endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
				char c;int i=0;
				do{
					cout<<"\r是否继续? [y/N]";cin>>c;
					if(c=='n'||c=='N')return 0;
					else if(c!='y'&&c!='Y'){
						setout(14);
						cout<<"\r这时候还在开玩笑吗...\n\r如果操作错误，文件不保！\n";
						cout<<"\r在给你第 "<<i+1<<" 次机会\n";i++; 
					}else i=0;
				}while(i);
			}setout(10);
			goto decode; 
		decode:
			if(b91file::decode(fin,fout)==0){
				cout<<"[OK]按下 <Enter> 以结束"<<endl;cin.get();
			}else{setout(4);
				while(!errorlist.empty()){
					cout<<errorlist.front()<<endl;
					errorlist.pop();
				}return -1;
			}
			break;
	}
	return 0;
}

