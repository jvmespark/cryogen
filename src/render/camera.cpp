//
// cryogen/render/camera.cpp
//
#include "cryogen/render/camera.hpp"

#include <cmath>

namespace cryogen::render {

void Camera::update_basis() {
    forward.x = std::cos(pitch) * std::cos(yaw);
    forward.y = std::sin(pitch);
    forward.z = std::cos(pitch) * std::sin(yaw);
    forward   = math::normalize(forward);
}

math::mat4 Camera::view() const {
    return math::look_at(position, position + forward, up);
}

math::mat4 Camera::projection() const {
    return math::perspective_vk(fov_y_radians, aspect, z_near, z_far);
}

} // namespace cryogen::render
