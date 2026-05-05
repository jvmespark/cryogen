// shaders/pbr.vert
// Forward PBR vertex shader. Reads vertex attributes from the binding layout
// declared in render::mesh_vertex_attributes() and the camera UBO at set=0
// binding=0; the model matrix and packed material come in via push constants.

#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_uv;

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
    vec4 emissive_metallic;
    vec4 roughness_ao_ior;
} pc;

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec3 v_world_normal;
layout(location = 2) out vec3 v_world_tangent;
layout(location = 3) out vec2 v_uv;

void main() {
    vec4 wp = pc.model * vec4(in_position, 1.0);
    v_world_pos     = wp.xyz;
    v_world_normal  = mat3(pc.model) * in_normal;
    v_world_tangent = mat3(pc.model) * in_tangent;
    v_uv            = in_uv;
    gl_Position     = cam.view_proj * wp;
}
