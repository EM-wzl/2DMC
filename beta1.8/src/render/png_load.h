#pragma once
// ===================== PNG 载入（GDI+）=====================
// 只用来读 assets/tiles/*.png 这类外部物品/方块贴图。
// 输出像素格式与引擎一致：u32 = 0xAABBGGRR（内存字节序 R,G,B,A）。
#include "../core/types.h"
#include <vector>

namespace atlas {

// 读 PNG 的**第一个正方形帧**（贴图集/动画贴图是竖排多帧，取左上角第一帧）。
// 成功返回 true，out 为该方形帧的像素，w=h=边长。
bool load_png_rgba(const char *path, std::vector<u32> &out, i32 &w, i32 &h);

// 读整张 PNG（不做方形裁剪）。64x32 的怪物皮肤不是正方形，用这个。
bool load_png_rgba_full(const char *path, std::vector<u32> &out, i32 &w, i32 &h);

}   // namespace atlas
