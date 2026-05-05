//
// cryogen/render/mesh.cpp
//
#include "cryogen/render/mesh.hpp"

#include <cstring>

#include "cryogen/core/log.hpp"

namespace cryogen::render {

Mesh::Mesh(VulkanContext& ctx, const MeshData& data) : ctx_(ctx) {
    vertex_count_ = static_cast<u32>(data.vertices.size());
    index_count_  = static_cast<u32>(data.indices.size());
    bounds_       = data.bounds;

    const VkDeviceSize vbytes = vertex_count_ * sizeof(Vertex);
    const VkDeviceSize ibytes = index_count_  * sizeof(u32);

    VkBufferUsageFlags vusage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferUsageFlags iusage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (ctx_.rt_enabled()) {
        vusage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        iusage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    vbo_ = ctx_.create_buffer(vbytes, vusage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ibo_ = ctx_.create_buffer(ibytes, iusage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ctx_.upload_buffer(vbo_.buffer, data.vertices.data(), vbytes);
    ctx_.upload_buffer(ibo_.buffer, data.indices.data(),  ibytes);
}

Mesh::~Mesh() {
    ctx_.destroy_buffer(vbo_);
    ctx_.destroy_buffer(ibo_);
}

VkVertexInputBindingDescription mesh_vertex_binding() {
    return { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
}

std::vector<VkVertexInputAttributeDescription> mesh_vertex_attributes() {
    return {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)   },
        { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)  },
        { 3, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv)       },
    };
}

} // namespace cryogen::render
