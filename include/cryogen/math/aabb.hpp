#pragma once
//
// cryogen/math/aabb.hpp
// Axis-aligned bounding box.
//

#include "cryogen/math/vec.hpp"

namespace cryogen::math {

struct Aabb {
    vec3 min = { +1e30f, +1e30f, +1e30f };
    vec3 max = { -1e30f, -1e30f, -1e30f };

    bool valid()  const { return max.x >= min.x && max.y >= min.y && max.z >= min.z; }
    vec3 center() const { return (min + max) * 0.5f; }
    vec3 extent() const { return (max - min) * 0.5f; }

    void expand(vec3 p) {
        min = math::min(min, p);
        max = math::max(max, p);
    }
    void expand(const Aabb& o) {
        min = math::min(min, o.min);
        max = math::max(max, o.max);
    }
};

inline bool overlaps(const Aabb& a, const Aabb& b) {
    return !(a.max.x < b.min.x || a.min.x > b.max.x ||
             a.max.y < b.min.y || a.min.y > b.max.y ||
             a.max.z < b.min.z || a.min.z > b.max.z);
}

} // namespace cryogen::math
