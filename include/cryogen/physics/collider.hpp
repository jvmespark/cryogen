#pragma once
//
// cryogen/physics/collider.hpp
// Convex collider primitives: sphere, box (oriented), plane (infinite).
// Used by the broadphase (AABB) and narrowphase (specific contact tests).
//

#include "cryogen/math/aabb.hpp"
#include "cryogen/math/quat.hpp"
#include "cryogen/math/transform.hpp"
#include "cryogen/math/vec.hpp"

namespace cryogen::physics {

enum class ShapeType : u8 { Sphere = 0, Box = 1, Plane = 2 };

struct ColliderComponent {
    ShapeType  type = ShapeType::Sphere;

    // Sphere
    float      radius = 0.5f;

    // Box (half-extents, oriented by entity rotation)
    math::vec3 half_extents { 0.5f, 0.5f, 0.5f };

    // Plane (normal in world space; offset = dot(normal, point on plane))
    math::vec3 plane_normal { 0.f, 1.f, 0.f };
    float      plane_offset = 0.f;

    // Material parameters
    float      restitution  = 0.2f;
    float      friction     = 0.5f;

    // Computed each step from the current transform.
    math::Aabb world_aabb{};
};

inline ColliderComponent make_sphere(float r,
                                     float restitution = 0.2f,
                                     float friction = 0.5f) {
    ColliderComponent c{};
    c.type = ShapeType::Sphere;
    c.radius = r;
    c.restitution = restitution;
    c.friction = friction;
    return c;
}

inline ColliderComponent make_box(math::vec3 half,
                                  float restitution = 0.2f,
                                  float friction = 0.5f) {
    ColliderComponent c{};
    c.type = ShapeType::Box;
    c.half_extents = half;
    c.restitution = restitution;
    c.friction = friction;
    return c;
}

inline ColliderComponent make_plane(math::vec3 n, float offset) {
    ColliderComponent c{};
    c.type = ShapeType::Plane;
    c.plane_normal = math::normalize(n);
    c.plane_offset = offset;
    return c;
}

} // namespace cryogen::physics
