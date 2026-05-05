#pragma once
//
// cryogen/render/pbr_pass.hpp
// Forward PBR raster pass:
//
//   - One render pass with color (swapchain) + depth attachments
//   - One framebuffer per swapchain image
//   - One pipeline (mesh layout = render::Vertex)
//   - Per-frame UBO (camera + light + material)
//
// The renderer holds a PbrPass and rebuilds it on swapchain resize.
//

#include <vector>

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"
#include "cryogen/math/mat.hpp"
#include "cryogen/math/vec.hpp"
#include "cryogen/render/material.hpp"

namespace cryogen::render {

class VulkanContext;
class Swapchain;
class Mesh;

struct CameraUBO {
    math::mat4 view;
    math::mat4 proj;
    math::mat4 view_proj;
    math::vec4 camera_pos;       // xyz=pos, w=time
    math::vec4 light_dir;        // xyz=dir
    math::vec4 light_color;      // xyz=color, w=intensity
};

struct PushConstants {
    math::mat4  model;
    MaterialGPU material;
};

class PbrPass {
public:
    PbrPass(VulkanContext& ctx, Swapchain& swap);
    ~PbrPass();

    PbrPass(const PbrPass&)            = delete;
    PbrPass& operator=(const PbrPass&) = delete;

    void on_swapchain_recreated(Swapchain& swap);

    // Begin/End the render pass and bind the pipeline. The renderer issues
    // the per-mesh draws between these calls.
    void begin(VkCommandBuffer cb, u32 swap_image_index, const CameraUBO& ubo);
    void draw_mesh(VkCommandBuffer cb, const Mesh& mesh,
                   const math::mat4& model, const Material& material);
    void end  (VkCommandBuffer cb);

    VkRenderPass render_pass() const { return render_pass_; }

private:
    void create_pass();
    void create_framebuffers(Swapchain& swap);
    void create_pipeline(Swapchain& swap);
    void create_uniform_buffers();
    void destroy_framebuffers();

    VulkanContext&           ctx_;
    Swapchain*               swap_ = nullptr;

    VkRenderPass             render_pass_     = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    VkDescriptorSetLayout    set_layout_      = VK_NULL_HANDLE;
    VkDescriptorPool         desc_pool_       = VK_NULL_HANDLE;
    VkDescriptorSet          desc_set_        = VK_NULL_HANDLE;
    VkPipelineLayout         pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline               pipeline_        = VK_NULL_HANDLE;

    // Per-frame UBO (one buffer; updated each frame; small, host-visible).
    VkBuffer                 ubo_buf_         = VK_NULL_HANDLE;
    VkDeviceMemory           ubo_mem_         = VK_NULL_HANDLE;
    void*                    ubo_mapped_      = nullptr;
};

} // namespace cryogen::render
