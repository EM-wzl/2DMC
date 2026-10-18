#pragma once
// ===================== 2D 渲染 API =====================
// 上层只需要这一组函数，完全不接触 OpenGL。所有图元按纹理分批，
// 同一纹理的精灵会合并进一次 draw call（图集把上千格压成 1 个纹理）。
//
// 坐标系：
//   世界坐标：y 向下，1 单位 = 1 格（与旧版控制台版的地牢几何一致）
//   屏幕坐标：y 向下，原点在客户区左上角，单位像素
// begin_world() 之后所有矩形坐标都是「格」，begin_ui() 之后是「像素」。
#include "../core/types.h"
#include "atlas.h"

namespace render {

bool init();
void shutdown();

// ---- 帧与坐标系 ----
void begin_frame(Color clearColor);                        // 清屏 + 重置统计
void begin_world(f32 centerX, f32 centerY, f32 tilePx);    // tilePx = 一格画多少像素
void begin_ui();
f32  tile_px();                                            // 当前一格的像素数
// 图集像素改过之后（如换皮肤）重新上传到显卡
void upload_atlas();

// ---- 图元（坐标为矩形左上角）----
void draw(i32 slot, f32 x, f32 y, f32 w, f32 h, Color tint = WHITE());
// 带旋转（绕矩形中心，弧度制）与翻转
void draw_ex(i32 slot, f32 x, f32 y, f32 w, f32 h, Color tint,
             f32 rot, bool flipX, bool flipY);
// ★ 四角分别着色：世界地形用它做平滑光照（顶点色由 GL 线性插值）
void draw_lit(i32 slot, f32 x, f32 y, f32 w, f32 h,
              Color c00, Color c10, Color c11, Color c01);
// 只取贴图的一部分（0..1 的归一化子矩形），用于流体高度、进度条填充
void draw_sub(i32 slot, f32 x, f32 y, f32 w, f32 h, f32 u0, f32 v0, f32 u1,
              f32 v1, Color tint);

void rect(f32 x, f32 y, f32 w, f32 h, Color c);
// 四角渐变的矩形（天空渐变、光照衰减）
void rect_lit(f32 x, f32 y, f32 w, f32 h, Color c00, Color c10, Color c11, Color c01);
void rect_outline(f32 x, f32 y, f32 w, f32 h, Color c, f32 thickness = 2);
void line(f32 x0, f32 y0, f32 x1, f32 y1, Color c, f32 thickness = 1);
void circle(f32 cx, f32 cy, f32 r, Color c, i32 segs = 20);
void ring(f32 cx, f32 cy, f32 r, f32 thickness, Color c, i32 segs = 24);
// 圆角面板（UI 用，避免满屏尖角矩形）
void panel(f32 x, f32 y, f32 w, f32 h, Color fill, Color border, f32 r = 6,
           f32 border_w = 2);

// 裁剪（屏幕坐标；嵌套会取交集）
void push_clip(f32 x, f32 y, f32 w, f32 h);
void pop_clip();

// ---- 文本（UTF-8）----
void text(f32 x, f32 y, const char *utf8, Color c, f32 scale = 1.0f);
void textf(f32 x, f32 y, Color c, f32 scale, const char *fmt, ...);
f32  text_w(const char *utf8, f32 scale = 1.0f);
f32  text_h(f32 scale = 1.0f);
void text_center(f32 cx, f32 y, const char *utf8, Color c, f32 scale = 1.0f);
void text_right(f32 rx, f32 y, const char *utf8, Color c, f32 scale = 1.0f);
// 带黑色描边的文本：在花色背景上也读得清（HUD 常用）
void text_shadow(f32 x, f32 y, const char *utf8, Color c, f32 scale = 1.0f);
void text_center_shadow(f32 cx, f32 y, const char *utf8, Color c, f32 scale = 1.0f);

void end();                       // 提交批次并呈现
void set_clear_color(Color c);
// 开关 alpha 混合。关掉时贴图按原样覆盖（含 alpha 通道的原始 RGB），
// 用于贴图调试图集：BMP 截图会把 alpha 压平，只有关掉混合才看得出
// 每张贴图的实际覆盖范围。
void set_blend(bool on);

// 把当前后台缓冲导出为 32 位 BMP（须在 end() 之后、present() 之前调用）
bool capture_bmp(const char *path);

// 当前帧统计
i32 last_draw_calls();
i32 last_quad_count();

}   // namespace render
