#pragma once
//
// cryogen/ecs/world.hpp
// Main ECS facade. Owns entity allocation and a heterogeneous map of
// component pools. Iteration is deterministic in entity-index order.
//

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cryogen/ecs/component.hpp"
#include "cryogen/ecs/entity.hpp"

namespace cryogen::ecs {

class World {
public:
    World();

    // Entity lifecycle ----------------------------------------------------
    Entity create();
    void   destroy(Entity e);
    bool   alive(Entity e) const;
    usize  entity_count() const { return alive_count_; }

    // Component access ----------------------------------------------------
    template <class T> T& add(Entity e, T value = {});
    template <class T> void remove(Entity e);
    template <class T> bool has(Entity e) const;
    template <class T> T*       try_get(Entity e);
    template <class T> const T* try_get(Entity e) const;
    template <class T> T&       get(Entity e);
    template <class T> const T& get(Entity e) const;

    // Iteration -----------------------------------------------------------
    template <class T, class Fn> void each(Fn&& fn);
    template <class T, class U, class Fn> void each(Fn&& fn);  // join

    // Pool query (internal use & replication) -----------------------------
    template <class T> ComponentPool<T>* pool();
    template <class T> const ComponentPool<T>* pool() const;

    // Linear list of currently-alive entity handles.
    std::vector<Entity> entities() const;

private:
    template <class T> ComponentPool<T>& ensure_pool();

    struct Slot {
        u8   generation = 0;
        bool alive      = false;
    };

    std::vector<Slot>             slots_;
    std::vector<u32>              free_indices_;
    usize                         alive_count_ = 0;

    std::unordered_map<TypeId, std::unique_ptr<IComponentPool>> pools_;
};

// ---- template impls -------------------------------------------------

template <class T>
ComponentPool<T>& World::ensure_pool() {
    const TypeId tid = type_id_for<T>();
    auto it = pools_.find(tid);
    if (it == pools_.end()) {
        auto p = std::make_unique<ComponentPool<T>>();
        ComponentPool<T>& ref = *p;
        pools_.emplace(tid, std::move(p));
        return ref;
    }
    return *static_cast<ComponentPool<T>*>(it->second.get());
}

template <class T>
ComponentPool<T>* World::pool() {
    auto it = pools_.find(type_id_for<T>());
    return it == pools_.end() ? nullptr
                              : static_cast<ComponentPool<T>*>(it->second.get());
}

template <class T>
const ComponentPool<T>* World::pool() const {
    auto it = pools_.find(type_id_for<T>());
    return it == pools_.end() ? nullptr
                              : static_cast<const ComponentPool<T>*>(it->second.get());
}

template <class T>
T& World::add(Entity e, T value) {
    return ensure_pool<T>().add(e.index(), std::move(value));
}

template <class T>
void World::remove(Entity e) {
    if (auto* p = pool<T>()) p->remove(e.index());
}

template <class T>
bool World::has(Entity e) const {
    auto* p = pool<T>();
    return p && p->has(e.index());
}

template <class T>
T* World::try_get(Entity e) {
    auto* p = pool<T>();
    return p ? p->try_get(e.index()) : nullptr;
}

template <class T>
const T* World::try_get(Entity e) const {
    auto* p = pool<T>();
    return p ? p->try_get(e.index()) : nullptr;
}

template <class T>
T& World::get(Entity e) { return pool<T>()->get(e.index()); }

template <class T>
const T& World::get(Entity e) const { return pool<T>()->get(e.index()); }

template <class T, class Fn>
void World::each(Fn&& fn) {
    auto* p = pool<T>();
    if (!p) return;
    auto& ents = p->entities();
    auto& data = p->data();
    for (usize i = 0; i < ents.size(); ++i) {
        Entity e = make_entity(ents[i], slots_[ents[i]].generation);
        fn(e, data[i]);
    }
}

template <class T, class U, class Fn>
void World::each(Fn&& fn) {
    auto* pt = pool<T>();
    auto* pu = pool<U>();
    if (!pt || !pu) return;

    // Iterate the smaller pool; pick the partner via random access.
    if (pt->size() <= pu->size()) {
        auto& ents = pt->entities();
        auto& data = pt->data();
        for (usize i = 0; i < ents.size(); ++i) {
            U* u = pu->try_get(ents[i]);
            if (!u) continue;
            Entity e = make_entity(ents[i], slots_[ents[i]].generation);
            fn(e, data[i], *u);
        }
    } else {
        auto& ents = pu->entities();
        auto& data = pu->data();
        for (usize i = 0; i < ents.size(); ++i) {
            T* t = pt->try_get(ents[i]);
            if (!t) continue;
            Entity e = make_entity(ents[i], slots_[ents[i]].generation);
            fn(e, *t, data[i]);
        }
    }
}

} // namespace cryogen::ecs
