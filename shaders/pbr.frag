// shaders/pbr.frag
// Cook-Torrance microfacet BRDF with GGX distribution, Schlick Fresnel and
// Smith geometry. Single directional light + a tiny ambient term

#version 450

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec3 v_world_normal;
layout(location = 2) in vec3 v_world_tangent;
layout(location = 3) in vec2 v_uv;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 camera_pos;
    vec4 light_dir;
    vec4 light_color;
} cam;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 base_color;
    vec4 emissive_metallic;   // xyz=emissive, w=metallic
    vec4 roughness_ao_ior;    // x=rough, y=ao, z=ior, w=pad
} pc;

layout(location = 0) out vec4 out_color;

const float PI = 3.14159265358979323846;

float DistributionGGX(vec3 N, vec3 H, float a) {
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float d = (NdotH*NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * d * d);
}

float GeometrySchlickGGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float r  = roughness + 1.0;
    float k  = (r*r) / 8.0;
    float ggx1 = GeometrySchlickGGX(max(dot(N, V), 0.0), k);
    float ggx2 = GeometrySchlickGGX(max(dot(N, L), 0.0), k);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(v_world_normal);
    vec3 V = normalize(cam.camera_pos.xyz - v_world_pos);
    vec3 L = normalize(-cam.light_dir.xyz);
    vec3 H = normalize(V + L);

    vec3  albedo    = pc.base_color.rgb;
    float metallic  = pc.emissive_metallic.w;
    float roughness = clamp(pc.roughness_ao_ior.x, 0.045, 1.0);
    float ao        = pc.roughness_ao_ior.y;
    vec3  emissive  = pc.emissive_metallic.xyz;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Specular term.
    float NDF = DistributionGGX(N, H, roughness * roughness);
    float G   = GeometrySmith   (N, V, L, roughness);
    vec3  F   = FresnelSchlick  (max(dot(H, V), 0.0), F0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3  num   = NDF * G * F;
    float denom = max(4.0 * NdotV * NdotL, 1e-4);
    vec3  specular = num / denom;

    // Diffuse term (energy conservation).
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    vec3 light_radiance = cam.light_color.rgb * cam.light_color.w;
    vec3 Lo = (diffuse + specular) * light_radiance * NdotL;

    // Cheap ambient term so unlit faces aren't pure black.
    vec3 ambient = vec3(0.03) * albedo * ao;

    vec3 color = ambient + Lo + emissive;

    // Reinhard tone-map then sRGB encode (the swapchain is _SRGB).
    color = color / (color + vec3(1.0));
    out_color = vec4(color, pc.base_color.a);
}
