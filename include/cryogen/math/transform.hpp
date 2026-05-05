#pragma once
//
// cryogen/math/transform.hpp
// World-space transform = translation + rotation + uniform-or-axis scale.
//

#include "cryogen/math/mat.hpp"
#include "cryogen/math/quat.hpp"
#include "cryogen/math/vec.hpp"

namespace cryogen::math {

struct Transform {
    vec3 position = {0,0,0};
    quat rotation = quat::identity();
    vec3 scale    = {1,1,1};

    mat4 matrix() const;        // T * R * S
    mat4 inverse_matrix() const;

    Transform combine(const Transform& child) const; // parent.combine(child)
};

} // namespace cryogen::math
