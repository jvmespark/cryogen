#pragma once
//
// cryogen/math/mat.hpp
// 4x4 column-major matrix matching GLSL's `mat4`.
//

#include <cmath>
#include <cstring>

#include "cryogen/math/vec.hpp"

namespace cryogen::math {

// Column-major: m.c[col][row]
struct mat4 {
    float c[4][4]{};

    constexpr mat4() = default;
    constexpr explicit mat4(float diag) {
        c[0][0] = diag; c[1][1] = diag; c[2][2] = diag; c[3][3] = diag;
    }

    static constexpr mat4 identity() { return mat4(1.f); }

    constexpr float* data()             { return &c[0][0]; }
    constexpr const float* data() const { return &c[0][0]; }
};

inline mat4 operator*(const mat4& a, const mat4& b) {
    mat4 r;
    for (int i = 0; i < 4; ++i) {        // column of result
        for (int j = 0; j < 4; ++j) {    // row of result
            float s = 0.f;
            for (int k = 0; k < 4; ++k) {
                s += a.c[k][j] * b.c[i][k];
            }
            r.c[i][j] = s;
        }
    }
    return r;
}

inline vec4 operator*(const mat4& m, vec4 v) {
    return {
        m.c[0][0]*v.x + m.c[1][0]*v.y + m.c[2][0]*v.z + m.c[3][0]*v.w,
        m.c[0][1]*v.x + m.c[1][1]*v.y + m.c[2][1]*v.z + m.c[3][1]*v.w,
        m.c[0][2]*v.x + m.c[1][2]*v.y + m.c[2][2]*v.z + m.c[3][2]*v.w,
        m.c[0][3]*v.x + m.c[1][3]*v.y + m.c[2][3]*v.z + m.c[3][3]*v.w,
    };
}

inline mat4 translate(vec3 t) {
    mat4 m = mat4::identity();
    m.c[3][0] = t.x; m.c[3][1] = t.y; m.c[3][2] = t.z;
    return m;
}

inline mat4 scale(vec3 s) {
    mat4 m;
    m.c[0][0] = s.x; m.c[1][1] = s.y; m.c[2][2] = s.z; m.c[3][3] = 1.f;
    return m;
}

// Right-handed perspective with depth in [0, 1] (Vulkan).
inline mat4 perspective_vk(float fov_y_radians, float aspect,
                           float z_near, float z_far) {
    const float f = 1.f / std::tan(fov_y_radians * 0.5f);
    mat4 m;
    m.c[0][0] = f / aspect;
    m.c[1][1] = -f;                                  // Vulkan Y-flip
    m.c[2][2] = z_far / (z_near - z_far);
    m.c[2][3] = -1.f;
    m.c[3][2] = (z_near * z_far) / (z_near - z_far);
    return m;
}

inline mat4 look_at(vec3 eye, vec3 center, vec3 up) {
    vec3 f = normalize(center - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);
    mat4 m = mat4::identity();
    m.c[0][0] =  s.x; m.c[1][0] =  s.y; m.c[2][0] =  s.z;
    m.c[0][1] =  u.x; m.c[1][1] =  u.y; m.c[2][1] =  u.z;
    m.c[0][2] = -f.x; m.c[1][2] = -f.y; m.c[2][2] = -f.z;
    m.c[3][0] = -dot(s, eye);
    m.c[3][1] = -dot(u, eye);
    m.c[3][2] =  dot(f, eye);
    return m;
}

inline mat4 inverse(const mat4& m) {
    const float* a = m.data();
    float inv[16];

    inv[0] =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15]
            + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
    inv[4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15]
            - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
    inv[8] =  a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15]
            + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
    inv[12]= -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14]
            - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
    inv[1] = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15]
            - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
    inv[5] =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15]
            + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
    inv[9] = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15]
            - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
    inv[13]=  a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14]
            + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
    inv[2] =  a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15]
            + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
    inv[6] = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15]
            - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
    inv[10]=  a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15]
            + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
    inv[14]= -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14]
            - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];
    inv[3] = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11]
            - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
    inv[7] =  a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11]
            + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
    inv[11]= -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11]
            - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
    inv[15]=  a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10]
            + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];

    float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
    if (std::fabs(det) < 1e-12f) return mat4::identity();
    det = 1.f / det;

    mat4 r;
    for (int i = 0; i < 16; ++i) r.data()[i] = inv[i] * det;
    return r;
}

} // namespace cryogen::math
