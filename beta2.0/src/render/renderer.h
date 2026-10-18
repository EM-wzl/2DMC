#pragma once
// ===================== 2D 渲染 API =====================
// 上层只需要这一组函数，完全不接触 OpenGL。所有图元按纹理分批，
// 同一纹理的精灵会合并进一次 draw call（图集把上千格压成 1 个纹理）。
#include "../core/types.h"
#include "atlas.h"

namespace render {

bool init();
void shutdown();

// ---- 坐标系 ----
// 世界坐标：y 向下（与地牢几何、旧版一致），1 单位 = 1 格
// 屏幕坐标：y 向下，原点在客户区左上角，单位像素
void begin_frame(Color clearColor);                        // 清屏 + 重置统计
void begin_world(f32 centerX, f32 centerY, f32 tilePx);   // tilePx = 一格画多少像素
void begin_ui();

// ---- 图元（坐标为矩形左上角）----
void draw(i32 slot, f32 x, f32 y, f32 w, f32 h, Color tint = WHITE());
// 带旋转（绕矩形中心，弧度制）与翻转
void draw_ex(i32 slot, f32 x, f32 y, f32 w, f32 h, Color tint,
             f32 rot, bool flipX, bool flipY);
void rect(f32 x, f32 y, f32 w, f32 h, Color c);
void rect_outline(f32 x, f32 y, f32 w, f32 h, Color c, f32 thickness = 2);
void line(f32 x0, f32 y0, f32 x1, f32 y1, Color c, f32 thickness = 1);
void circle(f32 cx, f32 cy, f32 r, Color c, i32 segs = 20);
void ring(f32 cx, f32 cy, f32 r, f32 thickness, Color c, i32 segs = 24);

// 裁剪（屏幕坐标；嵌套会取交集）
void push_clip(f32 x, f32 y, f32 w, f32 h);
void pop_clip();

// ---- 文本（UTF-8）----
void text(f32 x, f32 y, const char *utf8, Color c, f32 scale = 1.0f);
void textf(f32 x, f32 y, Color c, f32 scale, const char *fmt, ...);
f32  text_w(const char *utf8, f32 scale = 1.0f);
f32  text_h(f32 scale = 1.0f);
// 居中 / 右对齐的便捷版本
void text_center(f32 cx, f32 y, const char *utf8, Color c, f32 scale = 1.0f);
void text_right(f32 rx, f32 y, const char *utf8, Color c, f32 scale = 1.0f);

void end();                       // 提交批次并呈现
void set_clear_color(Color c);

// 把当前后台缓冲导出为 32 位 BMP（须在 end() 之后、present() 之前调用）
bool capture_bmp(const char *path);

// 当前帧统计
i32 last_draw_calls();

}   // namespace render