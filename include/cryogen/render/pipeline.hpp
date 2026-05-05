#pragma once
//
// cryogen/render/pipeline.hpp
// Thin RAII wrappers that build VkPipeline objects from SPIR-V on disk.
// Two builders: GraphicsPipelineBuilder (raster) and RayTracingPipelineBuilder.
// Both return owning handles; the renderer destroys them.
//

#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"

namespace cryogen::render {

class VulkanContext;

VkShaderModule load_shader_module(VulkanContext& ctx, const std::string& spv_path);

struct GraphicsPipelineConfig {
    std::string                                       vert_spv;
    std::string                                       frag_spv;
    VkRenderPass                                      render_pass = VK_NULL_HANDLE;
    VkPipelineLayout                                  layout      = VK_NULL_HANDLE;
    VkExtent2D                                        extent      = {0,0};
    VkVertexInputBindingDescription                   vbinding{};
    std::vector<VkVertexInputAttributeDescription>    vattrs;
    bool                                              depth_test  = true;
    bool                                              depth_write = true;
    VkCullModeFlags                                   cull_mode   = VK_CULL_MODE_BACK_BIT;
};

VkPipeline build_graphics_pipeline(VulkanContext& ctx, const GraphicsPipelineConfig& cfg);

// ---- Ray tracing pipeline -------------------------------------------------
struct RayTracingPipelineConfig {
    std::string      rgen_spv;
    std::string      rmiss_spv;
    std::string      rchit_spv;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    u32              max_recursion = 1;
};

struct RayTracingPipeline {
    VkPipeline                                  pipeline = VK_NULL_HANDLE;
    VkBuffer                                    sbt_buffer = VK_NULL_HANDLE;
    VkDeviceMemory                              sbt_memory = VK_NULL_HANDLE;
    VkStridedDeviceAddressRegionKHR             rgen_region{};
    VkStridedDeviceAddressRegionKHR             miss_region{};
    VkStridedDeviceAddressRegionKHR             hit_region{};
    VkStridedDeviceAddressRegionKHR             call_region{};
};

RayTracingPipeline build_ray_tracing_pipeline(VulkanContext& ctx, const RayTracingPipelineConfig& cfg);
void destroy_ray_tracing_pipeline(VulkanContext& ctx, RayTracingPipeline& rtp);

} // namespace cryogen::render
