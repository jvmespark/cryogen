//
// cryogen/scene/scene.cpp
//
#include "cryogen/scene/scene.hpp"

#include "cryogen/assets/asset_manager.hpp"
#include "cryogen/assets/mesh_loader.hpp"

namespace cryogen::scene {

Scene::Scene() = default;

void Scene::update_world_transforms() {
    world_.each<TransformComponent>(
        [](ecs::Entity, TransformComponent& tc) {
            tc.world = tc.local.matrix();
        });
}

ecs::Entity Scene::spawn_static_floor(assets::AssetManager& am) {
    const u32 mesh_id = am.register_mesh(assets::make_plane(20.f), "primitive:floor");
    const u32 mat_id  = am.register_material({.base_color={0.6f,0.6f,0.65f,1.f}, .roughness=0.9f},
                                              "primitive:floor_material");

    auto e = world_.create();
    auto& tc = world_.add<TransformComponent>(e);
    tc.local.position = {0,0,0};

    MeshComponent mc; mc.mesh_id = mesh_id; mc.material_id = mat_id;
    world_.add<MeshComponent>(e, mc);

    world_.add<NameComponent>(e, {"Floor"});
    world_.add<physics::ColliderComponent>(e, physics::make_plane({0,1,0}, 0.f));
    world_.add<physics::RigidBodyComponent>(e, physics::static_body());
    return e;
}

ecs::Entity Scene::spawn_dynamic_box(assets::AssetManager& am, math::vec3 pos) {
    const u32 mesh_id = am.register_mesh(assets::make_cube(1.f), "primitive:cube");
    const u32 mat_id  = am.register_material({.base_color={0.85f,0.32f,0.18f,1.f}, .metallic=0.05f, .roughness=0.45f},
                                              "primitive:cube_material");

    auto e = world_.create();
    auto& tc = world_.add<TransformComponent>(e);
    tc.local.position = pos;

    MeshComponent mc; mc.mesh_id = mesh_id; mc.material_id = mat_id;
    world_.add<MeshComponent>(e, mc);

    world_.add<NameComponent>(e, {"Cube"});
    world_.add<physics::ColliderComponent>(e, physics::make_box({0.5f,0.5f,0.5f}));
    world_.add<physics::RigidBodyComponent>(e, physics::dynamic_body(1.f, {6.f,6.f,6.f}));
    return e;
}

ecs::Entity Scene::spawn_dynamic_ball(assets::AssetManager& am, math::vec3 pos, float radius) {
    const u32 mesh_id = am.register_mesh(assets::make_sphere(radius), "primitive:sphere");
    const u32 mat_id  = am.register_material({.base_color={0.18f,0.65f,0.85f,1.f}, .metallic=0.7f, .roughness=0.25f},
                                              "primitive:sphere_material");

    auto e = world_.create();
    auto& tc = world_.add<TransformComponent>(e);
    tc.local.position = pos;

    MeshComponent mc; mc.mesh_id = mesh_id; mc.material_id = mat_id;
    world_.add<MeshComponent>(e, mc);

    world_.add<NameComponent>(e, {"Ball"});
    world_.add<physics::ColliderComponent>(e, physics::make_sphere(radius));
    world_.add<physics::RigidBodyComponent>(e, physics::dynamic_body(1.f, {2.5f,2.5f,2.5f}));
    return e;
}

} // namespace cryogen::scene
