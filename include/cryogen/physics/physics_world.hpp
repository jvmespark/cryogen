#pragma once
//
// cryogen/physics/physics_world.hpp
// Rigid-body physics simulation:
//
//   1. integrate forces  -> tentative velocities (semi-implicit Euler)
//   2. broadphase        -> sweep-and-prune on world AABBs
//   3. narrowphase       -> contact generation per shape pair
//   4. solver            -> sequential impulses (configurable iterations)
//   5. integrate vel.    -> positions, sync to TransformComponent
//

#include <vector>

#include "cryogen/core/types.hpp"
#include "cryogen/ecs/entity.hpp"
#include "cryogen/math/aabb.hpp"
#include "cryogen/math/vec.hpp"
#include "cryogen/physics/collider.hpp"
#include "cryogen/physics/rigid_body.hpp"

namespace cryogen::ecs { class World; }

namespace cryogen::physics {

struct ContactManifold {
    ecs::Entity a;
    ecs::Entity b;
    math::vec3  normal{0,0,0};   // points from A to B
    math::vec3  point {0,0,0};   // world space
    float       depth = 0.f;     // positive = penetration
    float       restitution = 0.f;
    float       friction = 0.f;
    // Solver scratch state.
    float       normal_impulse  = 0.f;
    float       tangent_impulse = 0.f;
};

struct PhysicsConfig {
    math::vec3 gravity { 0.f, -9.81f, 0.f };
    int        solver_iterations = 8;
    float      penetration_slop  = 0.005f;
    float      baumgarte         = 0.2f;
    float      sleep_velocity_sq = 1e-3f;   // unused, reserved
};

class PhysicsWorld {
public:
    explicit PhysicsWorld(PhysicsConfig cfg = {}) : cfg_(cfg) {}

    void step(ecs::World& world, float dt);
    const PhysicsConfig& config() const { return cfg_; }
    void set_config(PhysicsConfig cfg) { cfg_ = cfg; }

    const std::vector<ContactManifold>& contacts() const { return contacts_; }

private:
    void update_world_aabbs(ecs::World& world);
    void broadphase_sap(ecs::World& world,
                        std::vector<std::pair<ecs::Entity, ecs::Entity>>& pairs);
    void narrowphase(ecs::World& world,
                     const std::vector<std::pair<ecs::Entity, ecs::Entity>>& pairs);
    void integrate_velocities(ecs::World& world, float dt);
    void integrate_positions(ecs::World& world, float dt);
    void solve(ecs::World& world, float dt);

    PhysicsConfig                                 cfg_;
    std::vector<ContactManifold>                  contacts_;
};

} // namespace cryogen::physics
