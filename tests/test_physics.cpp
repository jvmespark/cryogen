//
// tests/test_physics.cpp
// Black-box physics smoke tests.
//
#include "test_util.hpp"

#include "cryogen/ecs/world.hpp"
#include "cryogen/physics/collider.hpp"
#include "cryogen/physics/physics_world.hpp"
#include "cryogen/physics/rigid_body.hpp"
#include "cryogen/scene/components.hpp"

using namespace cryogen;
using namespace cryogen::scene;

static void sphere_settles_on_plane() {
    ecs::World w;
    physics::PhysicsWorld phys;

    // Static plane y = 0.
    auto floor = w.create();
    w.add<TransformComponent>(floor);
    w.add<physics::ColliderComponent>(floor, physics::make_plane({0,1,0}, 0.f));
    w.add<physics::RigidBodyComponent>(floor, physics::static_body());

    // Sphere @ y=3.
    auto ball = w.create();
    TransformComponent btc; btc.local.position = {0.f, 3.f, 0.f};
    w.add<TransformComponent>(ball, btc);
    w.add<physics::ColliderComponent>(ball, physics::make_sphere(0.5f));
    auto rb = physics::dynamic_body(1.f); rb.linear_damping = 0.05f;
    w.add<physics::RigidBodyComponent>(ball, rb);

    // Step for ~3 seconds at 60Hz.
    for (int i = 0; i < 180; ++i) phys.step(w, 1.f/60.f);

    auto* tc = w.try_get<TransformComponent>(ball);
    auto* rbp= w.try_get<physics::RigidBodyComponent>(ball);
    CRYOGEN_CHECK(tc && rbp);
    // It shouldn't have fallen through the floor.
    CRYOGEN_CHECK(tc->local.position.y > 0.0f);
    CRYOGEN_CHECK(tc->local.position.y < 0.7f);     // close to resting (radius=0.5)
    // And it should be roughly at rest.
    CRYOGEN_CHECK(std::fabs(rbp->linear_velocity.y) < 0.5f);
}

static void two_spheres_separate() {
    ecs::World w;
    physics::PhysicsWorld phys;

    auto a = w.create();
    TransformComponent ta; ta.local.position = {0.f, 1.f, 0.f};
    w.add<TransformComponent>(a, ta);
    w.add<physics::ColliderComponent>(a, physics::make_sphere(0.5f));
    w.add<physics::RigidBodyComponent>(a, physics::dynamic_body(1.f));

    auto b = w.create();
    TransformComponent tb; tb.local.position = {0.6f, 1.f, 0.f};
    w.add<TransformComponent>(b, tb);
    w.add<physics::ColliderComponent>(b, physics::make_sphere(0.5f));
    w.add<physics::RigidBodyComponent>(b, physics::dynamic_body(1.f));

    physics::PhysicsConfig cfg = phys.config();
    cfg.gravity = {0,0,0};
    phys.set_config(cfg);

    for (int i = 0; i < 60; ++i) phys.step(w, 1.f/60.f);

    auto* tap = w.try_get<TransformComponent>(a);
    auto* tbp = w.try_get<TransformComponent>(b);
    const float dx = tbp->local.position.x - tap->local.position.x;
    CRYOGEN_CHECK(dx >= 1.0f - 0.05f);    // pushed apart to roughly r+r
}

int main() {
    sphere_settles_on_plane();
    two_spheres_separate();
    std::printf("test_physics: OK\n");
    return 0;
}
