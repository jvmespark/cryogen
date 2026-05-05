// shaders/raytracing.rchit
// Closest-hit shader.

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

void main() {
    // Reconstruct a flat shading direction from the ray + barycentrics.
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 fake_normal = normalize(vec3(bary.x - bary.y, bary.z, 0.5));

    vec3 light_dir = normalize(vec3(0.3, 1.0, 0.4));
    float ndotl = max(dot(fake_normal, light_dir), 0.0);
    vec3 albedo = vec3(0.7, 0.55, 0.35);
    hitValue = albedo * (0.15 + 0.85 * ndotl);
}
