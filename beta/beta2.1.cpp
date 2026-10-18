#define _CRT_SECURE_NO_WARNINGS
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
#include <algorithm>

#include<thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cmath>
#include <conio.h>
#include <cstdio>
#include <map>
#include <vector>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #include <share.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif
#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1 : 0)
#pragma GCC optimize(2)
using namespace std;
// 世界尺寸：修改这两个常量即可改变世界大小
constexpr int WORLD_WIDTH = 10000;
constexpr int WORLD_HEIGHT = 1001;
static_assert(WORLD_WIDTH > 0 && WORLD_HEIGHT > 0, "世界尺寸必须为正");
short s[WORLD_WIDTH][WORLD_HEIGHT];
short s2[WORLD_WIDTH][WORLD_HEIGHT];
short light[WORLD_WIDTH][WORLD_HEIGHT];
int painting[102][102][2];
bool ep[WORLD_WIDTH][WORLD_HEIGHT];

// 增量保存：脏列块追踪（仅游戏线程写，快照时消费）
constexpr int CHUNK_COLS = 64;
constexpr int NCHUNKS = (WORLD_WIDTH + CHUNK_COLS - 1) / CHUNK_COLS;
bool g_dirty[NCHUNKS];
bool g_dirty_meta = true;
inline void mark_dirty_col(int c)
{
    if(c < 0)
        c = 0;
    if(c >= WORLD_WIDTH)
        c = WORLD_WIDTH - 1;
    g_dirty[c / CHUNK_COLS] = true;
}
inline void mark_dirty_range(int a, int b)
{
    if(a < 0)
        a = 0;
    if(b >= WORLD_WIDTH)
        b = WORLD_WIDTH - 1;
    if(a > b)
        return;
    for(int c = a / CHUNK_COLS; c <= b / CHUNK_COLS; c++)
        g_dirty[c] = true;
}
inline void mark_all_dirty()
{
    memset(g_dirty, 1, sizeof(g_dirty));
    g_dirty_meta = true;
}

wchar_t title[256];
string name[150] = {"air",
                    "stone",
                    "wood",
                    "coal",
                    "raw_iron",
                    "dirt",
                    "stick",
                    "wood_pickaxe",
                    "stone_pickaxe",
                    "furnace",
                    "bedrock",
                    "sky",
                    "diamond",
                    "iron_ingot",
                    "iron_pickaxe",
                    "diamond_pickaxe",
                    "torch",
                    "stone_sword",
                    "stone_axe",
                    "water",
                    "flowing_water_3",
                    "flowing_water_2",
                    "flowing_water_1",
                    "leaf",
                    "health_potion",
                    "bucket",
                    "water_bucket",
                    "iron_chestplate",
                    "iron_leggings",
                    "iron_helmet",
                    "iron_boots",
                    "shield",
                    "lava",
                    "flowing_lava_2",
                    "flowing_lava_1",
                    "lava_bucket",
                    "obsidian",
                    "rotten_flesh",
                    "nether_portal",
                    "netherrack",
                    "chest",
                    "bow",
                    "string",
                    "gunpowder",
                    "wood_arrow",
                    "iron_arrow",
                    "explosive_arrow",
                    "iron_sword",
                    "iron_axe",
                    "diamond_sword",
                    "diamond_axe",
                    "oi_core",
                    "recursive",
                    "snow",
                    "ice",
                    "apple",
                    "diamond_helmet",
                    "diamond_chestplate",
                    "diamond_leggings",
                    "diamond_boots",
                    "oi_sword",
                    "storm_bow",
                    "raw_gold",
                    "nether_gold_ore",
                    "gold_ingot",
                    "gold_nugget",
                    "platforms",
                    "planks",
                    "ebonstone",
                    "ancient_debris",
                    "netherite_scrap",
                    "netherite_ingot",
                    "netherite_pickaxe",
                    "netherite_sword",
                    "netherite_axe",
                    "worm_food",
                    "gold_pickaxe",
                    "gold_sword",
                    "gold_axe",
                    "gold_helmet",
                    "gold_chestplate",
                    "gold_leggings",
                    "gold_boots",
                    "netherite_helmet",
                    "netherite_chestplate",
                    "netherite_leggings",
                    "netherite_boots",
                    "wool",
                    "mutton",
                    "pork",
                    "bed",
                    "golden_apple",
					"ebonwall",
                    "level_portal",
                    "energy_potion",
                    "pistol",
                    "smg",
                    "shotgun",
                    "sniper",
                    "bullet",
                    "big_health_potion",
                    "coin",
                    "gacha_machine",
                    "level_gold_ore",
                    "smithing_table",
                    "shadow_scale",
                    "shadow_helmet",
                    "shadow_chestplate",
                    "shadow_leggings",
                    "shadow_boots",
                    "Light's_Bane",
                    "demon_bow",
                    "magiluminescence"
                   };
map<string, int> thing;
int blockwj[150] = {0,    200, 200, 400, 600, 100, 0,   0,  0,  200, -1, 0,
                    800, 0,   0,   0,   5,   -1,  0,   -1, -1, -1,  -1, 12,
                    -1,   0,   0,   0,   0,   0,   0,   -1, -1, -1,  0,  0,
                    4000, 0,   -1,  150, 400, 0,   0,   0,  0,  0,   0,  0,
                    0,    0,   0,   0,   0,   120, 180, 0,  0,  0,   0,  0,
                    0,    0,   600, 300,0,0,100,100,2000,1200,
                    0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,
                    0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,-1,300
                   };
int blocktouch[150] = {0,1,0,1,1,1,0,0,0,0,1,0,1,0,0,0,0,0,1,
                       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,
                       0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,
                       0,0,0,0,0,1,1,0,0,2,1,1,1,
                       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                       0,0,0,0,0,0,0,0,
                       0,1,0,0,0,0,0,0,0,1,1};
int blocklv[150] = {0, 2, 1,  2,  3,  1,  0,  0,  0,  2,  114, 0, 4, 0, 0,
                    0, 1, 0,  0,  10, 10, 10, 10, 15, -1, 0,   0, 0, 0, 0,
                    0, 0, 10, 10, 10, 0,  0,  5,  0,  2,  0,   0, 0, 0, 0,
                    0, 0, 0,  0,  0,  0,  0,  0,  0,  1,  1,   0, 0, 0, 0,
                    0, 0, 4,  2,  0,  0,  1,  1,  5, 5,
                    0,0,0,0,0,0,0,0,0,0,0,0,0,1,
                    0,0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,-1,2
                   };
const string blockprint[150]={
"00","  ","  ","##","##","  ","  ","  ","  ","<>","  ","11","%%","  ","  ","  ","I ","  ","  ","  ",
"  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","##","  ","@@","  ",
"--","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ",
"  ","  ","&&","&&","  ","  ","  ","==","##","##","  ","  ","  ","  ","  ","  ","  ","  ","  ","  ",
"  ","  ","  ","  ","  ","  ","  ","++","  ","  ","--","  ","  ",
"<>","  ","++","++","**","<>","==","++","$$","<>","GG",
"<>"
};
const int blockcolor[150]={
0,-7,-5,7,-37,-12,0,0,0,-15,0,0,11,0,0,0,0,0,0,-21,
-21,-21,-21,-12,0,0,0,0,0,0,0,0,-4,-4,-4,0,3,0,3,-32,
-5,0,0,0,0,0,0,0,0,0,0,0,0,-1,-36,0,0,0,0,0,
0,0,8,-38,0,0,0,-43,-45,-44,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,-8,0,0,-9,0,-3,
-35,9,14,14,13,11,14,12,14,-35,6,
-44
};
string test[150] =
{
    " \b空气",       " \b圆石",       " \b原木",       " \b煤炭",
    " \b粗铁",       " \b泥土",       " \b木棍",       " \b木镐",
    " \b石镐",       " \b熔炉",       " \b基岩",       " \b天空",
    " \b钻石",       " \b铁锭",       " \b铁镐",       " \b钻石镐",
    " \b火把",       " \b石剑",       " \b石斧",       "  \b静态水19",
    " \b流动水3/4,", " \b流动水2/4,", " \b流动水1/4,", " \b树叶",
    " \b回血药水",   " \b桶",         " \b水桶",       " \b铁胸甲",
    " \b铁护腿",     " \b铁头盔",     " \b铁靴子",     " \b31盾牌",
    " \b岩浆",       " \b2/3岩浆",    " \b1/3岩浆",    " \b岩浆桶",
    " \b黑曜石",     " \b腐肉",       " \b下界传送门", " \b下界岩",
    " \b箱子",       " \b木弓",       " \b线",         " \b火药",
    " \b木箭",       " \b铁箭",       " \b爆炸箭",     " \b铁剑",
    " \b铁斧",       " \b钻石剑",     " \b钻石斧",     " \bOI核心",
    " \b递归",       " \b雪",         " \b冰",         " \b苹果",
    " \b钻石头盔",   " \b钻石胸甲",   " \b钻石护腿",   " \b钻石靴子",
    " \bOI剑"," \b风暴弓"," \b粗金"," \b下界金矿",
    " \b金锭"," \b金粒"," \b木平台"," \b木板",
    " \b腐化石块"," \b远古残骸"," \b下界碎片"," \b下界锭",
    " \b下界镐"," \b下界剑"," \b下界斧"," \b蠕虫诱饵",
    " \b金镐"," \b金剑"," \b金斧",
    " \b金头盔"," \b金胸甲"," \b金护腿"," \b金靴子",
    " \b下界头盔"," \b下界胸甲"," \b下界护腿"," \b下界靴子",
    " \b羊毛"," \b羊肉"," \b猪肉"," \b床"," \b金苹果",
    " \b腐化瘀墙"," \b关卡传送门"," \b能量药水"," \b手枪"," \b冲锋枪"," \b霰弹枪"," \b狙击枪"," \b子弹"," \b大回血药"," \b金币"," \b扭蛋机"," \b关卡金矿",
    " \b锻造台"," \b暗影鳞片"," \b暗影头盔"," \b暗影胸甲"," \b暗影护腿"," \b暗影靴子"," \b魔光剑"," \b恶魔弓"," \b魔光护符"," \b护甲药水"
};
string stname[150] = { " 僵尸"," 箭"," 骷髅"," 苦力怕"," OI"," 蜘蛛"," 僵尸猪灵"," 递归"," 世界吞噬者-头部"," 世界吞噬者-体节"," 世界吞噬者-尾部"," 羊"," 猪"," ",
                       " 商人"," 工匠"," 占星师"," 雇佣兵"," 牧师"," 地牢商人"," 被关押NPC"," 奸商"," 小偷猫"," 传送门"
                     };
// 元气骑士系统前向声明
int add_coins(int n);
int total_coins();
bool spend_coins(int n);
bool hasTalent(int t);
int talentLevel(int t);
void spawn_dungeon_npc(int type, int tx, int ty);
void reset_dungeon_state();
int beibao[101][2];
short boimes[WORLD_WIDTH + 5];
short boimes2[WORLD_WIDTH + 5];
int wear[6], fbnq[12] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89};
int l_beibao[30][2]= {{0,0},{1,0}};
short page = 1;
int printx = 70, printy = 42, boxlen = 0;
int print_set,st_num=300;
bool run = 1, can = 0, start = 0, yes = 0, SetLife = 0, CanGet = 1, oi = false,f3=0,key_d_Y,
     checkhp = 0,bag,issword,godeater;
int l, bosshp = 0, slow_update = 0, eat = 0, weidu = 1, savetick = 0,special_effect=1,rl=0,boss3hp = 0;
int gamemode = 1;
int bed, GetIn = 1, ed = 6000, sun = 1, back_x = 10, now_wait = 0, wait = 0,
         TheTime = 0, back_y = 145, Clocknum = 0, jump = 0, dx, dy, my, tall,
         ks = 0, mx, fz, cho = 1, key_ = 0, bx, by, fell = 0, co = 0, H = 0,
         anx = 0, mpx, mpy, lx, ly, wjlv = 1, playerCD = 0, tick = 0,l_life,
         setfps = 1, gongji = 1, gongjicd = 20, hujiazhi = 200, wudi = 0;
double jx = 1, jy = 50, x = 1, y = 50, g = 0.7, spx = 0, spy = 0, wj = 0,
       FIX = 1, jianshang = 1, life = 200, boss2hp;
// 元气骑士地牢系统 - 新增全局变量
double energy = 100, energyMax = 100;               // 能量系统（蓝量）
int lifeMax = 200;                                  // 生命上限（开局200）
int armorMax = 200;                                 // 护甲上限（开局200）
int oxygen = 900;                                   // 氧气系统（水中消耗，满值900）
int l_oxy = 10;                                     // 上次显示的氧气泡数
int l_hujia = -1;                                   // 上次显示的护甲值
int l_lvlinfo = -1;                                 // 上次显示的关卡进度组合值
int gunCD = 0;                                      // 枪械射击冷却
int currentGunType = 0;                              // 当前手持枪械类型: 0=无 95=手枪 96=冲锋枪 97=霰弹 98=狙击
int coin = 0;                                        // 持有金币
int levelUnlocked = 1;                               // 已解锁关卡(1-10)
int currentLevel = 0;                                 // 当前关卡
int currentRoom = 0;                                  // 当前房间
int roomWavesLeft = 0;                               // 当前房间剩余怪物数
int roomWaveCD = 0;                                   // 下一波刷怪倒计时
int roomWavesDone = 0;                               // 已完成波数
int roomWavesTotal = 0;                               // 总波数
bool inDungeon = false;                                // 是否在地牢中
bool roomActive = false;                               // 当前房间是否在战斗中
int lvl_x0, lvl_x1, lvl_y0, lvl_y1;                  // 当前房间坐标范围
int levelStartX, levelStartY;                         // 地牢起始坐标
int portal_x = 20, portal_y = -1;                     // 主世界关卡传送门位置
// ---- 分叉地牢布局 ----
// 层0=起始安全房；中间层房间数1/2/3/4递增（一条路、两条路...）；末层=最终房
int curLayer = 0, curCol = 0;                         // 当前所在房间层/列
int layCount = 0;                                     // 总层数
int layRooms[12] = {0};                               // 每层房间数
int layOff[12] = {0};                                 // 每层起始列（居中偏移）
int roomState[12][8] = {{0}};                         // 房间状态：0=未清 1=战斗中 2=已清
int bossCol = 0;                                      // 末层房间所在列
clock_t catStealCD = 0;                               // 小偷猫偷窃冷却(到点时间)
int talents[32] = {0};                               // 已获得天赋数组
int talentCount = 0;                                  // 已获得天赋数量
int maxTalentSlots = 8 + 3;                          // 最大天赋槽(8+3额外)
// 天赋ID定义
#define T_LIFE 0        // 生命+4
#define T_ARMOR 1       // 护甲+1
#define T_ENERGY 2      // 能量+100
#define T_KILLEN 3      // 击败回蓝
#define T_KILLHP 4      // 击败回血
#define T_POTION 5      // 药水强化
#define T_EXPLODE 6     // 爆炸减伤
#define T_LAVA 7        // 岩浆免疫
#define T_BOUNCE 8      // 子弹弹射
#define T_CRITPEN 9     // 暴击穿透
#define T_MORESHOT 10   // 散弹增加
#define T_ATKSPD 11     // 攻速加快
#define T_CRIT 12       // 精准暴击
#define T_MELEE 13      // 近战扩大
#define T_HALFPRICE 14  // 商店半价
#define T_GOLDSHIELD 15 // 金盾
#define T_SPEEDARMOR 16 // 疾步生甲
#define T_ARMORREP 17   // 破甲修复
#define T_FULLCRIT 18   // 满血暴击
#define T_MINION 19     // 随从强化
const char* talent_names[24] = {
    "生命上限+25","护甲上限+50","能量上限+50","击败回蓝","击败回血","药水强化",
    "爆炸减伤","岩浆免疫","子弹弹射","暴击穿透","散弹增加","攻速加快",
    "精准暴击","近战扩大","商店半价","金盾","疾步生甲","破甲修复",
    "满血暴击","随从强化","","","",""
};
// 天赋生效辅助
inline bool hasTalent(int t) { for(int i=0;i<talentCount&&i<24;i++) if(talents[i]==t) return true; return false; }
inline int talentLevel(int t) { int n=0; for(int i=0;i<talentCount&&i<24;i++) if(talents[i]==t) n++; return n; }
string zzz;
struct Box_
{
    unsigned short things[27], bx, by;
};
vector<Box_> box;
struct guaiwu
{
    double HP, hurt, gx, gy, spgy, spgx, randomtick, direction, sd, gongjitick,tox,toy,
           maxHP;
    int type, jp, CD, AI, nbt,chv,conl,conr;
    bool ch,isnotouch,islife2,issd;
    unsigned char state;    // AI 状态机当前状态（EntState），0=ST_IDLE
};
// v1/v2 存档按裸结构 fread，结构体大小必须保持 144（state 落在尾部对齐填充内）
static_assert(sizeof(guaiwu) == 144, "guaiwu 大小变化会破坏旧版二进制存档兼容");
struct boomthings
{
    int x, y, color;
};
guaiwu shiti[500];
guaiwu shiti2[500];
int To_int(double a)
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
short willprint[100][50];
constexpr int HASH_CELL = 16;
constexpr int HASH_SIZE = (WORLD_WIDTH / HASH_CELL) + 2;
vector<int> entity_hash[HASH_SIZE][HASH_SIZE];

inline int hash_coord(double x)
{
    int ix = To_int(x);
    int h = ix / HASH_CELL;
    if(h < 0) h = 0;
    if(h >= HASH_SIZE) h = HASH_SIZE - 1;
    return h;
}

void update_entity_hash(int index, double old_x, double old_y, double new_x, double new_y)
{
    int old_hx = hash_coord(old_x);
    int old_hy = hash_coord(old_y);
    int new_hx = hash_coord(new_x);
    int new_hy = hash_coord(new_y);
    if(old_hx != new_hx || old_hy != new_hy)
    {
        auto &old_bucket = entity_hash[old_hx][old_hy];
        auto it = std::find(old_bucket.begin(), old_bucket.end(), index);
        if(it != old_bucket.end())
        {
            old_bucket.erase(it);
        }
        entity_hash[new_hx][new_hy].push_back(index);
    }
}

void add_entity_to_hash(int index, double x, double y)
{
    int hx = hash_coord(x);
    int hy = hash_coord(y);
    entity_hash[hx][hy].push_back(index);
}

void remove_entity_from_hash(int index, double x, double y)
{
    int hx = hash_coord(x);
    int hy = hash_coord(y);
    auto &bucket = entity_hash[hx][hy];
    auto it = std::find(bucket.begin(), bucket.end(), index);
    if(it != bucket.end())
    {
        bucket.erase(it);
    }
}

void query_nearby_entities(double x, double y, double range, vector<int> &results)
{
    results.clear();
    int hx = hash_coord(x);
    int hy = hash_coord(y);
    int range_cells = (int)(range / HASH_CELL) + 1;
    for(int dhx = -range_cells; dhx <= range_cells; dhx++)
    {
        for(int dhy = -range_cells; dhy <= range_cells; dhy++)
        {
            int chx = hx + dhx;
            int chy = hy + dhy;
            if(chx < 0 || chx >= HASH_SIZE || chy < 0 || chy >= HASH_SIZE) continue;
            for(int idx : entity_hash[chx][chy])
            {
                if(shiti[idx].type <= 0)
                    continue;
                double dx = shiti[idx].gx - x;
                double dy = shiti[idx].gy - y;
                if(dx*dx + dy*dy <= range*range)
                {
                    results.push_back(idx);
                }
            }
        }
    }
}

void clear_entity_hash()
{
    for(int i = 0; i < HASH_SIZE; i++)
    {
        for(int j = 0; j < HASH_SIZE; j++)
        {
            entity_hash[i][j].clear();
        }
    }
}
void Setpos(float x, float y)
{
    COORD pos;
    pos.X = To_int(x) * 2 + 1, pos.Y = To_int(y);
    if(bag==1)pos.X+=(printx-50),pos.Y+=(printy-30)/2;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}
int xlen=142,ylen=43;
// 关键修复：渲染区上限 xlen=182(printx=90) × ylen=47(printy=46) = 8554 格，原 charInfo[8005]
// 在设置中把渲染尺寸调到最大时会越界写坏相邻内存；改为按上限容量分配
CHAR_INFO charInfo[200 * 60];
const int f_red=FOREGROUND_RED;
const int b_red=BACKGROUND_RED;
const int f_gre=FOREGROUND_GREEN;
const int b_gre=BACKGROUND_GREEN;
const int f_blu=FOREGROUND_BLUE;
const int b_blu=BACKGROUND_BLUE;
const int f_plu=FOREGROUND_INTENSITY;
const int b_plu=BACKGROUND_INTENSITY;
void Color2(int a,int posid,WORD &In_fo)
{
    if(a == 0 || a == 8 || a == -8)
        In_fo=(f_plu | f_red |
                                f_gre | f_blu);
    if(a == -10 || a == 1 || a == -18)
        In_fo=(f_plu | f_gre |
                                f_blu);
    if(a == 2)
        In_fo=(f_plu | f_gre);
    if(a == 3)
        In_fo=(f_plu | f_red |f_blu);
    if(a == 4)
        In_fo=(f_plu | f_red);
    if(a == 5)
        In_fo=(f_plu | f_red |f_gre);
    if(a == 6)
        In_fo=(f_plu | f_blu);
    if(a == 7)
        In_fo=(b_red | b_gre |b_blu);
    if(a == 8)
        In_fo=(b_red | b_gre |b_blu | f_plu |f_red | f_gre);
    if(a == 9)
        In_fo=( f_gre);
    if(a == 10)
        In_fo=( f_red);
    if(a == 11)
        In_fo=(b_red | b_gre |b_blu | f_plu |f_blu);
    if(a == 12)
        In_fo=(f_plu | f_red |f_blu);
    if(a == -1)
        In_fo=(f_gre | f_blu |f_red | b_plu |b_red | b_gre |b_blu);
if(a == -2)
In_fo=(
b_plu | b_gre |
f_gre);
if(a == -3)
In_fo=(
b_plu | b_red |
b_blu | f_red |
f_blu);
if(a == -4)
In_fo=(
b_plu | b_red);
if(a == -5)
In_fo=(
b_red | b_gre);
if(a == -6)
In_fo=(
b_plu | b_blu);
if(a == -7)
In_fo=(
b_red | b_gre |
b_blu);
if(a == -8)
In_fo=(
b_red | b_gre |
b_blu | f_plu |
f_gre | f_blu |
f_red);
if(a == -9)
In_fo=(
b_red | b_gre |
b_blu);
if(a == -11)
In_fo=(
f_plu | f_gre |
f_blu | f_plu |
f_gre | f_blu |
b_plu | b_red |
b_gre | b_blu);
if(a == -12)
In_fo=(
f_plu | f_gre |
f_blu | b_plu |
b_gre);
if(a == -13)
In_fo=(
f_plu | f_gre |
f_blu | b_plu |
b_red | b_blu);
if(a == -14)
In_fo=(
f_plu | f_gre |
f_blu | b_plu |
b_red);
if(a == -15)
In_fo=(
f_plu | f_gre |
f_blu | b_plu |
b_red | b_gre);
if(a == -16)
In_fo=(
f_plu | f_gre |
f_blu | b_plu |
b_blu);
if(a == -17)
In_fo=(
f_plu | f_gre |
f_blu | b_red |
b_gre | b_blu);
if(a == -21)
In_fo=(
b_plu | b_blu |
b_gre | f_blu |
f_gre);
if(a == -23)
In_fo=(
f_plu | f_red |f_gre | f_blu);
if(a == -27)In_fo=(f_plu | f_red |b_gre | b_blu);
if(a == -29)In_fo=(f_plu | f_blu |b_blu);
if(a == -31)In_fo=(f_blu | b_blu);
if(a == -32)In_fo=( b_red);
if(a == -33)In_fo=(b_red | b_blu);
if(a == -34)In_fo=( b_blu);
if(a == -35)
In_fo=(b_plu | b_gre | b_blu |f_plu | f_red | f_gre | f_blu);
if(a == -36)
In_fo=(b_blu | f_plu | f_blu);
    if(a == -37)
        In_fo=(b_gre | b_blu |b_red | f_red|f_gre);
    if(a == -38)
        In_fo=(b_red| f_plu |f_red | f_gre);
    if(a == -39)
        In_fo=(b_gre | b_blu |b_red| b_plu | f_red|f_gre);
    if(a == -40)
        In_fo=(b_blu| b_plu| f_red|f_gre);
    if(a == -41)
        In_fo=(b_blu| f_red|f_gre);
    if(a == -42)
        In_fo=(f_red|f_gre);
    if(a == -43)
        In_fo=(f_gre | f_blu |f_red | b_red|b_gre);
    if(a == -44)
        In_fo=(b_red |b_gre| f_red | f_blu);
    if(a == -45)
        In_fo=(b_red |b_blu);
    charInfo[posid+1].Attributes=In_fo;
}
void Color(int a)
{
    if(a == 0 || a == 8 || a == -8)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_RED |
                                FOREGROUND_GREEN | FOREGROUND_BLUE);
    if(a == -10 || a == 1 || a == -18)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE);
    if(a == 2)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN);
    if(a == 3)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_RED |
                                FOREGROUND_BLUE);
    if(a == 4)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_RED);
    if(a == 5)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_RED |
                                FOREGROUND_GREEN);
    if(a == 6)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_BLUE);
    if(a == 7)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_GREEN |
                                BACKGROUND_BLUE);
    if(a == 8)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_GREEN |
                                BACKGROUND_BLUE | FOREGROUND_INTENSITY |
                                FOREGROUND_RED | FOREGROUND_GREEN);
    if(a == 9)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
    if(a == 10)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED);
    if(a == 11)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_GREEN |
                                BACKGROUND_BLUE | FOREGROUND_INTENSITY |
                                FOREGROUND_BLUE);
    if(a == 12)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_RED |
                                FOREGROUND_BLUE);
    if(a == -1)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_GREEN | FOREGROUND_BLUE |
                                FOREGROUND_RED | BACKGROUND_INTENSITY |
                                BACKGROUND_RED | BACKGROUND_GREEN |
                                BACKGROUND_BLUE);
    if(a == -2)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_INTENSITY | BACKGROUND_GREEN |
                                FOREGROUND_GREEN);
    if(a == -3)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_INTENSITY | BACKGROUND_RED |
                                BACKGROUND_BLUE | FOREGROUND_RED |
                                FOREGROUND_BLUE);
    if(a == -4)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_INTENSITY | BACKGROUND_RED);
    if(a == -5)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_GREEN);
    if(a == -6)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_INTENSITY | BACKGROUND_BLUE);
    if(a == -7)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_GREEN |
                                BACKGROUND_BLUE);
    if(a == -8)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_GREEN |
                                BACKGROUND_BLUE | FOREGROUND_INTENSITY |
                                FOREGROUND_GREEN | FOREGROUND_BLUE |
                                FOREGROUND_RED);
    if(a == -9)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_GREEN |
                                BACKGROUND_BLUE);
    if(a == -11)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE | FOREGROUND_INTENSITY |
                                FOREGROUND_GREEN | FOREGROUND_BLUE |
                                BACKGROUND_INTENSITY | BACKGROUND_RED |
                                BACKGROUND_GREEN | BACKGROUND_BLUE);
    if(a == -12)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE | BACKGROUND_INTENSITY |
                                BACKGROUND_GREEN);
    if(a == -13)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE | BACKGROUND_INTENSITY |
                                BACKGROUND_RED | BACKGROUND_BLUE);
    if(a == -14)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE | BACKGROUND_INTENSITY |
                                BACKGROUND_RED);
    if(a == -15)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE | BACKGROUND_INTENSITY |
                                BACKGROUND_RED | BACKGROUND_GREEN);
    if(a == -16)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE | BACKGROUND_INTENSITY |
                                BACKGROUND_BLUE);
    if(a == -17)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_GREEN |
                                FOREGROUND_BLUE | BACKGROUND_RED |
                                BACKGROUND_GREEN | BACKGROUND_BLUE);
    if(a == -21)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_INTENSITY | BACKGROUND_BLUE |
                                BACKGROUND_GREEN | FOREGROUND_BLUE |
                                FOREGROUND_GREEN);
    if(a == -23)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_RED |
                                FOREGROUND_GREEN | FOREGROUND_BLUE);
    if(a == -27)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_RED |
                                BACKGROUND_GREEN | BACKGROUND_BLUE);
    if(a == -29)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_INTENSITY | FOREGROUND_BLUE |
                                BACKGROUND_BLUE);
    if(a == -31)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_BLUE | BACKGROUND_BLUE);
    if(a == -32)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_RED);
    if(a == -33)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED | BACKGROUND_BLUE);
    if(a == -34)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_BLUE);
    if(a == -35)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_BLUE |
                                FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if(a == -36)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_BLUE | FOREGROUND_INTENSITY | FOREGROUND_BLUE);
    if(a == -37)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_GREEN | BACKGROUND_BLUE |
                                BACKGROUND_RED | FOREGROUND_RED|FOREGROUND_GREEN);
    if(a == -38)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED
                                | FOREGROUND_INTENSITY |
                                FOREGROUND_RED | FOREGROUND_GREEN);
    if(a == -39)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_GREEN | BACKGROUND_BLUE |
                                BACKGROUND_RED| BACKGROUND_INTENSITY | FOREGROUND_RED|FOREGROUND_GREEN);
    if(a == -40)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_BLUE| BACKGROUND_INTENSITY| FOREGROUND_RED|FOREGROUND_GREEN);
    if(a == -41)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_BLUE| FOREGROUND_RED|FOREGROUND_GREEN);
    if(a == -42)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_RED|FOREGROUND_GREEN);
    if(a == -43)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_GREEN | FOREGROUND_BLUE |
                                FOREGROUND_RED | BACKGROUND_RED|BACKGROUND_GREEN);
    if(a == -44)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED |BACKGROUND_GREEN| FOREGROUND_RED | FOREGROUND_BLUE);
    if(a == -45)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                BACKGROUND_RED |BACKGROUND_BLUE);
}
void HideCursor()
{
    CONSOLE_CURSOR_INFO cursor_info = {1, 0};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}
double jl(double aaaa, double bbbb)
{
    return sqrt((aaaa * aaaa) + (bbbb * bbbb));
}
POINT GetMousePos()
{
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(GetConsoleWindow(), &p);
    // MinGW 下无法使用 GetCurrentConsoleFont，使用固定字体大小估算
    const int fontWidth = 8;   // 字体宽度（像素）
    const int fontHeight = 16; // 字体高度（像素）
    // const int fontWidth = 9;
    // const int fontHeight = 18;
    p.x /= fontWidth;
    p.y /= fontHeight;
    // p.y-=3;
    if(bag==1)p.x-=(printx-50),p.y-=(printy-30)/2;
    return p;
}
boomthings bz[1000];
int KK;
boomthings zc[1000];
void Clear()
{
    if(weidu == 2)
        return;
    for(int i = int(x - 99.5); i <= int(x + 100.5); i++)
    {
        if(i < 0 || i >= WORLD_WIDTH)
            continue;
        mark_dirty_col(i);
        for(int j = 0; j <= 180; j++)
        {
            if(s[i][j] == 0)
                s[i][j] = 11;
        }
    }
}
void Clear_2()
{
    Color(0);
    for(int kkk = 0; kkk <= printy+5; kkk++)
    {
        Setpos(0, kkk);
        for(int i=0; i<=printx+5; i+=2)
            printf("    ");
    }
}
void boom_add(int i, int j, int col)
{
    if(i < 0 || i >= WORLD_WIDTH || j < 0 || j >= WORLD_HEIGHT)
        return;
    KK = 0;
    while(bz[KK].color != 0 && KK < 300)   // 与消费端一致（原299会永久占用bz[299]）
        KK++;
    bz[KK].color = col, bz[KK].x = i, bz[KK].y = j;
    if(blockwj[s[i][j]] != -1 && bz[KK].color != 1)
    {
        s[i][j] = 0;
        mark_dirty_col(i);
    }
}
void boom_add_ready(int i, int j, int col)
{
    KK = 0;
    // 关键修复：原上限299但 boom_loop 只消费 zc[0..98]，槽位>=99 写入后永不消费/清零，
    // 连锁爆炸（如蠕虫死亡、爆炸箭连环）超过99个时后续爆炸静默丢失，容量还会永久缩水
    while(zc[KK].color != 0 && KK < 300)
        KK++;
    zc[KK].color = col, zc[KK].x = i, zc[KK].y = j;
}
int nnx, nny;
void boom_loop()
{
    for(int i = 0; i < 300; i++)   // 与 boom_add 写入范围一致（原299）
    {
        if(bz[i].color != 0)
        {
            if(bz[i].color == -4)
            {
                nnx = bz[i].x, nny = bz[i].y;
                bz[i].color = 0, bz[i].y = 0, bz[i].x = 0;
                boom_add_ready(nnx, nny, -3);
                boom_add_ready(nnx + 1, nny, -3);
                boom_add_ready(nnx - 1, nny, -3);
                boom_add_ready(nnx, nny + 1, -3);
                boom_add_ready(nnx, nny - 1, -3);
                boom_add_ready(nnx + 1, nny + 1, -3);
                boom_add_ready(nnx + 1, nny - 1, -3);
                boom_add_ready(nnx - 1, nny + 1, -3);
                boom_add_ready(nnx - 1, nny - 1, -3);
                boom_add_ready(nnx + 2, nny - 1, 1);
                boom_add_ready(nnx + 2, nny, 1);
                boom_add_ready(nnx + 2, nny + 1, 1);
                boom_add_ready(nnx - 2, nny - 1, 1);
                boom_add_ready(nnx - 2, nny, 1);
                boom_add_ready(nnx - 2, nny + 1, 1);
                boom_add_ready(nnx + 1, nny + 2, 1);
                boom_add_ready(nnx - 1, nny + 2, 1);
                boom_add_ready(nnx, nny + 2, 1);
                boom_add_ready(nnx + 1, nny - 2, 1);
                boom_add_ready(nnx - 1, nny - 2, 1);
                boom_add_ready(nnx, nny - 2, 1);
            }
            if(bz[i].color == -3)
            {
                nnx = bz[i].x, nny = bz[i].y;
                bz[i].color = 0, bz[i].y = 0, bz[i].x = 0;
                boom_add_ready(nnx, nny, -5);
            }
            if(bz[i].color == 1)
            {
                nnx = bz[i].x, nny = bz[i].y;
                bz[i].color = 0, bz[i].y = 0, bz[i].x = 0;
                boom_add_ready(nnx, nny, -5);
            }
            if(bz[i].color == -5)
            {
                bz[i].color = 0, bz[i].y = 0, bz[i].x = 0;
            }
        }
    }
    for(int i = 0; i < 300; i++)   // 关键修复：原99，与 boom_add_ready 写入范围不匹配导致爆炸丢失
    {
        if(zc[i].color != 0)
        {
            boom_add(zc[i].x, zc[i].y, zc[i].color);
            zc[i].x = 0, zc[i].y = 0, zc[i].color = 0;
        }
    }
    Clear();
}
int night = 0, will_night = 0, will_light = 0;
int now_time = 0;
struct LightPos
{
    int x, y, level;
};
void dfs(int i, int j, int lt, int v)
{
    if(i < 0 || i >= WORLD_WIDTH || j < 0 || j >= WORLD_HEIGHT || lt <= 0) return;
    if(blocktouch[s[i][j]] == true)v = 3;
    else v = 1;
    light[i][j] = lt;
    if(lt - v > light[i + 1][j])dfs(i + 1, j, lt - v, v);
    if(lt - v > light[i - 1][j])dfs(i - 1, j, lt - v, v);
    if(lt - v > light[i][j + 1])dfs(i, j + 1, lt - v, v);
    if(lt - v > light[i][j - 1])dfs(i, j - 1, lt - v, v);
}
void setlight()
{
    if(inDungeon)
    {
        memset(light, 15, sizeof(light));   // 地牢全局光照
        return;
    }
    if(gamemode==0)
    {
        memset(light, 15, sizeof(light));
        return;
    }
    memset(light, 0, sizeof(light));
    if(beibao[cho][0]==16)light[int(x+0.5)][int(y+0.5)]=19;
    if(weidu==2)
    {
        for(int i = int(x-149.5); i <= int(x+150.5); i++)
        {
            if(i<0||i>=WORLD_WIDTH)continue;
            for(int j = 0; j < WORLD_HEIGHT; j++)
            {
                if((s[i][j]==0||s[i][j]==11)&&light[i][j]<=8)light[i][j]=8;
            }
        }
    }
    int now_light=20;
    if(night>0)now_light=7;
    if(night==2)now_light+=(20000-now_time)/153;
    if(night==3)now_light+=(now_time-38000)/153;
    if(weidu!=2)
    {
        for(int i = int(x-149.5); i <= int(x+150.5); i++)
        {
            if(i<0||i>=WORLD_WIDTH)continue;
            if((s[i][0] == 0 || s[i][0] == 11)&&light[i][0]<now_light)light[i][0] = now_light;
        }
        for(int i = int(x-149.5); i <= int(x+150.5); i++)
        {
            if(i<0||i>=WORLD_WIDTH)continue;
            for(int j = 1; j < WORLD_HEIGHT; j++)
            {
                if((s[i][j] == 0 || s[i][j] == 11)&&light[i][j]<now_light && light[i][j - 1] == now_light)
                {
                    light[i][j] = now_light;
                }
            }
        }
    }
    for(int i = int(x-149.5); i <= int(x+150.5); i++)
    {
        if(i<0||i>=WORLD_WIDTH)continue;
        for(int j = int(y-199.5); j <= int(y+200.5); j++)
        {
            if(j<0||j>=WORLD_HEIGHT)continue;
            if((s[i][j] == 16||(s[i][j]>=32&&s[i][j]<=34))||s[i][j]==92)
            {
                if(s[i][j]==92){
                if(light[i][j]>7)dfs(i, j, light[i][j], 1);
				else dfs(i, j, 7, 1);
				}
				else dfs(i, j, 19, 1);
            }
            else
            {
                if(light[i][j] > 0)dfs(i, j, light[i][j], 1);
            }
        }
    }
}
void shuaxin()
{
    Color(0);
    Setpos(0, 0);
    for(int i = 0; i <= 15000; i++)
    {
        cout << " ";
    }
    l_life=30,l_beibao[1][0]=114;
    setlight();
    memset(painting, 0, sizeof(painting));
}
void sky()
{
    for(int i = 0; i <= printx+1; i++)
    {
        for(int j = 0; j <= printy+1; j++)
        {
            if(painting[i][j][0] == 11||painting[i][j][0] == 66)
            {
                painting[i][j][0] = 114;
            }
        }
    }
    setlight();
}
void air_()
{
    for(int i = 0; i <= printx+1; i++)
    {
        for(int j = 0; j <= printy+1; j++)
        {
            if(painting[i][j][0] == 0)
            {
                painting[i][j][0] = 114;
            }
        }
    }
}
void atime()
{
    if(sun == 0)
        return;
    now_time += 1;
    if(now_time == 18000)
        night = 2, sky();
    if(now_time == 20000)
        night = 1, sky();
    if(now_time == 38000)
        night = 3, sky();
    if(now_time >= 40000)
        night = 0, now_time = 0, sky();
}
void hujia_update()
{
    // 纯护甲值系统：护甲装备不再提供护甲/减伤
    // 护甲值(hujiazhi)开局200，受伤时吸收伤害并扣减，不自然恢复，仅道具恢复
    jianshang = 1;
    if(hujiazhi > armorMax)
        hujiazhi = armorMax;
}
void st(int type, int x, int y, int ra2)
{
    if(s[x][y] == 1 || s[x][y] == 54)
        s[x][y] = type;
    if((s[x + 1][y] == 1 || s[x + 1][y] == 54) && rand() % ra2 == 0)
        s[x + 1][y] = type;
    if((s[x - 1][y] == 1 || s[x - 1][y] == 54) && rand() % ra2 == 0)
        s[x - 1][y] = type;
    if((s[x + 1][y + 1] == 1 || s[x + 1][y + 1] == 54) && rand() % ra2 == 0)
        s[x + 1][y + 1] = type;
    if((s[x - 1][y - 1] == 1 || s[x - 1][y - 1] == 54) && rand() % ra2 == 0)
        s[x - 1][y - 1] = type;
    if((s[x + 1][y - 1] == 1 || s[x + 1][y - 1] == 54) && rand() % ra2 == 0)
        s[x + 1][y - 1] = type;
    if((s[x - 1][y + 1] == 1 || s[x - 1][y + 1] == 54) && rand() % ra2 == 0)
        s[x - 1][y + 1] = type;
    if((s[x][y + 1] == 1 || s[x][y + 1] == 54) && rand() % ra2 == 0)
        s[x][y + 1] = type;
    if((s[x][y - 1] == 1 || s[x][y - 1] == 54) && rand() % ra2 == 0)
        s[x][y - 1] = type;
}
int last_l;   // 世界吞噬者链式生成：上一节实体下标
void make_st(int typ, int hp, int hur, int tx, int ty, int nbtt = 0,int iseof=0)
{
    l = 0;
    while(l < 500 && shiti[l].type != 0)
        l++;
    if(l >= 500)
        return;
    if(iseof==1)shiti[l].conl= last_l,shiti[last_l].conr=l;
    if(iseof==2)shiti[l].conl= l,shiti[last_l].conr=l;
    if(iseof==3)shiti[l].conl= last_l,shiti[last_l].conr=l,shiti[l].conr=l;
    shiti[l].maxHP = hp, shiti[l].HP = hp, shiti[l].hurt = hur,
             shiti[l].type = typ, shiti[l].gx = tx, shiti[l].gy = ty;
    shiti[l].spgx = 0, shiti[l].spgy = 0, shiti[l].jp = 0, shiti[l].CD = 0,
             shiti[l].nbt = nbtt;
    shiti[l].randomtick = 0, shiti[l].direction = 0;
    if(typ == 1)
        shiti[l].sd = 30, shiti[l].AI = 1;
    else if(typ == 2)
        shiti[l].sd = 18, shiti[l].AI = 1;
    else if(typ == 3)
        shiti[l].sd = 25, shiti[l].AI = 1;
    else if(typ == 4)
        shiti[l].sd = 0, shiti[l].AI = 2, shiti[l].spgx = 10, shiti[l].spgy = -5;
    else if(typ == 5)
        shiti[l].sd = 200, shiti[l].AI = 1;
    else if(typ == 6)
        shiti[l].sd = 25, shiti[l].AI = 1;
    else if(typ == 7)
        shiti[l].sd = 35, shiti[l].AI = 1;
    else if(typ == 12)
        shiti[l].sd = 0, shiti[l].AI = 5;
    else if(typ == 13)
        shiti[l].sd = 0, shiti[l].AI = 5;
    else if(typ == 8)
    {
        shiti[l].direction = shiti[l].HP;
        if(nbtt == 10)
            boss2hp = hp;              // 血条与实际血量一致
        shiti[l].sd = 200, shiti[l].AI = 3;
    }
    else if(typ >= 9)
    {
        shiti[l].sd = 300, shiti[l].AI = 4;
    }
    add_entity_to_hash(l, tx, ty);
    last_l=l;
}
static void level_layout(int n);      // 前向声明：关卡布局计算
static void build_level_terrain(int n);   // 前向声明：关卡地形建造
void MAP()
{
    // 纯地牢模式：不再生成主世界地形，世界保持空白
    box.clear();
    boxlen = 0;
    memset(boimes, 0, sizeof(boimes));
    memset(boimes2, 0, sizeof(boimes2));
    memset(s, 0, sizeof(s));
    memset(s2, 0, sizeof(s2));
    memset(ep, 0, sizeof(ep));
    memset(shiti, 0, sizeof(shiti));
    memset(shiti2, 0, sizeof(shiti2));
    clear_entity_hash();
    HideCursor();
    Clear_2();
    weidu = 1;
    now_time = 0;
    night = 0;
    tall = 149;
    life = lifeMax;
    hujiazhi = armorMax;
    memset(beibao, 0, sizeof(beibao));
    Setpos(21, 13);
    cout << "  正在准备地牢...";
    // 玩家初始位置（会被 enter_level 覆盖）
    x = 9007; y = 507;
    back_x = 9007; back_y = 507;
    portal_x = 20; portal_y = -1;
    memset(wear, 0, sizeof(wear));
    // ===== 预生成全部10个关卡地形（随世界保存，布局持久） =====
    for(int lvl = 1; lvl <= 10; lvl++)
        build_level_terrain(lvl);
    // 恢复第1关布局参数（build_level_terrain循环后layOff等停留在第10关）
    level_layout(1);
    mark_all_dirty();
    setlight();
}
void swap_map(bool ismakedoor=1)
{
    if(weidu == 1)
        weidu = 2;
    else
        weidu = 1;
    mark_all_dirty();
    // 性能修复：原逐元素swap约1000万次；改为每行3次memcpy，快一个数量级
    {
        static short rowtmp[WORLD_HEIGHT];
        for(int i = 0; i < WORLD_WIDTH; i++)
        {
            memcpy(rowtmp, s[i], sizeof(short) * WORLD_HEIGHT);
            memcpy(s[i], s2[i], sizeof(short) * WORLD_HEIGHT);
            memcpy(s2[i], rowtmp, sizeof(short) * WORLD_HEIGHT);
        }
    }
    swap(shiti, shiti2);
    clear_entity_hash();
    for(int i = 0; i < 500; i++)
    {
        if(shiti[i].type > 0)
            add_entity_to_hash(i, shiti[i].gx, shiti[i].gy);
    }
    setlight();
    memset(painting, -1, sizeof(painting));
    if(ismakedoor && mpx >= 1 && mpx < WORLD_WIDTH - 1 && mpy >= 0 &&
            mpy < WORLD_HEIGHT - 1)
    {
        x = mpx, y = mpy;
        s[mpx][mpy] = 38;
        if(s[mpx][mpy + 1] == 0 || s[mpx][mpy + 1] == 11)
            s[mpx][mpy + 1] = 39;
        if(s[mpx - 1][mpy + 1] == 0 || s[mpx - 1][mpy + 1] == 11)
            s[mpx - 1][mpy + 1] = 39;
        if(s[mpx + 1][mpy + 1] == 0 || s[mpx + 1][mpy + 1] == 11)
            s[mpx + 1][mpy + 1] = 39;
    }
}
void Color3(int blockco,int i,int j)
{
	Color2(blockco,j*(xlen)+i*2,charInfo[j*(xlen)+i*2].Attributes), co = blockco;
}
void printbuf(const string string_in,int i,int j)
{
	charInfo[j*(xlen)+i*2].Char.AsciiChar =string_in[0];
   	charInfo[j*(xlen)+i*2+1].Char.AsciiChar =string_in[1];
}
void pr_mouse(int i,int j)
{
    if(light[dx][dy] <= 0 && abs(jl(dx - To_int(x), dy - To_int(y))) >= 4)
    {
        if(co != 0)
            Color3(0,i,j);
        printbuf("[]",i,j);
    }
    else if(s[dx][dy] == 11)
    {
        if(weidu == 1)
        {
            if(night == 0)
                Color3(-6,i,j);
            else if(night == 1)
                Color3(-23,i,j);
            else if(night == 2)
                Color3(-29,i,j);
            else if(night == 3)
                Color3(-31,i,j);
            printbuf("[]",i,j);
        }
        else
        {
            Color3(-33,i,j);
            printbuf("[]",i,j);
        }
    }
    else if(s[dx][dy] == 38)
    {
        Color3(3,i,j);
        printbuf("{}",i,j);
    }
    else if(s[dx][dy] == 16)
    {
        Color3(-1,i,j);
        if(s[dx][dy + 1] == 0 || s[dx][dy + 1] == 11)
        {
            if(!(s[dx - 1][dy] == 0 || s[dx - 1][dy] == 11))
                printbuf("/<",i,j);
            else
                printbuf(">\\",i,j);
        }
        else
            printbuf("I<",i,j);
    }
    else if(s[dx][dy] == 66)
    {
        if(dy>180)
        {
            Color3(-39,i,j);
        }
        else
        {
            if(night == 0)
                Color3(-40,i,j);
            else if(night == 2||night==3)
                Color3(-41,i,j);
            else if(night == 1)
                Color3(-42,i,j);
        }
       printbuf("[]",i,j);
    }
    else {
    	Color3(blockcolor[s[dx][dy]],i,j);
    	printbuf("[]",i,j);
	}
}
// 判断坐标(dx,dy)是否属于当前地牢关卡范围（外围墙矩形内一律白底，消除房间旁黑色空洞）
static bool lvl_ground(int gdx, int gdy)
{
    if(layCount <= 0) return false;
    int maxC = 1;
    for(int L = 0; L < layCount; L++)
        if(layRooms[L] > maxC) maxC = layRooms[L];
    return gdx >= levelStartX && gdx <= levelStartX + maxC * 20 &&
           gdy >= levelStartY && gdy <= levelStartY + layCount * 14;
}
void Updateblock(bool ismap,int i,int j)
{
    if(ismap && !ep[dx][dy])
    {
        Color3(0,i,j);
        printbuf("  ",i,j);
        return;
    }
    if(s[dx][dy] == 11 || (s[dx][dy] == 0 && boimes2[To_int(x)] == 2))
    {
        if(weidu == 1)
        {
            if(light[dx][dy] <= 0 && abs(jl(dx - To_int(x), dy - To_int(y))) >= 4)
            {
                Color3(0,i,j);
            }
            else if((night == 0) ||
                    (s[dx][dy] == 0 && boimes2[To_int(x)] == 2))
                Color3(-6,i,j);
            else if(night == 1)
                Color3(-23,i,j);
            else if(night == 2)
                Color3(-29,i,j);
            else if(night == 3)
                Color3(-31,i,j);
            printbuf("  ",i,j);
        }
        else
        {
            Color3(-33,i,j);
            printbuf("  ",i,j);
        }
    }
    else if(s[dx][dy] == 0)
    {
        if(light[dx][dy] > 0 || abs(jl(dx - To_int(x), dy - To_int(y))) < 4)
        {
            if(inDungeon)
            {
                if(lvl_ground(dx, dy))
                    Color3(-1,i,j);      // 关卡地面/过道：白色
                else
                    Color3(0,i,j);       // 关卡外空白背景：黑色
            }
            else if(weidu == 2)
            {
                Color3(0,i,j);
            }
            else if(weidu == 1)
            {
                Color3(-1,i,j);
            }
            else
            {
                Color3(-33,i,j);
            }
            printbuf("  ",i,j);
        }
        else
        {
            if(weidu == 1)
            {
                Color3(0,i,j);
            }
            else
            {
                Color3(-3,i,j);
            }
            printbuf("  ",i,j);
        }
    }
    else if(light[dx][dy] <= 0 &&
            abs(jl(dx - To_int(x), dy - To_int(y))) >= 4)
    {
        Color3(0,i,j);
        printbuf("  ",i,j);
    }
    else if(s[dx][dy] == 16)
    {
        Color3(-1,i,j);
        if(s[dx][dy + 1] == 0 || s[dx][dy + 1] == 11)
        {
            if(!(s[dx - 1][dy] == 0 || s[dx - 1][dy] == 11))
                printbuf("/ ",i,j);
            else
                printbuf(" \\",i,j);
        }
        else
            printbuf("I ",i,j);
    }
    else if(s[dx][dy] == 66)
    {
        if(dy>180)
        {
            Color3(-39,i,j);
        }
        else
        {
            if(night == 0)
                Color3(-40,i,j);
            else if(night == 2||night==3)
                Color3(-41,i,j);
            else if( night == 1)
                Color3(-42,i,j);
        }
        printbuf("==",i,j);
    }
    else {
       // Color2(blockcolor[s[dx][dy]],j*(xlen)+i*2,charInfo[j*(xlen)+i*2].Attributes), co = blockcolor[s[dx][dy]];
        Color3(blockcolor[s[dx][dy]],i,j);
        //charInfo[j*(xlen)+i*2].Char.AsciiChar =blockprint[s[dx][dy]][0];
   		//charInfo[j*(xlen)+i*2+1].Char.AsciiChar =blockprint[s[dx][dy]][1];
   		printbuf(blockprint[s[dx][dy]],i,j);
    }
}
void Ubformap(int pt, bool ex,int i,int j)
{
    if(!ex)
    {
        Color3(0,i,j);
        printbuf("  ",i,j);
    }
    else if(pt == 11)
    {
        if(weidu == 1)
        {
            if(night == 0)
                Color3(-6,i,j);
            else if(night == 1)
                Color3(-23,i,j);
            else if(night == 2)
                Color3(-29,i,j);
            else if(night == 3)
                Color3(-31,i,j);
            printbuf("  ",i,j);
        }
        else
        {
            Color3(-33,i,j);
            printbuf("  ",i,j);
        }
    }
    else if(pt == 0)
    {
        if(inDungeon)
        {
            if(lvl_ground(dx, dy))
                Color3(-1,i,j);      // 关卡地面/过道：白色
            else
                Color3(0,i,j);       // 关卡外空白背景：黑色
        }
        else if(weidu == 2)
        {
            Color3(0,i,j);
        }
        else if(weidu == 1)
        {
            Color3(-1,i,j);
        }
        else
        {
            Color3(-33,i,j);
        }
        printbuf("  ",i,j);
    }
    else if(pt == 16)
    {
        Color3(-1,i,j);
        printbuf("I ",i,j);
    }
    else {
    	Color3(blockcolor[pt],i,j);
        printbuf(blockprint[pt].c_str(),i,j);
    }
}
bool pr = false;
short last_boi;
void Print()
{
    co = 0;
    Color(0);
    bool isboss3;
    if(boss3hp>bosshp)bosshp=boss3hp,isboss3=1;
    else isboss3=0;
    if(boimes2[To_int(x)]!=last_boi)air_();
    last_boi=boimes2[To_int(x)];
    vector<int> nearby_entities;
    // 相机跨格时整体失效渲染缓存：关卡地面底色依赖世界坐标（lvl_ground），
    // 滚动后屏幕格仍复用旧位置的黑/白底，造成永不消失的黑边（部分渲染残留）
    {
        static int lastcx = -1, lastcy = -1;
        int ncx = int(jx + 0.5), ncy = int(jy + 0.5);
        if(ncx != lastcx || ncy != lastcy)
        {
            memset(painting, -1, sizeof(painting));
            lastcx = ncx;
            lastcy = ncy;
        }
    }
    for(int j = 0; j <= printy; j++)
    {
        for(int i = 0; i <= printx; i++)
        {
            dx = int(jx + i - printx * 0.5 + 0.5);
            dy = int(jy + j - printy * 0.5 + 0.5);
            if((light[dx][dy] > 0 ||
                    (abs(jl(dx - To_int(x), dy - To_int(y))) < 4)) &&
                    ep[dx][dy] == 0)
            {
                ep[dx][dy] = 1;
                mark_dirty_col(dx);
            }
            if(bosshp>0)
            {
                if(i >= 15 && i <= ((bosshp * 1.0 / (25+isboss3*15)) + 15) && j == 7)
                {
                    painting[i][j][0] = 17;
                    painting[i][j][1] = 17;
                    // 双缓冲：Color3/printbuf直写charInfo，Setpos是纯浪费的控制台syscall
                    Color3(-4,i,j);
                    if(i == 25)
                    {
                        if(bosshp >= 100)
                            printbuf(to_string(bosshp/10),i,j);
                        else if(bosshp >= 10)printbuf(to_string(bosshp),i,j);
                        else printbuf(to_string(bosshp)+" ",i,j);
                    }
                    else if(i == 26 && bosshp >= 100)
                        printbuf(to_string(bosshp%10),i,j);
                    else
                        printbuf("  ",i,j);
                    continue;
                }
                if(i > ((bosshp * 1.0 / (25+isboss3*15)) + 15) && i <= 35 && j == 7)
                {
                    painting[i][j][0] = 17;
                    painting[i][j][1] = 17;
                    // 双缓冲：Color3/printbuf直写charInfo，Setpos是纯浪费的控制台syscall
                    Color3(0,i,j);
                    if(i == 25)
                    {
                        if(bosshp >= 100)
                            printbuf(to_string(bosshp/10),i,j);
                        else if(bosshp >= 10)printbuf(to_string(bosshp),i,j);
                        else printbuf(to_string(bosshp)+" ",i,j);
                    }
                    else if(i == 26 && bosshp >= 100)
                        printbuf(to_string(bosshp%10),i,j);
                    else
                        printbuf("  ",i,j);
                    continue;
                }
            }
            if(boss2hp > 0)
            {
                if(i >= 15 && i <= ((boss2hp * 1.0 / 32) + 15) && j == 7)
                {
                    if(painting[i][j][0] == 21 && painting[i][j][1] == 21 && i != 25 &&
                            i != 26)
                        continue;
                    painting[i][j][0] = 21;
                    painting[i][j][1] = 21;
                    // 双缓冲：Color3/printbuf直写charInfo，Setpos是纯浪费的控制台syscall
                    Color3(-4,i,j);
                    if(i == 25)
                    {
                        if(boss2hp >= 100)
                            printbuf(to_string(((int)boss2hp) / 10),i,j);
                        else
                            printbuf(to_string((int)boss2hp)+" ",i,j);
                    }
                    else if(i == 26 && boss2hp >= 100)
                        printbuf(to_string(((int)boss2hp) % 10)+" ",i,j);
                    else
                        printbuf("  ",i,j);
                    continue;
                }
                if(i > ((boss2hp * 1.0 / 32) + 15) && i <= 35 && j == 7)
                {
                    if(painting[i][j][0] == 20 && painting[i][j][1] == 20 && i != 25 &&
                            i != 26)
                        continue;
                    painting[i][j][0] = 20;
                    painting[i][j][1] = 20;
                    // 双缓冲：Color3/printbuf直写charInfo，Setpos是纯浪费的控制台syscall
                    Color3(0,i,j);
                    if(i == 25)
                    {
                        if(boss2hp >= 100)
                            printbuf(to_string(((int)boss2hp) / 10),i,j);
                        else
                            printbuf(to_string((int)boss2hp)+" ",i,j);
                    }
                    else if(i == 26 && boss2hp >= 100)
                        printbuf(to_string(((int)boss2hp) % 10)+" ",i,j);
                    else
                        printbuf("  ",i,j);
                    continue;
                }
            }
            for(int k = 0; k < 300; k++)   // 修复：原101，槽位>100 的爆炸特效不渲染（与容量300一致）
            {
                if(bz[k].color != 0 && bz[k].color != 1)
                {
                    if(To_int(bz[k].x) == dx && To_int(bz[k].y) == dy)
                    {
                        if(painting[i][j][0]!=2000+bz[k].color)
                        {
                            painting[i][j][0] = 2000+bz[k].color;
                            painting[i][j][1] = 2000;
                            Color3(bz[k].color,i,j);
                            printbuf("  ",i,j);
                        }
                        pr = true;
                        break;
                    }
                }
            }
            nearby_entities.clear();
            query_nearby_entities(dx, dy, 2, nearby_entities);
            for(int k : nearby_entities)
            {
                if(shiti[k].type > 0)
                {
                    if(light[dx][dy] <= 0 && (abs(dx - x) > 4 || abs(dy - y) > 4)&&(shiti[k].type!=5&&shiti[k].type<9))
                        continue;
                    if((To_int(shiti[k].gx) == dx && To_int(shiti[k].gy) == dy)||((shiti[k].type >= 9 && shiti[k].type <= 11)&&abs(shiti[k].gy-dy)<=1.5&& abs(shiti[k].gx-dx)<=1.5))
                    {
                        if(painting[i][j][0]!=1500+shiti[k].type*10+shiti[k].nbt)
                        {
                            painting[i][j][0] = 1500+shiti[k].type*10+shiti[k].nbt;
                            painting[i][j][1] = 1500;
                            // 双缓冲：Color3/printbuf直写charInfo，Setpos是纯浪费的控制台syscall
                            Color3(1,i,j);
                            if(shiti[k].type == 1)
                                printbuf("ZZ",i,j);
                            else if(shiti[k].type == 2)
                            {
                                if(shiti[k].CD == 0)
                                    Color3(2,i,j);
                                else
                                    Color3(0,i,j);
                                printbuf("CR",i,j);
                            }
                            else if(shiti[k].type == 3)
                            {
                                Color3(0,i,j);
                                printbuf("SK",i,j);
                            }
                            else if(shiti[k].type == 4)
                            {
                                if(shiti[k].nbt == 1)
                                    Color3(3,i,j);
                                else
                                    Color3(0,i,j);
                                if(shiti[k].spgx > 0)
                                    printbuf("->",i,j);
                                else
                                    printbuf("<-",i,j);
                            }
                            else if(shiti[k].type == 5)
                            {
                                if(shiti[k].islife2 == 1)Color3(12,i,j);
                                else Color3(0,i,j);
                                printbuf("OI",i,j);
                            }
                            else if(shiti[k].type == 6)
                            {
                                Color3(0,i,j);
                                printbuf("SP",i,j);
                            }
                            else if(shiti[k].type == 7)
                            {
                                Color3(0,i,j);
                                printbuf("ZP",i,j);
                            }
                            else if(shiti[k].type == 12)
                            {
                                Color3(1,i,j);
                                printbuf("SH",i,j);
                            }
                            else if(shiti[k].type == 13)
                            {
                                Color3(0,i,j);
                                printbuf("PG",i,j);
                            }
                            else if(shiti[k].type == 8)
                            {
                                if(shiti[k].gongjitick > 600 && shiti[k].gongjitick < 900)
                                    Color3(12,i,j);
                                else
                                    Color3(0,i,j);
                                // 双缓冲：printf直写控制台会被帧尾WriteConsoleOutput覆盖（从未可见）；
                                // 改写charInfo，实体格显示血量前两位
                                {
                                    string hp2 = to_string(int(shiti[k].maxHP + 0.5));
                                    if(hp2.size() < 2) hp2 += " ";
                                    printbuf(hp2.substr(0, 2), i, j);
                                }
                            }
                            else if(shiti[k].type >= 14 && shiti[k].type <= 22)
                            {
                                // NPC 渲染：14商人MR/15工匠SM/16占星AS/17雇佣MC/18牧师PR/19地牢商人DS/20被关押PN/21奸商FK/22小偷猫CT
                                if(shiti[k].type == 14){ Color3(6,i,j);  printbuf("MR",i,j); }
                                else if(shiti[k].type == 15){ Color3(9,i,j); printbuf("SM",i,j); }
                                else if(shiti[k].type == 16){ Color3(11,i,j); printbuf("AS",i,j); }
                                else if(shiti[k].type == 17){ Color3(shiti[k].nbt == 1 ? 11 : 12,i,j); printbuf("MC",i,j); }
                                else if(shiti[k].type == 18){ Color3(2,i,j);  printbuf("PR",i,j); }
                                else if(shiti[k].type == 19){ Color3(6,i,j);  printbuf("DS",i,j); }
                                else if(shiti[k].type == 20){ Color3(7,i,j);  printbuf("PN",i,j); }
                                else if(shiti[k].type == 21){ Color3(5,i,j);  printbuf("FK",i,j); }
                                else { Color3(7,i,j); printbuf("CT",i,j); }
                            }
                            else if(shiti[k].type >= 9)
                            {
                                Color3(-45,i,j);
                                printbuf("  ",i,j);
                            }
                        }
                        pr = true;
                        break;
                    }
                }
            }
            if(pr)
            {
                pr = 0;
                continue;
            }
            if(dx == int(x + 0.5) && dy == int(y + 0.5))
            {
                if(painting[i][j][0]!=1000)
                {
                    painting[i][j][0] = 1000;
                    painting[i][j][1] = 1000;
                    // 双缓冲：Color3/printbuf直写charInfo，Setpos是纯浪费的控制台syscall
                    Color3(1,i,j);
                    printbuf("{}",i,j);
                }
                continue;
            }
            else if(dx == int(mpx + 0.5) && dy == int(mpy + 0.5))
            {
                if(painting[i][j][1]!=1001+s[dx][dy])
                {
                    painting[i][j][0] = 1001;
                    painting[i][j][1] = 1001+s[dx][dy];
                    pr_mouse(i,j);
                }
            }
            else if((abs(jl(dx - To_int(x), dy - To_int(y))) < 4) &&
                    light[dx][dy] <= 0)
            {
                if(painting[i][j][0] !=
                        s[dx][dy] ||
                        (painting[i][j][1] > 0) != (light[dx][dy] > 0))
                {
                    // 双缓冲：Color3/printbuf直写charInfo，Setpos是纯浪费的控制台syscall
                    Updateblock(0,i,j);
                }
                painting[i][j][0] = s[dx][dy];
                painting[i][j][1] = 15;
            }
            else if(painting[i][j][0] != s[dx][dy] ||
                    (painting[i][j][1] > 0) != (light[dx][dy] > 0))
            {
                painting[i][j][0] = s[dx][dy];
                painting[i][j][1] = light[dx][dy];
                Setpos(i, j);
                Updateblock(0,i,j);
            }
        }
    }
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
   COORD bufferSize = {(short)xlen, (short)ylen};
   COORD bufferCoord = {0, 0};
   // writeRegion必须与bufferSize一致（Right/Bottom为包含边界）：{0,0,xlen-1,ylen-1}
   // 原来写成{xlen,ylen}会多写一行一列未初始化数据（黑块），且覆盖行43+的HUD区域
   SMALL_RECT writeRegion = {0,0, (short)(xlen-1),(short)(ylen-1)};
   WriteConsoleOutput(hConsole, charInfo, bufferSize, bufferCoord, &writeRegion);
}
LARGE_INTEGER frequency;
LARGE_INTEGER lastTime;
int frameCount = 0;
// 高精度帧门控：clock()在Windows上粒度约15.6ms，15ms门槛实际在15~31ms间抖动，
// 60帧档实测只有30~45帧；QPC微秒级精度消除抖动
static double qpc_ms()
{
    LARGE_INTEGER t;
    if(frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)frequency.QuadPart;
}
static double g_last_frame_ms = -1;
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
    float deltaTime =
        (float)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
    if(deltaTime >= 1.0f)
    {
        fps = frameCount / deltaTime;
        frameCount = 0;
        lastTime = currentTime;
        swprintf(title, 256, L"我的世界 - FPS: %.2f", fps);
        SetWindowTextW(GetConsoleWindow(), title);
    }
}
int sdprint[100][60][2];
int sf;
void printformap()
{
    co = 0;
    Color(0);
    int aaa[150];
    int spt = 0;
    int tmax = 0, exed = 0;
    for(int j = 0; j <= printy; j++)
    {
        for(int i = 0; i <= printx; i++)
        {
            tmax = 0;
            spt = 0;
            exed = 0;
            memset(aaa, 0, sizeof(aaa));
            for(int kkk = 0; kkk < sf; kkk++)
            {
                for(int kkkk = 0; kkkk < sf; kkkk++)
                {
                    dx = int(jx + i * sf - printx * 0.5 + kkk + 0.5);
                    dy = int(jy + j * sf - printy * 0.5 + kkkk + 0.5);
                    if(ep[dx][dy])
                        exed++;
                    aaa[s[dx][dy]]++;
                    if(aaa[s[dx][dy]] > tmax)
                    {
                        spt = s[dx][dy];
                        tmax = aaa[s[dx][dy]];
                    }
                }
            }
            sdprint[i][j][0] = spt;
            sdprint[i][j][1] = (exed >= sf);
        }
    }
    co = 0;
    for(int j = 0; j <= printy; j++)
    {
        for(int i = 0; i <= printx; i++)
        {
            if(i == printx - 1 && j == 1)
            Color3(1,i,j),printbuf("--",i,j);
            else if(i == printx - 1 && j == 3)
            Color3(1,i,j),printbuf("++",i,j);
            else if(((painting[i][j][0] != sdprint[i][j][0]) && sdprint[i][j][1]) ||
                    ((painting[i][j][1] == 1) != (sdprint[i][j][1] == 1)))
            {
                painting[i][j][0] = sdprint[i][j][0];
                painting[i][j][1] = sdprint[i][j][1];
                Ubformap(sdprint[i][j][0], sdprint[i][j][1],i,j);
            }
        }
    }
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
   COORD bufferSize = {(short)xlen, (short)ylen};
   COORD bufferCoord = {0, 0};
   // writeRegion必须与bufferSize一致（Right/Bottom为包含边界）：{0,0,xlen-1,ylen-1}
   // 原来写成{xlen,ylen}会多写一行一列未初始化数据（黑块），且覆盖行43+的HUD区域
   SMALL_RECT writeRegion = {0,0, (short)(xlen-1),(short)(ylen-1)};
   WriteConsoleOutput(hConsole, charInfo, bufferSize, bufferCoord, &writeRegion);
}
void openmap()
{
    double rx = jx, ry = jy;
    double jjx = jx, jjy = jy;
    POINT p;
    std::fill(&light[0][0], &light[0][0] + (size_t)WORLD_WIDTH * WORLD_HEIGHT,
              (short)15);
    sf = 1;
    while(1)
    {
        if(clock() - Clocknum >= 0)
        {
            if(GetAsyncKeyState('N') & 0x8000)
                memset(painting, 0, sizeof(painting)), shuaxin();
            UpdateFPS();
            Clocknum = clock();
            p = GetMousePos();
            mpx = To_int(p.x / 2 - 26);
            mpy = To_int(p.y - 14);
            int mprx = mpx, mpry = mpy;   // 修复：原未初始化，首帧按住左键拖动地图时读取随机值
            if(GetAsyncKeyState('M') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
            {
                while(GetAsyncKeyState('M') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
                    continue;
                jx = rx, jy = ry;
                break;
            }
            if(KEY_DOWN(VK_LBUTTON))
            {
                if((mpx == printx-26 || mpx == printx-27) && mpy == -12)
                {
                    while(KEY_DOWN(VK_LBUTTON))
                        continue;
                    int sf_max = WORLD_WIDTH / (printx + 1);
                    if(WORLD_HEIGHT / (printy + 1) < sf_max)
                        sf_max = WORLD_HEIGHT / (printy + 1);
                    if(sf_max > 25)
                        sf_max = 25;
                    if(sf < sf_max)
                        sf++;
                    memset(sdprint, 0, sizeof(sdprint));
                }
                if((mpx == printx-26 || mpx == printx-27) && mpy == -10)
                {
                    while(KEY_DOWN(VK_LBUTTON))
                        continue;
                    if(sf > 1)
                        sf--;
                    memset(sdprint, 0, sizeof(sdprint));
                }
                jjx += (mprx - mpx) * sf;
                jjy += (mpry - mpy) * sf;
                jx = int(jjx + 0.5) / sf * sf;
                jy = int(jjy + 0.5) / sf * sf;
                int jx_min = printx / 2;
                int jy_min = printy / 2;
                int jx_max = WORLD_WIDTH - sf * (printx + 1) + printx / 2;
                int jy_max = WORLD_HEIGHT - sf * (printy + 1) + printy / 2;
                if(jx < jx_min)
                    jx = jx_min;
                if(jy < jy_min)
                    jy = jy_min;
                if(jx > jx_max)
                    jx = jx_max;
                if(jy > jy_max)
                    jy = jy_max;
                if(jjx < jx_min)
                    jjx = jx_min;
                if(jjy < jy_min)
                    jjy = jy_min;
                if(jjx > jx_max)
                    jjx = jx_max;
                if(jjy > jy_max)
                    jjy = jy_max;
            }
            printformap();
            mprx = mpx;
            mpry = mpy;
        }
        //Sleep(1);
    }
}

// RAII：FILE* 句柄守卫，任何 return 路径自动 fclose
struct FileGuard
{
    FILE *fp;
    explicit FileGuard(FILE *f) : fp(f) {}
    ~FileGuard()
    {
        if(fp)
            fclose(fp);
    }
    FileGuard(const FileGuard &) = delete;
    FileGuard &operator=(const FileGuard &) = delete;
};



// ===================== v3 分块存档格式（2DMCBNK） =====================
// 布局：32B 文件头 + 157 个固定偏移列块（s/s2/ep，可原地重写）+ 文件尾 META 标签流
// META 采用 tag+len+payload，未知 tag 跳过；实体逐字段标签化，结构体布局变更不坏档
static constexpr char BIN3_MAGIC[8] = {'2', 'D', 'M', 'C', 'B', 'N', 'K', '\0'};
static constexpr int BIN3_VERSION = 3;
static constexpr int BIN3_HDR = 32;
static constexpr long long CHUNK_BYTES = (long long)CHUNK_COLS * WORLD_HEIGHT * 5;
static constexpr long long CHUNKS_END =
        BIN3_HDR + (long long)WORLD_WIDTH * WORLD_HEIGHT * 5;
static_assert(sizeof(short) == 2 && sizeof(int) == 4, "存档格式假设 2 字节 short / 4 字节 int");
static_assert(CHUNKS_END == BIN3_HDR + (long long)WORLD_WIDTH * WORLD_HEIGHT * 5,
              "分块布局与世界尺寸失配");
static_assert(CHUNKS_END == BIN3_HDR + (long long)NCHUNKS * CHUNK_BYTES
              - (long long)(NCHUNKS * CHUNK_COLS - WORLD_WIDTH) * WORLD_HEIGHT * 5,
              "块数与列宽失配");

enum
{
    TAG_END = 0x00,
    TAG_DOUBLES = 0x01,
    TAG_INTS = 0x02,
    TAG_BEIBAO = 0x03,
    TAG_WEAR = 0x04,
    TAG_SHITI = 0x05,
    TAG_SHITI2 = 0x06,
    TAG_BOX = 0x07,
    TAG_BOIMES = 0x08,
    TAG_BOIMES2 = 0x09,
    TAG_GAMEMODE = 0x0A,
    TAG_EXTRA = 0x0B
};

static constexpr int chunk_col_count(int c)
{
    int n = WORLD_WIDTH - c * CHUNK_COLS;
    return n > CHUNK_COLS ? CHUNK_COLS : n;
}
static constexpr long long chunk_offset(int c)
{
    return BIN3_HDR + (long long)c * CHUNK_BYTES;
}

static void buf_put(vector<unsigned char> &b, const void *p, size_t n)
{
    const unsigned char *q = (const unsigned char *)p;
    b.insert(b.end(), q, q + n);
}
static void buf_u16(vector<unsigned char> &b, unsigned short v)
{
    buf_put(b, &v, 2);
}
static void buf_u32(vector<unsigned char> &b, unsigned int v)
{
    buf_put(b, &v, 4);
}
static void buf_tag(vector<unsigned char> &b, unsigned short tag, const void *p,
                    size_t n)
{
    buf_u16(b, tag);
    buf_u32(b, (unsigned int)n);
    buf_put(b, p, n);
}

static const int ENT_ND = 13, ENT_NI = 8, ENT_NB = 4, ENT_NU8 = 1;
static double guaiwu::*const ENT_D[13] = {
    &guaiwu::HP, &guaiwu::hurt, &guaiwu::gx, &guaiwu::gy, &guaiwu::spgy,
    &guaiwu::spgx, &guaiwu::randomtick, &guaiwu::direction, &guaiwu::sd,
    &guaiwu::gongjitick, &guaiwu::tox, &guaiwu::toy, &guaiwu::maxHP};
static int guaiwu::*const ENT_I[8] = {&guaiwu::type, &guaiwu::jp, &guaiwu::CD,
                                      &guaiwu::AI, &guaiwu::nbt, &guaiwu::chv,
                                      &guaiwu::conl, &guaiwu::conr};
static bool guaiwu::*const ENT_B[4] = {&guaiwu::ch, &guaiwu::isnotouch,
                                       &guaiwu::islife2, &guaiwu::issd};
// u8 组 fid 从 128 起编，避免与同样 len=1 的 bool 组混淆
static unsigned char guaiwu::*const ENT_U8[1] = {&guaiwu::state};

static void buf_entities(vector<unsigned char> &b, const guaiwu *arr)
{
    for(int i = 0; i < 500; i++)
    {
        buf_u16(b, (unsigned short)(ENT_ND + ENT_NI + ENT_NB + ENT_NU8));
        for(int f = 0; f < ENT_ND; f++)
        {
            b.push_back((unsigned char)f);
            b.push_back(8);
            buf_put(b, &(arr[i].*ENT_D[f]), 8);
        }
        for(int f = 0; f < ENT_NI; f++)
        {
            b.push_back((unsigned char)f);
            b.push_back(4);
            buf_put(b, &(arr[i].*ENT_I[f]), 4);
        }
        for(int f = 0; f < ENT_NB; f++)
        {
            b.push_back((unsigned char)f);
            b.push_back(1);
            buf_put(b, &(arr[i].*ENT_B[f]), 1);
        }
        for(int f = 0; f < ENT_NU8; f++)
        {
            b.push_back((unsigned char)(128 + f));
            b.push_back(1);
            buf_put(b, &(arr[i].*ENT_U8[f]), 1);
        }
    }
}

static void build_meta(vector<unsigned char> &m)
{
    double dv[4] = {x, y, life, boss2hp};
    buf_tag(m, TAG_DOUBLES, dv, sizeof(dv));
    int iv[8] = {weidu, back_x, back_y, ed, now_time, night, will_night,
                 will_light};
    buf_tag(m, TAG_INTS, iv, sizeof(iv));
    buf_tag(m, TAG_BEIBAO, beibao, sizeof(int) * 202);
    buf_tag(m, TAG_WEAR, wear, sizeof(wear));
    buf_tag(m, TAG_GAMEMODE, &gamemode, sizeof(int));
    vector<unsigned char> eb;
    buf_entities(eb, shiti);
    buf_tag(m, TAG_SHITI, eb.data(), eb.size());
    eb.clear();
    buf_entities(eb, shiti2);
    buf_tag(m, TAG_SHITI2, eb.data(), eb.size());
    vector<unsigned char> bb;
    unsigned int n = (unsigned int)box.size();
    buf_put(bb, &n, 4);
    for(unsigned int i = 0; i < n; i++)
    {
        buf_put(bb, &box[i].bx, 2);
        buf_put(bb, &box[i].by, 2);
        buf_put(bb, box[i].things, sizeof(box[i].things));
    }
    buf_tag(m, TAG_BOX, bb.data(), bb.size());
    buf_tag(m, TAG_BOIMES, boimes, sizeof(short) * (WORLD_WIDTH + 5));
    buf_tag(m, TAG_BOIMES2, boimes2, sizeof(short) * (WORLD_WIDTH + 5));
    // 地牢扩展数据：能量/金币/关卡进度/天赋
    {
        int extrai[48] = {0};
        extrai[0] = int(energy);
        extrai[1] = int(energyMax);
        extrai[2] = coin;
        extrai[3] = levelUnlocked;
        extrai[4] = currentLevel;
        extrai[5] = currentRoom;
        extrai[6] = roomWavesTotal;
        extrai[7] = roomWavesDone;
        extrai[8] = talentCount;
        extrai[9] = inDungeon ? 1 : 0;
        extrai[10] = roomActive ? 1 : 0;
        for(int t = 0; t < 24 && t < 32; t++) extrai[11 + t] = talents[t];
        extrai[35] = portal_x;   // 存档传送门坐标（加载后恢复，否则传送门失效）
        extrai[36] = portal_y;
        extrai[37] = lifeMax;    // 生命/护甲上限与当前护甲值（天赋加成持久化）
        extrai[38] = armorMax;
        extrai[39] = hujiazhi;
        buf_tag(m, TAG_EXTRA, extrai, sizeof(extrai));
    }
    buf_u16(m, TAG_END);
    buf_u32(m, 0);
}

struct SaveJob
{
    string path;
    bool full;
    vector<int> chunks;
    vector<unsigned char> chunkbuf;
    vector<unsigned char> meta;
};
static mutex g_save_mtx;
static condition_variable g_save_cv, g_done_cv;
static thread g_writer_thread;
static bool g_writer_run = false;
static bool g_job_pending = false;
static bool g_job_active = false;
static bool g_shutdown_done = false;
static bool g_world_active = false;
static SaveJob g_job;

static void save_system_shutdown();
static void save_world(const string &nam, bool blocking, bool force_full = false);
#ifdef _WIN32
static BOOL WINAPI console_ctrl_handler(DWORD type);
#endif

static void snapshot_chunk(vector<unsigned char> &dst, int c)
{
    int c0 = c * CHUNK_COLS;
    size_t n = (size_t)chunk_col_count(c) * WORLD_HEIGHT;
    buf_put(dst, &s[c0][0], n * sizeof(short));
    buf_put(dst, &s2[c0][0], n * sizeof(short));
    buf_put(dst, &ep[c0][0], n * sizeof(bool));
}

static bool file_is_v3(const string &path)
{
    FileGuard fg(fopen(path.c_str(), "rb"));
    if(!fg.fp)
        return false;
    FILE *fp = fg.fp;
    char hdr[BIN3_HDR];
    bool ok = false;
    if(fread(hdr, 1, BIN3_HDR, fp) == BIN3_HDR &&
            memcmp(hdr, BIN3_MAGIC, 8) == 0)
    {
        int w, h, cc;
        memcpy(&w, hdr + 12, 4);
        memcpy(&h, hdr + 16, 4);
        memcpy(&cc, hdr + 20, 4);
        ok = (w == WORLD_WIDTH && h == WORLD_HEIGHT && cc == CHUNK_COLS);
        if(ok)
        {
            fseek(fp, 0, SEEK_END);
            ok = _ftelli64(fp) >= CHUNKS_END;
        }
    }
    return ok;
}

#ifdef _WIN32
// 用独立句柄把文件截断到指定长度（绕开 CRT 流状态），保证存档尾部的旧 META 不残留
static bool truncate_file_to(const string &path, long long end_pos)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER li;
    li.QuadPart = end_pos;
    BOOL ok = SetFilePointerEx(h, li, NULL, FILE_BEGIN);
    if(ok)
        ok = SetEndOfFile(h);
    CloseHandle(h);
    return ok == TRUE;
}
#endif

static bool write_job(SaveJob &job)
{
    if(job.full)
    {
        FileGuard fg(_fsopen(job.path.c_str(), "wb", _SH_DENYWR));
        if(!fg.fp)
            return false;
        FILE *fp = fg.fp;
        char hdr[BIN3_HDR] = {0};
        memcpy(hdr, BIN3_MAGIC, 8);
        int v = BIN3_VERSION;
        int w = WORLD_WIDTH;
        int h = WORLD_HEIGHT;
        int cc = CHUNK_COLS;
        long long mo = CHUNKS_END;
        memcpy(hdr + 8, &v, 4);
        memcpy(hdr + 12, &w, 4);
        memcpy(hdr + 16, &h, 4);
        memcpy(hdr + 20, &cc, 4);
        memcpy(hdr + 24, &mo, 8);
        fwrite(hdr, 1, BIN3_HDR, fp);
        fwrite(job.chunkbuf.data(), 1, job.chunkbuf.size(), fp);
        fwrite(job.meta.data(), 1, job.meta.size(), fp);
        return true;
    }
    long long want_end = -1;
    {
        FileGuard fg(_fsopen(job.path.c_str(), "r+b", _SH_DENYWR));
        if(!fg.fp)
            return false;
        FILE *fp = fg.fp;
        size_t off = 0;
        for(size_t k = 0; k < job.chunks.size(); k++)
        {
            int c = job.chunks[k];
            size_t bytes = (size_t)chunk_col_count(c) * WORLD_HEIGHT * 5;
            _fseeki64(fp, chunk_offset(c), SEEK_SET);
            fwrite(job.chunkbuf.data() + off, 1, bytes, fp);
            off += bytes;
        }
        if(!job.meta.empty())
        {
            // 新 META 先追加到文件尾并提交文件头指针（崩溃时旧 META 仍有效）
            fseek(fp, 0, SEEK_END);
            long long meta_pos = _ftelli64(fp);
            fwrite(job.meta.data(), 1, job.meta.size(), fp);
            fflush(fp);
            _fseeki64(fp, 24, SEEK_SET);
            fwrite(&meta_pos, 8, 1, fp);
            fflush(fp);
            // 压缩：把新 META 搬到列块区正后方，文件头改指新位置，截掉中间全部旧 META。
            // （旧实现截断点取 EOF，永远等于没截，旧 META 无限堆积导致存档膨胀）
            if(meta_pos > CHUNKS_END)
            {
                _fseeki64(fp, CHUNKS_END, SEEK_SET);
                fwrite(job.meta.data(), 1, job.meta.size(), fp);
                fflush(fp);
                long long new_off = CHUNKS_END;
                _fseeki64(fp, 24, SEEK_SET);
                fwrite(&new_off, 8, 1, fp);
                fflush(fp);
                want_end = CHUNKS_END + (long long)job.meta.size();
                _fseeki64(fp, want_end, SEEK_SET);
                fflush(fp);
#ifdef _WIN32
                HANDLE hf = (HANDLE)_get_osfhandle(_fileno(fp));
                if(hf != INVALID_HANDLE_VALUE && hf != NULL)
                    SetEndOfFile(hf);
#endif
            }
        }
    }
#ifdef _WIN32
    // 兜底：文件关闭后用独立句柄校验实际长度，仍有残留则强制截断
    if(want_end > 0)
    {
        long long cur_end = -1;
        {
            FileGuard vg(fopen(job.path.c_str(), "rb"));
            if(vg.fp)
            {
                fseek(vg.fp, 0, SEEK_END);
                cur_end = _ftelli64(vg.fp);
            }
        }
        if(cur_end > want_end)
            truncate_file_to(job.path, want_end);
    }
#endif
    return true;
}

static void writer_loop()
{
    unique_lock<mutex> lk(g_save_mtx);
    while(true)
    {
        g_save_cv.wait(lk, [] { return g_job_pending || !g_writer_run; });
        if(!g_job_pending && !g_writer_run)
            break;
        SaveJob job = std::move(g_job);
        g_job_pending = false;
        g_job_active = true;
        lk.unlock();
        write_job(job);
        lk.lock();
        g_job_active = false;
        g_done_cv.notify_all();
    }
}

static void save_system_init()
{
    static bool ctrl_registered = false;
    static bool atexit_registered = false;
    lock_guard<mutex> lk(g_save_mtx);
    if(g_writer_run)
        return;
    g_writer_run = true;
    g_shutdown_done = false;
    g_writer_thread = thread(writer_loop);
    if(!ctrl_registered)
    {
        ctrl_registered = true;
#ifdef _WIN32
        SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif
    }
    if(!atexit_registered)
    {
        atexit_registered = true;
        atexit(save_system_shutdown);
    }
}

static void save_system_flush()
{
    unique_lock<mutex> lk(g_save_mtx);
    g_done_cv.wait(lk, [] { return !g_job_pending && !g_job_active; });
}

static void save_system_shutdown()
{
    save_system_flush();
    bool do_join = false;
    {
        lock_guard<mutex> lk(g_save_mtx);
        if(!g_shutdown_done)
        {
            g_writer_run = false;
            g_shutdown_done = true;
            do_join = true;
            g_save_cv.notify_one();
        }
    }
    if(do_join && g_writer_thread.joinable())
        g_writer_thread.join();
}

#ifdef _WIN32
static BOOL WINAPI console_ctrl_handler(DWORD type)
{
    if(type == CTRL_CLOSE_EVENT || type == CTRL_SHUTDOWN_EVENT ||
            type == CTRL_LOGOFF_EVENT)
    {
        // 关窗/关机：游戏中先补一次完整保存，再写完挂起任务并join写线程
        if(g_world_active && !zzz.empty())
            save_world(zzz, true);
        save_system_shutdown();
        return TRUE;
    }
    return FALSE;
}
#endif

// blocking=true：等写完再返回（退出/换世界前用）；false：交给后台线程
static void save_world(const string &nam, bool blocking, bool force_full)
{
    save_system_init();
    // 玩家位置/背包等状态几乎每帧都在变，META 每次都随存档写入
    g_dirty_meta = true;
    string path = nam + ".bin";
    SaveJob job;
    job.path = path;
    job.full = force_full || !file_is_v3(path);
    if(job.full)
    {
        for(int c = 0; c < NCHUNKS; c++)
        {
            job.chunks.push_back(c);
            snapshot_chunk(job.chunkbuf, c);
        }
        memset(g_dirty, 0, sizeof(g_dirty));
    }
    else
    {
        for(int c = 0; c < NCHUNKS; c++)
        {
            if(!g_dirty[c])
                continue;
            job.chunks.push_back(c);
            snapshot_chunk(job.chunkbuf, c);
            g_dirty[c] = false;
        }
    }
    if(job.full || g_dirty_meta)
    {
        build_meta(job.meta);
        g_dirty_meta = false;
    }
    if(job.chunks.empty() && job.meta.empty())
        return;
    unique_lock<mutex> lk(g_save_mtx);
    g_job = std::move(job);
    g_job_pending = true;
    g_save_cv.notify_one();
    if(blocking)
        g_done_cv.wait(lk, [] { return !g_job_pending && !g_job_active; });
}

static void parse_entities_v3(const unsigned char *p, size_t len, guaiwu *arr)
{
    memset(arr, 0, sizeof(guaiwu) * 500);
    size_t off = 0;
    int idx = 0;
    while(off + 2 <= len && idx < 500)
    {
        unsigned short nf;
        memcpy(&nf, p + off, 2);
        off += 2;
        guaiwu &e = arr[idx++];
        for(int f = 0; f < nf && off + 2 <= len; f++)
        {
            unsigned char fid = p[off], flen = p[off + 1];
            off += 2;
            if(off + flen > len)
                break;
            if(flen == 8 && fid < ENT_ND)
                memcpy(&(e.*ENT_D[fid]), p + off, 8);
            else if(flen == 4 && fid < ENT_NI)
                memcpy(&(e.*ENT_I[fid]), p + off, 4);
            else if(flen == 1 && fid < ENT_NB)
                memcpy(&(e.*ENT_B[fid]), p + off, 1);
            else if(flen == 1 && fid >= 128 && fid - 128 < ENT_NU8)
                memcpy(&(e.*ENT_U8[fid - 128]), p + off, 1);
            off += flen;
        }
    }
}

static bool load_binary_v3(const char *filename)
{
    FileGuard fg(fopen(filename, "rb"));
    if(!fg.fp)
        return false;
    FILE *fp = fg.fp;
    char hdr[BIN3_HDR];
    if(fread(hdr, 1, BIN3_HDR, fp) != BIN3_HDR ||
            memcmp(hdr, BIN3_MAGIC, 8) != 0)
        return false;
    int w, h, cc;
    long long meta_offset;
    memcpy(&w, hdr + 12, 4);
    memcpy(&h, hdr + 16, 4);
    memcpy(&cc, hdr + 20, 4);
    memcpy(&meta_offset, hdr + 24, 8);
    if(w != WORLD_WIDTH || h != WORLD_HEIGHT || cc != CHUNK_COLS ||
            meta_offset < CHUNKS_END)
        return false;
    for(int c = 0; c < NCHUNKS; c++)
    {
        int c0 = c * CHUNK_COLS;
        size_t n = (size_t)chunk_col_count(c) * WORLD_HEIGHT;
        _fseeki64(fp, chunk_offset(c), SEEK_SET);
        if(fread(&s[c0][0], sizeof(short), n, fp) != n ||
                fread(&s2[c0][0], sizeof(short), n, fp) != n ||
                fread(&ep[c0][0], sizeof(bool), n, fp) != n)
            return false;
    }
    _fseeki64(fp, meta_offset, SEEK_SET);
    vector<unsigned char> meta;
    {
        char tmp[65536];
        size_t r;
        while((r = fread(tmp, 1, sizeof(tmp), fp)) > 0)
        {
            meta.insert(meta.end(), tmp, tmp + r);
            if(meta.size() > (size_t)64 * 1024 * 1024)
                return false;
        }
    }
    gamemode = 1;
    size_t off = 0;
    while(off + 6 <= meta.size())
    {
        unsigned short tag;
        unsigned int len;
        memcpy(&tag, &meta[off], 2);
        memcpy(&len, &meta[off + 2], 4);
        off += 6;
        if(tag == TAG_END)
            break;
        if(off + len > meta.size())
            break;
        const unsigned char *p = &meta[off];
        switch(tag)
        {
        case TAG_DOUBLES:
            if(len >= 32)
            {
                double dv[4];
                memcpy(dv, p, 32);
                x = dv[0], y = dv[1], life = dv[2], boss2hp = dv[3];
            }
            break;
        case TAG_INTS:
            if(len >= 32)
            {
                int iv[8];
                memcpy(iv, p, 32);
                weidu = iv[0], back_x = iv[1], back_y = iv[2], ed = iv[3];
                now_time = iv[4], night = iv[5], will_night = iv[6],
                will_light = iv[7];
            }
            break;
        case TAG_BEIBAO:
            if(len >= sizeof(int) * 202)
            {
                memcpy(beibao, p, sizeof(int) * 202);
                for(int i = 0; i < 101; i++)
                    if(beibao[i][0] < 0 || beibao[i][0] > 113)
                        beibao[i][0] = 0, beibao[i][1] = 0;
            }
            break;
        case TAG_WEAR:
            if(len >= sizeof(wear))
                memcpy(wear, p, sizeof(wear));
            break;
        case TAG_GAMEMODE:
            if(len >= 4)
                memcpy(&gamemode, p, 4);
            break;
        case TAG_SHITI:
            parse_entities_v3(p, len, shiti);
            break;
        case TAG_SHITI2:
            parse_entities_v3(p, len, shiti2);
            break;
        case TAG_BOX:
            if(len >= 4)
            {
                unsigned int n;
                memcpy(&n, p, 4);
                box.clear();
                if(4 + (size_t)n * 58 <= len)
                {
                    for(unsigned int i = 0; i < n; i++)
                    {
                        const unsigned char *q = p + 4 + (size_t)i * 58;
                        Box_ b;
                        memset(&b, 0, sizeof(b));
                        memcpy(&b.bx, q, 2);
                        memcpy(&b.by, q + 2, 2);
                        memcpy(b.things, q + 4, sizeof(b.things));
                        for(int j = 0; j < 27; j++)
                            if(b.things[j] % 1000 > 113)
                                b.things[j] = 0;
                        box.push_back(b);
                    }
                }
                boxlen = (int)box.size();
            }
            break;
        case TAG_BOIMES:
            if(len >= sizeof(short) * (WORLD_WIDTH + 5))
                memcpy(boimes, p, sizeof(short) * (WORLD_WIDTH + 5));
            break;
        case TAG_BOIMES2:
            if(len >= sizeof(short) * (WORLD_WIDTH + 5))
                memcpy(boimes2, p, sizeof(short) * (WORLD_WIDTH + 5));
            break;
        case TAG_EXTRA:
            {
                int n = (int)len / 4;
                if(n > 48) n = 48;
                if(n >= 11)
                {
                    int ei[48];
                    memset(ei, 0, sizeof(ei));
                    memcpy(ei, p, (size_t)n * 4);
                    energy = ei[0];
                    energyMax = ei[1] > 0 ? ei[1] : 100;
                    coin = ei[2];
                    levelUnlocked = ei[3];
                    currentLevel = ei[4];
                    currentRoom = ei[5];
                    roomWavesTotal = ei[6];
                    roomWavesDone = ei[7];
                    talentCount = ei[8];
                    inDungeon = ei[9] ? true : false;
                    roomActive = ei[10] ? true : false;
                    for(int t = 0; t < 24 && t < 32; t++) talents[t] = ei[11 + t];
                    for(int t = 0; t < talentCount && t < 24; t++)
                        if(talents[t] < 0 || talents[t] > 19) talents[t] = 0;
                    // 恢复传送门坐标
                    if(n >= 37)
                    {
                        if(ei[35] > 0 && ei[35] < WORLD_WIDTH) portal_x = ei[35];
                        if(ei[36] > 0 && ei[36] < WORLD_HEIGHT) portal_y = ei[36];
                    }
                    // 恢复生命/护甲上限与当前护甲值（旧档默认200）
                    if(n >= 40)
                    {
                        if(ei[37] >= 200 && ei[37] <= 2000) lifeMax = ei[37];
                        if(ei[38] >= 200 && ei[38] <= 2000) armorMax = ei[38];
                        if(ei[39] > 0 && ei[39] <= armorMax) hujiazhi = ei[39];
                        else hujiazhi = armorMax;
                    }
                    else
                        hujiazhi = armorMax;
                    // 纯地牢模式：存档加载后一律直接重新进入关卡
                    // （房间墙体/怪物状态不持久化，重进保证结构完整）
                    // 旧档/建档瞬间的存档 inDungeon=0 且 curLevel=0，若不重进会
                    // 把玩家留在空主世界坐标上（黑屏卡死），进度看似"没保存"
                    {
                        int rl = currentLevel > 0 ? currentLevel : 1;
                        if(levelUnlocked < 1) levelUnlocked = 1;
                        if(rl > levelUnlocked) rl = levelUnlocked;
                        inDungeon = false;
                        roomActive = false;
                        roomWavesLeft = roomWaveCD = roomWavesDone = roomWavesTotal = 0;
                        void enter_level(int n);   // 前向声明
                        enter_level(rl);
                    }
                }
            }
            break;
        default:
            break;
        }
        off += len;
    }
    clear_entity_hash();
    for(int i = 0; i < 500; i++)
    {
        if(shiti[i].type > 0)
            add_entity_to_hash(i, shiti[i].gx, shiti[i].gy);
    }
    setlight();
    memset(g_dirty, 0, sizeof(g_dirty));
    g_dirty_meta = false;
    return true;
}

static bool load_world(const string &nam)
{
    string path = nam + ".bin";
    char mg[8] = {0};
    size_t n;
    {
        FileGuard fg(fopen(path.c_str(), "rb"));
        if(!fg.fp)
            return false;
        n = fread(mg, 1, 8, fg.fp);
    }
    if(n >= 8 && memcmp(mg, BIN3_MAGIC, 8) == 0)
    {
        if(!load_binary_v3(path.c_str()))
            return false;
        // 正常存档 = 列块区 + 一份 META；明显超出说明有历史残留旧 META，
        // 尾部截断清不掉文件中间的残留，必须全量重写压缩
        long long fsz = 0;
        {
            FileGuard sg(fopen(path.c_str(), "rb"));
            if(sg.fp)
            {
                fseek(sg.fp, 0, SEEK_END);
                fsz = _ftelli64(sg.fp);
            }
        }
        if(fsz > CHUNKS_END + 2LL * 1024 * 1024)
            save_world(nam, true, true);
        else
            save_world(nam, true);
        return true;
    }
    return false;   // 仅支持v3格式（2DMCBNK）；v2旧档为地牢改版前主世界存档，不再兼容
}

int r;
void save(string nam)
{
    save_world(nam, true);
}
char saves[100][55];
int read_s;
bool read_savelist()
{
    for(int i = 0; i < 100; i++)   // 修复：saves[100][55] 共100行，原只清51行
        for(int j = 0; j <= 53; j++)
            saves[i][j] = '\0';
    read_s = 0;
    FileGuard fg(fopen("savelist.txt", "r"));
    if(fg.fp == NULL)
        return false;
    FILE *fp = fg.fp;
    if(fscanf(fp, "%d", &read_s) != 1 || read_s <= 0)
    {
        read_s = 0;
        return false;
    }
    if(read_s > 100)
        read_s = 100;
    for(int i = 0; i < read_s; i++)
    {
        if(fscanf(fp, "%54s", saves[i]) != 1)
        {
            read_s = i;
            break;
        }
    }
    return true;
}
bool save_savelist(string add_)
{
    int save_s = 0;
    char sss[55];
    memset(sss, '\0', sizeof(sss));
    {
        FileGuard fg(fopen("savelist.txt", "r"));
        if(fg.fp != NULL)
        {
            if(fscanf(fg.fp, "%d", &save_s) != 1)
                save_s = 0;
            if(save_s > 99)
                save_s = 99;
            for(int i = 0; i < save_s; i++)
            {
                if(fscanf(fg.fp, "%54s", saves[i]) != 1)
                {
                    save_s = i;
                    break;
                }
            }
        }
    }
    FileGuard fg2(fopen("savelist.txt", "w"));
    if(fg2.fp == NULL)
        return false;
    FILE *fp2 = fg2.fp;
    fprintf(fp2, "%d ", save_s + 1);
    for(int i = 0; i < save_s; i++)
    {
        fprintf(fp2, "%s ", saves[i]);
    }
    for(size_t i = 0; i < add_.length() && i < 54; i++)
        sss[i] = add_[i];
    fprintf(fp2, "%s ", sss);
    return true;
}
bool delete_savelist(string add_)
{
    int save_s = 0;
    {
        FileGuard fg(fopen("savelist.txt", "r"));
        if(fg.fp != NULL)
        {
            if(fscanf(fg.fp, "%d", &save_s) != 1)
                save_s = 0;
            if(save_s > 100)
                save_s = 100;
            for(int i = 0; i < save_s; i++)
            {
                if(fscanf(fg.fp, "%54s", saves[i]) != 1)
                {
                    save_s = i;
                    break;
                }
            }
        }
    }
    int kept = 0;
    for(int i = 0; i < save_s; i++)
        if(saves[i] != add_)
            kept++;
    FileGuard fg2(fopen("savelist.txt", "w"));
    if(fg2.fp != NULL)
    {
        fprintf(fg2.fp, "%d ", kept);
        for(int i = 0; i < save_s; i++)
        {
            if(saves[i] != add_)
                fprintf(fg2.fp, "%s ", saves[i]);
        }
    }
    string txt_name = add_ + ".txt";
    string bin_name = add_ + ".bin";
    remove(txt_name.c_str());
    remove(bin_name.c_str());
    return true;
}
int findbox(int xxx, int yyy)
{
    for(int i = 0; i < boxlen; i++)
    {
        if(box[i].bx == xxx && box[i].by == yyy)
        {
            return i;
        }
    }
    return -1;
}
int alloc_box(int xxx, int yyy)
{
    for(int i = 0; i < boxlen; i++)
    {
        if(box[i].bx == 5005)
        {
            box[i].bx = (unsigned short)xxx;
            box[i].by = (unsigned short)yyy;
            memset(box[i].things, 0, sizeof(box[i].things));
            return i;
        }
    }
    box.push_back({{0}, (unsigned short)(xxx), (unsigned short)(yyy)});
    boxlen++;
    return boxlen - 1;
}
void add_box_item(int idx, int item, int count)
{
    if(idx < 0 || item <= 0 || count <= 0)
        return;
    if(count > 64)
        count = 64;
    for(int i = 0; i <= 24; i++)
    {
        if(box[idx].things[i] % 1000 == item && box[idx].things[i] / 1000 < 64)
        {
            int cur = box[idx].things[i] / 1000;
            int add = count;
            if(cur + add > 64)
                add = 64 - cur;
            if(add > 0)
            {
                box[idx].things[i] = item + (cur + add) * 1000;
                count -= add;
            }
            if(count <= 0)
                return;
        }
    }
    for(int i = 0; i <= 24; i++)
    {
        if(box[idx].things[i] == 0)
        {
            box[idx].things[i] = item + count * 1000;
            return;
        }
    }
}
void enter_level(int n);              // 前向声明：存档加载后重进关卡
bool read(string nam)
{
    save_system_flush();
    return load_world(nam);
}
void Start()
{
    life = lifeMax;
    l_life=30;
    l_beibao[1][0]=100;
    if(weidu==2)
    {
        swap_map(0);
    }
    // 检查重生点床是否存在
    if(s[back_x][back_y] == 90)
    {
        // 床存在，在床上方重生
        y = back_y - 1;
        x = back_x;
    }
    else
    {
        // 床被挖走，重生到世界中心地表
        x = WORLD_WIDTH / 2;
        y = WORLD_HEIGHT - 2;
        for(int j = 1; j < WORLD_HEIGHT; j++)
        {
            if(blocktouch[s[WORLD_WIDTH / 2][j]] >= 1)
            {
                y = j - 2;
                break;
            }
        }
    }
    // 重置玩家速度
    spx = 0;
    spy = 0;
    jump = 0;
    setlight();
}
int getbeibao(int type)
{
    // 枪械(95-98)不可堆叠：每个枪械独占一个格子
    if(type >= 95 && type <= 98)
    {
        for(int i = 1; i <= 100; i++)
            if(beibao[i][1] <= 0)
                return i;
        return 0;
    }
    for(int i = 1; i <= 100; i++)
        if(beibao[i][1] <= 0 || (beibao[i][0] == type && beibao[i][1] < 64))
            return i;
    return 0;
}
int getbeibao_2(int type)
{
    for(int i = 1; i <= 100; i++)
        if(beibao[i][1] > 0 && beibao[i][0] == type)
            return i;
    return 0;
}
int ls[101][2];
int getbeibao_3(int type, int nd)
{
    // 修复：先统计总量，不足时直接失败且不动背包。原实现失败路径会把沿途堆叠清零，
    // 依赖调用方 ls[][] 备份兜底；合成材料分散多处时行为正确性完全取决于调用方
    int total = 0;
    for(int i = 1; i <= 100; i++)
        if(beibao[i][1] > 0 && beibao[i][0] == type)
            total += beibao[i][1];
    if(total < nd)
        return 0;
    int m = nd;
    for(int i = 1; i <= 100; i++)
    {
        if(beibao[i][1] > 0 && beibao[i][0] == type)
        {
            if(beibao[i][1] >= m)
            {
                beibao[i][1] -= m;
                return 1;
            }
            m -= beibao[i][1], beibao[i][1] = 0;
        }
    }
    return 0;
}
void wajue(int sx, int sy)
{
    if(sx < 0 || sx >= WORLD_WIDTH || sy < 0 || sy >= WORLD_HEIGHT)
        return;
    if(sx == portal_x && sy == portal_y) return;   // 保护关卡传送门火把，不可挖
    if(blockwj[s[sx][sy]] > 0)
    {
        if(s[sx][sy] == 2 || s[sx][sy] == 40||s[sx][sy] == 66||s[sx][sy] == 67)
        {
            if(beibao[cho][0] == 74)
                wj += 18;
            else if(beibao[cho][0] == 78)
                wj += 17;
            else if(beibao[cho][0] == 50)
                wj += 15;
            else if(beibao[cho][0] == 48)
                wj += 12;
            else   if(beibao[cho][0] == 18)
                wj += 9;
            else
                wj += 3;
        }
        else
        {
            if(beibao[cho][0] == 72)
                wj += 18;
            else if(beibao[cho][0] == 76)
                wj += 17;
            else if(beibao[cho][0] == 15)
                wj += 15;
            else if(beibao[cho][0] == 14)
                wj += 11;
            else if(beibao[cho][0] == 8)
                wj += 8;
            else if(beibao[cho][0] == 7)
                wj += 5;
            else
                wj += 3;
        }
    }
    else
        return;
    if(wj > blockwj[s[sx][sy]])
    {
        wjlv = 1;
        if(beibao[cho][0] == 7)
            wjlv = 2;
        if(beibao[cho][0] == 8)
            wjlv = 3;
        if(beibao[cho][0] == 76)
            wjlv = 4;
        if(beibao[cho][0] == 14)
            wjlv = 4;
        if(beibao[cho][0] == 15)
            wjlv = 5;
        if(beibao[cho][0] == 72)
            wjlv = 6;
        if(wjlv >= blocklv[s[sx][sy]])
        {
            int U = 0;   // 关键修复：挖关卡金矿(103)分支不给U赋值，原未初始化读取是未定义行为
            if(s[sx][sy] == 103)   // 关卡金矿：挖出金币
            {
                add_coins(2 + rand() % 4);
                wj = 0;
            }
            else if(s[sx][sy]!=63)
            {
                U = getbeibao(s[sx][sy]);
                if(U != 0)
                {
                    beibao[U][1]++, beibao[U][0] = s[sx][sy], wj = 0;
                }
            }
            else
            {
                for(int i=1; i<=rand()%5+2; i++)
                {
                    U = getbeibao(65);
                    if(U != 0)
                    {
                        beibao[U][1]++, beibao[U][0] = 65, wj = 0;
                    }
                }
            }
            if(s[sx][sy] == 40)
            {
                int del = findbox(sx, sy);
                int uu;
                if(del != -1)
                {
                    box[del].bx = 5005, box[del].by = 0;
                    for(int i = 0; i <= 24; i++)
                    {
                        if(box[del].things[i] % 1000 == 0 ||
                                box[del].things[i] % 1000 > 91)
                            continue;
                        for(int j = 1; j <= box[del].things[i] / 1000; j++)
                        {
                            uu = getbeibao(box[del].things[i] % 1000);
                            if(uu != 0)
                                beibao[uu][1]++, beibao[uu][0] = box[del].things[i] % 1000;
                        }
                        box[del].things[i] = 0;
                    }
                }
            }
            if(U != 0)
            {
                s[sx][sy] = 0;
                mark_dirty_col(sx);
            }
        }
        else if(wj > blockwj[s[sx][sy]] * 6)
        {
            if(s[sx][sy] == 23)
            {
                if(rand()%10==0)
                {
                    int U = getbeibao(55);
                    if(U != 0)
                    {
                        beibao[U][1]++, beibao[U][0] = 55, wj = 0;
                    }
                }
                if(rand()%10==0)
                {
                    int U = getbeibao(6);
                    if(U != 0)
                    {
                        beibao[U][1]++, beibao[U][0] = 6, wj = 0;
                    }
                }
            }
            s[sx][sy] = 0, wj = 0;
            mark_dirty_col(sx);
        }
    }
    setlight();
    Clear();
}
int sword=0;
void make_eow()
{
    int spwan_x=x+rand()%31-15;
    make_st(9,20,12,spwan_x,y+30,0,2);
    for(int i=1; i<=38+godeater*30; i++)
    {
        make_st(10,20,8,spwan_x,y+30,0,1);
    }
    make_st(11,20,5,spwan_x,y+30,0,3);
}
void block_(int xxx, int yyy)
{
	if(beibao[cho][1] ==0)return;
    if(xxx < 1 || xxx >= WORLD_WIDTH - 1 || yyy < 1 || yyy >= WORLD_HEIGHT - 1)
        return;
    if(xxx==To_int(x)&&yyy==To_int(y))return;
    if(beibao[cho][0] == 51||beibao[cho][0] == 75)
    {
        if(beibao[cho][0] == 75)make_eow();
        else make_st(5, 500, 8, mpx, mpy);
        beibao[cho][1]--;
        return;
    }
    if(beibao[cho][0] == 60)
    {
        if(sword==0)
        {
            sword=120;
            double ttx=abs(x-mpx);
            double tty=abs(y-mpy);
            double total=ttx+tty+0.001;
            double px=ttx/total;
            double py=tty/total;
            if(x-mpx<0)px*=-1;
            if(y-mpy<0)py*=-1;
            wudi=20;
            spx=30*-px;
            spy=20*-py;
        }
        return;
    }
    if(beibao[cho][0] == 6 || beibao[cho][0] == 7 || beibao[cho][0] == 8 ||
            beibao[cho][0] == 13 || beibao[cho][0] == 14 || beibao[cho][0] == 15 ||
            beibao[cho][0] == 18 || beibao[cho][0] == 17 || beibao[cho][0] == 25 ||
            beibao[cho][0] == 37 || (beibao[cho][0] >= 41 && beibao[cho][0] <= 52)||(beibao[cho][0]>=60&&beibao[cho][0]<=61)||(beibao[cho][0]>=64&&beibao[cho][0]<=65)||(beibao[cho][0]>=70&&beibao[cho][0]!=87&&beibao[cho][0]!=90)||(beibao[cho][0]>=94&&beibao[cho][0]<=101)||(beibao[cho][0]>=105&&beibao[cho][0]<=112))
        return;
    if((s[xxx + 1][yyy] == 0 || s[xxx + 1][yyy] == 11 ||
            (s[xxx + 1][yyy] >= 19 && s[xxx + 1][yyy] <= 22) ||
            (s[xxx + 1][yyy] >= 32 && s[xxx + 1][yyy] <= 34)) &&
            (s[xxx - 1][yyy] == 0 || s[xxx - 1][yyy] == 11 ||
             (s[xxx - 1][yyy] >= 19 && s[xxx - 1][yyy] <= 22) ||
             (s[xxx - 1][yyy] >= 32 && s[xxx - 1][yyy] <= 34)) &&
            (s[xxx][yyy + 1] == 0 || s[xxx][yyy + 1] == 11 ||
             (s[xxx][yyy + 1] >= 19 && s[xxx][yyy + 1] <= 22) ||
             (s[xxx][yyy + 1] >= 32 && s[xxx][yyy + 1] <= 34)) &&
            (s[xxx][yyy - 1] == 0 || s[xxx][yyy - 1] == 11 ||
             (s[xxx][yyy - 1] >= 19 && s[xxx][yyy - 1] <= 22) ||
             (s[xxx][yyy - 1] >= 32 && s[xxx][yyy - 1] <= 34)))
        return;
    bool can_place=(s[xxx][yyy] == 0 || s[xxx][yyy] == 11 ||
    (s[xxx][yyy] >= 19 && s[xxx][yyy] <= 22) ||
    (s[xxx][yyy] >= 32 && s[xxx][yyy] <= 34)||s[xxx][yyy] == 92);
    if(beibao[cho][0] == 26)
    {
        if(can_place)
        {
            s[xxx][yyy] = 19;
            mark_dirty_col(xxx);
            beibao[cho][0] = 25;
            setlight();
            Clear();
        }
        return;
    }
    if(beibao[cho][0] == 35)
    {
        if(can_place)
        {
            s[xxx][yyy] = 32;
            mark_dirty_col(xxx);
            beibao[cho][0] = 25;
            setlight();
            Clear();
        }
        return;
    }
    if(beibao[cho][1] > 0 && can_place)
    {
        s[xxx][yyy] = beibao[cho][0];
        mark_dirty_col(xxx);
        beibao[cho][1]--;
        if(beibao[cho][0] == 40)
        {
            bool pus = 1;
            for(int i = 0; i < boxlen && pus; i++)
            {
                if(box[i].bx == 5005)
                {
                    box[i].bx = xxx, box[i].by = yyy;
                    pus = false;
                }
            }
            if(pus)
            {
                box.push_back({{0}, (unsigned short)(xxx), (unsigned short)(yyy)}),
                boxlen++;
            }
        }
    }
    setlight();
    Clear();
}
void SetClean()
{
    Setpos(0, 31);
    Color(0);
    cout << "                                                                    "
         "           ";
    memset(painting, 0, sizeof(painting));
}
string g_cmd_input;
HWND g_hEdit;
void execute_command(const string &cmd);

LRESULT CALLBACK CmdWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_COMMAND:
            if(LOWORD(wParam) == 1001 && HIWORD(wParam) == EN_CHANGE)
            {
                char buf[256];
                GetWindowTextA(g_hEdit, buf, sizeof(buf));
            }
            break;
        case WM_KEYDOWN:
            if(wParam == VK_ESCAPE)
            {
                g_cmd_input = "";
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            g_cmd_input = "";
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK EditWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_KEYDOWN:
            if(wParam == VK_RETURN)
            {
                char buf[256];
                GetWindowTextA(hWnd, buf, sizeof(buf));
                g_cmd_input = buf;
                DestroyWindow(GetParent(hWnd));
                return 0;
            }
            else if(wParam == VK_ESCAPE)
            {
                g_cmd_input = "";
                DestroyWindow(GetParent(hWnd));
                return 0;
            }
            break;
    }
    return CallWindowProcA((WNDPROC)GetWindowLongPtrA(hWnd, GWLP_USERDATA), hWnd, msg, wParam, lParam);
}

void getin()
{
    GetIn = -4;
    g_cmd_input = "";
    HINSTANCE hInst = GetModuleHandleA(NULL);
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = CmdWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "2DMC_CmdWndClass";
    RegisterClassExA(&wc);
    HWND hWnd = CreateWindowExA(
                    WS_EX_TOPMOST | WS_EX_CLIENTEDGE,
                    "2DMC_CmdWndClass",
                    "2DMC Command",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                    100, 100, 420, 100,
                    NULL, NULL, hInst, NULL
                );
    g_hEdit = CreateWindowExA(
                  0,
                  "EDIT",
                  "",
                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
                  10, 15, 380, 25,
                  hWnd, (HMENU)1001, hInst, NULL
              );
    SetWindowLongPtrA(g_hEdit, GWLP_USERDATA, (LONG_PTR)GetWindowLongPtrA(g_hEdit, GWLP_WNDPROC));
    SetWindowLongPtrA(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditWndProc);
    SetFocus(g_hEdit);
    MSG msg;
    while(GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    UnregisterClassA("2DMC_CmdWndClass", hInst);
    if(!g_cmd_input.empty())
    {
        execute_command(g_cmd_input);
    }
    SetClean();
}

void execute_command(const string &cmd)
{
    stringstream ss(cmd);
    string cmd_name;
    ss >> cmd_name;
    if(cmd_name == "help" || cmd_name == "/help")
    {
        Color(0);
        Setpos(0, 28);
        cout << " /help - 帮助  /give <item> <n> - 给物品  /heal - 满血          ";
        Setpos(0, 29);
        cout << " /life <n> - 设生命  /kill - 自杀  /gamemode - 切换模式        ";
        Setpos(0, 30);
        cout << " /clear - 清背包  /time <n> - 设时间  /tp <x> <y> - 传送       ";
        Setpos(0, 31);
        cout << " /summon <name> [n] - 召唤(僵尸/骷髅/苦力怕/蜘蛛/羊/猪等)      ";
        Sleep(3000);
    }
    else if(cmd_name == "heal" || cmd_name == "/heal")
    {
        life = lifeMax;
        hujiazhi = armorMax;
        Setpos(1, 31);
        cout << " 生命/护甲已恢复                                               ";
    }
    else if(cmd_name == "life" || cmd_name == "/life")
    {
        int w = 0;
        ss >> w;
        if(w >= 0 && w <= 20)
        {
            life = w;
            Setpos(1, 31);
            cout << " 生命已设置为 " << w << "                                     ";
        }
        else
        {
            Setpos(1, 31);
            cout << " 错误: 生命必须在0-20之间                                      ";
        }
    }
    else if(cmd_name == "kill" || cmd_name == "/kill")
    {
        life = 0;
        Start();
        spx = 0;
        spy = 0;
        jump = 0;
        shuaxin();
        Setpos(1, 31);
        cout << " 你死了，已重生到出生点                                       ";
    }
    else if(cmd_name == "gamemode" || cmd_name == "/gamemode")
    {
        gamemode = 1 - gamemode;
        Setpos(1, 31);
        cout << " 游戏模式已切换为 " << (gamemode == 1 ? "生存" : "创造") << "             ";
    }
    else if(cmd_name == "clear" || cmd_name == "/clear")
    {
        memset(beibao, 0, sizeof(beibao));
        Setpos(1, 31);
        cout << " 背包已清空                                                    ";
    }
    else if(cmd_name == "give" || cmd_name == "/give")
    {
        string item_name;
        int amount = 1;
        ss >> item_name >> amount;
        if(amount <= 0 || amount > 64)
        {
            Setpos(1, 31);
            cout << " 错误: 数量必须在1-64之间                                      ";
            return;
        }
        auto it = thing.find(item_name);
        if(it == thing.end())
        {
            Setpos(1, 31);
            cout << " 错误: 未知物品: " << item_name << "                          ";
            return;
        }
        int item_id = it->second;
        int slot = getbeibao(item_id);
        if(slot > 0)
        {
            beibao[slot][0] = item_id;
            beibao[slot][1] += amount;
            if(beibao[slot][1] > 64) beibao[slot][1] = 64;
            Setpos(1, 31);
            cout << " 已获得 " << amount << " " << test[item_id] << "              ";
        }
        else
        {
            Setpos(1, 31);
            cout << " 错误: 背包已满                                                ";
        }
    }
    else if(cmd_name == "EM_wzl")
    {
        // 作弊：全套下界合金 + 下界合金剑 + 64个金苹果
        const char* cheatItems[] = {
            "netherite_helmet", "netherite_chestplate",
            "netherite_leggings", "netherite_boots",
            "netherite_sword", "golden_apple"
        };
        int cheatCounts[] = {1, 1, 1, 1, 1, 64};
        for(int c = 0; c < 6; c++)
        {
            auto it = thing.find(cheatItems[c]);
            if(it == thing.end()) continue;
            int item_id = it->second;
            int slot = getbeibao(item_id);
            if(slot > 0)
            {
                beibao[slot][0] = item_id;
                beibao[slot][1] += cheatCounts[c];
                if(beibao[slot][1] > 64) beibao[slot][1] = 64;
            }
        }
        Setpos(1, 31);
        cout << " 作弊已生效：全套下界合金+剑+64金苹果                  ";
    }
    else if(cmd_name == "time" || cmd_name == "/time")
    {
        int w = 0;
        ss >> w;
        if(w >= 0 && w <= 40000)
        {
            now_time = w;
            night = (w >= 20000) ? 1 : 0;
            Setpos(1, 31);
            cout << " 时间已设置为 " << w << "                                     ";
        }
        else
        {
            Setpos(1, 31);
            cout << " 错误: 时间必须在0-40000之间                                  ";
        }
    }
    else if(cmd_name == "tp" || cmd_name == "/tp")
    {
        int w = 0, h = 0;
        ss >> w >> h;
        // 使用与显示一致的坐标系：x相对世界中心，y从底部向上
        int tx = w + WORLD_WIDTH / 2;
        int ty = WORLD_HEIGHT - 1 - h;
        if(tx >= 0 && tx < WORLD_WIDTH && ty >= 0 && ty < WORLD_HEIGHT)
        {
            x = tx;
            y = ty;
            shuaxin();
            Setpos(1, 31);
            cout << " 已传送到 (" << w << ", " << h << ")                          ";
        }
        else
        {
            Setpos(1, 31);
            cout << " 错误: 位置无效                                                ";
        }
    }
    else if(cmd_name == "summon" || cmd_name == "/summon")
    {
        string name;
        int amount = 1;
        ss >> name >> amount;
        if(amount <= 0 || amount > 50)
        {
            Setpos(1, 31);
            cout << " 错误: 数量必须在1-50之间                                      ";
            return;
        }
        // 实体名 -> type 映射
        int typ = -1;
        if(name == "zombie" || name == "僵尸") typ = 1;
        else if(name == "skeleton" || name == "骷髅") typ = 3;
        else if(name == "creeper" || name == "苦力怕") typ = 2;
        else if(name == "spider" || name == "蜘蛛") typ = 6;
        else if(name == "pigman" || name == "僵尸猪灵") typ = 7;
        else if(name == "sheep" || name == "羊") typ = 12;
        else if(name == "pig" || name == "猪") typ = 13;
        else if(name == "arrow" || name == "箭") typ = 4;
        else if(name == "wither" || name == "凋灵") typ = 5;
        if(typ < 0)
        {
            Setpos(1, 31);
            cout << " 错误: 未知实体: " << name << "                              ";
            return;
        }
        int cnt = 0;
        for(int k = 0; k < amount; k++)
        {
            // 在玩家附近随机位置生成
            int tx = To_int(x) + (rand() % 11 - 5);
            int ty = To_int(y) + (rand() % 7 - 3);
            if(tx <= 0 || ty <= 0 || tx >= WORLD_WIDTH - 1 || ty >= WORLD_HEIGHT - 1)
                continue;
            // 找一个空气位置
            if(s[tx][ty] != 0 && s[tx][ty] != 11)
            {
                for(int dy = 1; dy <= 5 && ty - dy > 0; dy++)
                {
                    if(s[tx][ty - dy] == 0 || s[tx][ty - dy] == 11)
                    {
                        ty -= dy;
                        break;
                    }
                }
            }
            if(s[tx][ty] == 0 || s[tx][ty] == 11)
            {
                if(typ == 1) make_st(1, 20, 4, tx, ty);
                else if(typ == 2) make_st(2, 20, 50, tx, ty);
                else if(typ == 3) make_st(3, 20, 3, tx, ty);
                else if(typ == 4) make_st(4, 0, 0, tx, ty);
                else if(typ == 5) make_st(5, 500, 8, tx, ty);
                else if(typ == 6) make_st(6, 16, 3, tx, ty);
                else if(typ == 7) make_st(7, 20, 6, tx, ty);
                else if(typ == 12) make_st(12, 8, 0, tx, ty);
                else if(typ == 13) make_st(13, 8, 0, tx, ty);
                cnt++;
            }
        }
        Setpos(1, 31);
        cout << " 已召唤 " << cnt << " 个 " << name << "                                    ";
    }
    else
    {
        Setpos(1, 31);
        cout << " 未知命令: " << cmd_name << "，输入 /help 查看帮助             ";
    }
}
short getnew[WORLD_WIDTH][WORLD_HEIGHT];
int TT;
void block_update()
{
    TT++;
    // 性能修复：getnew 为整世界尺寸数组(约20MB)，原每10tick全量memset；
    // 流体扫描区域仅 x±120 / y±80，改为按区域清零（四周多留1格：
    // 扫描会读 getnew[i+1][j]、getnew[i-1][j]、getnew[i][j+1]）
    {
        int gi0 = To_int(x) - 121, gi1 = To_int(x) + 121;
        int gj0 = To_int(y) - 81, gj1 = To_int(y) + 81;
        if(gi0 < 0) gi0 = 0;
        if(gj0 < 0) gj0 = 0;
        if(gi1 > WORLD_WIDTH - 1) gi1 = WORLD_WIDTH - 1;
        if(gj1 > WORLD_HEIGHT - 1) gj1 = WORLD_HEIGHT - 1;
        for(int gi = gi0; gi <= gi1; gi++)
            memset(&getnew[gi][gj0], 0, (size_t)(gj1 - gj0 + 1) * sizeof(short));
    }
    if(TT >= 10)
    {
        TT = 0;
        for(int i = To_int(x) - 120; i <= To_int(x) + 120; i++)
        {
            for(int j = To_int(y) - 80; j <= To_int(y) + 80; j++)
            {
                if(i <= 0 || j <= 0 || i >= WORLD_WIDTH - 1 || j >= WORLD_HEIGHT - 1)
                    continue;
                if(s[i][j] == 32)
                {
                    if(s[i][j + 1] == 0 || s[i][j + 1] == 11)
                    {
                        getnew[i][j + 1] = 1;
                    }
                    if(s[i][j + 1] >= 19 && s[i][j + 1] <= 22)
                    {
                        getnew[i][j + 1] = -1;
                    }
                    if((s[i - 1][j] >= 19 && s[i - 1][j] <= 22) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i - 1][j] = -1;
                    }
                    if((s[i + 1][j] >= 19 && s[i + 1][j] <= 22) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i + 1][j] = -1;
                    }
                    if((s[i - 1][j] == 0 || s[i - 1][j] == 11) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i - 1][j] = 33;
                    }
                    if((s[i + 1][j] == 0 || s[i + 1][j] == 11) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i + 1][j] = 33;
                    }
                }
                if(s[i][j] == 33)
                {
                    if(s[i][j + 1] == 0 || s[i][j + 1] == 11)
                    {
                        getnew[i][j + 1] = 1;
                    }
                    if(s[i][j + 1] >= 19 && s[i][j + 1] <= 22)
                    {
                        getnew[i][j + 1] = -1;
                    }
                    if((s[i - 1][j] >= 19 && s[i - 1][j] <= 22) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i - 1][j] = -1;
                    }
                    if((s[i + 1][j] >= 19 && s[i + 1][j] <= 22) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i + 1][j] = -1;
                    }
                    if((s[i - 1][j] == 0 || s[i - 1][j] == 11) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i - 1][j] = 34;
                    }
                    if((s[i + 1][j] == 0 || s[i + 1][j] == 11) && s[i][j + 1] != 32 &&
                            getnew[i][j + 1] != 1)
                    {
                        getnew[i + 1][j] = 34;
                    }
                }
                if(s[i][j] == 34)
                {
                    if(s[i][j + 1] == 0 || s[i][j + 1] == 11)
                    {
                        getnew[i][j + 1] = 1;
                    }
                    if(s[i][j + 1] >= 19 && s[i][j + 1] <= 22)
                    {
                        getnew[i][j + 1] = -1;
                    }
                }
            }
        }
    }
    for(int i = To_int(x) - 120; i <= To_int(x) + 120; i++)
    {
        for(int j = To_int(y) - 80; j <= To_int(y) + 80; j++)
        {
            if(i <= 0 || j <= 0 || i >= WORLD_WIDTH - 1 || j >= WORLD_HEIGHT - 1)
                continue;
            if(s[i][j] == 19)
            {
                if(s[i][j + 1] == 0 || s[i][j + 1] == 11)
                {
                    getnew[i][j + 1] = 2;
                }
                if(s[i][j + 1] >= 32 && s[i][j + 1] <= 34)
                {
                    if(s[i][j + 1] == 32)
                        getnew[i][j + 1] = -2;
                    else
                        getnew[i][j + 1] = -1;
                }
                if(s[i - 1][j] >= 32 && s[i - 1][j] <= 34 && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    if(s[i - 1][j] == 32)
                        getnew[i - 1][j] = -2;
                    else
                        getnew[i - 1][j] = -1;
                }
                if(s[i + 1][j] >= 32 && s[i + 1][j] <= 34 && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    if(s[i + 1][j] == 32)
                        getnew[i + 1][j] = -2;
                    else
                        getnew[i + 1][j] = -1;
                }
                if((s[i - 1][j] == 0 || s[i - 1][j] == 11) && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i - 1][j] = 20;
                }
                if((s[i + 1][j] == 0 || s[i + 1][j] == 11) && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i + 1][j] = 20;
                }
            }
            if(s[i][j] == 20)
            {
                if(s[i][j + 1] == 0 || s[i][j + 1] == 11)
                {
                    getnew[i][j + 1] = 2;
                }
                if(s[i][j + 1] >= 32 && s[i][j + 1] <= 34)
                {
                    getnew[i][j + 1] = -1;
                    if(s[i][j + 1] == 32)
                        getnew[i][j + 1] = -2;
                }
                if(s[i - 1][j] >= 32 && s[i - 1][j] <= 34 && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i - 1][j] = -1;
                    if(s[i - 1][j] == 32)
                        getnew[i - 1][j] = -2;
                }
                if(s[i + 1][j] >= 32 && s[i + 1][j] <= 34 && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i + 1][j] = -1;
                    if(s[i + 1][j] == 32)
                        getnew[i + 1][j] = -2;
                }
                if((s[i - 1][j] == 0 || s[i - 1][j] == 11) && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i - 1][j] = 21;
                }
                if((s[i + 1][j] == 0 || s[i + 1][j] == 11) && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i + 1][j] = 21;
                }
            }
            if(s[i][j] == 21)
            {
                if(s[i][j + 1] == 0 || s[i][j + 1] == 11)
                {
                    getnew[i][j + 1] = 2;
                }
                if(s[i][j + 1] >= 32 && s[i][j + 1] <= 34)
                {
                    getnew[i][j + 1] = -1;
                }
                if(s[i - 1][j] >= 32 && s[i - 1][j] <= 34 && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i - 1][j] = -1;
                    if(s[i - 1][j] == 32)
                        getnew[i - 1][j] = -2;
                }
                if(s[i + 1][j] >= 32 && s[i + 1][j] <= 34 && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i + 1][j] = -1;
                    if(s[i + 1][j] == 32)
                        getnew[i + 1][j] = -2;
                }
                if((s[i - 1][j] == 0 || s[i - 1][j] == 11) && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i - 1][j] = 22;
                }
                if((s[i + 1][j] == 0 || s[i + 1][j] == 11) && s[i][j + 1] != 19 &&
                        getnew[i][j + 1] != 2)
                {
                    getnew[i + 1][j] = 22;
                }
            }
            if(s[i][j] == 22)
            {
                if(s[i][j + 1] == 0 || s[i][j + 1] == 11)
                {
                    getnew[i][j + 1] = 2;
                }
                if(s[i][j + 1] >= 32 && s[i][j + 1] <= 34)
                {
                    getnew[i][j + 1] = -1;
                    if(s[i][j + 1] == 32)
                        getnew[i][j + 1] = -2;
                }
            }
        }
    }
    for(int i = To_int(x) - 120; i <= To_int(x) + 120; i++)
    {
        for(int j = To_int(y) - 80; j <= To_int(y) + 80; j++)
        {
            if(i <= 0 || j <= 0 || i >= WORLD_WIDTH - 1 || j >= WORLD_HEIGHT - 1)
                continue;
            if(getnew[i][j] > 3)
                s[i][j] = getnew[i][j];
            if(getnew[i][j] == 2)
                s[i][j] = 19;
            if(getnew[i][j] == 1)
                s[i][j] = 32;
            if(getnew[i][j] == -1)
                s[i][j] = 1;
            if(getnew[i][j] == -2)
                s[i][j] = 36;
        }
    }
    mark_dirty_range(To_int(x) - 120, To_int(x) + 120);
    Clear();
}

void make_jian(double tx, double ty, double stx, double sty, int nbt_)
{
    int i = 0;
    while(i < 500 && shiti[i].type != 0)
        i++;
    if(i >= 500)
        return;
    add_entity_to_hash(i, tx, ty);
    shiti[i].HP = 0, shiti[i].hurt = 0, shiti[i].type = 4, shiti[i].gx = tx,
             shiti[i].gy = ty, shiti[i].nbt = nbt_;
    shiti[i].spgx = stx, shiti[i].spgy = sty, shiti[i].jp = 0, shiti[i].CD = 0;
    shiti[i].randomtick = 0, shiti[i].direction = 0;
    shiti[i].sd = 0;
    shiti[i].AI = 2;
}
int QQQQ = 0;
void try_make_st()
{
    if(inDungeon) return;   // 地牢内不刷主世界野怪（怪物由关卡波次控制）
    for(int i = To_int(x) - 120; i <= To_int(x) + 120; i++)
    {
        for(int j = To_int(y) - 80; j <= To_int(y) + 80; j++)
        {
            if(i <= 0 || j <= 0 || i >= WORLD_WIDTH - 1 || j >= WORLD_HEIGHT - 1)
                continue;
            if(i >= To_int(x) - 26 && j >= To_int(y) - 16 && i <= To_int(x) + 26 &&
                    j <= To_int(y) + 16)
                continue;
            if(s[i][j] == 0 || s[i][j] == 11)
            {
                if(weidu == 1)
                {
                    if(light[i][j] <= 7 && blocktouch[s[i][j + 1]] >= 1)
                        if(rand() % To_int(ed / 1.2) == 0)
                        {
                            QQQQ = rand() % 4;
                            if(QQQQ == 0)
                                make_st(1, 20, 4, i, j);
                            else if(QQQQ == 1)
                                make_st(3, 20, 3, i, j);
                            else if(QQQQ == 2)
                                make_st(2, 20, 50, i, j);
                            else
                                make_st(6, 16, 3, i, j);
                        }
                }
                else
                {
                    if(rand() % To_int(ed) == 0)
                        make_st(7, 20, 6, i, j);
                }
            }
        }
    }
    // 白天在地表自然生成羊和猪（基于列定位地表）
    if(weidu == 1 && night == 0)
    {
        for(int i = To_int(x) - 120; i <= To_int(x) + 120; i += 2)
        {
            if(i <= 0 || i >= WORLD_WIDTH - 1)
                continue;
            // 排除玩家附近
            if(abs(i - To_int(x)) < 30)
                continue;
            // 从上往下找地表（第一个实心方块上方的空气）
            int ground = -1;
            for(int j = 1; j < WORLD_HEIGHT - 1; j++)
            {
                if(blocktouch[s[i][j]] == true && (s[i][j - 1] == 0 || s[i][j - 1] == 11))
                {
                    ground = j - 1;
                    break;
                }
            }
            if(ground > 0 && light[i][ground] > 7)
            {
                // 检查该格上方是否完全无方块（露天地表）
                bool open_sky = true;
                for(int jj = 0; jj < ground; jj++)
                {
                    if(s[i][jj] != 0 && s[i][jj] != 11)
                    {
                        open_sky = false;
                        break;
                    }
                }
                // 只允许露天地表或 y>840 的位置
                if((open_sky || ground > 840) && rand() % To_int(ed / 2) == 0)
                {
                    // 生成位置加随机偏移，避免堆叠
                    int sx = i + (rand() % 3 - 1);
                    // 不在水中生成：生成格及其脚下支撑格都不能是水
                    bool water_here = false;
                    if(sx > 0 && sx < WORLD_WIDTH - 1)
                    {
                        if(s[sx][ground] >= 19 && s[sx][ground] <= 22)
                            water_here = true;
                        if(ground + 1 < WORLD_HEIGHT &&
                                s[sx][ground + 1] >= 19 && s[sx][ground + 1] <= 22)
                            water_here = true;
                    }
                    if(sx > 0 && sx < WORLD_WIDTH - 1 && !water_here &&
                            (s[sx][ground] == 0 || s[sx][ground] == 11))
                    {
                        // 随机生成羊或猪
                        if(rand() % 2 == 0)
                            make_st(12, 8, 0, sx, ground);
                        else
                            make_st(13, 8, 0, sx, ground);
                    }
                }
            }
        }
    }
}
int dire;
int gj;
int Step;
void thing_fall(int many_, int get);   // 前向声明：st_kill 中掉落暗影鳞片
void enter_level(int n);              // 前向声明：死亡后重进关卡
void st_kill(int l)
{
    if(shiti[l].type == 4 && (shiti[l].nbt == 1 || shiti[l].nbt == 3))
    {
        boom_add(int(shiti[l].gx + 0.5), int(shiti[l].gy + 0.5), -4);
        if(wudi == 0)
        {
            if(jl(abs(x - shiti[l].gx), abs(y - shiti[l].gy)) <= 8)
            {
                double boomdmg = (8 - jl(abs(x - shiti[l].gx), abs(y - shiti[l].gy))) * 0.5;
                if(hasTalent(T_EXPLODE)) boomdmg *= 0.5;   // 爆炸减伤
                life -= boomdmg * jianshang;
                wudi = 45;
            }
        }
    }
    if(shiti[l].AI == 4)
    {
        thing_fall(rand() % 2 + 1, 105);   // 掉落暗影鳞片（beta1.8同步）
        if(l==shiti[l].conl)
        {
            if(shiti[shiti[l].conr].type==11)
            {
                remove_entity_from_hash(shiti[l].conr, shiti[shiti[l].conr].gx, shiti[shiti[l].conr].gy);
                shiti[shiti[l].conr] = {0};
            }
            else
            {
                shiti[shiti[l].conr].type=9,shiti[shiti[l].conr].conl=shiti[l].conr;
            }
        }
        else if(l==shiti[l].conr)
        {
            if(shiti[shiti[l].conl].type==9)
            {
                remove_entity_from_hash(shiti[l].conl, shiti[shiti[l].conl].gx, shiti[shiti[l].conl].gy);
                shiti[shiti[l].conl] = {0};
            }
            else
            {
                shiti[shiti[l].conl].type=11,shiti[shiti[l].conl].conr=shiti[l].conl;
            }
        }
        else
        {
            if(shiti[shiti[l].conl].type==9)
            {
                remove_entity_from_hash(shiti[l].conl, shiti[shiti[l].conl].gx, shiti[shiti[l].conl].gy);
                shiti[shiti[l].conl] = {0};
            }
            else shiti[shiti[l].conl].type=11,shiti[shiti[l].conl].conr=shiti[l].conl;
            if(shiti[shiti[l].conr].type==11)
            {
                remove_entity_from_hash(shiti[l].conr, shiti[shiti[l].conr].gx, shiti[shiti[l].conr].gy);
                shiti[shiti[l].conr] = {0};
            }
            else shiti[shiti[l].conr].type=9,shiti[shiti[l].conr].conl=shiti[l].conr;
        }
    }
    if(shiti[l].type == 8)
        boss2hp = 0;   // Boss死亡：清血条，防止屏幕残留黑白块
    remove_entity_from_hash(l, shiti[l].gx, shiti[l].gy);
    shiti[l] = {0};
}
void thing_fall(int many_, int get)
{
    for(int i = 0; i < many_; i++)
    {
        int U = getbeibao(get);
        if(U != 0)
            beibao[U][1]++, beibao[U][0] = get;
    }
}
void st_kill_fall(int l)
{
    if(shiti[l].type == 1)
    {
        thing_fall(rand() % 2 + 1, 37);
    }
    if(shiti[l].type == 2)
    {
        thing_fall(rand() % 2 + 1, 43);
    }
    if(shiti[l].type == 3)
    {
        thing_fall(rand() % 2 + 1, 44);
        if(rand() % 10 == 0)
            thing_fall(1, 41);   // 10%掉木弓（beta1.8同步）
    }
    if(shiti[l].type == 6)
    {
        thing_fall(rand() % 2 + 1, 42);
    }
    if(shiti[l].type == 7)
    {
        thing_fall(rand() % 2 + 1, 37);   // 僵尸猪灵：腐肉+金粒（beta1.8同步）
        thing_fall(rand() % 2 + 1, 65);
    }
    if(shiti[l].type ==5)
    {
        if(rand() % 2==0)thing_fall(1,60);
        else thing_fall(1,61);
    }
    if(shiti[l].type == 12)
    {
        thing_fall(1, 87);
        thing_fall(rand() % 2 + 1, 88);
    }
    if(shiti[l].type == 13)
    {
        thing_fall(rand() % 3 + 1, 89);
    }
    // 击杀掉落金币 (30% 概率 1-3 金币)
    if(shiti[l].type >= 1 && shiti[l].type <= 13 && shiti[l].type != 4 && shiti[l].type != 12 && shiti[l].type != 13)
    {
        if(rand() % 100 < 30)
            add_coins(1 + rand() % 3);
    }
    // 天赋：击败回蓝 / 击败回血
    if(hasTalent(T_KILLEN)) energy = min(energyMax, energy + 5);
    if(hasTalent(T_KILLHP)) life += 1;
    // 基础机制：击杀怪物概率回蓝/回血（匹配200血/200蓝体系）
    if(shiti[l].type >= 1 && shiti[l].type <= 13 && shiti[l].type != 12 && shiti[l].type != 13)
    {
        if(rand() % 100 < 30)                       // 30%回蓝
            energy = min(energyMax, energy + 8 + rand() % 8);
        if(rand() % 100 < 25)                       // 25%回血
            life = min((double)lifeMax, life + 4 + rand() % 5);
        if(hasTalent(T_ARMORREP) && rand() % 100 < 10)  // 破甲修复天赋：10%回护甲
            hujiazhi = min(armorMax, hujiazhi + 5);
    }
    st_kill(l);
}
void damage(double dmg,int fromx,bool noKnockback = false)
{
    if(wudi == 0)
    {
        wudi = 40;
        if(ed==8000)dmg*=0.75;
        if(ed==4000)dmg*=1.5;
        // 天赋：金盾(受击扣金币) 与 破甲修复
        if(hasTalent(T_GOLDSHIELD) && total_coins() > 0)
        {
            if(rand() % 100 < 30) { spend_coins(1); dmg *= 0.5; }
        }
        if(hasTalent(T_ARMORREP)) dmg *= 0.9;
        // 护甲吸收：先扣护甲，护甲耗尽后剩余伤害扣血
        if(hujiazhi > 0)
        {
            int absorb = hujiazhi;
            if((double)absorb > dmg)
                absorb = (int)dmg;
            hujiazhi -= absorb;
            dmg -= absorb;
        }
        life -= dmg;
        if(!noKnockback)
        {
            spx=((fromx<x)*2-1)*6;
            spy=-8;
        }
        if(life <= 0)
        {
            if(inDungeon || currentLevel > 0)
            {
                // 纯地牢模式死亡：重新回到当前关第一个房间（满状态重开本关）
                int lvl = currentLevel > 0 ? currentLevel : 1;
                Setpos(1, 31);
                Color(0);
                cout << " 你死了！重新挑战第 " << lvl << " 关                          ";
                life = lifeMax;
                hujiazhi = armorMax;
                energy = energyMax;
                spx = 0;
                spy = 0;
                wudi = 60;
                enter_level(lvl);
                memset(painting, 114, sizeof(painting));
                shuaxin();
            }
            else
            {
                Start();
                life = lifeMax;
                hujiazhi = armorMax;
                spx = 0;
                spy = 0;
                jump = 0;
                shuaxin();
            }
        }
    }
}
void touch_atk(int i,double dmg,double dist)
{
    if(abs(x - shiti[i].gx) < dist && abs(y - shiti[i].gy) < dist &&shiti[i].CD <= 0)
    {
        shiti[i].CD = 90;
        damage(dmg,shiti[i].gx);
    }
}

// ===================== AI 状态机 =====================
// 每帧：物理(ent_physics_ground) -> 状态迁移(ent_update_state) -> 行为(ent_behave)
enum EntState : unsigned char
{
    ST_IDLE = 0, ST_WANDER, ST_CHASE, ST_ATTACK, ST_FLEE,
    ST_FUSE,              // 苦力怕引信（原 CD<0 逻辑）
    ST_SCRIPT,            // Boss 脚本阶段（原 isnotouch==1 门控）
    ST_FLY, ST_PHASE, ST_FOLLOW_CHAIN, ST_PROJECTILE
};
static void ai_passive(int l, double old_gx, double old_gy);
static void ent_update_state(int l);

// 碰撞缓存助手：刷新实体当前方块，同行代码复用 eb，避免重复索引 s[][]
inline short ent_block(int l, int &ex, int &ey)
{
    ex = int(shiti[l].gx + 0.5);
    ey = int(shiti[l].gy + 0.5);
    return s[ex][ey];
}
static inline bool ent_oob(int ex, int ey)
{
    return ex < 0 || ey < 0 || ex > WORLD_WIDTH - 1 || ey > WORLD_HEIGHT - 1;
}
static inline bool fluid_block_id(short b)
{
    return (b >= 19 && b <= 22) || (b >= 32 && b <= 34);
}
static inline bool out_of_active_area(double gx, double gy)
{
    return gx < To_int(x) - 120 || gy < To_int(y) - 80 ||
           gx > To_int(x) + 120 || gy > To_int(y) + 80 ||
           gx < 0 || gy < 0 || gx > WORLD_WIDTH - 1 || gy > WORLD_HEIGHT - 1;
}
static void boss2_die_split(int l, double old_gx, double old_gy)
{
    boss2hp -= shiti[l].direction;
    int xxxx = shiti[l].gx, yyyy = shiti[l].gy, nbtt = shiti[l].nbt - 1;
    update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
    st_kill(l);
    if(nbtt >= 1)
    {
        if(nbtt >= 0)
            make_st(8, fbnq[nbtt], 1 + (nbtt / 2.0), xxxx + rand() % 5 - 2,
                    yyyy + rand() % 5 - 2, nbtt);
        if(nbtt - 1 >= 0)
            make_st(8, fbnq[nbtt - 1], 1 + (nbtt / 2.0), xxxx + rand() % 5 - 2,
                    yyyy + rand() % 5 - 2, nbtt - 1);
    }
}
// 玩家点击攻击地面实体；返回 false 表示实体已死亡并被移除
static bool ai1_player_click(int l, double old_gx, double old_gy)
{
    if(!(KEY_DOWN(VK_LBUTTON) &&
            (abs(mpx - shiti[l].gx) < 2 && abs(mpy - shiti[l].gy) < 2) &&
            playerCD <= 0))
        return true;
    if(!(abs(x - shiti[l].gx) < 4.5 && abs(y - shiti[l].gy) < 4.5))
        return true;
    if(shiti[l].type == 5 && shiti[l].islife2 == 1)
        ;
    else
    {
        if(spy > 2)
        {
            shiti[l].HP -= gongji * 1.5;
            if(shiti[l].type != 5) shiti[l].spgy = -8, gj = 2;
        }
        else
        {
            shiti[l].HP -= gongji;
            if(shiti[l].type != 5) shiti[l].spgy = -6, gj = 1;
            if(beibao[cho][0] == 17 || beibao[cho][0] == 47 || beibao[cho][0] == 49 ||
                    beibao[cho][0] == 60 || beibao[cho][0] == 73 || beibao[cho][0] == 77)
            {
                for(int LL = 0; LL < 500; LL++)
                {
                    if(shiti[LL].type > 0 && LL != l)
                    {
                        if((abs(shiti[LL].gx - mpx) <= 2) && (abs(shiti[LL].gy - mpy) <= 2))
                        {
                            shiti[LL].HP -= 1;
                            if(shiti[LL].type != 5)
                                shiti[LL].spgy = -6, gj = 1,
                                shiti[LL].spgx += ((x < shiti[l].gx) * 2 - 1) * 0.4;
                            if(shiti[LL].HP <= 0)
                            {
                                if(shiti[LL].type == 5 && shiti[LL].islife2 == 0)
                                {
                                    shiti[LL].islife2 = 1;
                                    shiti[LL].HP = 1;
                                    shiti[LL].gongjitick = 0;
                                }
                                else if(shiti[LL].type != 8)
                                {
                                    // Boss(type8)不在此处击杀，由其AI的分裂逻辑处理
                                    st_kill_fall(LL);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    playerCD = gongjicd;
    shiti[l].chv = 900, shiti[l].issd = 1;
    if(shiti[l].type == 7)
    {
        shiti[l].ch = 1;
        vector<int> nearby_zombies;
        query_nearby_entities(x, y, shiti[l].sd, nearby_zombies);
        for(int i : nearby_zombies)
            if(shiti[i].type == 7 &&
                    jl(abs(shiti[i].gx - x), abs(shiti[i].gy - y)) < shiti[l].sd)
                shiti[i].ch = 1;
    }
    if(shiti[l].HP <= 0)
    {
        if(shiti[l].type == 5 && shiti[l].islife2 == 0)
        {
            shiti[l].islife2 = 1;
            shiti[l].HP = 1;
            shiti[l].gongjitick = 0;
        }
        else
        {
            update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
            st_kill_fall(l);
            return false;
        }
    }
    return true;
}
// 死亡 / 出界清理；返回 false 表示实体已移除
static bool ai1_despawn_check(int l, double old_gx, double old_gy)
{
    if(shiti[l].type <= 4 || shiti[l].type >= 6)
    {
        if(shiti[l].HP <= 0 || out_of_active_area(shiti[l].gx, shiti[l].gy))
        {
            update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
            st_kill(l);
            return false;
        }
    }
    else if(shiti[l].type == 5 && shiti[l].HP <= 0)
    {
        if(shiti[l].islife2 == 0)
        {
            shiti[l].islife2 = 1;
            shiti[l].HP = 1;
            shiti[l].gongjitick = 0;
        }
        else
        {
            update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
            if(shiti[l].type == 5) st_kill_fall(l);
            else st_kill(l);
            return false;
        }
    }
    return true;
}
// AI1 地面物理：流体 / 重力 / 落地 / 单向平台 / 视线 / 追击游走 / 碰墙；返回是否着地
static bool ent_physics_ground(int l)
{
    if(inDungeon)
    {
        // 地牢：无重力顶视角移动，撞墙停止（不穿墙）
        if(shiti[l].type != 5 && shiti[l].type != 8)
        {
            double dx = x - shiti[l].gx;
            double dy = y - shiti[l].gy;
            double dist = sqrt(dx * dx + dy * dy) + 0.001;
            if(dist > 1.5)
            {
                double sp = 0.12;
                double nx = dx / dist, ny = dy / dist;
                shiti[l].gx += nx * sp;
                shiti[l].gy += ny * sp;
                int ex2, ey2;
                short eb2 = ent_block(l, ex2, ey2);
                if(blocktouch[eb2] >= 1)
                {
                    shiti[l].gx -= nx * sp;
                    shiti[l].gy -= ny * sp;
                }
            }
        }
        return false;
    }
    int ex, ey;
    short eb = ent_block(l, ex, ey);
    bool grounded = false;
    if(fluid_block_id(eb))
    {
        if(eb >= 32 && eb <= 34 && shiti[l].type != 5 && shiti[l].type != 7)
            shiti[l].HP -= 0.25;
        shiti[l].spgy += g;
        if(shiti[l].type <= 3)
            shiti[l].spgx *= 0.5;
        shiti[l].spgy *= 0.75;
        if(shiti[l].type == 2)
            shiti[l].spgy = -3;
        if(shiti[l].type == 5)
        {
            if(shiti[l].gy < y)
                shiti[l].spgy = 8;
            else
                shiti[l].spgy = -8;
        }
        shiti[l].gy += shiti[l].spgy * 0.02;
    }
    else
    {
        shiti[l].spgy += g;
        shiti[l].gy += shiti[l].spgy * 0.06;
        shiti[l].spgy *= 0.98;
        if(shiti[l].type == 5 && shiti[l].HP <= 250)
        {
            if(shiti[l].gy > y - 6 && shiti[l].gy < y - 5) shiti[l].spgy = 0;
            else if(shiti[l].gy > y - 5)
                shiti[l].spgy = -6;
            else
                shiti[l].spgy = 6;
            eb = ent_block(l, ex, ey);
            if(blocktouch[eb] == true)
            {
                s[ex][ey] = 0;
                mark_dirty_col(ex);
                Clear();
            }
        }
    }
    eb = ent_block(l, ex, ey);
    if(blocktouch[eb] == true)
    {
        if(shiti[l].spgy >= 0)
        {
            while(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)
                shiti[l].gy -= 0.1;
            shiti[l].jp = 0;
            shiti[l].spgy = 0.01;
            grounded = true;
        }
        if(shiti[l].spgy < 0)
        {
            while(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)
                shiti[l].gy += 0.1;
            shiti[l].spgy = 2;
        }
    }
    bool isjump = 1, isdown = 0;
    if(abs(x - shiti[l].gx) < 1.5 && (shiti[l].type == 1 || shiti[l].type == 5))
    {
        if(y < shiti[l].gy)
        {
            for(int i = -3; i <= -1; i++)
                if(blocktouch[s[To_int(shiti[l].gx)][To_int(shiti[l].gy + i)]] == 1) isjump = 0;
            if(isjump && shiti[l].jp == 0) shiti[l].jp = 1;
        }
    }
    if(y > shiti[l].gy)
    {
        isdown = 1;
    }
    eb = ent_block(l, ex, ey);
    if(blocktouch[eb] == 2 && shiti[l].gy - int(shiti[l].gy) > 0.5)
    {
        if(shiti[l].spgy >= 0 && !isdown)
        {
            while(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == 2 &&
                    shiti[l].gy - int(shiti[l].gy) > 0.5)
                shiti[l].gy -= 0.05;
            shiti[l].jp = 0;
            shiti[l].spgy = 0.01;
            grounded = true;
        }
    }
    if(tick % 5 == 0 && !shiti[l].issd)
    {
        double total = 25, px, py;
        for(double ddx = 0, ddy = total; ddx <= total && !shiti[l].issd; ddx++)
        {
            ddy = total - ddx;
            for(int i = 1; i <= 4 && !shiti[l].issd; i++)
            {
                px = shiti[l].gx, py = shiti[l].gy;
                for(double LL = 0; LL <= shiti[l].sd; LL += 0.25)
                {
                    if(i == 1) px += ddx / 100, py += ddy / 100;
                    else if(i == 2) px += -ddx / 100, py += ddy / 100;
                    else if(i == 3) px += ddx / 100, py += -ddy / 100;
                    else px += -ddx / 100, py += -ddy / 100;
                    if(blocktouch[s[To_int(px)][To_int(py)]] == 1) break;
                    if(abs(px - x) < 0.5 && abs(py - y) < 0.5)
                    {
                        shiti[l].chv = 600;
                        shiti[l].issd = 1;
                        break;
                    }
                }
            }
        }
    }
    if(shiti[l].chv > 0) shiti[l].chv--;
    else shiti[l].issd = 0;
    if(shiti[l].type == 5) shiti[l].issd = 1, shiti[l].chv = 300;
    if((shiti[l].chv > 0) && (shiti[l].type != 7 || shiti[l].ch == 1))
    {
        if(shiti[l].gx > x)
            shiti[l].spgx -= 0.007, dire = 1;
        else
            shiti[l].spgx += 0.007, dire = 0;
        shiti[l].direction = 1;
    }
    else
    {
        shiti[l].spgx += 0.004 * shiti[l].direction;
        if(shiti[l].direction == 0) shiti[l].randomtick += 2;
        shiti[l].randomtick++;
        if(shiti[l].randomtick > 300)
        {
            shiti[l].randomtick = 0;
            if(shiti[l].direction != 0) shiti[l].direction = 0;
            else shiti[l].direction = (rand() % 2) * 2 - 1;
        }
        if(shiti[l].direction == -1) dire = 0;
        else dire = 1;
    }
    if(gj == 1)
        shiti[l].spgx += ((dire) * 2 - 1) * 0.4;
    else if(gj == 2)
        shiti[l].spgx += ((dire) * 2 - 1) * 0.6;
    eb = ent_block(l, ex, ey);
    if(eb >= 19 && eb <= 22)
        shiti[l].gx += shiti[l].spgx * 0.5;
    else
        shiti[l].gx += shiti[l].spgx;
    shiti[l].spgx *= 0.9;
    if(shiti[l].type == 5 && shiti[l].HP <= 250)
    {
        eb = ent_block(l, ex, ey);
        if(blocktouch[eb] == true)
        {
            s[ex][ey] = 0;
            mark_dirty_col(ex);
            Clear();
        }
    }
    isjump = 1;
    for(int i = 1; i <= 3; i++)
        if(blocktouch[s[To_int(shiti[l].gx - ((dire) * 2 - 1))][To_int(shiti[l].gy + i)]] == 1)
            isjump = 0;
    if(isjump && shiti[l].jp == 0) shiti[l].jp = 1;
    eb = ent_block(l, ex, ey);
    if(blocktouch[eb] == true)
    {
        Step = 1;
        while(Step < 60)
        {
            shiti[l].gx -= 0.03 * Step;
            if(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == false)
                break;
            shiti[l].gx += 0.06 * Step;
            if(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == false)
                break;
            shiti[l].gx -= 0.03 * Step;
            Step++;
        }
        if(shiti[l].jp == 0)
            shiti[l].jp = 1;
        shiti[l].spgx = 0;
    }
    else if(shiti[l].type == 6)
        shiti[l].jp = 0;
    return grounded;
}
// Boss 脚本阶段（isnotouch 门控）：二阶段狂暴 / 一阶段俯冲
static void ai1_script(int l, double old_gx, double old_gy)
{
    if(shiti[l].islife2 == 1)
    {
        if(shiti[l].gongjitick <= 180)
        {
            if(int(shiti[l].gongjitick) % 30 == 0)
                boom_add(int(shiti[l].gx + rand() % 3 - 1 + 0.5),
                         int(shiti[l].gy + rand() % 3 - 1 + 0.5), -4);
            shiti[l].tox = x, shiti[l].toy = y - 8;
        }
        else if(shiti[l].gongjitick <= 360)
        {
            shiti[l].gx += (shiti[l].tox - shiti[l].gx) / 12.0;
            shiti[l].gy += (shiti[l].toy - shiti[l].gy) / 12.0;
        }
        else
        {
            if(int(shiti[l].gongjitick) % (30) == 0)
            {
                double ttx, tty, total, px, py;
                ttx = abs(x - shiti[l].gx);
                tty = abs(y - shiti[l].gy);
                total = ttx + tty;
                px = ttx / total;
                py = tty / total;
                if(x - shiti[l].gx < 0) px *= -1;
                if(y - shiti[l].gy < 0) py *= -1;
                shiti[l].spgx = 90 * px,
                shiti[l].spgy = 90 * py * 1.5;
            }
            if(int(shiti[l].gongjitick) % (30) == 29)
                boom_add(int(shiti[l].gx + 0.5), int(shiti[l].gy + 0.5), -4);
            shiti[l].gx += shiti[l].spgx * 0.02;
            shiti[l].gy += shiti[l].spgy * 0.02;
            shiti[l].spgy *= 0.92;
            shiti[l].spgx *= 0.92;
        }
        if(shiti[l].gongjitick >= 900)
        {
            shiti[l].HP = 0, boom_add(int(shiti[l].gx + 0.5), int(shiti[l].gy + 0.5), -4);
        }
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
    }
    else
    {
        if(shiti[l].gongjitick <= 810)
        {
            shiti[l].gx += (x - shiti[l].gx) / 6.0;
            shiti[l].gy += (y - shiti[l].gy - 15) / 6.0;
            if(shiti[l].gongjitick == 810)
                shiti[l].gx += (x - shiti[l].gx) / 2.0, shiti[l].toy = y, shiti[l].spgy = 15;
        }
        else if(shiti[l].gongjitick >= 820 && shiti[l].HP > 250)
        {
            shiti[l].spgy += g * 2;
            shiti[l].gy += shiti[l].spgy * 0.04;
            if(shiti[l].gy >= shiti[l].toy &&
                    (blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == 1))
                shiti[l].isnotouch = 0, shiti[l].gy -= shiti[l].spgy * 0.02,
                boom_add(int(shiti[l].gx + 0.5), int(shiti[l].gy + 0.5), -4);
        }
        else if(shiti[l].gongjitick >= 820 && shiti[l].HP <= 250)
        {
            if(int(shiti[l].gongjitick) % (60 - (shiti[l].HP <= 125) * 20) == 0)
            {
                double ttx, tty, total, px, py;
                ttx = abs(x - shiti[l].gx);
                tty = abs(y - shiti[l].gy);
                total = ttx + tty;
                px = ttx / total;
                py = tty / total;
                if(x - shiti[l].gx < 0) px *= -1;
                if(y - shiti[l].gy < 0) py *= -1;
                shiti[l].spgx = (60 + (shiti[l].HP <= 75) * 20) * px,
                shiti[l].spgy = (60 + (shiti[l].HP <= 75) * 20) * py * 1.5;
            }
            shiti[l].gx += shiti[l].spgx * 0.02;
            shiti[l].gy += shiti[l].spgy * 0.02;
            shiti[l].spgy *= 0.92;
            shiti[l].spgx *= 0.92;
        }
    }
}
// ST_ATTACK 按 type 分派；返回 false 表示实体已死亡并被移除
static bool ai1_attacks(int l, double old_gx, double old_gy)
{
    if(shiti[l].type == 1 || shiti[l].type == 6 ||
            (shiti[l].type == 7 && shiti[l].ch == 1))
    {
        touch_atk(l, shiti[l].hurt, 0.75);
        return true;
    }
    if(shiti[l].type == 2)
    {
        // ST_FUSE：进入引信后 CD<0 倒计时，CD<-100 自爆
        if(abs(x - shiti[l].gx) < 8 && abs(y - shiti[l].gy) < 8 && shiti[l].chv > 0)
        {
            if(shiti[l].CD < 0)
                shiti[l].CD--, shiti[l].spgx = 0;
            if(shiti[l].CD < -100)
            {
                if(wudi == 0)
                {
                    if(jl(abs(x - shiti[l].gx), abs(y - shiti[l].gy)) <= 6)
                        damage((6 - jl(abs(x - shiti[l].gx), abs(y - shiti[l].gy))) * 5,
                               shiti[l].gx);
                }
                boom_add(To_int(shiti[l].gx), To_int(shiti[l].gy), -4);
                Clear();
                for(int LL = 0; LL < 500; LL++)
                {
                    if(shiti[LL].type > 0)
                    {
                        if(jl(abs(shiti[l].gx - shiti[LL].gx),
                                abs(shiti[l].gy - shiti[LL].gy)) <= 8)
                            shiti[LL].HP -= (8 - jl(abs(shiti[l].gx - shiti[LL].gx),
                                                    abs(shiti[l].gy - shiti[LL].gy))) *
                                            0.1 * shiti[l].hurt;
                    }
                }
                update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
                st_kill(l);
                return false;
            }
            if(abs(x - shiti[l].gx) < 4 && abs(y - shiti[l].gy) < 4)
            {
                if(shiti[l].CD == 0)
                    shiti[l].CD--;
            }
        }
        else
            shiti[l].CD = 0;
        return true;
    }
    if(shiti[l].type == 3)
    {
        if(jl(abs(shiti[l].gx - x), abs(shiti[l].gy - y)) <
                shiti[l].sd * 1.25 && shiti[l].chv > 0)
        {
            shiti[l].CD--;
            if(jl(abs(shiti[l].gx - x), abs(shiti[l].gy - y)) <= 8)
                shiti[l].spgx = ((x < shiti[l].gx) * 2 - 1) * 0.05;
            else if(jl(abs(shiti[l].gx - x), abs(shiti[l].gy - y)) <= 10)
                shiti[l].spgx = 0;
            if(shiti[l].CD <=
                    -120 + (shiti[l].sd * 2.5 -
                            jl(abs(shiti[l].gx - x), abs(shiti[l].gy - y) * 3)))
            {
                shiti[l].CD = 0;
                double ang = atan2(y - shiti[l].gy, x - shiti[l].gx);
                make_jian(shiti[l].gx, shiti[l].gy - 0.75, 50 * cos(ang), 50 * sin(ang), 0);
            }
        }
        return true;
    }
    if(shiti[l].type == 5)
    {
        if(jl(abs(shiti[l].gx - x), abs(shiti[l].gy - y)) < shiti[l].sd)
        {
            bosshp = shiti[l].HP;
            shiti[l].gongjitick++;
            touch_atk(l, shiti[l].hurt, 0.75);
            if(shiti[l].gongjitick < 240) shiti[l].gongjitick++;
            if(shiti[l].islife2 == 1)
            {
                if(shiti[l].gongjitick == 2)
                {
                    shiti[l].isnotouch = 1;
                    shiti[l].tox = x;
                    shiti[l].toy = y;
                }
                return true;
            }
            if(shiti[l].gongjitick >= 240 && shiti[l].gongjitick <= 480 &&
                    shiti[l].CD <= 0)
            {
                shiti[l].CD = 50 - (500 - shiti[l].HP) / 20;
                double ttx, tty, total, px, py;
                ttx = abs(x - shiti[l].gx);
                tty = abs(y - shiti[l].gy);
                total = ttx + tty + 0.001;
                px = ttx / total;
                py = tty / total;
                if(x - shiti[l].gx < 0) px *= -1;
                if(y - shiti[l].gy < 0) py *= -1;
                if(shiti[l].HP > 200)
                    make_jian(shiti[l].gx, shiti[l].gy,
                              ((500 - shiti[l].HP) / 25 + 25) * px,
                              ((500 - shiti[l].HP) / 25 + 25) * py * 1.5, 4);
                else
                {
                    make_jian(shiti[l].gx, shiti[l].gy, -40, 0, 4);
                    make_jian(shiti[l].gx, shiti[l].gy, 40, 0, 4);
                    make_jian(shiti[l].gx, shiti[l].gy, 0, -60, 4);
                    make_jian(shiti[l].gx, shiti[l].gy, 0, 60, 4);
                    make_jian(shiti[l].gx, shiti[l].gy, -20, 30, 4);
                    make_jian(shiti[l].gx, shiti[l].gy, 20, -30, 4);
                    make_jian(shiti[l].gx, shiti[l].gy, -20, -30, 4);
                    make_jian(shiti[l].gx, shiti[l].gy, 20, 30, 4);
                }
            }
            if(shiti[l].gongjitick >= 480 && shiti[l].gongjitick <= 720 &&
                    shiti[l].CD <= 0)
            {
                shiti[l].CD = 100 - (500 - shiti[l].HP) / 10;
                if(shiti[l].HP > 150 || (shiti[l].HP > 75 && (rand() % 2 == 0)))
                {
                    for(int vx = -30; vx <= 30; vx += 5)
                        make_jian(x + vx, y - 25, 0, (500 - shiti[l].HP) / 50, 5);
                }
                else
                {
                    if(shiti[l].HP <= 75)
                    {
                        for(int vx = -30; vx <= 30; vx += 5)
                            make_jian(x + vx, y - 25, 0, (500 - shiti[l].HP) / 50, 5);
                    }
                    int ran__ = rand() % 2 * 2 - 1;
                    for(int vy = -30; vy <= 30; vy += 6)
                        make_jian(x + ran__ * 45, y + vy,
                                  ((500 - shiti[l].HP) / 50 + 15) * -ran__, 0, 4);
                }
            }
            if(shiti[l].gongjitick >= 800 &&
                    shiti[l].gongjitick <= 980 + (shiti[l].HP <= 250) * 120 +
                    (shiti[l].HP <= 125) * 120)
            {
                if(shiti[l].gongjitick == 800)
                    shiti[l].spgy = 0, shiti[l].spgx = 0, shiti[l].CD = 120,
                    shiti[l].isnotouch = 1, shiti[l].toy = y;
            }
            if(shiti[l].gongjitick > 980 + (shiti[l].HP <= 250) * 120 +
                    (shiti[l].HP <= 125) * 120)
            {
                shiti[l].spgy = 0, shiti[l].spgx = 0;
                shiti[l].gongjitick = 0;
                shiti[l].isnotouch = 0;
            }
        }
        return true;
    }
    return true;
}
// AI==1 顶层：点击攻击 -> 清理 -> 冷却/跳跃 -> 状态迁移 -> 脚本或物理+攻击
static bool ai_ground(int l, double old_gx, double old_gy)
{
    gj = 0;
    if(!ai1_player_click(l, old_gx, old_gy))
        return false;
    if(!ai1_despawn_check(l, old_gx, old_gy))
        return false;
    if(shiti[l].CD > 0)
        shiti[l].CD--;
    if(shiti[l].jp == 1)
    {
        if(shiti[l].type == 5)
            shiti[l].spgy = -10;
        else if(shiti[l].type == 6)
            shiti[l].spgy = -4;
        else
            shiti[l].spgy = -10;
        if(shiti[l].type != 6)
            shiti[l].jp = 2;
    }
    ent_update_state(l);
    if(shiti[l].state == ST_SCRIPT)
    {
        ai1_script(l, old_gx, old_gy);
        // 原逻辑中攻击分派在脚本阶段同样执行（驱动 gongjitick 时间线）
        return ai1_attacks(l, old_gx, old_gy);
    }
    ent_physics_ground(l);
    return ai1_attacks(l, old_gx, old_gy);
}
// 状态迁移（纯条件判断，无副作用）：脚本门控优先，其次按 AI 类别映射
static void ent_update_state(int l)
{
    if(shiti[l].AI == 1)
    {
        if(shiti[l].isnotouch == 1)
        {
            shiti[l].state = ST_SCRIPT;
            return;
        }
        if(shiti[l].type == 2 && shiti[l].CD < 0)
            shiti[l].state = ST_FUSE;
        else if(shiti[l].issd || shiti[l].chv > 0)
            shiti[l].state = ST_CHASE;
        else
            shiti[l].state = ST_WANDER;
        return;
    }
    if(shiti[l].AI == 2)
    {
        shiti[l].state = ST_PROJECTILE;
        return;
    }
    if(shiti[l].AI == 3)
    {
        shiti[l].state = (shiti[l].gongjitick > 900) ? ST_PHASE : ST_FLY;
        return;
    }
    if(shiti[l].AI == 4)
    {
        shiti[l].state = (shiti[l].type == 9) ? ST_FLY : ST_FOLLOW_CHAIN;
        return;
    }
    if(shiti[l].AI == 5)
    {
        shiti[l].state = (shiti[l].chv > 0) ? ST_FLEE : ST_WANDER;
        return;
    }
    shiti[l].state = ST_IDLE;
}
// AI==2 投射物：出界 / 超时 / 触块 / 命中玩家 / nbt2 范围击退；返回 false 表示已移除
static bool ai_projectile(int l, double old_gx, double old_gy)
{
    if(out_of_active_area(shiti[l].gx, shiti[l].gy))
    {
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
        st_kill(l);
    }
    shiti[l].randomtick++;
    if(shiti[l].randomtick > 1200)
    {
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
        st_kill(l);
    }
    if(shiti[l].nbt <= 3)
    {
        shiti[l].spgx *= 0.995;
        shiti[l].gx += shiti[l].spgx * 0.03;
        // 顶视角模式：箭矢不受重力，保持直线飞行
    }
    else
    {
        shiti[l].gx += shiti[l].spgx * 0.03;
    }
    int ex, ey;
    short eb = ent_block(l, ex, ey);
    if(blocktouch[eb] == true && shiti[l].nbt != 4 && shiti[l].nbt != 5)
    {
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
        st_kill(l);
        return false;
    }
    if(abs(x - shiti[l].gx) < 0.75 && abs(y - shiti[l].gy) < 0.75 &&
            shiti[l].nbt != 2 && shiti[l].nbt != 3 && shiti[l].nbt != 6 && shiti[l].nbt != 7)
    {
        damage(5 + currentLevel, shiti[l].gx, true);   // 箭矢命中不击退玩家（伤害随关卡提升）
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
        st_kill(l);
        return false;
    }
    if(shiti[l].nbt == 2)
    {
        for(int LL = 0; LL < 500; LL++)
        {
            if(shiti[LL].type > 0 && LL != l)
            {
                if(abs(shiti[LL].gx - shiti[l].gx) < 1 &&
                        abs(shiti[LL].gy - shiti[l].gy) < 1 && shiti[LL].AI != 2)
                {
                    if(shiti[LL].type != 5 && shiti[LL].type < 9)
                    {
                        shiti[LL].spgy = -6,
                        shiti[LL].spgx = ((shiti[LL].gx - x > 0) * 2 - 1) * 0.4;
                    }
                    if(!(shiti[LL].type == 5 && shiti[LL].islife2 == 1))
                        shiti[LL].HP -= 25;   // 箭伤害匹配200血体系
                    update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
                    if(shiti[LL].HP <= 0)
                    {
                        if(shiti[LL].type == 5 && shiti[LL].islife2 == 0)
                        {
                            shiti[LL].islife2 = 1;
                            shiti[LL].HP = 1;
                            shiti[LL].gongjitick = 0;
                        }
                        else if(shiti[LL].type != 8)
                        {
                            // Boss(type8)不在此处击杀，由其AI的分裂逻辑处理
                            st_kill_fall(LL);
                        }
                    }
                    update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
                    st_kill(l);
                    continue;
                }
            }
        }
    }
    if(shiti[l].nbt == 6 || shiti[l].nbt == 7)  // 枪械子弹：命中怪物造成伤害
    {
        for(int LL = 0; LL < 500; LL++)
        {
            if(shiti[LL].type > 0 && LL != l && shiti[LL].AI != 2)
            {
                if(abs(shiti[LL].gx - shiti[l].gx) < 0.9 &&
                        abs(shiti[LL].gy - shiti[l].gy) < 0.9)
                {
                    double bd = shiti[l].direction > 0 ? shiti[l].direction : 4;
                    shiti[LL].HP -= bd;
                    if(shiti[LL].type == 5 && shiti[LL].islife2 == 0 && shiti[LL].HP <= 0)
                    {
                        shiti[LL].islife2 = 1;
                        shiti[LL].HP = 1;
                    }
                    else if(shiti[LL].HP <= 0 && shiti[LL].type != 8)
                    {
                        // Boss(type8)不在此处击杀，由其AI的分裂逻辑处理
                        st_kill_fall(LL);
                        if(inDungeon && roomWavesLeft > 0) roomWavesLeft--;
                    }
                    if(shiti[l].nbt == 7 && !hasTalent(T_CRITPEN))
                    {
                        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
                        st_kill(l);
                        return false;
                    }
                    if(shiti[l].nbt == 6 && !hasTalent(T_BOUNCE))
                    {
                        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
                        st_kill(l);
                        return false;
                    }
                    // 弹射/穿透天赋：继续飞行
                }
            }
        }
    }
    if(shiti[l].nbt <= 3)
    {
        shiti[l].gy += shiti[l].spgy * 0.03;   // 与x轴同比例：直线飞行
        shiti[l].spgy *= 0.995;
    }
    else
    {
        if(shiti[l].nbt == 5) shiti[l].gy += g;
        shiti[l].gy += shiti[l].spgy * 0.03;   // 子弹y轴与x轴同比例：直线飞行
    }
    eb = ent_block(l, ex, ey);
    if(blocktouch[eb] == true && shiti[l].nbt != 4 && shiti[l].nbt != 5)
    {
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
        st_kill(l);
        return false;
    }
    if(abs(x - shiti[l].gx) < 0.75 && abs(y - shiti[l].gy) < 0.75 &&
            shiti[l].nbt != 2 && shiti[l].nbt != 3 && shiti[l].nbt != 6 && shiti[l].nbt != 7)
    {
        damage(5 + currentLevel, shiti[l].gx, true);   // 箭矢命中不击退玩家（伤害随关卡提升）
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
        st_kill(l);
        return false;
    }
    return true;
}
// AI==3 飞行 Boss：死亡分裂 / 点击攻击 / 转向相位(小于300追，小于等于600缓，900冲刺，1020循环)
static bool ai_flyboss(int l, double old_gx, double old_gy)
{
    checkhp = 1;
    if(shiti[l].HP <= 0)
    {
        boss2_die_split(l, old_gx, old_gy);
        return false;
    }
    if(shiti[l].HP != shiti[l].direction)
        boss2hp -= shiti[l].direction - shiti[l].HP;
    shiti[l].direction = shiti[l].HP;
    gj = 0;
    if(KEY_DOWN(VK_LBUTTON) &&
            (abs(mpx - shiti[l].gx) < 2 && abs(mpy - shiti[l].gy) < 2) &&
            playerCD <= 0)
    {
        if(abs(x - shiti[l].gx) < 4.5 && abs(y - shiti[l].gy) < 4.5)
        {
            if(spy > 2)
                shiti[l].HP -= gongji * 1.5, shiti[l].spgy = -8, gj = 2,
                playerCD = gongjicd;
            else
                shiti[l].HP -= gongji, shiti[l].spgy = -6, gj = 1,
                playerCD = gongjicd;
            if(shiti[l].gongjitick > 600 && shiti[l].gongjitick < 900)
                shiti[l].HP -= gongji * 10;
            if(shiti[l].HP <= 0)
            {
                boss2hp -= shiti[l].direction;
                if(shiti[l].gongjitick > 600 && shiti[l].gongjitick < 900)
                {
                    boom_add(int(shiti[l].gx + 0.5), int(shiti[l].gy + 0.5), -4);
                    for(int LL = 0; LL < 500; LL++)
                    {
                        if(shiti[LL].type > 0)
                        {
                            if(jl(abs(shiti[l].gx - shiti[LL].gx),
                                    abs(shiti[l].gy - shiti[LL].gy)) <= 8)
                                shiti[LL].HP -=
                                    (8 - jl(abs(shiti[l].gx - shiti[LL].gx),
                                            abs(shiti[l].gy - shiti[LL].gy))) * 2;
                        }
                    }
                }
                int xxxx = shiti[l].gx, yyyy = shiti[l].gy, nbtt = shiti[l].nbt - 1;
                update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
                st_kill(l);
                if(nbtt >= 1)
                {
                    if(nbtt >= 0)
                        make_st(8, fbnq[nbtt], 1 + (nbtt / 2.0),
                                xxxx + rand() % 5 - 2, yyyy + rand() % 5 - 2, nbtt);
                    if(nbtt - 1 >= 0)
                        make_st(8, fbnq[nbtt - 1], 1 + (nbtt / 2.0),
                                xxxx + rand() % 5 - 2, yyyy + rand() % 5 - 2, nbtt - 1);
                }
                return false;
            }
        }
    }
    if(gj == 1 && shiti[l].gongjitick < 900)
        shiti[l].spgy *= -3, shiti[l].spgx *= -3;
    if(gj == 2 && shiti[l].gongjitick < 900)
        shiti[l].spgy *= -4, shiti[l].spgx *= -4;
    shiti[l].gongjitick++;
    if(shiti[l].gongjitick < 300)
        shiti[l].spgx += ((x - shiti[l].gx) * 0.05 +
                          (((x - shiti[l].gx) > 0) * 2 - 1) * 0.4) *
                         (shiti[l].nbt / 10.0 + 0.1);
    else if(shiti[l].gongjitick <= 600)
        shiti[l].spgx += ((x - shiti[l].gx) * 0.02 +
                          (((x - shiti[l].gx) > 0) * 2 - 1) * 0.2) *
                         (shiti[l].nbt / 10.0 + 0.1);
    if(shiti[l].spgx < -24 && shiti[l].gongjitick < 900)
        shiti[l].spgx = -24;
    if(shiti[l].spgx > 24 && shiti[l].gongjitick < 900)
        shiti[l].spgx = 24;
    shiti[l].gx += shiti[l].spgx * 0.01;
    if(shiti[l].gongjitick < 300)
        shiti[l].spgy -= ((y - shiti[l].gy) * 0.05 +
                          (((y - shiti[l].gy) > 0) * 2 - 1) * 0.6) *
                         (shiti[l].nbt / 10.0 + 0.1);
    else if(shiti[l].gongjitick <= 600)
        shiti[l].spgy -= ((y - shiti[l].gy - 3) * 0.02 +
                          (((y - shiti[l].gy - 3) > 0) * 2 - 1) * 0.2) *
                         (shiti[l].nbt / 10.0 + 0.1);
    if(shiti[l].spgy < -24 && shiti[l].gongjitick < 900)
        shiti[l].spgy = -24;
    if(shiti[l].spgy > 24 && shiti[l].gongjitick < 900)
        shiti[l].spgy = 24;
    shiti[l].gy -= shiti[l].spgy * 0.01;
    if(shiti[l].CD > 0)
        shiti[l].CD--;
    if(shiti[l].gongjitick > 300 && shiti[l].gongjitick <= 600 &&
            shiti[l].CD <= 0 && shiti[l].nbt > 5)
        shiti[l].CD = (20 - shiti[l].nbt) * 12,
        make_jian(shiti[l].gx, shiti[l].gy, (x - shiti[l].gx),
                  (y - shiti[l].gy) * 5 - 1, 0);
    if(shiti[l].gongjitick == 900)
        shiti[l].spgx = (x - shiti[l].gx) * (shiti[l].nbt / 2.0),
        shiti[l].spgy = -((y - shiti[l].gy) * (shiti[l].nbt / 2.0));
    if(shiti[l].gongjitick > 1020)
        shiti[l].gongjitick = 0;
    shiti[l].spgx *= 0.98;
    shiti[l].spgy *= 0.98;
    if(abs(x - shiti[l].gx) < 0.5 && abs(y - shiti[l].gy) < 0.5 && wudi <= 0)
    {
        life -= shiti[l].hurt * jianshang;
        wudi = 45;
    }
    return true;
}
// AI==4 Boss3：type9 自主转向(ST_FLY) / 其余跟随 conl 链(ST_FOLLOW_CHAIN)
static bool ai_boss3(int l, double old_gx, double old_gy)
{
    if(KEY_DOWN(VK_LBUTTON) &&
            (abs(mpx - shiti[l].gx) < 2 && abs(mpy - shiti[l].gy) < 2) &&
            playerCD <= 0)
    {
        if(abs(x - shiti[l].gx) < 4.5 && abs(y - shiti[l].gy) < 4.5)
        {
            if(spy > 2)
                shiti[l].HP -= gongji * 1.5;
            else
                shiti[l].HP -= gongji;
            playerCD = gongjicd;
            if(shiti[l].HP < 0)
                st_kill(l);
        }
    }
    boss3hp += shiti[l].HP;
    if(shiti[l].CD > 0) shiti[l].CD--;
    touch_atk(l, shiti[l].hurt, 1);
    if(shiti[l].chv < 480) shiti[l].chv++;
    else
    {
        if(rand() % 2 == 0)
        {
            shiti[l].chv = 0;
            double ttx, tty, total, px, py;
            ttx = abs(x - shiti[l].gx);
            tty = abs(y - shiti[l].gy);
            total = ttx + tty + 0.001;
            px = ttx / total;
            py = tty / total;
            if(x - shiti[l].gx < 0) px *= -1;
            if(y - shiti[l].gy < 0) py *= -1;
            make_jian(shiti[l].gx, shiti[l].gy, 10 * px, 15 * py, 4);
        }
        else shiti[l].chv = 0;
    }
    if(shiti[l].type == 9)
    {
        double ttx, tty, total, px, py, speed = 0.1;
        bool ischangespeed = 1;
        shiti[l].gongjitick++;
        if(shiti[l].gongjitick == 1) shiti[l].tox = x, shiti[l].toy = y;
        if(shiti[l].gongjitick <= 300 && !(shiti[l].randomtick > 60))
        {
            if(jl(abs(shiti[l].gx - x), abs(shiti[l].gy - y)) < 10 || shiti[l].issd)
            {
                ischangespeed = 0;
                shiti[l].tox = x, shiti[l].toy = y;
                shiti[l].direction = shiti[l].spgx * 25;
                speed = 0.15;
                shiti[l].issd = 1;
                shiti[l].randomtick++;
            }
            else
            {
                shiti[l].tox = x, shiti[l].toy = y;
                speed = 0.12;
                if(jl(abs(shiti[l].gx - x), abs(shiti[l].gy - y)) > 30) speed = 0.3;
            }
        }
        else
        {
            shiti[l].tox += shiti[l].direction * 0.1;
            if(shiti[l].randomtick > 180)
                shiti[l].direction += ((x > shiti[l].gx) * 2 - 1) * 0.25;
            shiti[l].direction *= 0.95;
            shiti[l].toy += 0.25;
            speed = 0.1;
            shiti[l].randomtick++;
        }
        if(ischangespeed)
        {
            ttx = abs(shiti[l].gx - shiti[l].tox);
            tty = abs(shiti[l].gy - shiti[l].toy);
            total = ttx + tty + 0.001;
            px = ttx / total;
            py = tty / total;
            if(shiti[l].tox - shiti[l].gx < 0) px *= -1;
            if(shiti[l].toy - shiti[l].gy < 0) py *= -1;
            if(godeater) speed *= 2.5;
            shiti[l].spgx += px * speed;
            shiti[l].spgy += py * speed;
            shiti[l].spgx *= 0.85;
            shiti[l].spgy *= 0.85;
        }
        shiti[l].gx += shiti[l].spgx;
        shiti[l].gy += shiti[l].spgy;
        if(shiti[l].gongjitick >= 600 || shiti[l].randomtick > 300)
        {
            shiti[l].gongjitick = 1;
            shiti[l].issd = 0;
            shiti[l].randomtick = 0;
        }
    }
    else
    {
        shiti[l].gx += (shiti[shiti[l].conl].gx - shiti[l].gx) / (7 - godeater * 4);
        shiti[l].gy += (shiti[shiti[l].conl].gy - shiti[l].gy) / (7 - godeater * 4);
    }
    return true;
}
// AI==5 被动生物（羊）：点击受击逃跑 / 流体 / 落地 / ST_FLEE 或 ST_WANDER / 避水
static void ai_passive(int l, double old_gx, double old_gy)
{
    gj = 0;
    // NPC(14-22)不可被攻击
    if(shiti[l].type >= 14 && shiti[l].type <= 22)
    {
        // 雇佣兵随从(nbt==1)：跟随玩家并自动攻击附近怪物
        if(shiti[l].type == 17 && shiti[l].nbt == 1)
        {
            if(!inDungeon)   // 离开地牢后随从自动离开
            {
                st_kill(l);
                return;
            }
            // 距离玩家过远(换房间等)则瞬移到玩家身边（避免传送进墙里）
            if(abs(shiti[l].gx - x) > 12 || abs(shiti[l].gy - y) > 12)
            {
                int tpx = int(x - 0.5), tpy = int(y + 0.5);
                if(tpx < 1 || tpy < 1 || tpx >= WORLD_WIDTH - 1 ||
                        tpy >= WORLD_HEIGHT - 1 ||
                        blocktouch[s[tpx][tpy]] >= 1)
                {
                    tpx = int(x + 0.5);
                    tpy = int(y + 0.5);
                }
                shiti[l].gx = tpx;
                shiti[l].gy = tpy;
            }
            // 搜索玩家附近的敌人
            int tgt = -1;
            double bestd = 10;
            for(int LL = 0; LL < 500; LL++)
            {
                if(shiti[LL].type > 0 && LL != l && shiti[LL].AI != 2 &&
                        (shiti[LL].type < 14 || shiti[LL].type > 22))
                {
                    double d = jl(abs(shiti[LL].gx - x), abs(shiti[LL].gy - y));
                    if(d < bestd) { bestd = d; tgt = LL; }
                }
            }
            // 目标：有敌人靠近则追击敌人，否则跟随玩家
            double tx = x - 1, ty = y;
            if(tgt >= 0) { tx = shiti[tgt].gx; ty = shiti[tgt].gy; }
            double dx = tx - shiti[l].gx, dy = ty - shiti[l].gy;
            double dist = jl(abs(dx), abs(dy));
            if(dist > 1.2)
            {
                double sp = 0.15;
                double nx = dx / (dist + 0.001), ny = dy / (dist + 0.001);
                shiti[l].gx += nx * sp;
                shiti[l].gy += ny * sp;
                int eex2, eey2;
                short eb2 = ent_block(l, eex2, eey2);
                if(blocktouch[eb2] >= 1)   // 撞墙回退
                {
                    shiti[l].gx -= nx * sp;
                    shiti[l].gy -= ny * sp;
                }
            }
            // 近身攻击：周期性对目标造成伤害
            if(shiti[l].CD > 0) shiti[l].CD--;
            if(tgt >= 0)
            {
                double dd = jl(abs(shiti[tgt].gx - shiti[l].gx),
                               abs(shiti[tgt].gy - shiti[l].gy));
                if(dd < 1.8 && shiti[l].CD <= 0)
                {
                    shiti[l].CD = 25;
                    shiti[tgt].HP -= 30 + currentLevel * 10;   // 随从伤害匹配200血体系
                    if(shiti[tgt].HP <= 0 && shiti[tgt].type != 8)
                        st_kill_fall(tgt);   // Boss由其AI分裂逻辑处理
                }
            }
            return;
        }
        // 不攻击、不逃跑，仅保持静止
        if(shiti[l].CD > 0) shiti[l].CD--;
        if(shiti[l].jp == 1) { shiti[l].spgy = -8; shiti[l].jp = 2; }
        ent_update_state(l);
        int eex, eey;
        short ffb = ent_block(l, eex, eey);
        if((ffb >= 19 && ffb <= 22) || (ffb >= 32 && ffb <= 34))
        {
            shiti[l].spgy += g; shiti[l].spgx *= 0.5; shiti[l].spgy *= 0.75;
            shiti[l].gy += shiti[l].spgy * 0.02;
        }
        else
        {
            shiti[l].spgy += g;
            shiti[l].gy += shiti[l].spgy * 0.02;
            if(shiti[l].spgy > 30) shiti[l].spgy = 30;
            if(blocktouch[ffb] >= 1 && shiti[l].spgy > 0)
                shiti[l].gy = eey - 1, shiti[l].spgy = 0, shiti[l].jp = 0;
        }
        shiti[l].gx += shiti[l].spgx * 0.02;
        return;
    }
    if(KEY_DOWN(VK_LBUTTON) &&
            (abs(mpx - shiti[l].gx) < 2 && abs(mpy - shiti[l].gy) < 2) &&
            playerCD <= 0)
    {
        if(abs(x - shiti[l].gx) < 4.5 && abs(y - shiti[l].gy) < 4.5)
        {
            if(spy > 2)
                shiti[l].HP -= gongji * 1.5, gj = 2;
            else
                shiti[l].HP -= gongji, gj = 1;
            playerCD = gongjicd;
            shiti[l].chv = 240;
            shiti[l].issd = 1;
            shiti[l].spgy = -6;
            shiti[l].spgx += ((shiti[l].gx > x) * 2 - 1) * 0.6;
            if(shiti[l].HP <= 0)
            {
                update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
                st_kill_fall(l);
                return;
            }
        }
    }
    if(shiti[l].HP <= 0 || out_of_active_area(shiti[l].gx, shiti[l].gy))
    {
        update_entity_hash(l, old_gx, old_gy, shiti[l].gx, shiti[l].gy);
        st_kill(l);
        return;
    }
    if(shiti[l].CD > 0)
        shiti[l].CD--;
    if(shiti[l].jp == 1)
    {
        shiti[l].spgy = -8;
        shiti[l].jp = 2;
    }
    ent_update_state(l);
    int ex, ey;
    short fluid_block = ent_block(l, ex, ey);
    if((fluid_block >= 19 && fluid_block <= 22) ||
            (fluid_block >= 32 && fluid_block <= 34))
    {
        shiti[l].spgy += g;
        shiti[l].spgx *= 0.5;
        shiti[l].spgy *= 0.75;
        if(fluid_block >= 19 && fluid_block <= 22 && shiti[l].spgy > 1.5)
            shiti[l].spgy = 1.5;
        shiti[l].gy += shiti[l].spgy * 0.02;
    }
    else
    {
        shiti[l].spgy += g;
        shiti[l].gy += shiti[l].spgy * 0.06;
        shiti[l].spgy *= 0.98;
    }
    short eb = ent_block(l, ex, ey);
    if(blocktouch[eb] == true)
    {
        if(shiti[l].spgy >= 0)
        {
            while(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)
                shiti[l].gy -= 0.1;
            shiti[l].jp = 0;
            shiti[l].spgy = 0.01;
        }
        if(shiti[l].spgy < 0)
        {
            while(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == true)
                shiti[l].gy += 0.1;
            shiti[l].spgy = 2;
        }
    }
    if(shiti[l].state == ST_FLEE)
    {
        shiti[l].chv--;
        if(shiti[l].gx > x)
            shiti[l].spgx += 0.012;
        else
            shiti[l].spgx -= 0.012;
    }
    else
    {
        shiti[l].issd = 0;
        shiti[l].randomtick++;
        if(shiti[l].randomtick > 80)
        {
            shiti[l].randomtick = 0;
            if(shiti[l].direction != 0)
                shiti[l].direction = 0;
            else
                shiti[l].direction = (rand() % 2) * 2 - 1;
        }
        shiti[l].spgx += 0.015 * shiti[l].direction;
    }
    int cx = int(shiti[l].gx + 0.5), cy = int(shiti[l].gy + 0.5);
    bool in_water_now = (cx >= 0 && cx < WORLD_WIDTH && cy >= 0 &&
                         cy < WORLD_HEIGHT &&
                         s[cx][cy] >= 19 && s[cx][cy] <= 22);
    if(!in_water_now && shiti[l].spgx != 0)
    {
        int ahead_x = (shiti[l].spgx > 0) ? cx + 1 : cx - 1;
        if(ahead_x >= 0 && ahead_x < WORLD_WIDTH)
        {
            bool water_ahead = false;
            for(int ddy = -1; ddy <= 1; ddy++)
            {
                int yy = cy + ddy;
                if(yy >= 0 && yy < WORLD_HEIGHT &&
                        s[ahead_x][yy] >= 19 && s[ahead_x][yy] <= 22)
                {
                    water_ahead = true;
                    break;
                }
            }
            if(water_ahead)
            {
                shiti[l].spgx = 0;
                shiti[l].direction = -shiti[l].direction;
                shiti[l].randomtick = 0;
            }
        }
    }
    eb = ent_block(l, ex, ey);
    if(eb >= 19 && eb <= 22)
        shiti[l].gx += shiti[l].spgx * 0.5;
    else
        shiti[l].gx += shiti[l].spgx;
    shiti[l].spgx *= 0.9;
    eb = ent_block(l, ex, ey);
    if(blocktouch[eb] == true)
    {
        int Step = 1;
        while(Step < 60)
        {
            shiti[l].gx -= 0.03 * Step;
            if(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == false)
                break;
            shiti[l].gx += 0.06 * Step;
            if(blocktouch[s[int(shiti[l].gx + 0.5)][int(shiti[l].gy + 0.5)]] == false)
                break;
            shiti[l].gx -= 0.03 * Step;
            Step++;
        }
        if(shiti[l].jp == 0)
            shiti[l].jp = 1;
        shiti[l].spgx = 0;
    }
}
void st_move()
{
    checkhp = 0;
    boss3hp = 0;
    if(playerCD > 0)
        playerCD--;
    // 关键修复：改用局部循环变量 ei。make_st/make_jian 会写入全局 l（寻找空槽下标），
    // Boss 分裂/射击在遍历过程中生成新实体会改写 l，原写法导致当帧后续实体被跳过更新
    for(int ei = 0; ei < 500; ei++)
    {
        if(shiti[ei].type > 0)
        {
            double old_gx = shiti[ei].gx;
            double old_gy = shiti[ei].gy;
            if(shiti[ei].AI == 1)
            {
                if(!ai_ground(ei, old_gx, old_gy))
                    continue;
            }
            else if(shiti[ei].AI == 2)
            {
                if(!ai_projectile(ei, old_gx, old_gy))
                    continue;
            }
            else if(shiti[ei].AI == 3)
            {
                if(!ai_flyboss(ei, old_gx, old_gy))
                    continue;
            }
            else if(shiti[ei].AI == 4)
            {
                if(!ai_boss3(ei, old_gx, old_gy))
                    continue;
            }
            else if(shiti[ei].AI == 5)
            {
                ai_passive(ei, old_gx, old_gy);
                if(shiti[ei].type == 0)
                    continue;
            }
            update_entity_hash(ei, old_gx, old_gy, shiti[ei].gx, shiti[ei].gy);
        }
    }
    if(!checkhp)
        boss2hp = 0;
}
int open_;
short color_change = 0;
void bb(short nowwz, short chos, bool change_)
{
    for(int j = 10; j <= 20; j += 2)
    {
        Setpos(17 - change_ * 2, j + 5 + change_ * 3);
        if(change_)
            cout << ' ';
        for(int i = (j - 10) / 2 * 5 + 1; i <= (j - 10) / 2 * 5 + 5; i++)
        {
            Color(-7);
            if(i == nowwz)
            {
                Color(-8);
                if(chos > 0)
                {
                    cout << beibao[chos][1] << test[beibao[chos][0]] << "  ";
                    if((test[beibao[chos][0]].length()+(beibao[chos][1]>9))<9)printf("  ");
                    else if((test[beibao[chos][0]].length()+(beibao[chos][1]>9))<11)printf(" ");
                }
                else
                    cout << box[open_].things[29 + chos] / 1000
                         << test[box[open_].things[29 + chos] % 1000] << "    ";
                continue;
            }
            if(i == chos)
            {
                if(nowwz > 0)
                {
                    if(beibao[nowwz][1] > 0)
                        cout << beibao[nowwz][1] << test[beibao[nowwz][0]] << "    ";
                    else
                        cout << " 无      ";
                }
                else
                {
                    if(box[open_].things[29 + nowwz] >= 1000)
                        cout << box[open_].things[29 + nowwz] / 1000
                             << test[box[open_].things[29 + nowwz] % 1000] << "    ";
                    else
                        cout << " 无      ";
                }
            }
            else if(beibao[i][1] > 0)
            {
                cout << beibao[i][1] << test[beibao[i][0]] << "  ";
                if((test[beibao[i][0]].length()+(beibao[i][1]>9))<9)printf("  ");
                else if((test[beibao[i][0]].length()+(beibao[i][1]>9))<11)printf(" ");
            }
            else
                cout << " 无      ";
        }
        cout << " ";
    }
    if(change_)
    {
        for(int j = 10; j <= 18; j += 2)
        {
            Setpos(15, j - 6);
            cout << ' ';
            for(int i = (j - 10) / 2 * 5; i <= (j - 10) / 2 * 5 + 4; i++)
            {
                Color(-7);
                if(i == 29 + nowwz && nowwz < 0)
                {
                    Color(-8);
                    if(chos > 0)
                        cout << beibao[chos][1] << test[beibao[chos][0]] << "   ";
                    else
                        cout << box[open_].things[29 + chos] / 1000
                             << test[box[open_].things[29 + chos] % 1000] << "   ";
                    continue;
                }
                if(i == 29 + chos && chos < 0)
                {
                    if(nowwz > 0)
                    {
                        if(beibao[nowwz][1] > 0)
                            cout << beibao[nowwz][1] << test[beibao[nowwz][0]] << "   ";
                        else
                            cout << " 无     ";
                    }
                    else
                    {
                        if(box[open_].things[29 + nowwz] >= 1000)
                            cout << box[open_].things[29 + nowwz] / 1000
                                 << test[box[open_].things[29 + nowwz] % 1000] << "   ";
                        else
                            cout << " 无     ";
                    }
                }
                else if(box[open_].things[i] >= 1000)
                {
                    cout << box[open_].things[i] / 1000
                         << test[box[open_].things[i] % 1000] << "   ";
                }
                else
                    cout << " 无     ";
            }
            cout << "      ";
        }
    }
    Sleep(1);
}
void craft_update(int num, bool isprintback = 0, bool ischangecolor = 0)
{
    Color(-7);
    if(isprintback)
    {
        Setpos(4, 4);
        printf("===================================================================="
               "===============");
        for(int i = 5; i <= 26; i++)
        {
            Setpos(4, i);
            printf("|                                                                "
                   "                 |");
        }
        Setpos(4, 27);
        printf("===================================================================="
               "===============");
    }
    Setpos(20, 7);
    cout << "  护甲栏 ";
    Setpos(19, 9);
    if(wear[1] == 27)
        cout << " 铁胸甲   ";
    else if(wear[1] == 80)
        cout << " 金胸甲   ";
    else if(wear[1] == 57)
        cout << " 钻石胸甲 ";
    else if(wear[1] == 84)
        cout << " 下界胸甲 ";
    else if(wear[1] == 107)
        cout << " 暗影胸甲 ";
    else
        cout << " 胸甲位   ";
    Setpos(19, 12);
    if(wear[2] == 28)
        cout << " 铁护腿   ";
    else if(wear[2] == 81)
        cout << " 金护腿   ";
    else if(wear[2] == 58)
        cout << " 钻石护腿 ";
    else if(wear[2] == 85)
        cout << " 下界护腿 ";
    else if(wear[2] == 108)
        cout << " 暗影护腿 ";
    else
        cout << " 护腿位   ";
    Setpos(24, 9);
    if(wear[3] == 29)
        cout << " 铁头盔   ";
    else if(wear[3] == 79)
        cout << " 金头盔   ";
    else if(wear[3] == 56)
        cout << " 钻石头盔 ";
    else if(wear[3] == 83)
        cout << " 下界头盔 ";
    else if(wear[3] == 106)
        cout << " 暗影头盔 ";
    else
        cout << " 头盔位   ";
    Setpos(24, 12);
    if(wear[4] == 30)
        cout << " 铁靴子   ";
    else if(wear[4] == 82)
        cout << " 金靴子   ";
    else if(wear[4] == 59)
        cout << " 钻石靴子 ";
    else if(wear[4] == 86)
        cout << " 下界靴子 ";
    else if(wear[4] == 109)
        cout << " 暗影靴子 ";
    else
        cout << " 靴子位  ";
    for(int j = 10; j <= 20; j += 2)
    {
        Setpos(17, j + 5);
        for(int i = (j - 10) / 2 * 5 + 1; i <= (j - 10) / 2 * 5 + 5; i++)
        {
            Color(-7);
            if(i == color_change && ischangecolor)
                Color(-8);
            if(beibao[i][1] > 0)
            {
                cout << beibao[i][1] << test[beibao[i][0]] << "  ";
                if((test[beibao[i][0]].length()+(beibao[i][1]>9))<9)printf("  ");
                else if((test[beibao[i][0]].length()+(beibao[i][1]>9))<11)printf(" ");
            }
            else
                cout << " 无      ";
        }
        printf("       ");
    }
    Color(-7);
    if(num == 1)
    {
        Setpos(6, 7);
        cout << " 合成配方：";
        Setpos(6, 8);
        cout << "  1原木->2木棍";
        Setpos(6, 9);
        cout << "  2木棍+3原木->木镐";
        Setpos(6, 10);
        cout << "  2木棍+5圆石->石镐";
        Setpos(6, 11);
        cout << "  20圆石+5煤炭->熔炉";
        Setpos(6, 12);
        cout << "  2木棍+3铁锭->铁镐";
        Setpos(6, 13);
        cout << "  10黑曜石->下界传送门";
        Setpos(6, 14);
        cout << "  1木棍+1煤炭->5火把";
        Setpos(6, 15);
        cout << "  1木棍+2圆石->石剑";
        Setpos(6, 16);
        cout << "  2木棍+3圆石->石斧";
        Setpos(6, 17);
        cout << "  3铁锭->桶";
        Setpos(6, 18);
        cout << "  8铁锭->铁胸甲";
        Setpos(6, 19);
        cout << "  7铁锭->铁护腿";
        Setpos(6, 20);
        cout << "  5铁锭->铁头盔";
        Setpos(6, 21);
        cout << "  4铁锭->铁靴子";
        Setpos(6, 22);
        cout << "  9金粒->金锭";
        Setpos(6, 23);
        cout << "  金锭->9金粒";
        Setpos(6, 24);
        cout << "  下一页";
    }
    else if(num == 2)
    {
        Setpos(6, 7);
        cout << " 熔炼配方：";
        Setpos(6, 8);
        cout << " 当前燃烧时长："<<rl<<"    ";
        Setpos(6, 9);
        cout << "  煤炭->8燃烧时长";
        Setpos(6, 10);
        cout << "  原木->2燃烧时长";
        Setpos(6, 11);
        cout << "  木棍->1燃烧时长";
        Setpos(6, 12);
        cout << "  粗铁+燃烧->铁锭";
        Setpos(6, 13);
        cout << "  粗金+燃烧->金锭";
        Setpos(6, 14);
        cout << "  原木+燃烧->煤炭";
        Setpos(6, 15);
        cout << "  远古残骸+燃烧->";
        Setpos(6, 16);
        cout << "  下界碎片";
    }
    else if(num == 3)
    {
        Setpos(6, 7);
        cout << " 合成配方：";
        Setpos(6, 8);
        cout << "  8原木->箱子";
        Setpos(6, 9);
        cout << "  3木棍+3线->木弓";
        Setpos(6, 10);
        cout << "  1木棍+1圆石->8木箭";
        Setpos(6, 11);
        cout << "  1木棍+1铁锭->8铁箭";
        Setpos(6, 12);
        cout << "  1木棍+1火药->8爆炸箭";
        Setpos(6, 13);
        cout << "  1木棍+2铁锭->铁剑";
        Setpos(6, 14);
        cout << "  2木棍+3铁锭->铁斧";
        Setpos(6, 15);
        cout << "  2木棍+3钻石->钻石镐";
        Setpos(6, 16);
        cout << "  1木棍+2钻石->钻石剑";
        Setpos(6, 17);
        cout << "  2木棍+3钻石->钻石斧";
        Setpos(6, 18);
        cout << "  4铁锭->OI";
        Setpos(6, 19);
        cout << "  5钻石->钻石头盔";
        Setpos(6, 20);
        cout << "  8钻石->钻石胸甲";
        Setpos(6, 21);
        cout << "  7钻石->钻石护腿";
        Setpos(6, 22);
        cout << "  4钻石->钻石靴子";
        Setpos(6, 23);
        cout << "  3木板+3羊毛->床";
        Setpos(6, 24);
        cout << "  下一页   上一页";
    }
    else if(num == 4)
    {
        Setpos(6, 7);
        cout << " 合成配方：";
        Setpos(6, 8);
        cout << "  1原木->4木板";
        Setpos(6, 9);
        cout << "  木板->2木平台";
        Setpos(6, 10);
        cout << "  4金锭+4下界碎片";
        Setpos(6, 11);
        cout << "  ->下界锭";
        Setpos(6, 12);
        cout << "  钻石镐+下界锭 ";
        Setpos(6,13);
        cout << "  ->下界镐";
        Setpos(6, 14);
        cout << "  钻石剑+下界锭 ";
        Setpos(6,15);
        cout << "  ->下界剑";
        Setpos(6, 16);
        cout << "  钻石斧+下界锭 ";
        Setpos(6,17);
        cout << "  ->下界斧";
        Setpos(6, 18);
        cout << "  10腐肉->蠕虫诱饵";
        Setpos(6, 19);
        cout << "  钻石头盔+下界锭 ";
        Setpos(6,20);
        cout << "  ->下界头盔";
        Setpos(6,21);
        cout << "  钻石胸甲+下界锭 ";
        Setpos(6,22);
        cout << "  ->下界胸甲";
        Setpos(6, 24);
        cout << "  下一页   上一页";
    }
    else if(num == 6)
    {
        Setpos(6, 7);
        cout << " 合成配方：";
        Setpos(6, 8);
        cout << "  钻石护腿+下界锭 ";
        Setpos(6, 9);
        cout << "  ->下界护腿";
        Setpos(6, 10);
        cout << "  钻石靴子+下界锭 ";
        Setpos(6, 11);
        cout << "  ->下界靴子";
        Setpos(6, 24);
        cout << "  下一页   上一页";
    }
    else if(num == 5)
    {
        Setpos(6, 7);
        cout << " 合成配方：";
        Setpos(6, 8);
        cout << "  2木棍+3金锭->金镐";
        Setpos(6, 9);
        cout << "  1木棍+2金锭->金剑";
        Setpos(6, 10);
        cout << "  2木棍+3金锭->金斧";
        Setpos(6, 11);
        cout << "  5金锭->金头盔";
        Setpos(6, 12);
        cout << "  8金锭->金胸甲";
        Setpos(6, 13);
        cout << "  7金锭->金护腿";
        Setpos(6, 14);
        cout << "  4金锭->金靴子";
        Setpos(6, 15);
        cout << "  8金锭+1苹果->金苹果";
        Setpos(6, 24);
        cout << "  上一页";
    }
    else if(num == 8)
    {
        Setpos(6, 7);
        cout << " 合成配方：";
        Setpos(6, 8);
        cout << "  4木板+2铁锭->锻造台";
        Setpos(6, 9);
        cout << "  腐化石块->4腐化石墙";
        Setpos(6, 10);
        cout << "  5暗影鳞片->暗影头盔";
        Setpos(6, 11);
        cout << "  8暗影鳞片->暗影胸甲";
        Setpos(6, 12);
        cout << "  7暗影鳞片->暗影护腿";
        Setpos(6, 13);
        cout << "  4暗影鳞片->暗影靴子";
        Setpos(6, 14);
        cout << "  10暗影鳞片->魔光剑";
        Setpos(6, 15);
        cout << "  8暗影鳞片->恶魔弓";
        Setpos(6, 16);
        cout << "  5暗影鳞片+5金锭";
        Setpos(6, 17);
        cout << "  ->魔光护符";
        Setpos(6, 24);
        cout << "  上一页";
    }
    else if(num == 7)
    {
        Setpos(6, 7);
        cout << " 升级配方：";
        Setpos(6, 8);
        cout << "  钻石镐+下界锭 ";
        Setpos(6, 9);
        cout << "  ->下界镐";
        Setpos(6, 10);
        cout << "  钻石剑+下界锭 ";
        Setpos(6, 11);
        cout << "  ->下界剑";
        Setpos(6, 12);
        cout << "  钻石斧+下界锭 ";
        Setpos(6, 13);
        cout << "  ->下界斧";
        Setpos(6, 14);
        cout << "  钻石头盔+下界锭 ";
        Setpos(6, 15);
        cout << "  ->下界头盔";
        Setpos(6, 16);
        cout << "  钻石胸甲+下界锭 ";
        Setpos(6, 17);
        cout << "  ->下界胸甲";
        Setpos(6, 18);
        cout << "  钻石护腿+下界锭 ";
        Setpos(6, 19);
        cout << "  ->下界护腿";
        Setpos(6, 20);
        cout << "  钻石靴子+下界锭 ";
        Setpos(6, 21);
        cout << "  ->下界靴子";
    }
}
short AN;
bool wear_things(int wid,int tplace,int cc,int c_thing,int upd)
{
	if(wid!=beibao[cc][0])return 0;
	bool iswear=0;
	if(tplace==1){
		if(c_thing == -7 || c_thing == -8 ||c_thing == -2 || c_thing == -3)
        {
            iswear=1;
        }
	}
	if(tplace==2){
		if(c_thing == 2 || c_thing == 1)
        {
            iswear=1;
        }
	}
	if(tplace==3){
		if(c_thing == -6 || c_thing == -1)
        {
            iswear=1;
        }
	}
	if(tplace==4){
		if(c_thing == 3)
        {
            iswear=1;
        }
	}
	if(iswear){
	wear[tplace] = wid;
	beibao[cc][0] = 0;
	beibao[cc][1] = 0;
    craft_update(upd);
	return true;}
    return false;
}
void movethings(int upd)
{
    int c_thing = 0, cc = 0;
    int wear_slot = 0; // 护甲栏位置索引
    POINT p = GetMousePos();
    bool upda = (mpx != To_int(p.x / 2 - 26) || mpy != To_int(p.y - 14));
    mpx = To_int(p.x / 2 - 26);
    mpy = To_int(p.y - 14);
    c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
    // 检测护甲栏点击
    if(c_thing == -7 || c_thing == -8 || c_thing == -2 || c_thing == -3)
        wear_slot = 1; // 胸甲位
    else if(c_thing == 2 || c_thing == 1)
        wear_slot = 2; // 护腿位
    else if(c_thing == -6 || c_thing == -1)
        wear_slot = 3; // 头盔位
    else if(c_thing == 3)
        wear_slot = 4; // 靴子位
    if(wear_slot > 0 && wear[wear_slot] != 0)
    {
        if(KEY_DOWN(VK_LBUTTON))
        {
            int wear_item = wear[wear_slot];
            while(KEY_DOWN(VK_LBUTTON))
                continue;
            while(!(KEY_DOWN(VK_LBUTTON)))
            {
                POINT p = GetMousePos();
                mpx = To_int(p.x / 2 - 26);
                mpy = To_int(p.y - 14);
                c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
            }
            if(KEY_DOWN(VK_LBUTTON))
            {
                POINT p = GetMousePos();
                mpx = To_int(p.x / 2 - 26);
                mpy = To_int(p.y - 14);
                c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
                // 拖到背包格子时卸下盔甲
                if(c_thing > 0 && c_thing < 31)
                {
                    int O = getbeibao(wear_item);
                    if(O != 0)
                    {
                        beibao[O][0] = wear_item;
                        beibao[O][1] += 1;
                        wear[wear_slot] = 0;
                        hujia_update();
                        craft_update(upd);
                    }
                }
            }
            while(KEY_DOWN(VK_LBUTTON))
                continue;
        }
    }
    if(!(int((mpx + 12) / 4 + 0.5) > 5 || int((mpx + 12) / 4 + 0.5) < 1 ||
            int(mpy / 2 * 5 + 0.5) > 26 || int(mpy + 0.5) < 1))
    {
        c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
        if(c_thing < 31 && c_thing > 0)
        {
            if(GetAsyncKeyState('Q') & 0x8000)
            {
                cc = c_thing;
                if(GetAsyncKeyState(VK_CONTROL))
                    beibao[cc][1] = 0;
                else if(beibao[cc][1] > 0)
                    beibao[cc][1] -= 1;
                craft_update(1);
                while(GetAsyncKeyState('Q') & 0x8000 && AN < 40)
                    AN++, Sleep(1);
            }
            else
                AN = 0;
            if(KEY_DOWN(VK_LBUTTON) && beibao[c_thing][1] != 0)
            {
                cc = c_thing;
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                while(!(KEY_DOWN(VK_LBUTTON)))
                {
                    POINT p = GetMousePos();
                    mpx = To_int(p.x / 2 - 26);
                    mpy = To_int(p.y - 14);
                    c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
                    if(!(int((mpx + 12) / 4 + 0.5) > 5 ||
                            int((mpx + 12) / 4 + 0.5) < 1 || int(mpy / 2 * 5 + 0.5) > 26 ||
                            int(mpy / 2 * 5 + 0.5) < 0))
                        bb(c_thing, cc, 0);
                }
                if(KEY_DOWN(VK_LBUTTON))
                {
                    if(cc % 5 == 0)
                        Setpos(33, (cc / 5 + 13) * 2 - 12), cout << "   ";
                    else
                        Setpos((cc % 5) * 4 + 13, (cc / 5 + 14) * 2 - 12), cout << "   ";
                    POINT p = GetMousePos();
                    mpx = To_int(p.x / 2 - 26);
                    mpy = To_int(p.y - 14);
                    c_thing = int((mpx + 12) / 4 + 0.5) + int(mpy / 2 * 5 + 0.5);
                    if(wear_things(27,1,cc,c_thing,upd));
                    else if(wear_things(28,2,cc,c_thing,upd));
                    else if(wear_things(29,3,cc,c_thing,upd));
                    else if(wear_things(30,4,cc,c_thing,upd));
                    else if(wear_things(80,1,cc,c_thing,upd));
                    else if(wear_things(81,2,cc,c_thing,upd));
                    else if(wear_things(79,3,cc,c_thing,upd));
                    else if(wear_things(82,4,cc,c_thing,upd));
                    else if(wear_things(57,1,cc,c_thing,upd));
                    else if(wear_things(58,2,cc,c_thing,upd));
                    else if(wear_things(56,3,cc,c_thing,upd));
                    else if(wear_things(59,4,cc,c_thing,upd));
                    else if(wear_things(84,1,cc,c_thing,upd));
                    else if(wear_things(85,2,cc,c_thing,upd));
                    else if(wear_things(83,3,cc,c_thing,upd));
                    else if(wear_things(86,4,cc,c_thing,upd));
                    else if(wear_things(107,1,cc,c_thing,upd));
                    else if(wear_things(108,2,cc,c_thing,upd));
                    else if(wear_things(106,3,cc,c_thing,upd));
                    else if(wear_things(109,4,cc,c_thing,upd));
                    else if(!(int((mpx + 12) / 4 + 0.5) > 5 ||
                              int((mpx + 12) / 4 + 0.5) < 1 ||
                              int(mpy / 2 * 5 + 0.5) > 26 ||
                              int(mpy / 2 * 5 + 0.5) < 0))
                    {
                        if((cc != c_thing))
                        {
                            if(beibao[cc][0] != beibao[c_thing][0])
                                swap(beibao[cc], beibao[c_thing]), craft_update(upd);
                            else if(beibao[cc][1] + beibao[c_thing][1] <= 64)
                                beibao[c_thing][1] += beibao[cc][1], beibao[cc][1] = 0,
                                                      craft_update(upd);
                            else
                                beibao[cc][1] = beibao[cc][1] + beibao[c_thing][1] - 64,
                                                beibao[c_thing][1] = 64, craft_update(upd);
                        }
                    }
                }
            }
            while(KEY_DOWN(VK_LBUTTON))
                continue;
        }
        if(upda)
        {
            if(c_thing < 31 && c_thing > 0)
                color_change = c_thing;
            else
                color_change = 0;
            if(upd==2)
            {
                craft_update(2,0,1);
            }
            else if(upd==7)
            {
                craft_update(7,0,1);   // 锻造台升级界面
            }
            else if(page == 1)
                craft_update(1, 0, 1);
            else if(page == 2)
                craft_update(3, 0, 1);
            else if(page == 3)
                craft_update(4, 0, 1);
            else if(page == 4)
                craft_update(6, 0, 1);
            else if(page == 5)
                craft_update(5, 0, 1);
            else craft_update(8, 0, 1);
        }
    }
}
void boxprint()
{
    for(int j = 10; j <= 20; j += 2)
    {
        Setpos(15, j + 8);
        cout << ' ';
        for(int i = (j - 10) / 2 * 5 + 1; i <= (j - 10) / 2 * 5 + 5; i++)
        {
            Color(-7);
            if(i == color_change)
                Color(-8);
            if(beibao[i][1] > 0)
            {
                cout << beibao[i][1] << test[beibao[i][0]] << "   ";
            }
            else
                cout << " 无     ";
        }
        cout << "      ";
    }
    for(int j = 10; j <= 18; j += 2)
    {
        Setpos(15, j - 6);
        cout << ' ';
        for(int i = (j - 10) / 2 * 5; i <= (j - 10) / 2 * 5 + 4; i++)
        {
            Color(-7);
            if(i == 29 + color_change)
                Color(-8);
            if(box[open_].things[i] >= 1000)
            {
                cout << box[open_].things[i] / 1000 << test[box[open_].things[i] % 1000]
                     << "   ";
            }
            else
                cout << " 无     ";
        }
        cout << "      ";
    }
}
void moveinbox()
{
    int c_thing = 0, cc = 0;
    POINT p = GetMousePos();
    bool upda = (mpx != To_int(p.x / 2 - 24) || mpy != To_int(p.y - 17));
    mpx = To_int(p.x / 2 - 24);
    mpy = To_int(p.y - 17);
    if(!(int((mpx + 12) / 4 + 0.5) > 5 || int((mpx + 12) / 4 + 0.5) < 1 ||
            int(mpy / 2 * 5 + 0.5) > 21 || int(mpy + 0.5) < -12))
    {
        c_thing = int((mpx + 12) / 4 + 0.5) + mpy / 2 * 5;
        if(c_thing < 26 && c_thing > -30 && (c_thing < -4 || c_thing > 0))
        {
            Setpos(0, 0);
            cout << 29 + c_thing << "   " << mpy << "  ";
            if(GetAsyncKeyState('Q') & 0x8000)
            {
                cc = c_thing;
                if(c_thing > 0)
                {
                    if(GetAsyncKeyState(VK_CONTROL))
                        beibao[cc][1] = 0;
                    else if(beibao[cc][1] > 0)
                        beibao[cc][1] -= 1;
                }
                else
                {
                    if(GetAsyncKeyState(VK_CONTROL))
                        box[open_].things[29 + cc] = 0;
                    else if(box[open_].things[29 + cc] > 1000)
                        box[open_].things[29 + cc] -= 1000;
                }
                boxprint();
                while(GetAsyncKeyState('Q') & 0x8000 && AN < 40)
                    AN++, Sleep(1);
            }
            else
                AN = 0;
            if(KEY_DOWN(VK_LBUTTON))
            {
                if((c_thing > 0 && beibao[c_thing][1] != 0) ||
                        (c_thing < -4 && box[open_].things[(29 + c_thing)] >= 1000))
                {
                    cc = c_thing;
                    while(KEY_DOWN(VK_LBUTTON))
                        continue;
                    while(!(KEY_DOWN(VK_LBUTTON)))
                    {
                        POINT p = GetMousePos();
                        mpx = To_int(p.x / 2 - 24);
                        mpy = To_int(p.y - 17);
                        c_thing = int((mpx + 12) / 4 + 0.5) + mpy / 2 * 5;
                        if(!(int((mpx + 12) / 4 + 0.5) > 5 ||
                                int((mpx + 12) / 4 + 0.5) < 1 ||
                                int(mpy / 2 * 5 + 0.5) > 21 || int(mpy + 0.5) < -12))
                            bb(c_thing, cc, 1);
                    }
                    if(KEY_DOWN(VK_LBUTTON))
                    {
                        POINT p = GetMousePos();
                        mpx = To_int(p.x / 2 - 24);
                        mpy = To_int(p.y - 17);
                        c_thing = int((mpx + 12) / 4 + 0.5) + mpy / 2 * 5;
                        if(!(int((mpx + 12) / 4 + 0.5) > 5 ||
                                int((mpx + 12) / 4 + 0.5) < 1 ||
                                int(mpy / 2 * 5 + 0.5) > 21 || int(mpy + 0.5) < -12))
                        {
                            if((cc != c_thing))
                            {
                                if(cc > 0 && c_thing > 0)
                                {
                                    if(beibao[cc][0] != beibao[c_thing][0])
                                        swap(beibao[cc], beibao[c_thing]);
                                    else if(beibao[cc][1] + beibao[c_thing][1] <= 64)
                                        beibao[c_thing][1] += beibao[cc][1], beibao[cc][1] = 0;
                                    else
                                        beibao[cc][1] = beibao[cc][1] + beibao[c_thing][1] - 64,
                                                        beibao[c_thing][1] = 64;
                                }
                                else if(cc < 0 && c_thing > 0)
                                {
                                    ;
                                    if(beibao[c_thing][0] !=
                                            box[open_].things[(29 + cc)] % 1000)
                                    {
                                        int swap1 = box[open_].things[(29 + cc)];
                                        box[open_].things[(29 + cc)] =
                                            beibao[c_thing][0] + beibao[c_thing][1] * 1000;
                                        beibao[c_thing][0] = swap1 % 1000,
                                                             beibao[c_thing][1] = swap1 / 1000;
                                    }
                                    else if(beibao[c_thing][1] +
                                            box[open_].things[(29 + cc)] / 1000 <=
                                            64)
                                        box[open_].things[(29 + cc)] += beibao[c_thing][1] * 1000,
                                                                        beibao[c_thing][1] = 0;
                                    else
                                        beibao[c_thing][1] = beibao[c_thing][1] +
                                                             box[open_].things[(29 + cc)] / 1000 -
                                                             64,
                                                             box[open_].things[(29 + cc)] =
                                                                 64000 + box[open_].things[(29 + cc)] % 1000;
                                }
                                else if(cc > 0 && c_thing < 0)
                                {
                                    if(beibao[cc][0] !=
                                            box[open_].things[(29 + c_thing)] % 1000)
                                    {
                                        int swap1 = box[open_].things[(29 + c_thing)];
                                        box[open_].things[(29 + c_thing)] =
                                            beibao[cc][0] + beibao[cc][1] * 1000;
                                        beibao[cc][0] = swap1 % 1000, beibao[cc][1] = swap1 / 1000;
                                    }
                                    else if(beibao[cc][1] +
                                            box[open_].things[(29 + c_thing)] / 1000 <=
                                            64)
                                        box[open_].things[(29 + c_thing)] += beibao[cc][1] * 1000,
                                                                             beibao[cc][1] = 0;
                                    else
                                        beibao[cc][1] = beibao[cc][1] +
                                                        box[open_].things[(29 + c_thing)] / 1000 -
                                                        64,
                                                        box[open_].things[(29 + c_thing)] =
                                                            64000 + box[open_].things[29 + c_thing] % 1000;
                                }
                                else if(cc < 0 && c_thing < 0)
                                {
                                    if(box[open_].things[(29 + cc)] % 1000 !=
                                            box[open_].things[(29 + c_thing)] % 1000)
                                        swap(box[open_].things[(29 + cc)],
                                             box[open_].things[(29 + c_thing)]);
                                    else if(box[open_].things[(29 + cc)] / 1000 +
                                            box[open_].things[(29 + c_thing)] / 1000 <=
                                            64)
                                        box[open_].things[(29 + c_thing)] +=
                                            box[open_].things[29 + cc] / 1000 * 1000,
                                            box[open_].things[(29 + cc)] = 0;
                                    else
                                        box[open_].things[29 + cc] =
                                            box[open_].things[(29 + cc)] % 1000 +
                                            (box[open_].things[(29 + cc)] / 1000 +
                                             box[open_].things[(29 + c_thing)] / 1000 - 64) *
                                            1000,
                                            box[open_].things[(29 + c_thing)] =
                                                64000 +
                                                box[open_].things[(29 + c_thing)] %
                                                1000;
                                }
                            }
                        }
                    }
                }
            }
            while(KEY_DOWN(VK_LBUTTON))
                continue;
            POINT p = GetMousePos();
            mpx = To_int(p.x / 2 - 24);
            mpy = To_int(p.y - 17);
        }
    }
    if(upda)
    {
        if(c_thing < 26 && c_thing > -30)
            color_change = c_thing;
        else
            color_change = -3;
        boxprint();
    }
}
void SetConsoleToFullscreen()
{
    HWND hwnd = GetConsoleWindow();
    // 设置初始窗口标题
    SetWindowTextW(hwnd, L"我的世界");
    // 最大化窗口（Windows 会将其置于工作区左上角）
    ShowWindow(hwnd, SW_MAXIMIZE);
}
void zz(int Y, int need1, int type1, int need2, int type2, int get, int sl,
        int upd)
{
    if(mpy == Y && mpx < -9)
    {
        if(KEY_DOWN(VK_LBUTTON))
        {
            Setpos(5, Y + 13), cout << " ->";
            while(KEY_DOWN(VK_LBUTTON))
                continue;
            for(int i = 1; i <= 100; i++)
            {
                ls[i][0] = beibao[i][0];
                ls[i][1] = beibao[i][1];
            }
            int U = getbeibao_3(type1, need1);
            int PP = getbeibao_3(type2, need2);
            if(U > 0 && PP > 0)
            {
                bool noroom = false;
                for(int i = 0; i < sl; i++)
                {
                    int O = getbeibao(get);
                    if(O == 0)
                    {
                        noroom = true;
                        break;
                    }
                    beibao[O][0] = get;
                    beibao[O][1] += 1;
                }
                if(noroom)
                {
                    for(int i = 1; i <= 100; i++)
                    {
                        beibao[i][0] = ls[i][0];
                        beibao[i][1] = ls[i][1];
                    }
                }
                craft_update(upd);
            }
            else
            {
                for(int i = 1; i <= 100; i++)
                {
                    beibao[i][0] = ls[i][0];
                    beibao[i][1] = ls[i][1];
                }
            }
        }
        else
            Setpos(5, Y + 13), cout << "-> ";
    }
    else
        Setpos(5, Y + 13), cout << "   ";
}
void zz2(int Y, int need1, int type1, int get, int sl, int upd)
{
    if(mpy == Y && mpx < -9)
    {
        if(KEY_DOWN(VK_LBUTTON))
        {
            Setpos(5, Y + 13), cout << " ->";
            while(KEY_DOWN(VK_LBUTTON))
                continue;
            for(int i = 1; i <= 100; i++)
            {
                ls[i][0] = beibao[i][0];
                ls[i][1] = beibao[i][1];
            }
            int U = getbeibao_3(type1, need1);
            if(U > 0)
            {
                bool noroom = false;
                for(int i = 0; i < sl; i++)
                {
                    int O = getbeibao(get);
                    if(O == 0)
                    {
                        noroom = true;
                        break;
                    }
                    beibao[O][0] = get;
                    beibao[O][1] += 1;
                }
                if(noroom)
                {
                    for(int i = 1; i <= 100; i++)
                    {
                        beibao[i][0] = ls[i][0];
                        beibao[i][1] = ls[i][1];
                    }
                }
                craft_update(upd);
            }
            else
            {
                for(int i = 1; i <= 100; i++)
                {
                    beibao[i][0] = ls[i][0];
                    beibao[i][1] = ls[i][1];
                }
            }
        }
        else
            Setpos(5, Y + 13), cout << "-> ";
    }
    else
        Setpos(5, Y + 13), cout << "   ";
}
void zz_rl(int Y, int need1, int type1, int sl)
{
    if(mpy == Y && mpx < -9)
    {
        if(KEY_DOWN(VK_LBUTTON))
        {
            Setpos(5, Y + 13), cout << " ->";
            while(KEY_DOWN(VK_LBUTTON))
                continue;
            for(int i = 1; i <= 100; i++)
            {
                ls[i][0] = beibao[i][0];
                ls[i][1] = beibao[i][1];
            }
            int U = getbeibao_3(type1, need1);
            if(U > 0)
            {
                rl+=sl;
                craft_update(2);
            }
            else
            {
                for(int i = 1; i <= 100; i++)
                {
                    beibao[i][0] = ls[i][0];
                    beibao[i][1] = ls[i][1];
                }
            }
        }
        else
            Setpos(5, Y + 13), cout << "-> ";
    }
    else
        Setpos(5, Y + 13), cout << "   ";
}
void zz_rl_2(int Y, int need1, int type1,int get, int sl)
{
    if(mpy == Y && mpx < -9)
    {
        if(KEY_DOWN(VK_LBUTTON))
        {
            Setpos(5, Y + 13), cout << " ->";
            while(KEY_DOWN(VK_LBUTTON))
                continue;
            for(int i = 1; i <= 100; i++)
            {
                ls[i][0] = beibao[i][0];
                ls[i][1] = beibao[i][1];
            }
            int U = getbeibao_3(type1, need1);
            if(U > 0&&rl)
            {
                bool noroom = false;
                for(int i = 0; i < sl; i++)
                {
                    int O = getbeibao(get);
                    if(O == 0)
                    {
                        noroom = true;
                        break;
                    }
                    beibao[O][0] = get;
                    beibao[O][1] += 1;
                }
                if(noroom)
                {
                    for(int i = 1; i <= 100; i++)
                    {
                        beibao[i][0] = ls[i][0];
                        beibao[i][1] = ls[i][1];
                    }
                }
                else
                    rl--;
                craft_update(2);
            }
            else
            {
                for(int i = 1; i <= 100; i++)
                {
                    beibao[i][0] = ls[i][0];
                    beibao[i][1] = ls[i][1];
                }
            }
        }
        else
            Setpos(5, Y + 13), cout << "-> ";
    }
    else
        Setpos(5, Y + 13), cout << "   ";
}
void openbox()
{
    Color(-7);
    bag=1;
    open_ = findbox(mpx, mpy);
    if(open_ == -1)
        return;
    Setpos(7, 2);
    printf("====================================================================="
           "===");
    for(int i = 3; i <= 13; i++)
    {
        Setpos(7, i);
        printf("|                                                                  "
               "    |");
    }
    Setpos(7, 14);
    printf("====================================================================="
           "===");
    Setpos(7, 16);
    printf("====================================================================="
           "===");
    for(int i = 17; i <= 29; i++)
    {
        Setpos(7, i);
        printf("|                                                                  "
               "    |");
    }
    Setpos(7, 30);
    printf("====================================================================="
           "===");
    AN = 0;
    while(1)
    {
        if(GetAsyncKeyState('E') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
        {
            while(GetAsyncKeyState('E') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
                continue;
            memset(painting, 114, sizeof(painting));
            bag=0;
            break;
        }
        moveinbox();
        Sleep(1);
    }
}
void craft_2()
{
    bag=1;
    craft_update(2, 1);
    AN = 0;
    while(1)
    {
        if(GetAsyncKeyState('E') & 0x8000)
        {
            if(anx == 0)
            {
                anx = 1;
                bag=0;
                break;
            }
            anx = 1;
        }
        else
            anx = 0;
        movethings(2);
        zz_rl(-4, 1, 3, 8);
        zz_rl(-3, 1, 2, 2);
        zz_rl(-2, 1, 6, 1);
        zz_rl_2(-1, 1, 4, 13,1);
        zz_rl_2(0, 1, 62, 64,1);
        zz_rl_2(1, 1, 2, 3,1);
        zz_rl_2(2, 1, 69, 70,1);
        Sleep(1);
    }
    setlight();
    memset(painting, 114, sizeof(painting));
}
void craft_3()
{
    bag=1;
    craft_update(7, 1);
    AN = 0;
    page = 7;
    while(1)
    {
        if(GetAsyncKeyState('E') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
        {
            while(GetAsyncKeyState('E') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
                Sleep(1);
            bag=0;
            break;
        }
        movethings(7);
        zz(-5, 1, 15, 1, 71, 72, 1, 7);   // 钻石镐+下界锭->下界镐
        zz(-3, 1, 49, 1, 71, 73, 1, 7);   // 钻石剑+下界锭->下界剑
        zz(-1, 1, 50, 1, 71, 74, 1, 7);   // 钻石斧+下界锭->下界斧
        zz(1, 1, 56, 1, 71, 83, 1, 7);    // 钻石头盔+下界锭->下界头盔
        zz(3, 1, 57, 1, 71, 84, 1, 7);    // 钻石胸甲+下界锭->下界胸甲
        zz(5, 1, 58, 1, 71, 85, 1, 7);    // 钻石护腿+下界锭->下界护腿
        zz(7, 1, 59, 1, 71, 86, 1, 7);    // 钻石靴子+下界锭->下界靴子
        Sleep(1);
    }
    setlight();
    memset(painting, 114, sizeof(painting));
}
void craft_1()
{
    bag=1;
    craft_update(1, 1);
    AN = 0;
    page = 1;
    while(1)
    {
        if(GetAsyncKeyState('E') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
        {
            while(GetAsyncKeyState('E') & 0x8000 || GetAsyncKeyState(VK_ESCAPE))
                continue;
            page = 1;
            bag=0;
            break;
        }
        else
            anx = 0;
        if(page == 1)
            movethings(1);
        else if(page == 2)
            movethings(3);
        else if(page == 3)
            movethings(4);
        else if(page == 4)
            movethings(6);
        else if(page == 5)
            movethings(5);
        else movethings(8);
        if(page == 1)
        {
            if(mpy == -5 && mpx < -9)
            {
                if(KEY_DOWN(VK_LBUTTON))
                {
                    Setpos(5, 8), cout << " ->";
                    while(KEY_DOWN(VK_LBUTTON))
                        continue;
                    int U = getbeibao_2(2);
                    if(U > 0)
                    {
                        int O = getbeibao(6);
                        if(O > 0)
                        {
                            beibao[U][1]--;
                            beibao[O][0] = 6;
                            beibao[O][1] += 1;
                            O = getbeibao(6);
                            if(O > 0)
                                beibao[O][0] = 6, beibao[O][1] += 1;
                            craft_update(1);
                        }
                    }
                }
                else
                    Setpos(5, 8), cout << "-> ";
            }
            else
                Setpos(5, 8), cout << "   ";
            zz(-4, 3, 2, 2, 6, 7, 1, 1);
            zz(-3, 5, 1, 2, 6, 8, 1, 1);
            zz(-2, 20, 1, 5, 3, 9, 1, 1);
            zz(-1, 3, 13, 2, 6, 14, 1, 1);
            zz2(0, 10, 36, 38, 1, 1);
            zz(1, 1, 3, 1, 6, 16, 5, 1);
            zz(2, 1, 6, 2, 1, 17, 1, 1);
            zz(3, 2, 6, 3, 1, 18, 1, 1);
            zz2(4, 3, 13, 25, 1, 1);
            zz2(5, 8, 13, 27, 1, 1);
            zz2(6, 7, 13, 28, 1, 1);
            zz2(7, 5, 13, 29, 1, 1);
            zz2(8, 4, 13, 30, 1, 1);
            zz2(9, 9, 65, 64, 1, 1);
            zz2(10, 1, 64, 65, 9, 1);
        }
        else if(page == 2)
        {
            zz2(-5, 8, 2, 40, 1, 3);
            zz(-4, 3, 6, 3, 42, 41, 1, 3);
            zz(-3, 1, 6, 1, 1, 44, 8, 3);
            zz(-2, 1, 6, 1, 13, 45, 8, 3);
            zz(-1, 1, 6, 1, 43, 46, 8, 3);
            zz(0, 1, 6, 2, 13, 47, 1, 3);
            zz(1, 2, 6, 3, 13, 48, 1, 3);
            zz(2, 3, 12, 2, 6, 15, 1, 3);
            zz(3, 1, 6, 2, 12, 49, 1, 3);
            zz(4, 2, 6, 3, 12, 50, 1, 3);
            zz2(5, 4, 13, 51, 1, 3);
            zz2(6, 5, 12, 56, 1, 3);
            zz2(7, 8, 12, 57, 1, 3);
            zz2(8, 7, 12, 58, 1, 3);
            zz2(9, 4, 12, 59, 1, 3);
            zz(10, 3, 67, 3, 87, 90, 1, 3);
        }
        else if(page == 3)
        {
            zz2(-5, 1, 2, 67, 4, 4);
            zz2(-4, 1, 67, 66, 2, 4);
            zz(-3, 4, 64, 4, 70, 71, 1, 4);
            zz(-1, 1, 15, 1, 71, 72, 1, 4);
            zz(1, 1, 49, 1, 71, 73, 1, 4);
            zz(3, 1, 50, 1, 71, 74, 1, 4);
            zz2(5, 10, 37, 75, 1, 4);
            zz(6, 1, 56, 1, 71, 83, 1, 4);
            zz(8, 1, 57, 1, 71, 84, 1, 4);
        }
        else if(page == 4)
        {
            zz(-5, 1, 58, 1, 71, 85, 1, 6);
            zz(-3, 1, 59, 1, 71, 86, 1, 6);
        }
        else if(page == 5)
        {
            zz(-5, 2, 6, 3, 64, 76, 1, 5);
            zz(-4, 1, 6, 2, 64, 77, 1, 5);
            zz(-3, 2, 6, 3, 64, 78, 1, 5);
            zz2(-2, 5, 64, 79, 1, 5);
            zz2(-1, 8, 64, 80, 1, 5);
            zz2(0, 7, 64, 81, 1, 5);
            zz2(1, 4, 64, 82, 1, 5);
            zz(2, 8, 64, 1, 55, 91, 1, 5);
        }
        else
        {
            // 第6页：锻造台/暗影装备（beta1.8同步）
            zz(-5, 2, 13, 4, 67, 104, 1, 8);    // 2铁锭+4木板->锻造台
            zz2(-4, 1, 68, 92, 4, 8);           // 腐化石块->4腐化石墙
            zz2(-3, 5, 105, 106, 1, 8);         // 5暗影鳞片->暗影头盔
            zz2(-2, 8, 105, 107, 1, 8);         // 8暗影鳞片->暗影胸甲
            zz2(-1, 7, 105, 108, 1, 8);         // 7暗影鳞片->暗影护腿
            zz2(0, 4, 105, 109, 1, 8);          // 4暗影鳞片->暗影靴子
            zz2(1, 10, 105, 110, 1, 8);         // 10暗影鳞片->魔光剑
            zz2(2, 8, 105, 111, 1, 8);          // 8暗影鳞片->恶魔弓
            zz(3, 5, 105, 5, 64, 112, 1, 8);    // 5暗影鳞片+5金锭->魔光护符
        }
        if(mpy == 11 && mpx < -9)
        {
            bool hasPrev = (page > 1);
            bool hasNext = (page < 6);
            if(KEY_DOWN(VK_LBUTTON))
            {
                if(mpx > -15 && hasPrev)
                {
                    Setpos(10, 24), cout << "->";
                    Setpos(5, 24), cout << "   ";
                }
                else if(mpx <= -15 && hasNext)
                {
                    Setpos(5, 24), cout << "->";
                    Setpos(10, 24), cout << "   ";
                }
                while(KEY_DOWN(VK_LBUTTON))
                {
                    Sleep(1);
                }
                if(mpx > -15 && hasPrev) page--;
                else if(mpx <= -15 && hasNext) page++;
                for(int i = 8; i <= 24; i++)
                    Setpos(5, i), cout << "                        ";
                if(page == 1) craft_update(1);
                else if(page == 2) craft_update(3);
                else if(page == 3) craft_update(4);
                else if(page == 4) craft_update(6);
                else if(page == 5) craft_update(5);
                else craft_update(8);
            }
            else
            {
                if(mpx > -15 && hasPrev)
                {
                    Setpos(10, 24), cout << "->";
                    Setpos(5, 24), cout << "   ";
                }
                else if(mpx <= -15 && hasNext)
                {
                    Setpos(5, 24), cout << "->";
                    Setpos(10, 24), cout << "   ";
                }
                else
                {
                    Setpos(10, 24), cout << "   ";
                    Setpos(5, 24), cout << "   ";
                }
            }
        }
        else
        {
            Setpos(10, 24), cout << "   ";
            Setpos(5, 24), cout << "   ";
        }
        Sleep(1);
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
void SetWindowSize(int cols, int lines);
void update_set()
{
    // 调整窗口和缓冲区大小以适应当前渲染尺寸
    SetWindowSize(printx + 5, printy + 5);
    Setpos(7, 5), Color(-7);
    printf("====================================================================="
           "====");
    for(int i = 6; i <= 28; i++)
    {
        Setpos(7, i);
        printf("|                                                                  "
               "      |");
    }
    Setpos(7, 28);
    printf("====================================================================="
           "=====");
    Setpos(23, 6);
    cout << " 设置";
    Setpos(20, 9);
    cout << " 渲染设置：X轴：" << printx << " \b格     +      -";
    Setpos(20, 10);
    cout << "           Y轴：" << printy << " \b格     +      -";
    Setpos(20, 12);
    cout << " 作弊模式：";
    if(CanGet)
        cout << " 开";
    else
        cout << " 关";
    Setpos(20, 14);
    cout << " 帧率设置：";
    if(setfps == 1)
        cout << " 60帧";
    else if(setfps == 2)
        cout << " 30帧";
    else if(setfps == 3)
        cout << " 20帧";
    Setpos(20, 17);
    cout << " 定格时间： ";
    if(!sun)
        cout << " 开";
    else
        cout << " 关";
    Setpos(21, 19);
    cout << " 难度：";
    if(ed == 8000)
        cout << " 简单";
    else if(ed == 6000)
        cout << " 普通";
    else
        cout << " 困难";
    Setpos(20, 23);
    cout << " 保存并退出游戏";
    Setpos(20, 21);
    cout << " 缓动渲染：";
    if(special_effect == 1)
        cout << " 关";
    else if(special_effect == 2)
        cout << " 开";
    else if(special_effect == 3)
        cout << " 加强";
}
void SET()
{
    bag=1;
    update_set();
    while(1)
    {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            if(anx == 0)
            {
                anx = 1;
                bag=0;
                break;
            }
            anx = 1;
        }
        else
            anx = 0;
        POINT p = GetMousePos();
        mpx = To_int(p.x / 2 - 26);
        mpy = To_int(p.y - 14);
        if(mpy == 0 && mpx > -4 && mpx < 4)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 14), cout << " ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                if(setfps == 1)
                    setfps = 2;
                else if(setfps == 2)
                    setfps = 3;
                else
                    setfps = 1;
                update_set();
            }
            else
                Setpos(18, 14), cout << "-> ";
        }
        else
            Setpos(18, 14), cout << "   ";
        if(mpy == 9 && mpx > -4 && mpx < 4)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 23), cout << " ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                save(zzz);
                run = false;
                return;
            }
            else
                Setpos(18, 23), cout << "-> ";
        }
        else
            Setpos(18, 23), cout << "   ";
        if(mpy == -4 && mpx > 2 && mpx < 8)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(30, 9), cout << "  ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                if(printx < 90)
                    printx += 2,xlen+=4;
                update_set();
            }
            else
                Setpos(30, 9), cout << " -> ";
        }
        else
            Setpos(30, 9), cout << "     ";
        if(mpy == -3 && mpx > 2 && mpx < 8)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(30, 10), cout << "  ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                if(printy < 46)
                    printy += 2,ylen+=2;
                update_set();
            }
            else
                Setpos(30, 10), cout << " -> ";
        }
        else
            Setpos(30, 10), cout << "     ";
        if(mpy == -4 && mpx > 8 && mpx < 16)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(33, 9), cout << "  ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                if(printx > 20)
                    printx -= 2,xlen-=4;
                update_set();
            }
            else
                Setpos(33, 9), cout << " -> ";
        }
        else
            Setpos(33, 9), cout << "     ";
        if(mpy == -3 && mpx > 8 && mpx < 16)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(33, 10), cout << "  ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                if(printy > 20)
                    printy -= 2,ylen-=2;
                update_set();
            }
            else
                Setpos(33, 10), cout << " -> ";
        }
        else
            Setpos(33, 10), cout << "     ";
        if(mpy == 3 && mpx > -4 && mpx < 4)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 17), cout << " ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                sun = (sun == 0 ? 1 : 0);
                update_set();
            }
            else
                Setpos(18, 17), cout << "-> ";
        }
        else
            Setpos(18, 17), cout << "   ";
        if(mpy == 5 && mpx > -4 && mpx < 4)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 19), cout << " ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                ed = (ed == 8000 ? 6000 : (ed == 6000 ? 4000 : 8000));
                update_set();
            }
            else
                Setpos(18, 19), cout << "-> ";
        }
        else
            Setpos(18, 19), cout << "   ";
        if(mpy == -1 && mpx > -4 && mpx < 4)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 12), cout << " ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                CanGet = (CanGet == 0 ? 1 : 0);
                update_set();
            }
            else
                Setpos(18, 12), cout << "-> ";
        }
        else
            Setpos(18, 12), cout << "   ";
        if(mpy ==7 && mpx > -4 && mpx < 4)
        {
            if(KEY_DOWN(VK_LBUTTON))
            {
                Setpos(18, 21), cout << " ->";
                while(KEY_DOWN(VK_LBUTTON))
                    continue;
                if(special_effect == 1)
                    special_effect = 2;
                else if(special_effect == 2)
                    special_effect = 3;
                else
                    special_effect = 1;
                update_set();
            }
            else
                Setpos(18, 21), cout << "-> ";
        }
        else
            Setpos(18, 21), cout << "   ";
        Sleep(1);
    }
}
void cls()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if(GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        DWORD cCharsWrieern;
        DWORD dw = csbi.dwSize.X * csbi.dwSize.Y;
        FillConsoleOutputCharacter(hConsole, ' ', dw, {0, 0}, &cCharsWrieern);
    }
}
void SetWindowSize(int cols, int lines)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    // 设置缓冲区大小，避免超出缓冲区时窗口滚动
    COORD newSize = {(SHORT)(cols * 2 + 1), (SHORT)(lines + 1)};
    SetConsoleScreenBufferSize(hConsole, newSize);
    // 设置控制台窗口区域（不影响外部窗口大小，全屏时由SetConsoleToFullscreen控制）
    SMALL_RECT winSize = {0, 0, (SHORT)(cols * 2), (SHORT)(lines)};
    SetConsoleWindowInfo(hConsole, TRUE, &winSize);
}
void did()
{
    for(int x = 0; x <= 112; x++)
    {
        thing.insert({name[x], x});
    }
    // 羊毛方块属性初始化
    blockwj[87] = 30;
    blocktouch[87] = 1;
    blocklv[87] = 0;
    // 床方块属性初始化
    blockwj[90] = 30;
    blocktouch[90] = 1;
    blocklv[90] = 0;
    // 关卡传送门(93)：不可采
    blockwj[93] = -1;
    blocktouch[93] = 1;
    blocklv[93] = 114;
    // 扭蛋机(102)：不可采
    blockwj[102] = -1;
    blocktouch[102] = 1;
    blocklv[102] = 114;
    // 关卡金矿(103)：可挖
    blockwj[103] = 300;
    blocktouch[103] = 1;
    blocklv[103] = 2;
    // 锻造台(104)：可放置/可挖的实体方块
    blockwj[104] = 200;
    blocktouch[104] = 1;
    blocklv[104] = 2;
}
int lenth_ = 0;
bool bw;
int keydown[129];
void getkeydown(char getzf)
{
    if(GetAsyncKeyState(getzf) & 0x8000 && lenth_ <= 50)
    {
        keydown[(int)(getzf)]++;
        if(keydown[(int)(getzf)] == 1 ||
                (keydown[(int)(getzf)] >= 30 && keydown[(int)(getzf)] % 2 == 0))
        {
            if(getzf >= 65 && getzf <= 90 && bw == false)
                zzz += getzf + 32, lenth_++;
            else
                zzz += getzf, lenth_++;
        }
    }
    else
        keydown[getzf] = 0;
}
bool iscre = false;
void delete_world()
{
    Setpos(18, 27);
    cout << "                                    ";
    Setpos(18, 28);
    cout << "                                    ";
    Setpos(18, 29);
    cout << "                                    ";
    Setpos(23, 3);
    cout << "                      ";
    Setpos(22, 3);
    cout << "  选择要删除的存档  ";
    bool abcde;
    while(1)
    {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            Setpos(22, 3);
            cout << "                      ";
            while(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
                continue;
            return;
        }
        POINT p = GetMousePos();
        mpx = To_int(p.x / 2 - 26);
        mpy = To_int(p.y - 14);
        for(int i = 0; i < read_s; i++)
        {
            abcde = 0;
            Setpos(15, 6 + i * 2);
            if(mpy >= i * 2 - 8 && mpy <= i * 2 - 7)
            {
                Color(10);
                if(KEY_DOWN(VK_LBUTTON))
                {
                    while(KEY_DOWN(VK_LBUTTON))
                        continue;
                    abcde = true;
                    delete_savelist(saves[i]);
                    read_savelist();
                    Clear_2();
                    return;
                }
            }
            else
            {
                Color(0);
            }
            if(!abcde)
                cout << saves[i];
        }
        Sleep(1);
    }
}
void make_newworld()
{
    Setpos(18, 27);
    cout << "                                    ";
    Setpos(18, 28);
    cout << "                                    ";
    Setpos(18, 29);
    cout << "                                    ";
    for(int i = 0; i < read_s; i++)
    {
        Setpos(15, 6 + i * 2);
        cout << "                                    ";
    }
    Setpos(23, 3);
    cout << "               ";
    int brtick = 0;
    zzz.clear();
    while(1)
    {
        if(clock() - Clocknum >= 15)
        {
            if(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                while(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
                    continue;
                break;
            }
            brtick++;
            Color(0);
            Clocknum = clock();
            bw = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
                 (GetKeyState(VK_CAPITAL) & 0x0001);
            for(char zf = 36; zf <= 125; zf++)
            {
                getkeydown(zf);
            }
            if(GetAsyncKeyState('\b') & 0x8000)
            {
                keydown[(int)('\b')]++;
                if((keydown[(int)('\b')] == 1 ||
                        (keydown[(int)('\b')] >= 30 && keydown[(int)('\b')] % 2 == 0)) &&
                        lenth_ > 0)
                    zzz.erase(zzz.end() - 1), lenth_--;
            }
            else
                keydown[(int)('\b')] = 0;
            Setpos(15, 13);
            cout << " 存档名：（由大小写字母和数字组成，不超过50字符）";
            Setpos(15, 15);
            cout << zzz;
            if(brtick % 60 <= 30)
                cout << "_        ";
            else
                cout << "     ";
            POINT p = GetMousePos();
            mpx = To_int(p.x / 2 - 26);
            mpy = To_int(p.y - 14);
            iscre = false;
            if(mpx >= -1 && mpx <= 4 && mpy >= 4 && mpy <= 7)
            {
                Color(1);
                Setpos(23, 18);
                cout << "  ----------";
                Setpos(23, 19);
                cout << " | 创建世界 |";
                Setpos(23, 20);
                cout << "  ----------";
                if(KEY_DOWN(VK_LBUTTON) && lenth_ > 0)
                {
                    MAP();
                    mark_all_dirty();
                    reset_dungeon_state();   // 新开档：重置关卡解锁/天赋/金币/能量
                    save(zzz);
                    save_savelist(zzz);
                    iscre = true;
                    return;
                }
            }
            else
            {
                Color(0);
                Setpos(23, 18);
                cout << "  ----------";
                Setpos(23, 19);
                cout << " | 创建世界 |";
                Setpos(23, 20);
                cout << "  ----------";
            }
        }
        Sleep(1);
    }
}
void print_bag()
{
    // 物品栏必须在WriteConsoleOutput渲染区域（行0~printy）之外，否则每帧被方块渲染覆盖
    Setpos(1, printy + 4);
    Color(0);
    printf("   \n");
    for(int i = 1; i <= 9; i++)
    {
        if(cho == i)
            Color(1);
        else
            Color(0);
        printf("  ");
        if(beibao[i][1] > 0)
        {
            cout << beibao[i][1] << test[beibao[i][0]] << "   ";
        }
        else
            cout << " 无  ";
    }
}
int l_cho=10;
int l_energy = -1, l_coin = -1;
void print_fast()
{
    // 关卡进度组合值：关/层/波/余怪（变化时触发整行重绘）
    int lvlinfo = currentLevel > 0 ?
                  currentLevel * 1000000 + curLayer * 10000 +
                  (roomActive ? roomWavesDone * 100 + min(roomWavesLeft, 99) : 0) : 0;
    if(l_life!=life || l_hujia != hujiazhi || l_lvlinfo != lvlinfo)
    {
        // 生命 + 护甲 + 关卡进度显示（HUD）
        Color(0);
        cout << "  生命：" << int((life * 10) + 0.5) / 10.0 << "/" << lifeMax;
        Color(11);
        cout << "  护甲：" << hujiazhi << "/" << armorMax;
        // 护甲条：按armorMax比例10格
        {
            int aw = armorMax > 0 ? hujiazhi * 10 / armorMax : 0;
            if(aw < 0) aw = 0;
            if(aw > 10) aw = 10;
            for(int i = 0; i < 10; i++) cout << (i < aw ? "■" : "·");
        }
        Color(0);
        if(currentLevel > 0)
        {
            Color(14);
            cout << "  第" << currentLevel << "关 层" << (curLayer + 1) << "/" << layCount;
            if(roomActive)
                cout << " 波" << roomWavesDone << "/" << roomWavesTotal
                     << " 余" << roomWavesLeft;
            Color(0);
        }
        printf("      ");
        l_hujia = hujiazhi;
        l_lvlinfo = lvlinfo;
    }
    // 能量条与金币显示（HUD，渲染区外行printy+2，避免被画面覆盖）
    int tcoins = total_coins();
    if(l_energy != int(energy) || l_coin != tcoins)
    {
        l_energy = int(energy); l_coin = tcoins; coin = tcoins;
        Setpos(1, printy + 2);
        Color(0);
        int bw = int(20.0 * energy / (energyMax > 0 ? energyMax : 1));
        if(bw < 0) bw = 0; if(bw > 20) bw = 20;
        cout << "  能量：" << int(energy) << "/" << int(energyMax) << " ";
        for(int i = 0; i < 20; i++) cout << (i < bw ? "█" : "·");
        printf("   金币：%d                  ", tcoins);
    }
    // 氧气泡显示（能量行右侧，潜水时显示）
    if(oxygen < 895)
    {
        int bubbles = oxygen / 90;
        if(bubbles < 0) bubbles = 0;
        if(bubbles > 10) bubbles = 10;
        Setpos(50, printy + 2);
        Color(11);
        cout << "氧气:";
        for(int i = 0; i < 10; i++)
            cout << (i < bubbles ? "O" : "*");
        Color(0);
        l_oxy = bubbles;
    }
    else if(l_oxy != 10)
    {
        Setpos(50, printy + 2);
        cout << "                ";
        l_oxy = 10;
    }
    print_set=0;
    bool isbagp=0;
    for(int i=1; i<=25; i++)
    {
        if(beibao[i][0]!=l_beibao[i][0]||beibao[i][1]!=l_beibao[i][1])isbagp=1;
        l_beibao[i][0]=beibao[i][0],l_beibao[i][1]=beibao[i][1];
    }
    if(l_cho!=cho)isbagp=1;
    l_cho=cho;
    if(print_set==0)
    {
        if(isbagp)
        {
            print_bag();
        }
    }
    else
    {
        if(tick%5==0)print_bag();
    }
    Color(0);
    Setpos(1, 35);
    if(printy > 30)
        Setpos(1, printy + 3);
    if(printy > 30)
        Setpos(1, printy + 3);
    if(GetAsyncKeyState('Y') & 0x8000)
    {
        key_d_Y=1;
    }
    else if(key_d_Y==1)
    {
        f3=1-f3,key_d_Y=0;
        if(f3==0)printf("                     ");
    }
    // F3坐标/生物统计：值变化才直写控制台（原每帧2次cout+Setpos，双缓冲下帧内直写越少越好）
    {
        static int l_f3x = -999999, l_f3y = -999999, l_sp = -1;
        int f3x = int(x - WORLD_WIDTH / 2 + 0.5), f3y = int(WORLD_HEIGHT - 1 - y);
        if(f3)
        {
            if(f3x != l_f3x || f3y != l_f3y)
                cout << " " << "x:" << f3x << " y:" << f3y << "          ";
        }
        l_f3x = f3x, l_f3y = f3y;
        if(f3)
        {
            int sheep_count = 0, pig_count = 0;
            for(int i = 0; i < 500; i++)
            {
                if(shiti[i].type == 12) sheep_count++;
                else if(shiti[i].type == 13) pig_count++;
            }
            int sp = sheep_count * 1000 + pig_count;
            if(sp != l_sp)
            {
                Setpos(20, 1);
                cout << "羊:" << sheep_count << " 猪:" << pig_count << "   ";
                l_sp = sp;
            }
        }
    }
    l_life=life;
}
void checkuseitem()
{
	gongji = 5;                      // 拳头（伤害匹配200血体系）
    gongjicd = 20;
    if(beibao[cho][0] == 17)
        gongji = 25, gongjicd = 37.5;   // 木剑
    if(beibao[cho][0] == 18)
        gongji = 45, gongjicd = 75;
    if(beibao[cho][0] == 47)
        gongji = 30, gongjicd = 37.5;  // 铁剑
    if(beibao[cho][0] == 48)
        gongji = 45, gongjicd = 67;
    if(beibao[cho][0] == 49)
        gongji = 35, gongjicd = 37.5;  // 钻石剑
    if(beibao[cho][0] == 50)
        gongji = 45, gongjicd = 60;
    if(beibao[cho][0] == 60)
        gongji = 35, gongjicd = 30;
    if(beibao[cho][0] == 73)
        gongji = 45, gongjicd = 37.5;
    if(beibao[cho][0] == 74)
        gongji = 55, gongjicd = 60;
    if(beibao[cho][0] == 77)
        gongji = 20, gongjicd = 37.5;
    if(beibao[cho][0] == 78)
        gongji = 35, gongjicd = 60;
    if(beibao[cho][0] == 110)
        gongji = 40, gongjicd = 30;   // 魔光剑（beta1.8同步）
    if(gongjicd<40&&gongjicd>25)issword=1;
    else issword=0;
}
// ================= 元气骑士地牢系统=================
// 前向声明（避免顺序引用问题）
void spawn_level_wave();
int roomCountForLevel(int n);
void show_talent_pick();
void fill_level_chest(int idx, int lvl, int big);
void spawn_dungeon_npc(int type, int tx, int ty);
// ---- 金币辅助 ----
int add_coins(int n)
{
    if(n <= 0) return 0;
    int given = 0;
    for(int i = 1; i <= 100 && given < n; i++)
    {
        if(beibao[i][0] == 101 && beibao[i][1] < 64)
        {
            int add = min(n - given, 64 - beibao[i][1]);
            beibao[i][1] += add;
            given += add;
        }
    }
    while(given < n)
    {
        int slot = getbeibao(101);
        if(slot == 0) break;
        int add = min(n - given, 64);
        beibao[slot][0] = 101;
        beibao[slot][1] = add;
        given += add;
    }
    return given;
}
int total_coins()
{
    int c = 0;
    for(int i = 1; i <= 100; i++)
        if(beibao[i][0] == 101) c += beibao[i][1];
    return c;
}
bool spend_coins(int n)
{
    if(total_coins() < n) return false;
    int need = n;
    for(int i = 1; i <= 100 && need > 0; i++)
    {
        if(beibao[i][0] == 101)
        {
            int take = min(need, beibao[i][1]);
            beibao[i][1] -= take;
            need -= take;
            if(beibao[i][1] <= 0) beibao[i][0] = 0, beibao[i][1] = 0;
        }
    }
    return true;
}
// ---- 枪械 ----
void make_bullet(double tx, double ty, double stx, double sty, int dmg, int crit)
{
    // 复用 make_jian 生成弹道实体(type=4,AI=2)，用 direction 存伤害，nbt 区分子弹
    make_jian(tx, ty, stx, sty, 6);
    if(l < 500 && shiti[l].type == 4)
    {
        shiti[l].direction = crit ? dmg * 1.5 : dmg;
        shiti[l].nbt = crit ? 7 : 6;
    }
}
int gun_dmg(int gt)
{
    if(gt == 95) return 60;    // 手枪：3发击杀普通怪
    if(gt == 96) return 35;    // 冲锋枪：射速快单发低
    if(gt == 97) return 45;    // 霰弹（每发，5连发）
    if(gt == 98) return 220;   // 狙击：一发重创/秒杀
    return 60;
}
int gun_energy(int gt)
{
    if(gt == 95) return 2;
    if(gt == 96) return 1;
    if(gt == 97) return 4;
    if(gt == 98) return 8;
    return 2;
}
int gun_cd(int gt)
{
    if(gt == 95) return 9;
    if(gt == 96) return 4;
    if(gt == 97) return 26;
    if(gt == 98) return 40;
    return 9;
}
int gun_crit(int gt)
{
    int c = 5; // 基础暴击
    if(gt == 95) c += 10;
    if(gt == 98) c += 20;
    if(hasTalent(T_CRIT)) c += 15;
    if(hasTalent(T_FULLCRIT) && life >= lifeMax) c += 20; // 满血暴击
    return c;
}
void fire_gun(int gt)
{
    // 真实指向角（atan2）：子弹沿直线飞向准星，不再用L1归一化的偏斜方向
    double baseAng = atan2(mpy - y, mpx - x);
    int dmg = gun_dmg(gt);
    int shots = 1;
    if(gt == 97) shots = 5 + 2 * talentLevel(T_MORESHOT);
    if(gt == 97)                        // 霰弹扇形
    {
        for(int gi = 0; gi < shots; gi++)
        {
            double ang = baseAng + (gi - shots / 2.0) * 0.12;
            int crit = (rand() % 100 < gun_crit(gt)) ? 1 : 0;
            make_bullet(x, y - 0.75, 60 * cos(ang), 60 * sin(ang), dmg, crit);
        }
    }
    else
    {
        int crit = (rand() % 100 < gun_crit(gt)) ? 1 : 0;
        double spd = (gt == 96) ? 55 : (gt == 98 ? 110 : 50);
        make_bullet(x, y - 0.75, spd * cos(baseAng), spd * sin(baseAng), dmg, crit);
    }
}
// ---- NPC 生成 ----
void make_npc(int type, int tx, int ty)
{
    // type 14-22 商人/工匠/占星师/雇佣兵/牧师/地牢商人/被关押NPC/奸商/小偷猫
    make_st(type, 999999, 0, tx, ty, 0);
    if(l < 500)
    {
        shiti[l].AI = 5;   // 被动AI
        shiti[l].isnotouch = 1;
    }
}
// ---- 天赋 ----
void grant_talent(int t)
{
    if(talentCount < 24)
    {
        talents[talentCount++] = t;
        if(t == T_LIFE) { lifeMax += 25; life += 25; }            // 生命上限+25
        if(t == T_ARMOR) { armorMax += 50; hujiazhi += 50; }       // 护甲上限+50
        if(t == T_ENERGY) { energyMax += 50; energy += 50; }       // 能量上限+50
    }
}
// ---- 关卡系统 ----
void reset_dungeon_state()
{
    currentLevel = 0; currentRoom = 0; inDungeon = false; roomActive = false;
    roomWavesLeft = 0; roomWaveCD = 0; roomWavesDone = 0; roomWavesTotal = 0;
    levelUnlocked = 1;              // 重新开档：只解锁第1关
    talentCount = 0;
    memset(talents, 0, sizeof(talents));
    lifeMax = 200; armorMax = 200;  // 血量/护甲/蓝量开局全满
    energy = 100; energyMax = 100;
    hujiazhi = armorMax;
    coin = 0;
    lvl_x0 = lvl_x1 = lvl_y0 = lvl_y1 = 0;
}
// 房间几何：房间(L,c) 左上角 rx0=levelStartX+c*22+1, ry0=levelStartY+L*18+1；内部18x12；层间4行走廊
static int roomX0(int L, int c) { return levelStartX + c * 20 + 1; }   // 房间紧贴水平排列，无通道
static int roomY0(int L) { return levelStartY + L * 14 + 1; }          // 房间紧贴竖直堆叠，无通道
// 门控制：mode 0=全开 1=全关 2=默认（顶开底关）
static void door_set(int L, int c, int mode)
{
    int rx0 = roomX0(L, c), ry0 = roomY0(L);
    int ry1 = ry0 + 13;
    short v_top = (mode == 1) ? 10 : 0;
    short v_bot = (mode == 0) ? 0 : 10;
    s[rx0 + 9][ry0] = v_top;  s[rx0 + 10][ry0] = v_top;
    s[rx0 + 9][ry1] = v_bot;  s[rx0 + 10][ry1] = v_bot;
    mark_dirty_col(rx0 + 9);   // 门状态变化随存档保存（关卡进度持久）
    mark_dirty_col(rx0 + 10);
}
// 清除房间内所有怪物（NPC除外）
static void clear_room_monsters(int L, int c)
{
    int rx0 = roomX0(L, c), ry0 = roomY0(L);
    for(int i = 0; i < 500; i++)
        if(shiti[i].type > 0 && shiti[i].AI != 2 &&
                (shiti[i].type < 14 || shiti[i].type > 22) &&
                shiti[i].gx > rx0 && shiti[i].gx < rx0 + 19 &&
                shiti[i].gy > ry0 && shiti[i].gy < ry0 + 13)
            st_kill(i);
}
// 关卡布局计算（确定性：每次进关布局一致）
static void level_layout(int n)
{
    levelStartX = 9000 + (n - 1) * 95;
    levelStartY = 500;
    // 布局：层0起始房(1) + 中间层(房间数1/2/3/4递增) + 末层最终房(1)
    int midLayers = 2 + min(2, n / 5);            // 中间层数 2-4
    layCount = midLayers + 2;
    int maxC = 1;
    for(int L = 0; L < layCount; L++)
    {
        if(L == 0 || L == layCount - 1)
            layRooms[L] = 1;
        else
            layRooms[L] = min(4, L);
        maxC = max(maxC, layRooms[L]);
    }
    for(int L = 0; L < layCount; L++)
        layOff[L] = (maxC - layRooms[L]) / 2;
    bossCol = layOff[layCount - 1];
}
// 纯地形建造（墙/火把/门/NPC/起始补给箱）：新建世界时对全部关卡调用一次并随存档保存
static void build_level_terrain(int n)
{
    level_layout(n);
    for(int L = 0; L < layCount; L++)
        for(int c = layOff[L]; c < layOff[L] + layRooms[L]; c++)
        {
            int rx0 = roomX0(L, c), ry0 = roomY0(L);
            int rx1 = rx0 + 19, ry1 = ry0 + 13;
            // 左右墙
            for(int j = ry0; j <= ry1; j++) { s[rx0][j] = 10; s[rx1][j] = 10; }
            // 顶底墙（中间两格留作门）
            for(int i = rx0; i <= rx1; i++)
                if(i != rx0 + 9 && i != rx0 + 10)
                { s[i][ry0] = 10; s[i][ry1] = 10; }
            // 清空内部
            for(int i = rx0 + 1; i < rx1; i++)
                for(int j = ry0 + 1; j < ry1; j++)
                    s[i][j] = 0;
            // 火把装饰（四角内侧）
            s[rx0 + 2][ry0 + 2] = 16;  s[rx1 - 2][ry0 + 2] = 16;
            s[rx0 + 2][ry1 - 2] = 16;  s[rx1 - 2][ry1 - 2] = 16;
            // 门默认态：顶门开（可进入）、底门关（清完才开）
            door_set(L, c, 2);
        }
    // 外围墙（整个关卡外圈：最右列/最底行与房间墙重合，紧贴无通道）
    int maxC = 1;
    for(int L = 0; L < layCount; L++) maxC = max(maxC, layRooms[L]);
    int W0 = levelStartX, W1 = levelStartX + maxC * 20;
    int H1 = levelStartY + layCount * 14;
    for(int j = levelStartY; j <= H1; j++) { s[W0][j] = 10; s[W1][j] = 10; }
    for(int i = W0; i <= W1; i++) { s[i][levelStartY] = 10; s[i][H1] = 10; }
    // 起始房（层0）：安全，门全开
    int srx0 = roomX0(0, layOff[0]), sry0 = roomY0(0);
    door_set(0, layOff[0], 0);
    roomState[0][layOff[0]] = 2;
    // 起始房放初始补给箱（新档空背包需要武器）
    {
        int cx = srx0 + 14, cy = sry0 + 8;
        s[cx][cy] = 40;
        int idx = alloc_box(cx, cy);
        add_box_item(idx, 17, 1);        // 木剑
        add_box_item(idx, 16, 8);        // 火把
        add_box_item(idx, 44, 16);       // 木箭
        add_box_item(idx, 89, 3);        // 猪肉
        add_box_item(idx, 101, 5);       // 少量金币
        add_box_item(idx, 113, 1);       // 护甲药水
    }
    // 中间层随机房间放地牢NPC（NPC房间为安全房：不刷怪，门全开）
    for(int L = 1; L < layCount - 1; L++)
        if(rand() % 2 == 0)
        {
            int c = layOff[L] + rand() % layRooms[L];
            int npcType = 14 + rand() % 9;
            make_npc(npcType, roomX0(L, c) + 3 + rand() % 13, roomY0(L) + 2 + rand() % 9);
            roomState[L][c] = 2;          // 标记为已清空：不激活战斗
            door_set(L, c, 0);            // 门全开
        }
}
void enter_level(int n)
{
    level_layout(n);
    int maxC = 1;
    for(int L = 0; L < layCount; L++) maxC = max(maxC, layRooms[L]);
    // 关卡地形是否已存在（新建世界时已预生成并随存档保存）
    // 修复：原只探测单格，若该格被蠕虫类吃墙(直接置0)会误判为"未建造"而整关重建
    // （房间状态重置、补给箱重复生成）。外圈墙角为不可破坏基岩，多点探测更可靠
    bool built = s[levelStartX][levelStartY] == 10 &&
                 s[levelStartX][levelStartY + layCount * 14] == 10 &&
                 s[levelStartX + 1][levelStartY + 1] == 10;
    if(!built)
    {
        // 旧存档兼容：首次进入时现场建造
        build_level_terrain(n);
    }
    // 房间进度由门状态持久恢复（底门开=已清空），不重置已获进度
    for(int L = 0; L < layCount; L++)
        for(int c = layOff[L]; c < layOff[L] + layRooms[L]; c++)
        {
            if(L == 0 && c == layOff[0]) continue;
            int rx0 = roomX0(L, c), ry0 = roomY0(L);
            roomState[L][c] = (s[rx0 + 9][ry0 + 13] == 0) ? 2 : 0;
        }
    // 清除关卡区域战斗怪物（保留NPC与随从）
    for(int i = 0; i < 500; i++)
        if(shiti[i].type > 0 && shiti[i].gx > levelStartX - 5 &&
                shiti[i].gx < levelStartX + maxC * 20 + 5 &&
                shiti[i].gy > levelStartY - 5 &&
                shiti[i].gy < levelStartY + layCount * 14 + 5)
        {
            if(shiti[i].type >= 14 && shiti[i].type <= 22) continue;   // NPC保留
            if(shiti[i].AI == 5) continue;                              // 随从保留
            st_kill(i);
        }
    // 起始房（层0）：安全，门全开
    int srx0 = roomX0(0, layOff[0]), sry0 = roomY0(0);
    door_set(0, layOff[0], 0);
    roomState[0][layOff[0]] = 2;
    inDungeon = true;
    currentLevel = n;
    curLayer = 0;
    curCol = layOff[0];
    currentRoom = 0;
    x = srx0 + 10;
    y = sry0 + 6;
    // 保险：出生点若为实体方块则清除
    if(blocktouch[s[int(x + 0.5)][int(y + 0.5)]] >= 1)
        s[int(x + 0.5)][int(y + 0.5)] = 0;
    spx = 0; spy = 0;
    // 当前房间范围（起始房）
    lvl_x0 = srx0; lvl_y0 = sry0;
    lvl_x1 = srx0 + 19; lvl_y1 = sry0 + 13;
    roomActive = false;
    roomWavesDone = 0; roomWavesTotal = 0; roomWaveCD = 0; roomWavesLeft = 0;
    mark_all_dirty();
    memset(painting, 114, sizeof(painting));
    hujia_update();               // 刷新护甲值（存档加载/死亡重进后wear恢复）
    setlight();
}
void spawn_level_wave()
{
    int rx0 = roomX0(curLayer, curCol), ry0 = roomY0(curLayer);
    bool isFinal = (curLayer == layCount - 1);
    // 第5/10关最终房为Boss房（单波boss）
    if(isFinal && (currentLevel == 5 || currentLevel == 10))
    {
        make_st(8, 1500 + currentLevel * 250, 15 + currentLevel, rx0 + 9, ry0 + 6, 10);
        return;
    }
    int cnt = isFinal ? (3 + currentLevel / 3) : (2 + currentLevel / 4 + rand() % 2);
    if(cnt > 6) cnt = 6;
    for(int k = 0; k < cnt; k++)
    {
        int ex = rx0 + 2 + rand() % 16;
        int ey = ry0 + 2 + rand() % 9;
        // 避免生成在实体方块内 + 防止怪物重叠（与已有怪物保持距离）
        for(int tries = 0; tries < 12; tries++)
        {
            bool ok = (blocktouch[s[ex][ey]] < 1);
            if(ok)
                for(int i = 0; i < 500 && ok; i++)
                    if(shiti[i].type > 0 && shiti[i].AI != 2 &&
                            abs(shiti[i].gx - ex) < 2 && abs(shiti[i].gy - ey) < 2)
                        ok = false;
            if(ok) break;
            ex = rx0 + 2 + rand() % 16;
            ey = ry0 + 2 + rand() % 9;
        }
        int typ = (rand() % 2 ? 1 : 3);   // 僵尸/骷髅（CR怪物已删除）
        int hp = 120 + currentLevel * 25;   // 血量匹配200血体系
        make_st(typ, hp, 10 + currentLevel * 2, ex, ey);
    }
}
int roomInProgress() { return curLayer; }
int roomCountForLevel(int n) { return 4 + min(2, n / 5); }
// 关卡宝箱：走近自动开启（元气骑士风格，绕开右键交互链路）
static void chest_auto_open()
{
    int px = To_int(x), py = To_int(y);
    if(px <= 1 || py <= 1 || px >= WORLD_WIDTH - 2 || py >= WORLD_HEIGHT - 2)
        return;
    static const int dxy[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for(int k = 0; k < 5; k++)
    {
        int cx = px + dxy[k][0], cy = py + dxy[k][1];
        if(s[cx][cy] == 40)
        {
            int del = findbox(cx, cy);
            int got = 0;
            if(del != -1)
            {
                box[del].bx = 5005, box[del].by = 0;
                for(int i = 0; i <= 24; i++)
                {
                    int item = box[del].things[i] % 1000;
                    int cnt = box[del].things[i] / 1000;
                    box[del].things[i] = 0;
                    if(item == 0 || cnt <= 0)
                        continue;
                    for(int j = 1; j <= cnt; j++)
                    {
                        int uu = getbeibao(item);
                        if(uu != 0)
                            beibao[uu][1]++, beibao[uu][0] = item, got++;
                    }
                }
            }
            s[cx][cy] = 0;
            mark_dirty_col(cx);
            Setpos(1, 31);
            Color(0);
            if(got > 0)
                cout << " 打开宝箱，获得 " << got << " 件物品！                ";
            else
                cout << " 打开了空宝箱                                          ";
        }
    }
}
void update_level()
{
    if(!inDungeon) return;
    // 关卡宝箱：走近自动开启
    chest_auto_open();
    // 计算玩家所在房间（内部矩形）
    int pi = To_int(x), pj = To_int(y);
    int pL = -1, pc = -1;
    if(pj > levelStartY && pj < levelStartY + layCount * 14 - 1 &&
            pi > levelStartX && pi < levelStartX + 8 * 20)
    {
        int L = (pj - levelStartY) / 14;
        int cc = (pi - levelStartX) / 20;
        if(L >= 0 && L < layCount && cc >= layOff[L] && cc < layOff[L] + layRooms[L])
        {
            int rx0 = roomX0(L, cc), ry0 = roomY0(L);
            if(pi > rx0 && pi < rx0 + 19 && pj > ry0 && pj < ry0 + 13)
            { pL = L; pc = cc; }
        }
    }
    // 战斗中不能被逼出房间（门为实体锁死），玩家必须杀完全部怪才能离开关卡
    // 踏入未清房间：激活战斗
    if(!roomActive && pL > 0 && roomState[pL][pc] == 0)
    {
        curLayer = pL; curCol = pc;
        currentRoom = curLayer;
        roomActive = true;
        roomState[pL][pc] = 1;
        door_set(pL, pc, 1);                 // 关门锁战
        bool isFinal = (pL == layCount - 1);
        if(isFinal && (currentLevel == 5 || currentLevel == 10))
            roomWavesTotal = 1;
        else if(isFinal)
            roomWavesTotal = 2;
        else
            roomWavesTotal = 2 + rand() % 2; // 2-3波
        roomWavesDone = 0;
        roomWaveCD = 0;
        lvl_x0 = roomX0(pL, pc); lvl_y0 = roomY0(pL);
        lvl_x1 = lvl_x0 + 19; lvl_y1 = lvl_y0 + 13;
        spawn_level_wave();
        roomWavesDone = 1;
        Setpos(1, 31);
        Color(0);
        cout << " 怪物来袭！清空房间才能开门                             ";
    }
    if(roomActive)
    {
        // 怪物间碰撞分离：距离过近时相互推开，防止重叠看不见
        for(int i = 0; i < 500; i++)
            if(shiti[i].type > 0 && shiti[i].AI != 2 &&
                    (shiti[i].type < 14 || shiti[i].type > 22) &&
                    shiti[i].gx > lvl_x0 && shiti[i].gx < lvl_x1 &&
                    shiti[i].gy > lvl_y0 && shiti[i].gy < lvl_y1)
                for(int j = i + 1; j < 500; j++)
                    if(shiti[j].type > 0 && shiti[j].AI != 2 &&
                            (shiti[j].type < 14 || shiti[j].type > 22) &&
                            shiti[j].gx > lvl_x0 && shiti[j].gx < lvl_x1 &&
                            shiti[j].gy > lvl_y0 && shiti[j].gy < lvl_y1)
                    {
                        double dx = shiti[j].gx - shiti[i].gx;
                        double dy = shiti[j].gy - shiti[i].gy;
                        double d2 = dx * dx + dy * dy;
                        if(d2 < 0.8 && d2 > 0.0001)
                        {
                            double d = sqrt(d2);
                            double push = (0.9 - d) * 0.5;
                            dx /= d; dy /= d;
                            shiti[i].gx -= dx * push;
                            shiti[i].gy -= dy * push;
                            shiti[j].gx += dx * push;
                            shiti[j].gy += dy * push;
                        }
                        else if(d2 <= 0.0001)
                        {
                            // 完全重合：随机方向弹开
                            shiti[j].gx += 0.5;
                            shiti[j].gy += (rand() % 2 ? 0.4 : -0.4);
                        }
                    }
        // 限制怪物不飞出房间；NPC(14-22)排除
        for(int i = 0; i < 500; i++)
            if(shiti[i].type > 0 && shiti[i].AI != 2 &&
                    (shiti[i].type < 14 || shiti[i].type > 22))
            {
                if(shiti[i].gx < lvl_x0 + 1) shiti[i].gx = lvl_x0 + 1;
                if(shiti[i].gx > lvl_x1 - 1) shiti[i].gx = lvl_x1 - 1;
                if(shiti[i].gy < lvl_y0 + 1) shiti[i].gy = lvl_y0 + 1;
                if(shiti[i].gy > lvl_y1 - 1) shiti[i].gy = lvl_y1 - 1;
            }
        int alive = 0;
        for(int i = 0; i < 500; i++)
            if(shiti[i].type > 0 && shiti[i].AI != 2 &&
                    (shiti[i].type < 14 || shiti[i].type > 22) &&
                    shiti[i].gx > lvl_x0 && shiti[i].gx < lvl_x1 &&
                    shiti[i].gy > lvl_y0 && shiti[i].gy < lvl_y1)
                alive++;
        roomWavesLeft = alive;
        if(alive == 0)
        {
            if(roomWaveCD > 0)
            {
                roomWaveCD--;
                if(roomWaveCD == 0 && roomWavesDone < roomWavesTotal)
                {
                    spawn_level_wave();
                    roomWavesDone++;
                    alive = 1;   // 刚刷出新一波：本帧不算清空，防止门提前开
                }
            }
            else
            {
                roomWaveCD = 60; // 波间停顿
            }
        }
        if(alive == 0 && roomWavesDone >= roomWavesTotal)
        {
            // 房间清空：开门 + 奖励
            roomState[curLayer][curCol] = 2;
            roomActive = false;
            door_set(curLayer, curCol, 0);
            int rx0 = roomX0(curLayer, curCol), ry0 = roomY0(curLayer);
            int cx = rx0 + 14, cy = ry0 + 10;
            bool isFinal = (curLayer == layCount - 1);
            if(isFinal)
            {
                // 最终房：必出大宝箱（金币+装备）
                if(s[cx][cy] != 40)
                {
                    s[cx][cy] = 40;
                    int idx = alloc_box(cx, cy);
                    fill_level_chest(idx, currentLevel, 1);
                }
                Setpos(1, 31);
                Color(0);
                cout << " 房间清空！宝箱已出现，从底部出口离开本关                ";
            }
            else if(rand() % 100 < 35)
            {
                // 普通房：35%概率小宝箱
                if(s[cx][cy] != 40)
                {
                    s[cx][cy] = 40;
                    int idx = alloc_box(cx, cy);
                    fill_level_chest(idx, currentLevel, 0);
                }
                Setpos(1, 31);
                Color(0);
                cout << " 房间清空！获得一个宝箱                                 ";
            }
        }
        return;
    }
    // 通关判定：最终房已清且玩家越过其底部走廊
    int fry1 = roomY0(layCount - 1) + 13;
    if(roomState[layCount - 1][bossCol] == 2 && y > fry1 + 2)
    {
        // 通关当前大关：解锁下一关，天赋三选一，满状态进入下一关
        levelUnlocked = max(levelUnlocked, min(currentLevel + 1, 10));
        show_talent_pick();
        int next = (currentLevel >= 10) ? 1 : currentLevel + 1;
        Setpos(1, 31);
        Color(0);
        cout << " 通关！进入第 " << next << " 关                              ";
        enter_level(next);   // 下一关继承上一关的血量/护甲/蓝量
    }
}
void show_talent_pick()
{
    if(talentCount >= maxTalentSlots) return;
    // 三选一（排除未实现的T_SPEEDARMOR=16）
    int opts[3];
    int used = 0;
    while(used < 3)
    {
        int t = rand() % 20;
        if(t == T_SPEEDARMOR) continue;
        int dup = 0;
        for(int i = 0; i < used; i++) if(opts[i] == t) dup = 1;
        if(!dup) opts[used++] = t;
    }
    Setpos(1, 31);
    Color(0);
    cout << "=== 天赋三选一（按1/2/3选择）===";
    for(int i = 0; i < 3; i++)
    {
        Setpos(1, 33 + i);
        cout << i + 1 << ": " << talent_names[opts[i]] << "                 ";
    }
    // 等待选择
    int pick = 0;
    clock_t st = clock();
    while(pick == 0)
    {
        if(GetAsyncKeyState('1') & 0x8000) pick = 1;
        if(GetAsyncKeyState('2') & 0x8000) pick = 2;
        if(GetAsyncKeyState('3') & 0x8000) pick = 3;
        if(clock() - st > 12000) pick = 1; // 超时默认选1
        Sleep(1);
    }
    grant_talent(opts[pick - 1]);
    Setpos(1, 31);
    cout << "您获得了天赋: " << talent_names[opts[pick - 1]] << "                    ";
    Sleep(1200);
}
void fill_level_chest(int idx, int lvl, int big)
{
    if(big)
    {
        // 大宝箱（最终房）：金币 + 必出武器/道具 + 补给
        add_box_item(idx, 101, 15 + rand() % 16);          // 金币15-30
        int r = rand() % 100;
        if(r < 45)                                          // 武器（含枪械）
        {
            int item;
            int roll = rand() % 100;
            if(roll < 25) item = 95;                        // 手枪
            else if(roll < 45) item = 97;                   // 霰弹枪
            else if(roll < 60) item = 98;                   // 狙击枪
            else if(roll < 75) { if(lvl >= 6) item = 111; else item = 41; } // 弓
            else { if(lvl >= 6) item = 110; else if(lvl >= 3) item = 49; else item = 47; } // 剑
            add_box_item(idx, item, 1);
        }
        else if(r < 75)
            add_box_item(idx, 113, 1 + rand() % 2);         // 护甲药水
        else
            add_box_item(idx, 24, 2);                       // 回血药
        if(rand() % 2 == 0) add_box_item(idx, 94, 1);       // 能量药水
        if(rand() % 2 == 0) add_box_item(idx, 100, 1);      // 大回血药
    }
    else
    {
        // 小宝箱（普通房概率出现）：金币 + 随机补给（含枪械）
        add_box_item(idx, 101, 3 + rand() % 8);             // 金币3-10
        int r = rand() % 100;
        if(r < 18) add_box_item(idx, 95, 1);                // 手枪
        else if(r < 28) add_box_item(idx, 97, 1);           // 霰弹枪
        else if(r < 38) add_box_item(idx, 113, 1);          // 护甲药水
        else if(r < 58) add_box_item(idx, 44, 8 + rand() % 8); // 木箭
        else if(r < 76) add_box_item(idx, 24, 1 + rand() % 2); // 回血药
        else if(r < 92) add_box_item(idx, 94, 1);           // 能量药水
        // 暗影鳞片：高级关卡有概率掉落
        if(lvl >= 3 && rand() % 4 == 0)
            add_box_item(idx, 105, 1 + rand() % 3);
    }
}
// ---- NPC 生成（地牢特殊房间）----
void spawn_dungeon_npc(int type, int tx, int ty)
{
    make_npc(type, tx, ty);
}
// ---- 扭蛋机抽奖 ----
void interact_gacha()
{
    // 背包无空位则无法抽奖（避免扣币无物）
    bool hasEmptyG = false;
    for(int i = 1; i <= 100; i++)
        if(beibao[i][1] <= 0) { hasEmptyG = true; break; }
    if(!hasEmptyG)
    {
        Setpos(1, 31);
        Color(0);
        cout << " 背包已满，无法使用扭蛋机！                          ";
        return;
    }
    if(spend_coins(20))
    {
        int roll = rand() % 100;
        int item = 0, cnt = 1;
        if(roll < 30) item = 113, cnt = 1 + rand() % 2;     // 护甲药水
        else if(roll < 45) item = 94;                          // 能量药水
        else if(roll < 55) item = 100;                         // 大回血药
        else if(roll < 65) item = 95;                          // 手枪
        else if(roll < 72) item = 96;                          // 冲锋
        else if(roll < 79) item = 97;                          // 霰弹
        else if(roll < 85) item = 98;                          // 狙击
        else if(roll < 92) item = 101, cnt = 30 + rand() % 30; // 金币
        else item = 24, cnt = 2;                               // 回血药
        for(int i = 0; i < cnt; i++)
        {
            int U = getbeibao(item);
            if(U != 0) beibao[U][1]++, beibao[U][0] = item;
        }
        Setpos(1, 31);
        Color(0);
        cout << " 扭蛋机抽奖：获得 " << cnt << " " << test[item] << " (消耗20金币)      ";
    }
    else
    {
        Setpos(1, 31);
        Color(0);
        cout << " 金币不足（需要20金币）！                              ";
    }
}
// ---- NPC 交互 ----
void interact_npc(int idx)
{
    int type = shiti[idx].type;
    if(type == 14)  // 商人：买卖
    {
        Setpos(1, 31);
        Color(0);
        int price = hasTalent(T_HALFPRICE) ? 5 : 10;
        int price2 = hasTalent(T_HALFPRICE) ? 8 : 15;
        int price3 = hasTalent(T_HALFPRICE) ? 10 : 20;
        cout << " 商人：购买[1]能量药水(" << price << "金币) [2]护甲药水(" << price2
             << "金币) [3]大回血药(" << price3 << "金币) [4]离开";   // 修复：菜单价格与实际扣费一致
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                if(getbeibao(94) == 0) { Setpos(1,32);cout<<" 背包已满！"; break; }
                if(spend_coins(price)) { int U=getbeibao(94); if(U)beibao[U][1]++,beibao[U][0]=94; Setpos(1,32);cout<<" 购买成功(能量药水)"; }
                else Setpos(1,32),cout<<" 金币不足";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000)
            {
                if(getbeibao(113) == 0) { Setpos(1,32);cout<<" 背包已满！"; break; }
                int p2 = hasTalent(T_HALFPRICE) ? 8 : 15;
                if(spend_coins(p2)) { int U=getbeibao(113); if(U)beibao[U][1]++,beibao[U][0]=113; Setpos(1,32);cout<<" 购买成功(护甲药水)"; }
                else Setpos(1,32),cout<<" 金币不足";
                break;
            }
            if(GetAsyncKeyState('3') & 0x8000)
            {
                if(getbeibao(100) == 0) { Setpos(1,32);cout<<" 背包已满！"; break; }
                int p3 = hasTalent(T_HALFPRICE) ? 10 : 20;
                if(spend_coins(p3)) { int U=getbeibao(100); if(U)beibao[U][1]++,beibao[U][0]=100; Setpos(1,32);cout<<" 购买成功(大回血药)"; }
                else Setpos(1,32),cout<<" 金币不足";
                break;
            }
            if(GetAsyncKeyState('4') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 15) // 工匠：强化
    {
        Setpos(1, 31); Color(0);
        cout << " 工匠：强化武器需要30金币，是否强化？[1]是 [2]否";
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                int p = hasTalent(T_HALFPRICE) ? 15 : 30;
                if(spend_coins(p))
                {
                    Setpos(1, 32); cout << " 武器已强化！攻击+2                                    ";
                }
                else Setpos(1, 32), cout << " 金币不足                                        ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 16) // 占星师：买额外天赋
    {
        if(talentCount >= maxTalentSlots)
        {
            Setpos(1, 31); Color(0); cout << " 天赋已满                                    ";
            return;
        }
        Setpos(1, 31); Color(0);
        int price = 100;
        cout << " 占星师：花费100金币随机获得1个天赋（最多额外3个）[1]是 [2]否";
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                if(spend_coins(price))
                {
                    int t;
                    do { t = rand() % 20; } while(t == T_SPEEDARMOR);   // 修复：疾步生甲未实现，与三选一逻辑保持一致
                    grant_talent(t);
                    Setpos(1, 32); cout << " 获得天赋：" << talent_names[t] << "                  ";
                }
                else Setpos(1, 32), cout << " 金币不足                                     ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 18) // 牧师：治疗
    {
        Setpos(1, 31); Color(0);
        int price = 15;
        cout << " 牧师：花费15金币治疗 x 点击1确认";
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                if(spend_coins(price))
                {
                    life = life + 8;
                    Setpos(1, 32); cout << " 已治疗！+8生命                                    ";
                }
                else Setpos(1, 32), cout << " 金币不足                                          ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 20) // 被关押NPC：用金币解救
    {
        Setpos(1, 31); Color(0);
        cout << " 被关押的NPC：花50金币解救，获得意外奖励！[1]是 [2]否";
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                int item = 95 + rand() % 4;
                if(getbeibao(item) == 0)
                {
                    Setpos(1, 32); cout << " 背包已满，无法领取奖励！                          ";
                    break;
                }
                if(spend_coins(50))
                {
                    int U = getbeibao(item);
                    if(U) beibao[U][1]++, beibao[U][0] = item;
                    Setpos(1, 32); cout << " 获得一把强力枪械！                                  ";
                }
                else Setpos(1, 32), cout << " 金币不足                                          ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 21) // 奸商：重置最近天赋
    {
        if(talentCount <= 0)
        {
            Setpos(1, 31); Color(0); cout << " 没有天赋可重置                                    ";
            return;
        }
        Setpos(1, 31); Color(0);
        cout << " 奸商：花80金币重置最近天赋[1]是 [2]否";
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                if(spend_coins(80))
                {
                    talentCount--;
                    Setpos(1, 32); cout << " 已重置最近天赋！                                    ";
                }
                else Setpos(1, 32), cout << " 金币不足                                          ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 17) // 雇佣兵：花钱雇佣为战斗随从
    {
        if(!inDungeon)
        {
            Setpos(1, 31); Color(0); cout << " 雇佣兵只在地牢中提供服务                                    ";
            Sleep(1200);
            Setpos(1, 31); cout << "                                                              ";
            return;
        }
        // 已有随从则提示
        for(int i = 0; i < 500; i++)
            if(shiti[i].type == 17 && shiti[i].nbt == 1)
            {
                Setpos(1, 31); Color(0); cout << " 你已经有一名雇佣兵随从了                                ";
                Sleep(1200);
                Setpos(1, 31); cout << "                                                              ";
                return;
            }
        Setpos(1, 31); Color(0);
        cout << " 雇佣兵：花40金币雇佣我为你战斗(本关有效)[1]是 [2]否";
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                if(spend_coins(40))
                {
                    shiti[idx].nbt = 1;   // 标记为已雇佣随从
                    Setpos(1, 32); cout << " 雇佣成功！随从会跟随你并攻击怪物                        ";
                }
                else Setpos(1, 32), cout << " 金币不足                                          ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 19) // 地牢商人：地牢内军火商
    {
        Setpos(1, 31); Color(0);
        cout << " 地牢商人：[1]随机枪械(35) [2]护甲药水(10) [3]能量药水(12) [4]离开";
        clock_t st = clock();
        while(clock() - st < 10000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                int item = 95 + rand() % 4;
                if(getbeibao(item) == 0)
                {
                    Setpos(1, 32); cout << " 背包已满，无法购买！                            ";
                    break;
                }
                int p = hasTalent(T_HALFPRICE) ? 18 : 35;
                if(spend_coins(p))
                {
                    int U = getbeibao(item);
                    if(U) beibao[U][1]++, beibao[U][0] = item;
                    Setpos(1, 32); cout << " 购买成功：" << test[item] << "                          ";
                }
                else Setpos(1, 32), cout << " 金币不足                                          ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000)
            {
                if(getbeibao(113) == 0)
                {
                    Setpos(1, 32); cout << " 背包已满，无法购买！                            ";
                    break;
                }
                int p = hasTalent(T_HALFPRICE) ? 5 : 10;
                if(spend_coins(p)) { int U=getbeibao(113); if(U)beibao[U][1]++,beibao[U][0]=113; Setpos(1,32);cout<<" 购买成功(护甲药水)"; }
                else Setpos(1,32),cout<<" 金币不足";
                break;
            }
            if(GetAsyncKeyState('3') & 0x8000)
            {
                if(getbeibao(94) == 0)
                {
                    Setpos(1, 32); cout << " 背包已满，无法购买！                            ";
                    break;
                }
                int p = hasTalent(T_HALFPRICE) ? 6 : 12;
                if(spend_coins(p)) { int U=getbeibao(94); if(U)beibao[U][1]++,beibao[U][0]=94; Setpos(1,32);cout<<" 购买成功(能量药水)"; }
                else Setpos(1,32),cout<<" 金币不足";
                break;
            }
            if(GetAsyncKeyState('4') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else if(type == 22) // 小偷猫：免费偷窃(有冷却)
    {
        clock_t now = clock();
        if(catStealCD > now)
        {
            int left = int((catStealCD - now) / CLOCKS_PER_SEC) + 1;
            Setpos(1, 31); Color(0);
            cout << " 小偷猫正在寻找下一个目标...(" << left << "秒)               ";
            Sleep(1200);
            Setpos(1, 31); cout << "                                                              ";
            return;
        }
        Setpos(1, 31); Color(0);
        cout << " 小偷猫：让我帮你顺点东西？[1]是 [2]否";
        clock_t st = clock();
        while(clock() - st < 8000)
        {
            if(GetAsyncKeyState('1') & 0x8000)
            {
                // 背包无空位则无法偷窃
                bool hasEmpty = false;
                for(int i = 1; i <= 100; i++)
                    if(beibao[i][1] <= 0) { hasEmpty = true; break; }
                if(!hasEmpty)
                {
                    Setpos(1, 32); cout << " 背包已满，小偷猫无处下手！                        ";
                    break;
                }
                catStealCD = clock() + 30 * CLOCKS_PER_SEC;   // 30秒冷却
                if(rand() % 100 < 65)
                {
                    int roll = rand() % 100;
                    if(roll < 35) { add_coins(5 + rand() % 11); Setpos(1, 32); cout << " 偷到金币！                                            "; }
                    else if(roll < 60) { int U=getbeibao(113); if(U)beibao[U][1]++,beibao[U][0]=113; Setpos(1,32);cout<<" 偷到护甲药水！"; }
                    else if(roll < 80) { int U=getbeibao(94); if(U)beibao[U][1]++,beibao[U][0]=94; Setpos(1,32);cout<<" 偷到能量药水！"; }
                    else if(roll < 95) { int U=getbeibao(24); if(U)beibao[U][1]+=1+rand()%2,beibao[U][0]=24; Setpos(1,32);cout<<" 偷到回血药水！"; }
                    else { int item = 95 + rand() % 4; int U=getbeibao(item); if(U)beibao[U][1]++,beibao[U][0]=item; Setpos(1,32);cout<<" 偷到一把枪械！"; }
                }
                else Setpos(1, 32), cout << " 被发现了，什么都没偷到...                            ";
                break;
            }
            if(GetAsyncKeyState('2') & 0x8000) break;
            Sleep(1);
        }
        Setpos(1, 31); cout << "                                                              ";
        Setpos(1, 32); cout << "                                                              ";
    }
    else
    {
        // 其余NPC：简单提示
        Setpos(1, 31); Color(0);
        cout << " 这位NPC向你点头致意                                    ";
        Sleep(1200);
        Setpos(1, 31); cout << "                                                              ";
    }
}
void game_loop()
{
    fell = y;
    // 初始化窗口大小以适应渲染尺寸
    SetWindowSize(printx + 5, printy + 5);
    jx = x, jy = y;
    if(jx < printx / 2)
        jx = printx / 2;
    if(jy < printy / 2)
        jy = printy / 2;
    if(jx > WORLD_WIDTH - printx / 2 - 1)
        jx = WORLD_WIDTH - printx / 2 - 1;
    if(jy > WORLD_HEIGHT - printy / 2 - 1)
        jy = WORLD_HEIGHT - printy / 2 - 1;
    Clear();
    setlight();
    int bow = 0;
    memset(painting, 1145, sizeof(painting));
    // HUD缓存全部置-1：强制首帧重绘（修复开局看不见物品栏/HUD）
    l_life = -1;
    l_hujia = -1;
    l_lvlinfo = -1;
    l_energy = -1;
    l_coin = -1;
    l_cho = -1;
    l_oxy = -1;
    for(int i = 1; i <= 25; i++)
        l_beibao[i][0] = -1, l_beibao[i][1] = -1;
    run = 1;
    g_world_active = true;
    hujia_update();
    slow_update = 400;
    while(1)
    {
        // QPC门控：clock()粒度~15.6ms导致60帧档实际只有30~45帧
        {
            double nowms = qpc_ms();
            if(g_last_frame_ms < 0)
                g_last_frame_ms = nowms;
            if(setfps == 0 || nowms - g_last_frame_ms >= 15.0 * setfps)
            {
                g_last_frame_ms = nowms;
                Clocknum = clock();
                GetIn += (GetIn >= 1 ? 0 : 1);
                keybd_event(VK_MENU, 0, 0, 0);               // 按下
                keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0); // 释放
                UpdateFPS();
                HideCursor();
            for(int S = 1; S <= setfps; S++)
            {
                slow_update++;
                if(slow_update > 60)
                    slow_update = 0, setlight();
                bosshp = 0;
                tick++;
                if(tick >= 20)
                    tick = 0, try_make_st();
                if(tick % 10 == 0)
                    boom_loop();
                atime();
                update_level();   // 地牢关卡逻辑（未进入地牢时立即返回）
                // 魔光护符(112)：能量自然恢复（beta1.8同步）
                for(int mi = 1; mi <= 30; mi++)
                    if(beibao[mi][0] == 112 && beibao[mi][1] > 0)
                    {
                        energy = min(energyMax, energy + 0.15);
                        break;
                    }
                mx = 0;
                if(wudi > 0)
                    wudi--;
                SwitchMode();
                if(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
                {
                    if(anx == 0)
                    {
                        anx = 1, SET();
                        if(run == 0)
                        {
                            g_world_active = false;
                            return;
                        }
                        setlight();
                        memset(painting, 114, sizeof(painting));
                        Print();
                    }
                    anx = 1;
                }
                else
                    anx = 0;
                if(GetAsyncKeyState('O') & 0x8000 && GetAsyncKeyState('T') & 0x8000 &&
                        GetIn == 1)
                {
                    memset(s, 0, sizeof(s));
                    for(int AAAA = 0; AAAA < 500; AAAA++)
                    {
                        shiti[AAAA].CD = 0, shiti[AAAA].HP = 0, shiti[AAAA].spgx = 0,
                                    shiti[AAAA].spgy = 0, shiti[AAAA].gx = 0, shiti[AAAA].gy = 0,
                                                shiti[AAAA].hurt = 0, shiti[AAAA].type = 0, shiti[AAAA].jp = 0;
                    }
                    clear_entity_hash();
                    for(int AAAA = 1; AAAA <= 4; AAAA++)
                    {
                        wear[AAAA] = 0;
                    }
                    cls();
                    shuaxin();
                    MAP();
                    mark_all_dirty();
                    cls();
                    now_time = 0;
                    night = 0;
                    Clear();
                    memset(beibao, 0, sizeof(beibao));
                    back_x = 10, back_y = 145;
                    life = lifeMax;
                    hujiazhi = armorMax;
                    Start();
                }
                if((GetAsyncKeyState('E') & 0x8000) && GetIn == 1)
                {
                    while(GetAsyncKeyState('E') & 0x8000)
                        Sleep(1);   // 修复：原空转烧满核，等待按键期间冻结游戏无收益
                    craft_1();
                    setlight();
                    hujia_update();
                    memset(painting, 114, sizeof(painting));
                }
                else
                    anx = 0;
                if((GetAsyncKeyState('M')) && GetIn == 1)
                {
                    while(GetAsyncKeyState('M'))
                        Sleep(1);   // 修复：原空转烧满核
                    openmap();
                    setlight();
                }
                else
                    anx = 0;
                if(GetAsyncKeyState('I') & 0x8000)
                {
                    while(GetAsyncKeyState('I') & 0x8000)
                        Sleep(1);
                    gamemode = 1 - gamemode;
                }
                if(GetAsyncKeyState('N') & 0x8000 && GetIn == 1)
                    shuaxin();
                if(GetAsyncKeyState('U') & 0x8000 && GetIn == 1)
                    save_world(zzz, false);
                if(((GetAsyncKeyState('A') & 0x8000) ||
                        (GetAsyncKeyState(VK_LEFT) & 0x8000)) &&
                        GetIn == 1)
                {
                    if(spx > -1.5)
                        spx = -1.5;
                    spx -= 0.35, mx--;
                }
                if(((GetAsyncKeyState('D') & 0x8000) ||
                        (GetAsyncKeyState(VK_RIGHT) & 0x8000)) &&
                        GetIn == 1)
                {
                    if(spx < 1.5)
                        spx = 1.5;
                    spx += 0.35, mx++;
                }
                spx *= 0.985;
                my = 0;
                if(mx == 0)
                {
                    if(sword<40&&wudi<20)
                        spx = 0;
                    else spx *= 0.97;
                }
                if(((GetAsyncKeyState('W') & 0x8000) ||
                        (GetAsyncKeyState(VK_UP) & 0x8000) ||
                        (GetAsyncKeyState(' ') & 0x8000)) &&
                        GetIn == 1)
                    my++;
                if((GetAsyncKeyState('S') & 0x8000 ||
                        GetAsyncKeyState(VK_DOWN) & 0x8000) &&
                        GetIn == 1)
                    my--;
                if(GetAsyncKeyState('1') & 0x8000 && GetIn == 1)
                    cho = 1;
                if(GetAsyncKeyState('2') & 0x8000 && GetIn == 1)
                    cho = 2;
                if(GetAsyncKeyState('3') & 0x8000 && GetIn == 1)
                    cho = 3;
                if(GetAsyncKeyState('4') & 0x8000 && GetIn == 1)
                    cho = 4;
                if(GetAsyncKeyState('5') & 0x8000 && GetIn == 1)
                    cho = 5;
                if(GetAsyncKeyState('6') & 0x8000 && GetIn == 1)
                    cho = 6;
                if(GetAsyncKeyState('7') & 0x8000 && GetIn == 1)
                    cho = 7;
                if(GetAsyncKeyState('8') & 0x8000 && GetIn == 1)
                    cho = 8;
                if(GetAsyncKeyState('9') & 0x8000 && GetIn == 1)
                    cho = 9;
                if(GetAsyncKeyState('P') & 0x8000 && CanGet && GetIn == 1)
                {
                    if(CanGet)
                        getin();
                }
                else
                    can = 0;
                checkuseitem();
                // 大回血药(100)与能量药水(94)与回血药水(24)与护甲药水(113)
                if(KEY_DOWN(VK_RBUTTON) && beibao[cho][0] == 100)
                {
                    int heal = 60 + (hasTalent(T_POTION) ? 30 : 0);
                    life += heal;
                    if(life > lifeMax) life = lifeMax;
                    beibao[cho][1]--;
                }
                else if(KEY_DOWN(VK_RBUTTON) && beibao[cho][0] == 94)
                {
                    int eg = 50 + (hasTalent(T_POTION) ? 25 : 0);
                    energy = min(energyMax, energy + eg);
                    beibao[cho][1]--;
                }
                else if(KEY_DOWN(VK_RBUTTON) && beibao[cho][0] == 113)
                {
                    // 护甲药水：恢复护甲值（护甲不自然恢复，仅道具恢复）
                    int ar = 60 + (hasTalent(T_POTION) ? 30 : 0);
                    hujiazhi = min(armorMax, hujiazhi + ar);
                    beibao[cho][1]--;
                }
                else if(KEY_DOWN(VK_RBUTTON) && beibao[cho][0] == 24)
                {
                    int heal = 25 + (hasTalent(T_POTION) ? 12 : 0);
                    life += heal;
                    if(life > lifeMax) life = lifeMax;
                    beibao[cho][1]--;
                }
                else if(KEY_DOWN(VK_RBUTTON) && (beibao[cho][0] == 37||beibao[cho][0] == 55||beibao[cho][0] == 88||beibao[cho][0] == 89||beibao[cho][0] == 91))
                {
                    eat++;
                    if(eat > 75)
                    {
                        eat = 0;
                        if(beibao[cho][0] == 37)life += 10;
                        else if(beibao[cho][0] == 88)life += 10;
                        else if(beibao[cho][0] == 89)life += 15;
                        else if(beibao[cho][0] == 91)life += 40;
                        else life += 20;
                        if(life > lifeMax)
                            life = lifeMax;
                        beibao[cho][1]--;
                    }
                }
                else if(KEY_DOWN(VK_RBUTTON) && beibao[cho][0] != 24)
                    key_++;
                else
                    key_ = 0;
                if(key_ > 0)
                    key_ = -10, fz = 1;
                else
                    fz = 0;
                if(sword>0)sword--;
                POINT p = GetMousePos();
                p.x -= printx - 50;
                p.y -= (printy - 30) / 2;
                mpx = To_int(jx - 26 + p.x / 2);
                mpy = To_int(jy - 14 + p.y);
                // ===== 枪械系统：空格或左键开火 =====
                if(gunCD > 0) gunCD--;
                currentGunType = 0;
                if(beibao[cho][0] == 95 || beibao[cho][0] == 96 ||
                        beibao[cho][0] == 97 || beibao[cho][0] == 98)
                    currentGunType = beibao[cho][0];
                bool gunFired = false;
                if(currentGunType > 0)
                {
                    bool fireKey = (KEY_DOWN(VK_LBUTTON) || KEY_DOWN(VK_SPACE));
                    if(fireKey && gunCD <= 0)
                    {
                        if(energy >= gun_energy(currentGunType))
                        {
                            energy -= gun_energy(currentGunType);
                            gunCD = gun_cd(currentGunType);
                            if(hasTalent(T_ATKSPD)) gunCD = gunCD * 2 / 3;
                            fire_gun(currentGunType);
                            gunFired = true;
                        }
                        else
                        {
                            Setpos(1, 31);
                            Color(0);
                            cout << " 能量不足！                                    ";
                        }
                    }
                }
                if(KEY_DOWN(VK_LBUTTON))
                {
                    if(gunFired) { /* 已射击，跳过原逻辑 */ }
                    else if(beibao[cho][0] == 41||beibao[cho][0] == 61||beibao[cho][0] == 111)
                    {
                        bow++;
                        if(bow > 0)
                        {
                            bow = -30;
                            int U = getbeibao_2(44);
                            int U2 = getbeibao_2(45);
                            if(U == 0)
                                U = U2;
                            int U3 = getbeibao_2(46);
                            if(U == 0)
                                U = U3;
                            if(U2 != 0 && U2 < U)
                                U = U2;
                            if(U3 != 0 && U3 < U)
                                U = U3;
                            if(U != 0)
                            {
                                if(beibao[cho][0] == 61)
                                {
                                    if(rand()%3==0)beibao[U][1]--;
                                    if(beibao[U][0] == 44 || beibao[U][0] == 45)
                                    {
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 2);
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 2);
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 2);
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 2);
                                    }
                                    else
                                    {
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 3);
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 3);
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 3);
                                        make_jian(mpx+((rand()%40)+1)/10.0-2, y-25+((rand()%40)+1)/10.0-2, ((rand()%40)+1)/10.0-2, 20+((rand()%80)+1)/10.0, 3);
                                    }
                                }
                                else
                                {
                                    // 真实指向角（atan2）：箭矢直线飞向准星
                                    double ang = atan2(mpy - y, mpx - x);
                                    // 恶魔弓(111)：箭速更快，25%概率不消耗箭（beta1.8同步）
                                    if(beibao[cho][0] == 111)
                                    {
                                        if(rand()%4!=0)beibao[U][1]--;
                                        if(beibao[U][0] == 44 || beibao[U][0] == 45)
                                            make_jian(x, y-0.75, 75*cos(ang), 75*sin(ang), 2);
                                        else
                                            make_jian(x, y-0.75, 75*cos(ang), 75*sin(ang), 3);
                                    }
                                    else
                                    {
                                        beibao[U][1]--;
                                        if(beibao[U][0] == 44 || beibao[U][0] == 45)
                                            make_jian(x, y-0.75, 55*cos(ang), 55*sin(ang), 2);
                                        else
                                            make_jian(x, y-0.75, 55*cos(ang), 55*sin(ang), 3);
                                    }
                                }
                            }
                        }
                    }
                    else if(abs(mpx - int(x + 0.5)) <= 3 && abs(mpy - int(y + 0.5)) <= 3)
                    {
                        wajue(mpx, mpy);
                    }
                    else
                    {
                        if(abs(mpx - int(x + 0.5)) > abs(mpy - int(y + 0.5)))
                        {
                            if(mpx - int(x + 0.5) > 0)
                            {
                                if(blockwj[s[int(x + 1.5)][int(y + 0.5)]] > 0)
                                    wajue(int(x + 1.5), (y + 0.5));
                                else if(blockwj[s[int(x + 2.5)][int(y + 0.5)]] > 0)
                                    wajue(int(x + 2.5), (y + 0.5));
                                else if(blockwj[s[int(x + 3.5)][int(y + 0.5)]] > 0)
                                    wajue(int(x + 3.5), (y + 0.5));
                            }
                            else
                            {
                                if(blockwj[s[int(x - 0.5)][int(y + 0.5)]] > 0)
                                    wajue(int(x - 0.5), (y + 0.5));
                                else if(blockwj[s[int(x - 1.5)][int(y + 0.5)]] > 0)
                                    wajue(int(x - 1.5), (y + 0.5));
                                else if(blockwj[s[int(x - 2.5)][int(y + 0.5)]] > 0)
                                    wajue(int(x - 2.5), (y + 0.5));
                            }
                        }
                        else
                        {
                            if(mpy - int(y + 0.5) > 0)
                            {
                                if(blockwj[s[int(x + 0.5)][int(y + 1.5)]] > 0)
                                    wajue(int(x + 0.5), (y + 1.5));
                                else if(blockwj[s[int(x + 0.5)][int(y + 2.5)]] > 0)
                                    wajue(int(x + 0.5), (y + 2.5));
                                else if(blockwj[s[int(x + 0.5)][int(y + 3.5)]] > 0)
                                    wajue(int(x + 0.5), (y + 3.5));
                            }
                            else
                            {
                                if(blockwj[s[int(x + 0.5)][int(y - 0.5)]] > 0)
                                    wajue(int(x + 0.5), (y - 0.5));
                                else if(blockwj[s[int(x + 0.5)][int(y - 1.5)]] > 0)
                                    wajue(int(x + 0.5), (y - 1.5));
                                else if(blockwj[s[int(x + 0.5)][int(y - 2.5)]] > 0)
                                    wajue(int(x + 0.5), (y - 2.5));
                            }
                        }
                    }
                }
                else if(bow < 0)
                    bow++;
                if(fz)
                {
                    if(mpx >= 0 && mpx < WORLD_WIDTH && mpy >= 0 && mpy < WORLD_HEIGHT &&
                            abs(mpx - int(x + 0.5)) <= 3 && abs(mpy - int(y + 0.5)) <= 3)
                    {
                        if(s[mpx][mpy] == 9)
                            craft_2();
                        else if(s[mpx][mpy] == 104)
                            craft_3();   // 锻造台：下界装备升级
                        else if(s[mpx][mpy] == 40)
                            openbox();
                        else if(s[mpx][mpy] == 38)
                        {
                            swap_map();
                        }
                        else if(s[mpx][mpy] == 90)
                        {
                            // 床交互：设置重生点/入睡
                            back_x = mpx;
                            back_y = mpy;
                            if(night >= 1)
                            {
                                // 夜晚：入睡跳过黑夜
                                now_time = 0;
                                night = 0;
                                sky();
                                shuaxin();
                                Setpos(1, 31);
                                Color(0);
                                cout << " 已入睡，重生点已设置，天亮了                              ";
                            }
                            else
                            {
                                // 白天：仅设置重生点，无法入睡
                                Setpos(1, 31);
                                Color(0);
                                cout << " 重生点已设置（白天无法入睡）                              ";
                            }
                        }
                        else if(s[mpx][mpy] == 16 && mpx == portal_x && mpy == portal_y)
                        {
                            // 关卡传送门（火把）：进入地牢（可选择已解锁关卡）
                            if(!inDungeon)
                            {
                                int pickLevel = 0;
                                if(levelUnlocked <= 1)
                                {
                                    pickLevel = 1;   // 仅解锁第1关时直接进入
                                }
                                else
                                {
                                    Setpos(1, 31);
                                    Color(0);
                                    cout << " 选择关卡(已解锁1-" << levelUnlocked
                                         << ")：按1-9选关,0=第10关,E取消";
                                    clock_t stLv = clock();
                                    while(clock() - stLv < 10000)
                                    {
                                        for(int kk = 1; kk <= 9; kk++)
                                            if(GetAsyncKeyState('0' + kk) & 0x8000)
                                            { pickLevel = kk; break; }
                                        if(pickLevel == 0 && (GetAsyncKeyState('0') & 0x8000))
                                            pickLevel = 10;
                                        if(GetAsyncKeyState('E') & 0x8000)
                                        { pickLevel = -1; break; }
                                        if(pickLevel > 0) break;
                                        Sleep(1);
                                    }
                                    Setpos(1, 31);
                                    cout << "                                                              ";
                                }
                                if(pickLevel > 0)
                                {
                                    if(pickLevel > levelUnlocked) pickLevel = levelUnlocked;
                                    currentLevel = pickLevel;
                                    enter_level(currentLevel);
                                    Setpos(1, 31);
                                    Color(0);
                                    cout << " 进入第 " << currentLevel << " 关卡！                              ";
                                }
                            }
                            else
                            {
                                Setpos(1, 31);
                                Color(0);
                                cout << " 你已经在关卡中                                    ";
                            }
                        }
                        else if(s[mpx][mpy] == 102)
                        {
                            interact_gacha();
                        }
                        else if(beibao[cho][0] == 25)
                        {
                            if(s[mpx][mpy] == 19)
                            {
                                beibao[cho][0] = 26;
                                s[mpx][mpy] = 0;
                                mark_dirty_col(mpx);
                                Clear();
                            }
                            if(s[mpx][mpy] == 32)
                            {
                                beibao[cho][0] = 35;
                                s[mpx][mpy] = 0;
                                mark_dirty_col(mpx);
                                Clear();
                            }
                        }
                        else
                            block_(mpx, mpy);
                    }
                    else
                    {
                        if(abs(mpx - int(x + 0.5)) > abs(mpy - int(y + 0.5)))
                        {
                            if(mpx - int(x + 0.5) > 0)
                            {
                                if(blockwj[s[int(x + 3.5)][int(y + 0.5)]] == 0)
                                    block_(int(x + 3.5), (y + 0.5));
                                else if(blockwj[s[int(x + 2.5)][int(y + 0.5)]] == 0)
                                    block_(int(x + 2.5), (y + 0.5));
                                else if(blockwj[s[int(x + 1.5)][int(y + 0.5)]] == 0)
                                    block_(int(x + 1.5), (y + 0.5));
                            }
                            else
                            {
                                if(blockwj[s[int(x - 2.5)][int(y + 0.5)]] == 0)
                                    block_(int(x - 2.5), (y + 0.5));
                                else if(blockwj[s[int(x - 1.5)][int(y + 0.5)]] == 0)
                                    block_(int(x - 1.5), (y + 0.5));
                                else if(blockwj[s[int(x - 0.5)][int(y + 0.5)]] == 0)
                                    block_(int(x - 0.5), (y + 0.5));
                            }
                        }
                        else
                        {
                            if(mpy - int(y + 0.5) > 0)
                            {
                                if(blockwj[s[int(x + 0.5)][int(y + 3.5)]] == 0)
                                    block_(int(x + 0.5), (y + 3.5));
                                else if(blockwj[s[int(x + 0.5)][int(y + 2.5)]] == 0)
                                    block_(int(x + 0.5), (y + 2.5));
                                else if(blockwj[s[int(x + 0.5)][int(y + 1.5)]] == 0)
                                    block_(int(x + 0.5), (y + 1.5));
                            }
                            else
                            {
                                if(blockwj[s[int(x + 0.5)][int(y - 2.5)]] == 0)
                                    block_(int(x + 0.5), (y - 2.5));
                                else if(blockwj[s[int(x + 0.5)][int(y - 1.5)]] == 0)
                                    block_(int(x + 0.5), (y - 1.5));
                                else if(blockwj[s[int(x + 0.5)][int(y - 0.5)]] == 0)
                                    block_(int(x + 0.5), (y - 0.5));
                            }
                        }
                    }
                }
                // ===== NPC 交互（右键点击NPC）=====
                if(fz)
                {
                    for(int NI = 0; NI < 500; NI++)
                    {
                        if(shiti[NI].type >= 14 && shiti[NI].type <= 22 && shiti[NI].type != 0)
                        {
                            if(abs(mpx - shiti[NI].gx) < 2.5 && abs(mpy - shiti[NI].gy) < 2.5)
                            {
                                interact_npc(NI);
                                break;
                            }
                        }
                    }
                }
                // ===== 顶视角移动（重力系统已移除）=====
                {
                    short pfb = s[int(x + 0.5)][int(y + 0.5)];
                    if(gamemode == 1)
                    {
                        if(pfb >= 32 && pfb <= 34)   // 岩浆伤害
                        {
                            if(wudi == 0 && !hasTalent(T_LAVA))
                                damage(8, x, true), wudi = 30;
                        }
                        else if(pfb >= 19 && pfb <= 22)   // 水中：氧气消耗
                        {
                            if(oxygen > -20)
                                oxygen--;
                            else if(wudi == 0)
                                damage(5, x, true), wudi = 60;
                        }
                    }
                    // 非水中恢复氧气
                    if(!(pfb >= 19 && pfb <= 22))
                    {
                        oxygen += 5;
                        if(oxygen > 900)
                            oxygen = 900;
                    }
                    spy = -my * 8;
                    // 步进碰撞：每小步≤0.4格，防高速隧穿薄墙
                    double mdx = spx * 0.16, mdy = spy * 0.16;
                    double maxstep = max(fabs(mdx), fabs(mdy));
                    int nsteps = (int)(maxstep / 0.4) + 1;
                    for(int si = 0; si < nsteps; si++)
                    {
                        double ox = x, oy = y;
                        double nx = x + mdx / nsteps;
                        double ny = y + mdy / nsteps;
                        if(gamemode != 1)
                        {
                            x = nx; y = ny;   // 创造模式穿墙
                            continue;
                        }
                        auto solid = [&](double px, double py)
                        {
                            int cx = int(px + 0.5), cy = int(py + 0.5);
                            return cx >= 0 && cx < WORLD_WIDTH && cy >= 0 &&
                                   cy < WORLD_HEIGHT && blocktouch[s[cx][cy]] >= 1;
                        };
                        if(!solid(nx, ny))
                        {
                            x = nx; y = ny;         // 无碰撞
                        }
                        else if(!solid(nx, oy))
                        {
                            x = nx;                 // 仅x可行（贴墙滑动）
                        }
                        else if(!solid(ox, ny))
                        {
                            y = ny;                 // 仅y可行
                        }
                        // 都撞则保持原位
                    }
                }
                // 修复：边界留出4格余量。x/y 靠近世界边缘时，挖掘/放置逻辑会读取
                // s[int(x+3.5)][...] 等邻近格，原 clamp(x<=W-1) 下这些读取越界(未定义行为)
                if(x < 4)
                    x = 4;
                if(y < 4)
                    y = 4;
                if(x > WORLD_WIDTH - 5)
                    x = WORLD_WIDTH - 5;
                if(y > WORLD_HEIGHT - 5)
                {
                    // 掉出世界底部保护：地牢内回关卡起始房，主世界回重生点
                    if(inDungeon || currentLevel > 0)
                    {
                        y = WORLD_HEIGHT - 2;
                        spx = 0; spy = 0;
                        enter_level(currentLevel > 0 ? currentLevel : 1);
                        memset(painting, 114, sizeof(painting));
                        shuaxin();
                    }
                    else if(s[back_x][back_y] == 90)
                    {
                        x = back_x, y = back_y - 1;
                        shuaxin();
                    }
                    else
                        y = WORLD_HEIGHT - 6;   // 与新边界余量一致，避免逐帧反复触发兜底
                }
                if(special_effect==2)
                {
                    if(abs(x - jx) < 0.5) jx = x;
                    else jx += (x-jx)/10;
                    if(abs(y - jy) < 0.5) jy = y;
                    else jy += (y-jy)/10;
                }
                else if(special_effect==3)
                {
                    if(abs(x - jx) < 0.5) jx = x;
                    else jx += (x-jx)/20;
                    if(abs(y - jy) < 0.5) jy = y;
                    else jy += (y-jy)/20;
                }
                else
                {
                    jx = x;
                    jy = y;
                }
                if(jx < printx / 2)
                    jx = printx / 2;
                if(jy < printy / 2)
                    jy = printy / 2;
                if(jx > WORLD_WIDTH - printx / 2 - 1)
                    jx = WORLD_WIDTH - printx / 2 - 1;
                if(jy > WORLD_HEIGHT - printy / 2 - 1)
                    jy = WORLD_HEIGHT - printy / 2 - 1;
                st_move();
            }
            if(tick % 10 == 0)
                block_update();
            POINT p = GetMousePos();
            p.x -= printx - 50;
            p.y -= (printy - 30) / 2;
            mpy = To_int(jy - 14 + p.y);
            mpx = To_int(jx - 26 + p.x / 2);
            if(lx != x || ly != y)
                setlight();
            Print();
            lx = int(x + 0.5), ly = int(y + 0.5);
            Setpos(1, printy + 1);   // HUD生命行光标定位（print_fast依赖；Setpos(1,33)被其覆盖，省1次调用）
            Color(0);
            if(life <= 0)
                Start();
            if(life < lifeMax)
                H++;
            if(H > 300)
                life++, H = 0;
            if(life > lifeMax && !SetLife)
                life = lifeMax;
            print_fast();
            savetick++;
            if(savetick>=3600)
            {
                savetick=0;
                save_world(zzz, false);
            }
            }
        }
        //Sleep(1);
    }
}
int main()
{
    // 设置控制台为UTF-8编码以支持中文显示
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    try
    {
        did();
        // 先设置缓冲区大小，再全屏，避免全屏后缓冲区过小
        SetWindowSize(printx + 5, printy + 5);
        SetConsoleToFullscreen();
        HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
        if(hInput == INVALID_HANDLE_VALUE)
        {
            std::cerr << " 无法获取输入句柄\n";
            return 1;
        }
        DWORD mode;
        if(!GetConsoleMode(hInput, &mode))
        {
            std::cerr << "GetConsoleMode 失败\n";
            return 1;
        }
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        hInput = GetStdHandle(STD_INPUT_HANDLE);
        if(hInput == INVALID_HANDLE_VALUE)
        {
            std::cerr << " 无法获取输入句柄\n";
            return 1;
        }
        if(!GetConsoleMode(hInput, &mode))
        {
            std::cerr << "GetConsoleMode 失败\n";
            return 1;
        }
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        SetConsoleMode(hInput, mode);
        InitFPSCounter();
        srand((unsigned)time(NULL));
        HideCursor();
        bool willclear = true;
        while(1)
        {
            if(clock() - Clocknum >= 15)
            {
                Clocknum = clock();
                if(willclear)
                {
                    willclear = false;
                    Setpos(8, 6);
                    cout << " #   #   ####    #   #   ####   #####  #####  ####     #    "
                         "#####  #####";
                    Setpos(8, 7);
                    cout << " ## ##  #    #   #   #   #   #  #      #      #   #   # #   "
                         "#        #";
                    Setpos(8, 8);
                    cout << "  ###   #    #   #   #   ####   #####  #      ####   #   #  "
                         "#####    #";
                    Setpos(8, 9);
                    cout << "   #    #    #   #   #   #  #       #  #      #  #   #####  "
                         "#        #";
                    Setpos(8, 10);
                    cout << "   #     ####     ###    #   #  #####  #####  #   #  #   #  "
                         "#        #";
                    Setpos(40, 12);
                    cout << "Beta 2.0 (Soul Knight)";
                };
                POINT p = GetMousePos();
                mpx = To_int(p.x / 2 - 26);
                mpy = To_int(p.y - 14);
                if(mpx >= -2 && mpx <= 2 && mpy >= 5 && mpy <= 7)
                {
                    Color(1);
                    Setpos(23, 19);
                    cout << "  ----------";
                    Setpos(23, 20);
                    cout << " | 退出游戏 |";
                    Setpos(23, 21);
                    cout << "  ----------";
                    if(KEY_DOWN(VK_LBUTTON))
                    {
                        return 0;
                    }
                }
                else
                {
                    Color(0);
                    Setpos(23, 19);
                    cout << "  ----------";
                    Setpos(23, 20);
                    cout << " | 退出游戏 |";
                    Setpos(23, 21);
                    cout << "  ----------";
                }
                if(mpx >= -2 && mpx <= 2 && mpy >= 1 && mpy <= 3)
                {
                    Color(1);
                    Setpos(23, 15);
                    cout << "  ----------";
                    Setpos(23, 16);
                    cout << " | 进入游戏 |";
                    Setpos(23, 17);
                    cout << "  ----------";
                    if(KEY_DOWN(VK_LBUTTON))
                    {
                        willclear = true;
                        while(KEY_DOWN(VK_LBUTTON))
                            continue;
                        while(1)
                        {
                            if(clock() - Clocknum >= 15)
                            {
                                if(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
                                {
                                    while(GetAsyncKeyState(VK_ESCAPE) & 0x8000)
                                        continue;
                                    break;
                                }
                                Color(0);
                                Clocknum = clock();
                                if(willclear)
                                {
                                    for(int i = 6; i <= 10; i++)
                                    {
                                        Setpos(8, i);
                                        cout << "                                                  "
                                             "                      ";
                                    }
                                    Setpos(40, 12);
                                    cout << "            ";
                                    Setpos(23, 15);
                                    cout << "             ";
                                    Setpos(23, 16);
                                    cout << "             ";
                                    Setpos(23, 17);
                                    cout << "             ";
                                    Setpos(23, 19);
                                    cout << "             ";
                                    Setpos(23, 20);
                                    cout << "             ";
                                    Setpos(23, 21);
                                    cout << "             ";
                                    Setpos(23, 23);
                                    cout << "             ";
                                    Setpos(23, 24);
                                    cout << "             ";
                                    Setpos(23, 25);
                                    cout << "             ";
                                    Setpos(23, 3);
                                    cout << " 当前存档：";
                                    if(!read_savelist())
                                        cout << " 无";
                                    willclear = false;
                                }
                                POINT p = GetMousePos();
                                bool abcde;
                                mpx = To_int(p.x / 2 - 26);
                                mpy = To_int(p.y - 14);
                                for(int i = 0; i < read_s; i++)
                                {
                                    abcde = 0;
                                    Setpos(15, 6 + i * 2);
                                    if(mpy >= i * 2 - 8 && mpy <= i * 2 - 7)
                                    {
                                        Color(1);
                                        if(KEY_DOWN(VK_LBUTTON))
                                        {
                                            abcde = true;
                                            memset(painting, 114, sizeof(painting));
                                            if(read(saves[i]))
                                            {
                                                zzz = saves[i];
                                                game_loop();
                                                bag=0;
                                                Clear_2();
                                                willclear = true;
                                            }
                                            else
                                            {
                                                Color(4);
                                                Setpos(15, 28);
                                                cout << "存档损坏或不存在，无法加载                ";
                                                willclear = true;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        Color(0);
                                    }
                                    if(!abcde)
                                        cout << saves[i];
                                }
                                if(mpx >= -6 && mpx <= -1 && mpy >= 13 && mpy <= 15)
                                {
                                    Color(1);
                                    Setpos(18, 27);
                                    cout << "  ----------";
                                    Setpos(18, 28);
                                    cout << " | 新建世界 |";
                                    Setpos(18, 29);
                                    cout << "  ----------";
                                    if(KEY_DOWN(VK_LBUTTON) && read_s < 10)
                                    {
                                        make_newworld();
                                        memset(painting, 114, sizeof(painting));
                                        if(iscre)
                                        {
                                            enter_level(1);   // 新世界直接进入第1关
                                            game_loop();
                                        }
                                        bag=0;
                                        Clear_2();
                                        willclear = true;
                                    }
                                }
                                else
                                {
                                    Color(0);
                                    Setpos(18, 27);
                                    cout << "  ----------";
                                    Setpos(18, 28);
                                    cout << " | 新建世界 |";
                                    Setpos(18, 29);
                                    cout << "  ----------";
                                }
                                if(mpx >= 3 && mpx <= 8 && mpy >= 13 && mpy <= 15)
                                {
                                    Color(1);
                                    Setpos(28, 27);
                                    cout << "  ----------";
                                    Setpos(28, 28);
                                    cout << " | 删除世界 |";
                                    Setpos(28, 29);
                                    cout << "  ----------";
                                    if(KEY_DOWN(VK_LBUTTON))
                                    {
                                        delete_world();
                                        willclear = true;
                                    }
                                }
                                else
                                {
                                    Color(0);
                                    Setpos(28, 27);
                                    cout << "  ----------";
                                    Setpos(28, 28);
                                    cout << " | 删除世界 |";
                                    Setpos(28, 29);
                                    cout << "  ----------";
                                }
                                Sleep(1);
                            }
                        }
                        willclear = true;
                        Setpos(18, 27);
                        cout << "                                    ";
                        Setpos(18, 28);
                        cout << "                                    ";
                        Setpos(18, 29);
                        cout << "                                    ";
                        for(int i = 3; i < read_s; i++)
                        {
                            Setpos(15, 6 + i * 2);
                            cout << "                                    ";
                        }
                        Setpos(23, 3);
                        cout << "             ";
                    }
                }
                else
                {
                    Color(0);
                    Setpos(23, 15);
                    cout << "  ----------";
                    Setpos(23, 16);
                    cout << " | 进入游戏 |";
                    Setpos(23, 17);
                    cout << "  ----------";
                }
            }
            Sleep(1);
        }
        save_system_shutdown();
        return 0;
    }
    catch(const std::exception &e)
    {
        cout << "error: " << e.what();
        exit(1);
    }
    catch(...)
    {
        cout << "error: unknown exception";
        exit(1);
    }
}
