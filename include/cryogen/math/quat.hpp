#pragma once
//
// cryogen/math/quat.hpp
// Hamilton-product unit quaternions (x,y,z,w) for orientation.
//

#include <cmath>

#include "cryogen/math/mat.hpp"
#include "cryogen/math/vec.hpp"

namespace cryogen::math {

struct quat {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;

    constexpr quat() = default;
    constexpr quat(float x_, float y_, float z_, float w_)
        : x(x_), y(y_), z(z_), w(w_) {}

    static quat identity() { return {0,0,0,1}; }
    static quat from_axis_angle(vec3 axis, float angle) {
        const float h = angle * 0.5f;
        const float s = std::sin(h);
        const vec3 a = normalize(axis);
        return { a.x*s, a.y*s, a.z*s, std::cos(h) };
    }
    static quat from_euler(vec3 e_radians) {
        const float cx = std::cos(e_radians.x*0.5f), sx = std::sin(e_radians.x*0.5f);
        const float cy = std::cos(e_radians.y*0.5f), sy = std::sin(e_radians.y*0.5f);
        const float cz = std::cos(e_radians.z*0.5f), sz = std::sin(e_radians.z*0.5f);
        return {
            sx*cy*cz - cx*sy*sz,
            cx*sy*cz + sx*cy*sz,
            cx*cy*sz - sx*sy*cz,
            cx*cy*cz + sx*sy*sz,
        };
    }
};

constexpr quat operator*(quat a, quat b) {
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
    };
}

inline quat normalize(quat q) {
    const float l = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    return l > 1e-8f ? quat{q.x/l, q.y/l, q.z/l, q.w/l} : quat::identity();
}

inline quat conjugate(quat q) { return {-q.x, -q.y, -q.z, q.w}; }

inline vec3 rotate(quat q, vec3 v) {
    const vec3 u{q.x, q.y, q.z};
    const float s = q.w;
    return u * (2.f * dot(u, v))
         + v * (s*s - dot(u, u))
         + cross(u, v) * (2.f * s);
}

inline mat4 to_mat4(quat q) {
    const float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
    const float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
    const float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
    mat4 m = mat4::identity();
    m.c[0][0] = 1.f - 2.f*(yy+zz); m.c[0][1] = 2.f*(xy+wz);     m.c[0][2] = 2.f*(xz-wy);
    m.c[1][0] = 2.f*(xy-wz);       m.c[1][1] = 1.f - 2.f*(xx+zz); m.c[1][2] = 2.f*(yz+wx);
    m.c[2][0] = 2.f*(xz+wy);       m.c[2][1] = 2.f*(yz-wx);     m.c[2][2] = 1.f - 2.f*(xx+yy);
    return m;
}

inline quat slerp(quat a, quat b, float t) {
    float d = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (d < 0.f) { b = {-b.x,-b.y,-b.z,-b.w}; d = -d; }
    if (d > 0.9995f) {
        return normalize(quat{
            a.x + t*(b.x-a.x), a.y + t*(b.y-a.y),
            a.z + t*(b.z-a.z), a.w + t*(b.w-a.w),
        });
    }
    const float th0 = std::acos(d);
    const float th  = th0 * t;
    const float s0 = std::cos(th) - d*std::sin(th)/std::sin(th0);
    const float s1 = std::sin(th) / std::sin(th0);
    return {
        a.x*s0 + b.x*s1, a.y*s0 + b.y*s1,
        a.z*s0 + b.z*s1, a.w*s0 + b.w*s1,
    };
}

} // namespace cryogen::math
