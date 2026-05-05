//
// cryogen/net/replication.cpp
//
#include "cryogen/net/replication.hpp"

#include <unordered_map>

#include "cryogen/ecs/world.hpp"
#include "cryogen/scene/components.hpp"
#include "cryogen/physics/rigid_body.hpp"

namespace cryogen::net {

void serialize_state(BinaryWriter& w, const std::vector<ReplicatedState>& s) {
    w.write_u32(static_cast<u32>(s.size()));
    for (const auto& st : s) {
        w.write_u32(st.net_id);
        w.write_u32(st.flags);
        w.write_f32(st.position.x); w.write_f32(st.position.y); w.write_f32(st.position.z);
        w.write_f32(st.rotation.x); w.write_f32(st.rotation.y);
        w.write_f32(st.rotation.z); w.write_f32(st.rotation.w);
        w.write_f32(st.linear_velocity.x);
        w.write_f32(st.linear_velocity.y);
        w.write_f32(st.linear_velocity.z);
        w.write_u32(static_cast<u32>(st.hp));
    }
}

std::vector<ReplicatedState> deserialize_state(BinaryReader& r) {
    std::vector<ReplicatedState> out;
    const u32 n = r.read_u32();
    out.reserve(n);
    for (u32 i = 0; i < n && r.ok(); ++i) {
        ReplicatedState s;
        s.net_id = r.read_u32();
        s.flags  = r.read_u32();
        s.position.x = r.read_f32(); s.position.y = r.read_f32(); s.position.z = r.read_f32();
        s.rotation.x = r.read_f32(); s.rotation.y = r.read_f32();
        s.rotation.z = r.read_f32(); s.rotation.w = r.read_f32();
        s.linear_velocity.x = r.read_f32();
        s.linear_velocity.y = r.read_f32();
        s.linear_velocity.z = r.read_f32();
        s.hp = static_cast<i32>(r.read_u32());
        out.push_back(s);
    }
    return out;
}

void serialize_input(BinaryWriter& w, const InputCommand& c) {
    w.write_u32(c.sequence);
    w.write_f32(c.dt);
    w.write_f32(c.move.x); w.write_f32(c.move.y); w.write_f32(c.move.z);
    w.write_f32(c.look.x); w.write_f32(c.look.y); w.write_f32(c.look.z);
    w.write_u32(c.buttons);
}

InputCommand deserialize_input(BinaryReader& r) {
    InputCommand c;
    c.sequence = r.read_u32();
    c.dt       = r.read_f32();
    c.move.x   = r.read_f32(); c.move.y = r.read_f32(); c.move.z = r.read_f32();
    c.look.x   = r.read_f32(); c.look.y = r.read_f32(); c.look.z = r.read_f32();
    c.buttons  = r.read_u32();
    return c;
}

std::vector<ReplicatedState> snapshot_world(ecs::World& world) {
    using cryogen::scene::TransformComponent;
    using cryogen::scene::NetReplicated;
    using cryogen::scene::HealthComponent;

    std::vector<ReplicatedState> out;
    out.reserve(world.entity_count());

    world.each<NetReplicated, TransformComponent>(
        [&](ecs::Entity e, NetReplicated& nr, TransformComponent& tc) {
            ReplicatedState s;
            s.net_id   = nr.net_id;
            s.flags    = 1; // alive
            s.position = tc.local.position;
            s.rotation = tc.local.rotation;
            if (auto* rb = world.try_get<physics::RigidBodyComponent>(e))
                s.linear_velocity = rb->linear_velocity;
            if (auto* hp = world.try_get<HealthComponent>(e))
                s.hp = hp->hp;
            out.push_back(s);
        });
    return out;
}

void apply_snapshot(ecs::World& world, const std::vector<ReplicatedState>& states) {
    using cryogen::scene::TransformComponent;
    using cryogen::scene::NetReplicated;
    using cryogen::scene::HealthComponent;

    // Build an index: net_id -> Entity (linear scan; small N).
    std::unordered_map<u32, ecs::Entity> by_net;
    world.each<NetReplicated>(
        [&](ecs::Entity e, NetReplicated& nr) { by_net[nr.net_id] = e; });

    for (const auto& s : states) {
        auto it = by_net.find(s.net_id);
        ecs::Entity e;
        if (it == by_net.end()) {
            e = world.create();
            world.add<NetReplicated>(e, NetReplicated{ s.net_id, 0 });
            world.add<TransformComponent>(e);
        } else {
            e = it->second;
        }
        auto* tc = world.try_get<TransformComponent>(e);
        if (tc) { tc->local.position = s.position; tc->local.rotation = s.rotation; }
        if (auto* rb = world.try_get<physics::RigidBodyComponent>(e))
            rb->linear_velocity = s.linear_velocity;
        if (auto* hp = world.try_get<HealthComponent>(e))
            hp->hp = s.hp;
    }
}

} // namespace cryogen::net
