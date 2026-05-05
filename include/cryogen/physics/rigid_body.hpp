#pragma once
//
// cryogen/physics/rigid_body.hpp
// Rigid body component used by PhysicsWorld. Inverse mass form lets us model
// a static body trivially (inv_mass == 0).
//

#include "cryogen/math/quat.hpp"
#include "cryogen/math/vec.hpp"

namespace cryogen::physics {

enum class BodyType : u8 {
    Static    = 0,    // immovable, infinite mass
    Dynamic   = 1,    // simulated
    Kinematic = 2,    // moved externally (e.g. by net snapshots)
};

struct RigidBodyComponent {
    BodyType  type = BodyType::Dynamic;

    // Inverse mass in kg^-1. Zero for static/kinematic bodies.
    float     inv_mass = 1.f;

    // Inverse local-space inertia tensor diagonal. Treat all bodies as
    // box-like for simplicity; the solver only uses the diagonal entries.
    math::vec3 inv_inertia { 1.f, 1.f, 1.f };

    // Linear and angular state.
    math::vec3 linear_velocity  {0,0,0};
    math::vec3 angular_velocity {0,0,0};

    // Accumulators (cleared each step after integration).
    math::vec3 force  {0,0,0};
    math::vec3 torque {0,0,0};

    // Damping (drag-like).
    float linear_damping  = 0.01f;
    float angular_damping = 0.05f;

    // Awake flag for sleeping (not strictly used, but reserved for the
    // SAP broadphase optimisation path).
    bool awake = true;
};

// Convenience constructors -------------------------------------------------
inline RigidBodyComponent dynamic_body(float mass,
                                       math::vec3 inv_inertia = {1,1,1}) {
    RigidBodyComponent rb;
    rb.type     = BodyType::Dynamic;
    rb.inv_mass = (mass > 0.f) ? 1.f / mass : 0.f;
    rb.inv_inertia = inv_inertia;
    return rb;
}
inline RigidBodyComponent static_body() {
    RigidBodyComponent rb;
    rb.type     = BodyType::Static;
    rb.inv_mass = 0.f;
    rb.inv_inertia = {0,0,0};
    return rb;
}

} // namespace cryogen::physics
