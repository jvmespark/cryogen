#include "cryogen/math/transform.hpp"

namespace cryogen::math {

mat4 Transform::matrix() const {
    mat4 t = translate(position);
    mat4 r = to_mat4(rotation);
    mat4 s = ::cryogen::math::scale(scale);
    return t * r * s;
}

mat4 Transform::inverse_matrix() const {
    return inverse(matrix());
}

Transform Transform::combine(const Transform& child) const {
    Transform out;
    out.rotation = rotation * child.rotation;
    out.scale    = scale * child.scale;
    out.position = position + rotate(rotation, scale * child.position);
    return out;
}

} // namespace cryogen::math
