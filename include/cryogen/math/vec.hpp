#pragma once
//
// cryogen/math/vec.hpp
// Compact, header-only vector primitives. Layouts are POD and chosen to match
// std140 / scalar GLSL so we can memcpy structs into UBO/SSBO storage.
//

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cryogen::math {

struct vec2 {
    float x = 0.f, y = 0.f;

    constexpr vec2() = default;
    constexpr vec2(float s) : x(s), y(s) {}
    constexpr vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr float& operator[](std::size_t i)       { return (&x)[i]; }
    constexpr float  operator[](std::size_t i) const { return (&x)[i]; }
};

struct vec3 {
    float x = 0.f, y = 0.f, z = 0.f;

    constexpr vec3() = default;
    constexpr vec3(float s) : x(s), y(s), z(s) {}
    constexpr vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr float& operator[](std::size_t i)       { return (&x)[i]; }
    constexpr float  operator[](std::size_t i) const { return (&x)[i]; }
};

struct vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;

    constexpr vec4() = default;
    constexpr vec4(float s) : x(s), y(s), z(s), w(s) {}
    constexpr vec4(float x_, float y_, float z_, float w_)
        : x(x_), y(y_), z(z_), w(w_) {}
    constexpr vec4(vec3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}

    constexpr float& operator[](std::size_t i)       { return (&x)[i]; }
    constexpr float  operator[](std::size_t i) const { return (&x)[i]; }
};

// ---- vec3 ops -----------------------------------------------------
constexpr vec3 operator+(vec3 a, vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
constexpr vec3 operator-(vec3 a, vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
constexpr vec3 operator-(vec3 a)         { return {-a.x, -a.y, -a.z}; }
constexpr vec3 operator*(vec3 a, float s){ return {a.x*s, a.y*s, a.z*s}; }
constexpr vec3 operator*(float s, vec3 a){ return a*s; }
constexpr vec3 operator/(vec3 a, float s){ return {a.x/s, a.y/s, a.z/s}; }
constexpr vec3 operator*(vec3 a, vec3 b) { return {a.x*b.x, a.y*b.y, a.z*b.z}; }

constexpr vec3& operator+=(vec3& a, vec3 b)  { a = a + b; return a; }
constexpr vec3& operator-=(vec3& a, vec3 b)  { a = a - b; return a; }
constexpr vec3& operator*=(vec3& a, float s) { a = a * s; return a; }
constexpr vec3& operator/=(vec3& a, float s) { a = a / s; return a; }

constexpr float dot(vec3 a, vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
constexpr vec3 cross(vec3 a, vec3 b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
inline float length(vec3 a) { return std::sqrt(dot(a,a)); }
inline float length_sq(vec3 a) { return dot(a,a); }
inline vec3  normalize(vec3 a) {
    const float l = length(a);
    return l > 1e-8f ? a / l : vec3{0,0,0};
}
constexpr vec3 lerp(vec3 a, vec3 b, float t) {
    return a + (b - a) * t;
}

// ---- vec4 ops -----------------------------------------------------
constexpr vec4 operator+(vec4 a, vec4 b) { return {a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w}; }
constexpr vec4 operator-(vec4 a, vec4 b) { return {a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w}; }
constexpr vec4 operator*(vec4 a, float s){ return {a.x*s, a.y*s, a.z*s, a.w*s}; }
constexpr vec4 operator*(float s, vec4 a){ return a*s; }
constexpr float dot(vec4 a, vec4 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

// ---- vec2 ops -----------------------------------------------------
constexpr vec2 operator+(vec2 a, vec2 b) { return {a.x+b.x, a.y+b.y}; }
constexpr vec2 operator-(vec2 a, vec2 b) { return {a.x-b.x, a.y-b.y}; }
constexpr vec2 operator*(vec2 a, float s){ return {a.x*s, a.y*s}; }

inline vec3 reflect(vec3 i, vec3 n) { return i - n * (2.f * dot(n, i)); }

constexpr float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
constexpr vec3 min(vec3 a, vec3 b) {
    return { a.x<b.x?a.x:b.x, a.y<b.y?a.y:b.y, a.z<b.z?a.z:b.z };
}
constexpr vec3 max(vec3 a, vec3 b) {
    return { a.x>b.x?a.x:b.x, a.y>b.y?a.y:b.y, a.z>b.z?a.z:b.z };
}

constexpr float kPi      = 3.14159265358979323846f;
constexpr float kTwoPi   = 6.28318530717958647692f;
constexpr float kHalfPi  = 1.57079632679489661923f;
constexpr float deg2rad(float d) { return d * (kPi / 180.f); }
constexpr float rad2deg(float r) { return r * (180.f / kPi); }

} // namespace cryogen::math
