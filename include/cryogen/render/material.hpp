#pragma once
//
// cryogen/render/material.hpp
// PBR material parameters: base color, metallic, roughness, emissive.
//

#include "cryogen/math/vec.hpp"

namespace cryogen::render {

struct Material {
    math::vec4 base_color   { 0.8f, 0.8f, 0.8f, 1.0f };
    math::vec3 emissive     { 0.0f, 0.0f, 0.0f };
    float      metallic     = 0.0f;
    float      roughness    = 0.5f;
    float      ao           = 1.0f;
    float      ior          = 1.45f;
};

// std140-compatible packed form pushed to the GPU.
struct MaterialGPU {
    math::vec4 base_color;
    math::vec4 emissive_metallic; // xyz=emissive, w=metallic
    math::vec4 roughness_ao_ior;  // x=rough, y=ao, z=ior, w=pad
};

inline MaterialGPU pack(const Material& m) {
    return {
        m.base_color,
        { m.emissive.x, m.emissive.y, m.emissive.z, m.metallic },
        { m.roughness,  m.ao,         m.ior,        0.f }
    };
}

} // namespace cryogen::render
