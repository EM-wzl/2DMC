#pragma once
// ===================== 合成 / 熔炼 / 升级 配方数据表 =====================
//
// 数据来源：beta/beta1.8.cpp（9582 行）
//   四个配方注册函数定义 ...... 7395-7595   （zz / zz2 / zz_rl / zz_rl_2）
//   熔炉页调用清单 ............ 7642-7668   （craft_2）
//   下界升级页调用清单 ........ 7669-7695   （craft_3）
//   合成页调用清单 ............ 7696-7876   （craft_1，UI 的 5 个翻页）
//   各页显示文本 .............. 6671-6974   （craft_update）
//
// 旧版把「配方」写死在 UI 循环里（鼠标坐标 + 点击判定 + 背包操作混在一起），
// 这里把配方抽成纯数据：UI 只负责画 desc / 响应点击，配方内容一律走本表。
//
// ---- 四个旧函数 → 本表 kind 的对应 ----
//   zz(Y, need1, type1, need2, type2, get, sl, upd)  两种材料各 need 个 -> get x sl  -> R_TWO_MAT
//   zz2(Y, need1, type1, get, sl, upd)               一种材料 need 个  -> get x sl  -> R_ONE_MAT
//   zz_rl(Y, need1, type1, sl)                       材料 -> sl 点燃料 -> R_FUEL
//   zz_rl_2(Y, need1, type1, get, sl)                材料 + 1 点燃料 -> get x sl -> R_FUEL_USE
// （Y 只是旧版的鼠标行号 y = Y + 13，属于 UI 布局，不进数据表。）
//
// ---- page 编号：统一采用 craft_update(num) 的 num ----
// 旧版有两条页码线（craft_1 的全局 page 1..5，与 craft_update 的 num 1..7）互相错位，
// 这里一律用 craft_update 的 num，并在下面标出它对应 craft_1 的哪一屏：
//   page 1 = craft_update(1) 合成页① 基础工具/装备      <- craft_1 的 UI page 1
//   page 2 = craft_update(2) 熔炼页（燃料 rl）           <- craft_2
//   page 3 = craft_update(3) 合成页② 武器/弓箭/钻石     <- craft_1 的 UI page 2
//   page 4 = craft_update(4) 合成页③ 木板/下界锭/暗影    <- craft_1 的 UI page 3
//   page 5 = craft_update(5) 合成页⑤ 金装备             <- craft_1 的 UI page 5
//   page 6 = craft_update(6) 空页（旧版只剩说明文本）    <- craft_1 的 UI page 4
//   page 7 = craft_update(7) 下界升级页                 <- craft_3
#include "../core/types.h"

namespace item
{

// 配方类型
enum RecipeKind
{
    R_TWO_MAT,    // 两种材料 -> 产物
    R_ONE_MAT,    // 一种材料 -> 产物
    R_FUEL,       // 一种材料 -> 燃料点数 rl
    R_FUEL_USE,   // 一种材料 + 1 燃料 -> 产物
    // ★ 耐久度修复（旧版没有，新增功能）
    // type1/need1 是材料；out 是要修复的物品 id；out_n 是「回复耐久上限的百分之几」。
    // 执行时不产生新物品，而是把背包里该物品的 wear 回退。
    R_REPAIR,
};

struct Recipe
{
    u8  kind;
    u8  page;        // 1..8
    i16 need1, type1;   // 材料 1
    i16 need2, type2;   // 材料 2（R_TWO_MAT 才有意义，否则 -1）
    i16 out, out_n;     // 产物 id 与数量
                        // ★ R_FUEL 没有产物：out = -1，out_n = 该材料换来的燃料点数
                        //   （旧版 zz_rl 的 sl）
                        // ★ R_REPAIR：out = 要修的物品 id，out_n = 回复百分比
    const char *desc;   // 中文说明，UI 直接显示（已去掉旧版控制台排版的前导空格）
};

extern const Recipe RECIPES[];
extern const int RECIPE_COUNT;

// 第 page 页的显示标题（旧版 craft_update 里的中文标题）；页号非法返回 ""
const char *page_title(int page);

// ---- 页面附加信息（旧版 craft_update 里除逐条配方说明以外的文字）----
struct PageInfo
{
    const char *title;          // 标题行（旧版 Setpos(6,7)），如 "合成配方："
    const char *note;           // 信息行（旧版熔炼页 Setpos(6,8) 的 "当前燃烧时长："），无则 nullptr
    const char *hint;           // 底部翻页提示（旧版 Setpos(6,24)），无则 nullptr
    const char *const *orphan;  // 该页没有配方、但旧版仍在打印的说明行（只有 page 6），无则 nullptr
    int orphan_n;               // orphan 行数
};
const PageInfo &page_info(int page);

// 取某页配方在 RECIPES[] 里的区间 [first, first + count)，便于 UI 按页遍历
void page_range(int page, int &first, int &count);

// ============================================================================
//           ★ 网格合成（MC 式）：摆放位置决定配方
// ============================================================================
// 旧版是「材料数量 -> 产物」的按键式列表；现在改成和真 MC 一样：
// 把物品**按位置**摆进 2x2（背包里）或 3x3（工作台）的格子，
// 形状对上了就在右边的出口格里拿到成品。
//
// 形状配方：pat[3][3] 是图案，0=空格，>0=物品 id。图案可以摆在网格任意
//   位置（自动对齐到左上角再比较），但图案之外不能有多余物品。
// 无序配方：ing[] 是材料清单，摆哪儿都行，但不能多也不能少。
struct GridRecipe
{
    i16 pat[9];      // 形状：row-major 3x3，0=空
    i16 ing[4];      // 无序：材料 id，0=空（最多 4 种；数量用 cnt）
    i16 ing_cnt[4];  // 无序：每种材料要几个
    i16 out, out_n;
    u8  grid;        // 0=2x2/3x3 都行；2=只能 2x2；3=必须 3x3
    u8  shaped;      // 1=按形状，0=无序
    const char *desc;
};

extern const GridRecipe GRID_RECIPES[];
extern const int GRID_RECIPE_COUNT;

// 在 size x size 网格里找配方（cells[row*size+col]，0=空格）。
// 找到返回配方下标，没找到返回 -1。
int match_grid(const i16 *cells, int size);
// 该网格当前能合成出什么；没有配方时 out=0 并返回 false
bool grid_result(const i16 *cells, int size, i16 &out, i16 &out_n);

// ---- 合成执行（纯逻辑，UI 调用）----
// 玩家是否凑得齐 recipe 的材料
bool can_craft(const Recipe &r);
// 执行一次：扣材料、发产物、扣 1 点燃料（R_FUEL_USE）。
// 返回 true 表示成功；失败时不改变任何状态。
bool do_craft(const Recipe &r);
// 当前燃料点数（旧版全局 rl）
int  fuel();
void add_fuel(int n);
void use_fuel(int n);

}   // namespace item
