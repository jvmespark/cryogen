#pragma once
//
// cryogen/ecs/entity.hpp
// Versioned 32-bit entity handle: 24 bits index + 8 bits generation.
//

#include "cryogen/core/types.hpp"

namespace cryogen::ecs {

struct Entity {
    EntityId id = kInvalidEntity;

    constexpr u32 index()      const { return id & 0x00FFFFFFu; }
    constexpr u8  generation() const { return static_cast<u8>((id >> 24) & 0xFFu); }
    constexpr bool valid()     const { return id != kInvalidEntity; }

    constexpr bool operator==(const Entity& o) const { return id == o.id; }
    constexpr bool operator!=(const Entity& o) const { return id != o.id; }
};

constexpr Entity make_entity(u32 index, u8 generation) {
    return Entity{ (static_cast<u32>(generation) << 24) | (index & 0x00FFFFFFu) };
}

} // namespace cryogen::ecs
