#include "cryogen/ecs/world.hpp"

namespace cryogen::ecs {

World::World() = default;

Entity World::create() {
    u32 idx;
    if (!free_indices_.empty()) {
        idx = free_indices_.back();
        free_indices_.pop_back();
    } else {
        idx = static_cast<u32>(slots_.size());
        slots_.push_back(Slot{});
    }
    Slot& s = slots_[idx];
    s.alive = true;
    ++alive_count_;
    return make_entity(idx, s.generation);
}

void World::destroy(Entity e) {
    const u32 idx = e.index();
    if (idx >= slots_.size()) return;
    Slot& s = slots_[idx];
    if (!s.alive || s.generation != e.generation()) return;

    // Drop components from every pool.
    for (auto& [tid, pool] : pools_) {
        pool->remove(idx);
    }

    s.alive = false;
    s.generation = static_cast<u8>(s.generation + 1);   // invalidate stale handles
    free_indices_.push_back(idx);
    --alive_count_;
}

bool World::alive(Entity e) const {
    const u32 idx = e.index();
    if (idx >= slots_.size()) return false;
    const Slot& s = slots_[idx];
    return s.alive && s.generation == e.generation();
}

std::vector<Entity> World::entities() const {
    std::vector<Entity> out;
    out.reserve(alive_count_);
    for (u32 i = 0; i < slots_.size(); ++i) {
        if (slots_[i].alive) out.push_back(make_entity(i, slots_[i].generation));
    }
    return out;
}

} // namespace cryogen::ecs
