#pragma once
//
// cryogen/scene/components.hpp
// Standard component types shared across systems (render, physics, net).
//

#include <string>

#include "cryogen/core/types.hpp"
#include "cryogen/math/transform.hpp"
#include "cryogen/math/vec.hpp"

namespace cryogen::scene {

// Spatial transform. Owned by every entity that has a position in the world.
struct TransformComponent {
    math::Transform local{};
    // Pre-computed world matrix; refreshed by Scene::update_world_transforms().
    math::mat4      world = math::mat4::identity();
};

// Reference to a mesh resource owned by AssetManager. The id is content-addressed.
struct MeshComponent {
    u32 mesh_id = 0;
    u32 material_id = 0;
    bool visible = true;
};

struct NameComponent {
    std::string name;
};

// Integer health used by gameplay code; replicated by default.
struct HealthComponent {
    i32 hp = 100;
    i32 max_hp = 100;
};

// Marker that the entity should be replicated by the net layer.
struct NetReplicated {
    u32 net_id = 0;          // assigned by server on spawn
    u8  channel = 0;         // 0=unreliable snapshot, 1=reliable RPC
};

// A directional light, used by both the raster and ray-traced passes.
struct DirectionalLight {
    math::vec3 direction { 0.3f, -1.0f, 0.2f };
    math::vec3 color     { 1.0f,  1.0f, 1.0f };
    float      intensity = 3.0f;
};

} // namespace cryogen::scene
