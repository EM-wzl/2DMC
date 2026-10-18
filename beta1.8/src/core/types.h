#pragma once
// ===================== 基础类型与数学 =====================
// 全项目统一使用这里的别名；渲染层与游戏层不直接依赖任何平台头文件。
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

// 颜色：0xAABBGGRR 小端序，与 GL_BGRA 上传一致
using Color = u32;

constexpr Color rgba(u8 r, u8 g, u8 b, u8 a = 255)
{
    return (Color)(a << 24 | b << 16 | g << 8 | r);
}
// 注意不叫 RGB：wingdi.h 里有 RGB(r,g,b) 宏，同名会把调用点替换坏
constexpr Color col3(u8 r, u8 g, u8 b) { return rgba(r, g, b); }
constexpr Color WHITE() { return rgba(255, 255, 255); }

// 带 alpha 的调制：tint 与基色相乘
inline Color tint_of(Color base, f32 k, u8 a = 255)
{
    if(k < 0) k = 0;
    if(k > 4) k = 4;
    u8 r = (u8)std::min(255.0f, ((base & 0xFF) * k));
    u8 g = (u8)std::min(255.0f, (((base >> 8) & 0xFF) * k));
    u8 b = (u8)std::min(255.0f, (((base >> 16) & 0xFF) * k));
    return rgba(r, g, b, a);
}

struct Vec2
{
    f32 x = 0, y = 0;
    Vec2() = default;
    Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2 &o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2 &o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(f32 k) const { return {x * k, y * k}; }
    Vec2 &operator+=(const Vec2 &o) { x += o.x; y += o.y; return *this; }
    Vec2 &operator-=(const Vec2 &o) { x -= o.x; y -= o.y; return *this; }
    Vec2 &operator*=(f32 k) { x *= k; y *= k; return *this; }
    f32 len() const { return std::sqrt(x * x + y * y); }
    f32 len2() const { return x * x + y * y; }
    Vec2 norm() const
    {
        f32 l = len();
        return l > 1e-6f ? Vec2(x / l, y / l) : Vec2(0, 0);
    }
};

struct Rect
{
    f32 x = 0, y = 0, w = 0, h = 0;
    bool contains(f32 px, f32 py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    f32 cx() const { return x + w * 0.5f; }
    f32 cy() const { return y + h * 0.5f; }
};

inline f32 clampf(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline i32 clampi(i32 v, i32 lo, i32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline f32 lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
inline f32 dist(f32 ax, f32 ay, f32 bx, f32 by)
{
    f32 dx = ax - bx, dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}
// 与旧版 jl() 等价：两点曼哈顿轴距离的欧氏长度（旧代码大量使用）
inline f32 cheb_len(f32 dx, f32 dy)
{
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    return std::sqrt(dx * dx + dy * dy);
}
// 角度归一化到 (-PI, PI]
inline f32 wrap_angle(f32 a)
{
    constexpr f32 PI = 3.14159265358979f;
    while(a > PI) a -= 2 * PI;
    while(a <= -PI) a += 2 * PI;
    return a;
}
// 朝目标角度平滑旋转，每帧最大转速 maxStep
inline f32 turn_toward(f32 cur, f32 target, f32 maxStep)
{
    f32 d = wrap_angle(target - cur);
    if(d > maxStep) d = maxStep;
    if(d < -maxStep) d = -maxStep;
    return cur + d;
}
inline i32 sign_of(f32 v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

// ===================== 随机数 =====================
// 自成体系、可播种的 xorshift，避免依赖 rand() 的全局状态
class Rng
{
public:
    void seed(u64 s)
    {
        if(s == 0) s = 0x9E3779B97F4A7C15ull;
        st = s;
        for(int i = 0; i < 4; i++) next();
    }
    u32 next()
    {
        st ^= st << 13;
        st ^= st >> 7;
        st ^= st << 17;
        return (u32)(st >> 32);
    }
    // [0, n) 整数；n<=0 返回 0
    i32 range(i32 n) { return n > 0 ? (i32)(next() % (u32)n) : 0; }
    i32 range(i32 lo, i32 hi) { return lo + range(hi - lo + 1); }
    // [0,1)
    f32 unit() { return (f32)(next() >> 8) / 16777216.0f; }
    f32 unit(f32 lo, f32 hi) { return lo + unit() * (hi - lo); }
    // 百分比判定：p 为 0-100
    bool chance(i32 p) { return range(100) < p; }
private:
    u64 st = 0x2545F4914F6CDD1Dull;
};
// 全局共享随机源（C++17 inline 变量，无需单独的 .cpp 定义）
inline Rng g_rng;

// 相邻 8 方向的单位偏移，供寻路/蔓延使用
constexpr i32 DX8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr i32 DY8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
constexpr i32 DX4[4] = {1, -1, 0, 0};
constexpr i32 DY4[4] = {0, 0, 1, -1};