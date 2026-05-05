#pragma once
//
// cryogen/render/camera.hpp
// Free-fly perspective camera. Produces view+proj matrices in Vulkan-clip
// convention (y-flipped, z in [0,1]).
//

#include "cryogen/math/mat.hpp"
#include "cryogen/math/vec.hpp"

namespace cryogen::render {

class Camera {
public:
    math::vec3 position { 0.f, 1.5f, 4.f };
    math::vec3 forward  { 0.f, 0.f, -1.f };
    math::vec3 up       { 0.f, 1.f, 0.f };

    float fov_y_radians = 1.0472f;   // 60 deg
    float aspect        = 16.f / 9.f;
    float z_near        = 0.05f;
    float z_far         = 500.f;

    // Yaw/pitch in radians. Used by orbit/free controls.
    float yaw   = -1.5707963f;       // -pi/2 looking down -z
    float pitch = 0.f;

    void update_basis();             // recomputes `forward` from yaw/pitch
    math::mat4 view()       const;
    math::mat4 projection() const;
    math::mat4 view_proj()  const { return projection() * view(); }
};

} // namespace cryogen::render
