#pragma once
// ===================== 字体图集（GDI 生成）=====================
// 用系统字体把字形按需画进一张 GL 纹理：
//  - 白字黑底渲染后把亮度当 alpha，得到抗锯齿的白色字形，运行时用顶点色染色
//  - 支持中文（优先微软雅黑，回落宋体），无需任何字体文件
#include "../core/types.h"

namespace font {

struct Glyph
{
    f32 u0, v0, u1, v1;   // 纹理坐标
    f32 w, h;             // 像素尺寸
    f32 adv;              // 前进宽度
};

bool init();
void shutdown();
bool ready();

u32 texture();                  // GL 纹理 id
const Glyph *get(u32 codepoint);  // 按需生成并缓存
f32 line_height();              // scale=1 时的行高
f32 space_adv();                // 空白前进宽度

}   // namespace font