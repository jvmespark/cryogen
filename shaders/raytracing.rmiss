// shaders/raytracing.rmiss
// Miss shader: a simple two-tone procedural sky.

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    vec3 d = normalize(gl_WorldRayDirectionEXT);
    float t = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 horizon = vec3(0.65, 0.78, 0.92);
    vec3 zenith  = vec3(0.18, 0.34, 0.62);
    hitValue = mix(horizon, zenith, t);
}
