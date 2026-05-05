#pragma once
//
// cryogen/render/mesh.hpp
// GPU mesh resource: interleaved vertex buffer + index buffer.
// Vertex layout matches shaders/pbr.vert.
//

#include <cstddef>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"
#include "cryogen/math/aabb.hpp"
#include "cryogen/math/vec.hpp"
#include "cryogen/render/vulkan_context.hpp"

namespace cryogen::render {

struct Vertex {
    math::vec3 position;
    math::vec3 normal;
    math::vec3 tangent;
    math::vec2 uv;
};
static_assert(sizeof(Vertex) == 11 * sizeof(float),
              "Vertex layout must match shaders/pbr.vert std430 inputs.");

// CPU-side mesh data, produced by the asset loader.
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32>    indices;
    math::Aabb          bounds{};
    std::string         name;
};

// GPU-resident mesh.
class Mesh {
public:
    Mesh(VulkanContext& ctx, const MeshData& data);
    ~Mesh();

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    VkBuffer vertex_buffer() const { return vbo_.buffer; }
    VkBuffer index_buffer()  const { return ibo_.buffer; }
    u32      index_count()   const { return index_count_; }
    u32      vertex_count()  const { return vertex_count_; }
    VkDeviceAddress vertex_device_address() const { return vbo_.device_address; }
    VkDeviceAddress index_device_address()  const { return ibo_.device_address; }
    const math::Aabb& bounds() const { return bounds_; }

private:
    VulkanContext&                  ctx_;
    VulkanContext::AllocatedBuffer  vbo_{};
    VulkanContext::AllocatedBuffer  ibo_{};
    u32                             vertex_count_ = 0;
    u32                             index_count_  = 0;
    math::Aabb                      bounds_{};
};

// Vertex input description (for raster pipeline).
VkVertexInputBindingDescription                 mesh_vertex_binding();
std::vector<VkVertexInputAttributeDescription>  mesh_vertex_attributes();

} // namespace cryogen::render
