#pragma once
//
// cryogen/ecs/component.hpp
// Type-erased sparse-set component pool.
//

#include <cassert>
#include <cstring>
#include <memory>
#include <vector>

#include "cryogen/core/types.hpp"
#include "cryogen/ecs/entity.hpp"

namespace cryogen::ecs {

using TypeId = const void*;

template <class T>
inline TypeId type_id_for() {
    static const char tag = 0;
    return &tag;
}

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void   remove(u32 entity_index)             = 0;
    virtual bool   has(u32 entity_index) const          = 0;
    virtual usize  size() const                         = 0;
    virtual TypeId type() const                         = 0;
};

template <class T>
class ComponentPool final : public IComponentPool {
public:
    static constexpr u32 kTombstone = 0xffffffffu;

    T& add(u32 entity_index, T value) {
        if (entity_index >= sparse_.size()) {
            sparse_.resize(entity_index + 1, kTombstone);
        }
        u32 slot = sparse_[entity_index];
        if (slot == kTombstone) {
            slot = static_cast<u32>(dense_.size());
            sparse_[entity_index] = slot;
            dense_.push_back(entity_index);
            data_.push_back(std::move(value));
            return data_.back();
        }
        data_[slot] = std::move(value);
        return data_[slot];
    }

    bool has(u32 entity_index) const override {
        return entity_index < sparse_.size() && sparse_[entity_index] != kTombstone;
    }

    T* try_get(u32 entity_index) {
        if (!has(entity_index)) return nullptr;
        return &data_[sparse_[entity_index]];
    }

    const T* try_get(u32 entity_index) const {
        if (!has(entity_index)) return nullptr;
        return &data_[sparse_[entity_index]];
    }

    T& get(u32 entity_index) {
        assert(has(entity_index));
        return data_[sparse_[entity_index]];
    }

    const T& get(u32 entity_index) const {
        assert(has(entity_index));
        return data_[sparse_[entity_index]];
    }

    void remove(u32 entity_index) override {
        if (!has(entity_index)) return;
        u32 slot = sparse_[entity_index];
        u32 last = static_cast<u32>(data_.size() - 1);

        if (slot != last) {
            data_[slot]  = std::move(data_[last]);
            dense_[slot] = dense_[last];
            sparse_[dense_[slot]] = slot;
        }
        data_.pop_back();
        dense_.pop_back();
        sparse_[entity_index] = kTombstone;
    }

    usize  size() const override { return data_.size(); }
    TypeId type() const override { return type_id_for<T>(); }

    // Iteration: parallel arrays of entity indices and components.
    const std::vector<u32>& entities() const { return dense_; }
    std::vector<T>&         data()           { return data_; }
    const std::vector<T>&   data() const     { return data_; }

private:
    std::vector<u32> sparse_;   // entity_index -> slot
    std::vector<u32> dense_;    // slot -> entity_index
    std::vector<T>   data_;     // slot -> component
};

} // namespace cryogen::ecs
