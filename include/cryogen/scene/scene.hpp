#pragma once
//
// cryogen/scene/scene.hpp
// A "Scene" is a runtime container that bundles:
//   - an ECS World
//   - a PhysicsWorld
//   - the active Camera
//   - a directional light (the simplest "global env")
// Plus a couple of convenience helpers that all three apps (editor, runtime,
// server) need: spawn a default sandbox, refresh world transforms.
//

#include <memory>

#include "cryogen/ecs/world.hpp"
#include "cryogen/physics/physics_world.hpp"
#include "cryogen/render/camera.hpp"
#include "cryogen/scene/components.hpp"

namespace cryogen::assets { class AssetManager; }

namespace cryogen::scene {

class Scene {
public:
    Scene();

    ecs::World&            world()        { return world_; }
    const ecs::World&      world()  const { return world_; }
    physics::PhysicsWorld& physics()      { return phys_; }
    render::Camera&        camera()       { return camera_; }
    DirectionalLight&      sun()          { return sun_; }
    const DirectionalLight& sun() const   { return sun_; }

    // Update world matrices (TransformComponent::world) from local transforms.
    void update_world_transforms();

    // Spawn helpers used by the editor and tests.
    ecs::Entity spawn_static_floor(assets::AssetManager& am);
    ecs::Entity spawn_dynamic_box (assets::AssetManager& am, math::vec3 pos);
    ecs::Entity spawn_dynamic_ball(assets::AssetManager& am, math::vec3 pos, float radius = 0.5f);

private:
    ecs::World             world_;
    physics::PhysicsWorld  phys_;
    render::Camera         camera_;
    DirectionalLight       sun_;
};

} // namespace cryogen::scene
