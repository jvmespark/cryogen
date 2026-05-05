//
// cryogen/scene/serializer.cpp
//
#include "cryogen/scene/serializer.hpp"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "cryogen/assets/asset_manager.hpp"
#include "cryogen/core/log.hpp"
#include "cryogen/scene/scene.hpp"

using nlohmann::json;

namespace cryogen::scene {

namespace {

json vec3j(math::vec3 v) { return json::array({v.x, v.y, v.z}); }
json vec4j(math::vec4 v) { return json::array({v.x, v.y, v.z, v.w}); }
json quatj(math::quat q) { return json::array({q.x, q.y, q.z, q.w}); }

math::vec3 read_v3(const json& j, math::vec3 fallback = {}) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
}
math::vec4 read_v4(const json& j, math::vec4 fallback = {}) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() };
}
math::quat read_q(const json& j) {
    if (!j.is_array() || j.size() < 4) return math::quat::identity();
    return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() };
}

} // namespace

bool SceneIO::save(const Scene& scene_in, const std::string& path) {
    Scene& scene = const_cast<Scene&>(scene_in);  // ECS iteration is non-const
    json doc;
    doc["version"] = 1;

    const auto& cam = scene.camera();
    doc["camera"] = {
        {"position", vec3j(cam.position)},
        {"yaw",      cam.yaw},
        {"pitch",    cam.pitch},
        {"fov_y",    cam.fov_y_radians},
    };

    doc["sun"] = {
        {"direction", vec3j(scene.sun().direction)},
        {"color",     vec3j(scene.sun().color)},
        {"intensity", scene.sun().intensity},
    };

    json ents = json::array();
    auto& world = scene.world();
    for (auto e : world.entities()) {
        json ej;
        if (auto* nc = world.try_get<NameComponent>(e))
            ej["name"] = nc->name;

        if (auto* tc = world.try_get<TransformComponent>(e)) {
            ej["transform"] = {
                {"position", vec3j(tc->local.position)},
                {"rotation", quatj(tc->local.rotation)},
                {"scale",    vec3j(tc->local.scale)},
            };
        }
        if (auto* mc = world.try_get<MeshComponent>(e)) {
            ej["mesh"] = { {"mesh_id", mc->mesh_id},
                           {"material_id", mc->material_id},
                           {"visible", mc->visible} };
        }
        if (auto* cc = world.try_get<physics::ColliderComponent>(e)) {
            json c;
            c["type"] = static_cast<int>(cc->type);
            c["radius"] = cc->radius;
            c["half_extents"] = vec3j(cc->half_extents);
            c["plane_normal"] = vec3j(cc->plane_normal);
            c["plane_offset"] = cc->plane_offset;
            c["restitution"] = cc->restitution;
            c["friction"] = cc->friction;
            ej["collider"] = c;
        }
        if (auto* rb = world.try_get<physics::RigidBodyComponent>(e)) {
            ej["rigid_body"] = {
                {"type",            static_cast<int>(rb->type)},
                {"inv_mass",        rb->inv_mass},
                {"inv_inertia",     vec3j(rb->inv_inertia)},
                {"linear_velocity", vec3j(rb->linear_velocity)},
                {"angular_velocity",vec3j(rb->angular_velocity)},
                {"linear_damping",  rb->linear_damping},
                {"angular_damping", rb->angular_damping},
            };
        }
        ents.push_back(ej);
    }
    doc["entities"] = ents;

    std::ofstream f(path);
    if (!f.is_open()) {
        CRYOGEN_ERROR("scene", "Cannot open '", path, "' for writing");
        return false;
    }
    f << doc.dump(2);
    return true;
}

bool SceneIO::load(Scene& scene, assets::AssetManager& /*am*/, const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        CRYOGEN_ERROR("scene", "Cannot open '", path, "' for reading");
        return false;
    }
    std::stringstream ss; ss << f.rdbuf();
    json doc;
    try { doc = json::parse(ss.str()); }
    catch (const std::exception& e) {
        CRYOGEN_ERROR("scene", "JSON parse error: ", e.what());
        return false;
    }

    if (!doc.contains("version") || doc["version"].get<int>() != 1) {
        CRYOGEN_ERROR("scene", "Unsupported scene version");
        return false;
    }

    if (doc.contains("camera")) {
        const auto& c = doc["camera"];
        scene.camera().position = read_v3(c["position"], scene.camera().position);
        if (c.contains("yaw"))   scene.camera().yaw   = c["yaw"].get<float>();
        if (c.contains("pitch")) scene.camera().pitch = c["pitch"].get<float>();
        if (c.contains("fov_y")) scene.camera().fov_y_radians = c["fov_y"].get<float>();
        scene.camera().update_basis();
    }
    if (doc.contains("sun")) {
        const auto& s = doc["sun"];
        scene.sun().direction = read_v3(s["direction"], scene.sun().direction);
        scene.sun().color     = read_v3(s["color"],     scene.sun().color);
        if (s.contains("intensity")) scene.sun().intensity = s["intensity"].get<float>();
    }

    auto& world = scene.world();
    for (const auto& ej : doc["entities"]) {
        auto e = world.create();
        if (ej.contains("name"))
            world.add<NameComponent>(e, NameComponent{ ej["name"].get<std::string>() });

        if (ej.contains("transform")) {
            const auto& t = ej["transform"];
            TransformComponent tc;
            tc.local.position = read_v3(t["position"]);
            tc.local.rotation = read_q (t["rotation"]);
            tc.local.scale    = read_v3(t["scale"], {1,1,1});
            world.add<TransformComponent>(e, tc);
        }
        if (ej.contains("mesh")) {
            const auto& m = ej["mesh"];
            MeshComponent mc;
            mc.mesh_id     = m.value("mesh_id",     0u);
            mc.material_id = m.value("material_id", 0u);
            mc.visible     = m.value("visible",     true);
            world.add<MeshComponent>(e, mc);
        }
        if (ej.contains("collider")) {
            const auto& c = ej["collider"];
            physics::ColliderComponent cc;
            cc.type         = static_cast<physics::ShapeType>(c.value("type", 0));
            cc.radius       = c.value("radius", 0.5f);
            cc.half_extents = read_v3(c["half_extents"], {0.5f,0.5f,0.5f});
            cc.plane_normal = read_v3(c["plane_normal"], {0,1,0});
            cc.plane_offset = c.value("plane_offset", 0.f);
            cc.restitution  = c.value("restitution", 0.2f);
            cc.friction     = c.value("friction",    0.5f);
            world.add<physics::ColliderComponent>(e, cc);
        }
        if (ej.contains("rigid_body")) {
            const auto& r = ej["rigid_body"];
            physics::RigidBodyComponent rb;
            rb.type             = static_cast<physics::BodyType>(r.value("type", 1));
            rb.inv_mass         = r.value("inv_mass", 1.f);
            rb.inv_inertia      = read_v3(r["inv_inertia"], {1,1,1});
            rb.linear_velocity  = read_v3(r["linear_velocity"]);
            rb.angular_velocity = read_v3(r["angular_velocity"]);
            rb.linear_damping   = r.value("linear_damping",  0.01f);
            rb.angular_damping  = r.value("angular_damping", 0.05f);
            world.add<physics::RigidBodyComponent>(e, rb);
        }
    }
    return true;
}

} // namespace cryogen::scene
