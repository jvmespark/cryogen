//
// tests/test_math.cpp
// Sanity checks on the math primitives.
// (wrong column-major indexing, perspective_vk forgetting the Y flip, slerp on non-unit quats).
//
#include "test_util.hpp"

#include "cryogen/math/aabb.hpp"
#include "cryogen/math/mat.hpp"
#include "cryogen/math/quat.hpp"
#include "cryogen/math/transform.hpp"
#include "cryogen/math/vec.hpp"

using namespace cryogen;
using namespace cryogen::math;

static void vec_basics() {
    vec3 a{1, 2, 3};
    vec3 b{4, 5, 6};
    CRYOGEN_CHECK(approx_eq(dot(a, b), 32.f));
    vec3 c = cross(a, b);
    CRYOGEN_CHECK(approx_eq(c.x, -3.f));
    CRYOGEN_CHECK(approx_eq(c.y,  6.f));
    CRYOGEN_CHECK(approx_eq(c.z, -3.f));
    CRYOGEN_CHECK(approx_eq(length({3.f, 4.f, 0.f}), 5.f));

    vec3 n = normalize(vec3{2.f, 0.f, 0.f});
    CRYOGEN_CHECK(approx_eq(n.x, 1.f));
    CRYOGEN_CHECK(approx_eq(length(n), 1.f));
}

static void mat_identity_and_compose() {
    mat4 I = mat4::identity();
    vec4 v{1, 2, 3, 1};
    vec4 r = I * v;
    CRYOGEN_CHECK(approx_eq(r.x, 1) && approx_eq(r.y, 2) && approx_eq(r.z, 3) && approx_eq(r.w, 1));

    mat4 T = translate({5.f, 0.f, 0.f});
    vec4 p = T * vec4{0,0,0,1};
    CRYOGEN_CHECK(approx_eq(p.x, 5.f) && approx_eq(p.y, 0.f) && approx_eq(p.z, 0.f));
}

static void perspective_clip() {
    // perspective_vk should produce -1..1 z in NDC after divide by w; near
    // plane maps to z=0, far to z=1 (Vulkan reversed-Z is opt-in elsewhere).
    mat4 P = perspective_vk(1.0f, 1.0f, 0.1f, 100.0f);
    vec4 near = P * vec4{0, 0, -0.1f, 1.f};
    vec4 far  = P * vec4{0, 0, -100.f, 1.f};
    CRYOGEN_CHECK(near.w > 0.f && far.w > 0.f);
    const float zn = near.z / near.w;
    const float zf = far.z  / far.w;
    CRYOGEN_CHECK(approx_eq(zn, 0.f, 1e-3f));
    CRYOGEN_CHECK(approx_eq(zf, 1.f, 1e-3f));
}

static void quat_round_trip() {
    quat q = quat::from_axis_angle({0,1,0}, 1.5707963f); // 90° about Y
    vec3 r = rotate(q, vec3{1,0,0});
    CRYOGEN_CHECK(approx_eq(r.x,  0.f, 1e-4f));
    CRYOGEN_CHECK(approx_eq(r.y,  0.f, 1e-4f));
    CRYOGEN_CHECK(approx_eq(r.z, -1.f, 1e-4f));

    // identity * X = X
    vec3 x = rotate(quat::identity(), vec3{2.f, -1.f, 3.f});
    CRYOGEN_CHECK(approx_eq(x.x, 2.f) && approx_eq(x.y, -1.f) && approx_eq(x.z, 3.f));
}

static void transform_combine() {
    Transform parent;
    parent.position = {1, 2, 3};
    Transform child;
    child.position = {0, 0, 1};
    Transform combined = parent.combine(child);
    CRYOGEN_CHECK(approx_eq(combined.position.x, 1.f));
    CRYOGEN_CHECK(approx_eq(combined.position.y, 2.f));
    CRYOGEN_CHECK(approx_eq(combined.position.z, 4.f));
}

static void aabb_overlap() {
    Aabb a{{0,0,0}, {1,1,1}};
    Aabb b{{0.5f, 0.5f, 0.5f}, {2,2,2}};
    Aabb c{{2,2,2}, {3,3,3}};
    CRYOGEN_CHECK(overlaps(a, b));
    CRYOGEN_CHECK(!overlaps(a, c));
}

int main() {
    vec_basics();
    mat_identity_and_compose();
    perspective_clip();
    quat_round_trip();
    transform_combine();
    aabb_overlap();
    std::printf("test_math: OK\n");
    return 0;
}
