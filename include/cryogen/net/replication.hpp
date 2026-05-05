#pragma once
//
// cryogen/net/replication.hpp
// State replicated server -> client every snapshot tick.
//
// The server collects each replicated entity's components into a packed
// struct, deltas it against the previously-acked snapshot for that client,
// and sends only the diff. Clients reconstruct by applying received deltas
// onto their last accepted snapshot.
//

#include <vector>

#include "cryogen/core/types.hpp"
#include "cryogen/math/quat.hpp"
#include "cryogen/math/vec.hpp"
#include "cryogen/net/message.hpp"

namespace cryogen::ecs { class World; }

namespace cryogen::net {

struct ReplicatedState {
    u32         net_id  = 0;       // canonical server-assigned identifier
    u32         flags   = 0;       // bit 0: alive; bit 1: just-spawned
    math::vec3  position{0,0,0};
    math::quat  rotation = math::quat::identity();
    math::vec3  linear_velocity {0,0,0};
    i32         hp = 0;
};

struct InputCommand {
    u32        sequence = 0;       // client-side prediction sequence
    f32        dt = 1.f / 60.f;
    math::vec3 move {0,0,0};       // player-space wishdir
    math::vec3 look {0,0,0};       // pitch/yaw deltas
    u32        buttons = 0;        // bitfield
};

void serialize_state(BinaryWriter& w, const std::vector<ReplicatedState>& s);
std::vector<ReplicatedState> deserialize_state(BinaryReader& r);

void serialize_input(BinaryWriter& w, const InputCommand& c);
InputCommand deserialize_input(BinaryReader& r);

// Pull replicated state out of an ECS world (entities tagged NetReplicated).
std::vector<ReplicatedState> snapshot_world(ecs::World& world);

// Apply a replicated state set onto a client world. Entities not yet present
// are spawned; existing ones are updated in place.
void apply_snapshot(ecs::World& world, const std::vector<ReplicatedState>& states);

} // namespace cryogen::net
