//
// cryogen/render/pbr_pass.cpp
//
#include "cryogen/render/pbr_pass.hpp"

#include <array>
#include <cstring>

#include "cryogen/core/log.hpp"
#include "cryogen/render/mesh.hpp"
#include "cryogen/render/pipeline.hpp"
#include "cryogen/render/swapchain.hpp"
#include "cryogen/render/vulkan_context.hpp"

namespace cryogen::render {

PbrPass::PbrPass(VulkanContext& ctx, Swapchain& swap) : ctx_(ctx), swap_(&swap) {
    create_pass();
    create_framebuffers(swap);
    create_uniform_buffers();
    create_pipeline(swap);
}

PbrPass::~PbrPass() {
    if (ubo_mapped_) vkUnmapMemory(ctx_.device(), ubo_mem_);
    if (ubo_buf_)    vkDestroyBuffer(ctx_.device(), ubo_buf_, nullptr);
    if (ubo_mem_)    vkFreeMemory(ctx_.device(), ubo_mem_, nullptr);

    if (pipeline_)        vkDestroyPipeline(ctx_.device(), pipeline_, nullptr);
    if (pipeline_layout_) vkDestroyPipelineLayout(ctx_.device(), pipeline_layout_, nullptr);
    if (desc_pool_)       vkDestroyDescriptorPool(ctx_.device(), desc_pool_, nullptr);
    if (set_layout_)      vkDestroyDescriptorSetLayout(ctx_.device(), set_layout_, nullptr);

    destroy_framebuffers();
    if (render_pass_) vkDestroyRenderPass(ctx_.device(), render_pass_, nullptr);
}

void PbrPass::create_pass() {
    std::array<VkAttachmentDescription, 2> ats{};
    ats[0].format         = VK_FORMAT_B8G8R8A8_SRGB; // overridden via dynamic state if needed
    ats[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    ats[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ats[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    ats[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ats[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ats[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    ats[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    ats[1].format         = VK_FORMAT_D32_SFLOAT;
    ats[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    ats[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ats[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ats[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ats[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ats[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    ats[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference cref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &cref;
    sub.pDepthStencilAttachment = &dref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = static_cast<u32>(ats.size());
    ci.pAttachments    = ats.data();
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;
    CRYOGEN_VK_CHECK(vkCreateRenderPass(ctx_.device(), &ci, nullptr, &render_pass_));
}

void PbrPass::create_framebuffers(Swapchain& swap) {
    framebuffers_.resize(swap.image_count(), VK_NULL_HANDLE);
    for (u32 i = 0; i < swap.image_count(); ++i) {
        VkImageView atts[2] = { swap.color_view(i), swap.depth_view() };
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass      = render_pass_;
        ci.attachmentCount = 2;
        ci.pAttachments    = atts;
        ci.width           = swap.extent().width;
        ci.height          = swap.extent().height;
        ci.layers          = 1;
        CRYOGEN_VK_CHECK(vkCreateFramebuffer(ctx_.device(), &ci, nullptr, &framebuffers_[i]));
    }
}

void PbrPass::destroy_framebuffers() {
    for (auto fb : framebuffers_) if (fb) vkDestroyFramebuffer(ctx_.device(), fb, nullptr);
    framebuffers_.clear();
}

void PbrPass::create_uniform_buffers() {
    const VkDeviceSize sz = sizeof(CameraUBO);
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = sz;
    bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CRYOGEN_VK_CHECK(vkCreateBuffer(ctx_.device(), &bi, nullptr, &ubo_buf_));
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx_.device(), ubo_buf_, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = ctx_.find_memory_type(
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CRYOGEN_VK_CHECK(vkAllocateMemory(ctx_.device(), &ai, nullptr, &ubo_mem_));
    CRYOGEN_VK_CHECK(vkBindBufferMemory(ctx_.device(), ubo_buf_, ubo_mem_, 0));
    CRYOGEN_VK_CHECK(vkMapMemory(ctx_.device(), ubo_mem_, 0, sz, 0, &ubo_mapped_));
}

void PbrPass::create_pipeline(Swapchain& swap) {
    // Single uniform buffer at binding 0.
    VkDescriptorSetLayoutBinding b{};
    b.binding         = 0;
    b.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.bindingCount = 1; lci.pBindings = &b;
    CRYOGEN_VK_CHECK(vkCreateDescriptorSetLayout(ctx_.device(), &lci, nullptr, &set_layout_));

    VkDescriptorPoolSize ps{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets       = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &ps;
    CRYOGEN_VK_CHECK(vkCreateDescriptorPool(ctx_.device(), &pci, nullptr, &desc_pool_));

    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool     = desc_pool_;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts        = &set_layout_;
    CRYOGEN_VK_CHECK(vkAllocateDescriptorSets(ctx_.device(), &dai, &desc_set_));

    VkDescriptorBufferInfo dbi{ubo_buf_, 0, sizeof(CameraUBO)};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet          = desc_set_;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo     = &dbi;
    vkUpdateDescriptorSets(ctx_.device(), 1, &w, 0, nullptr);

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pli.setLayoutCount         = 1;
    pli.pSetLayouts            = &set_layout_;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges    = &pc;
    CRYOGEN_VK_CHECK(vkCreatePipelineLayout(ctx_.device(), &pli, nullptr, &pipeline_layout_));

    GraphicsPipelineConfig cfg;
    cfg.vert_spv    = "shaders/pbr.vert.spv";
    cfg.frag_spv    = "shaders/pbr.frag.spv";
    cfg.render_pass = render_pass_;
    cfg.layout      = pipeline_layout_;
    cfg.extent      = swap.extent();
    cfg.vbinding    = mesh_vertex_binding();
    cfg.vattrs      = mesh_vertex_attributes();
    pipeline_ = build_graphics_pipeline(ctx_, cfg);
}

void PbrPass::on_swapchain_recreated(Swapchain& swap) {
    swap_ = &swap;
    destroy_framebuffers();
    create_framebuffers(swap);
}

void PbrPass::begin(VkCommandBuffer cb, u32 swap_image_index, const CameraUBO& ubo) {
    std::memcpy(ubo_mapped_, &ubo, sizeof(CameraUBO));

    VkClearValue clears[2]{};
    clears[0].color = { {0.04f, 0.05f, 0.07f, 1.f} };
    clears[1].depthStencil = {1.f, 0};

    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass  = render_pass_;
    bi.framebuffer = framebuffers_[swap_image_index];
    bi.renderArea  = {{0,0}, swap_->extent()};
    bi.clearValueCount = 2;
    bi.pClearValues = clears;
    vkCmdBeginRenderPass(cb, &bi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{0.f, 0.f, (float)swap_->extent().width, (float)swap_->extent().height, 0.f, 1.f};
    VkRect2D   sc{{0,0}, swap_->extent()};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor (cb, 0, 1, &sc);

    vkCmdBindPipeline      (cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                            0, 1, &desc_set_, 0, nullptr);
}

void PbrPass::draw_mesh(VkCommandBuffer cb, const Mesh& mesh,
                        const math::mat4& model, const Material& material) {
    PushConstants pc{};
    pc.model    = model;
    pc.material = pack(material);
    vkCmdPushConstants(cb, pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    VkBuffer vbuf = mesh.vertex_buffer();
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vbuf, &off);
    vkCmdBindIndexBuffer  (cb, mesh.index_buffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed      (cb, mesh.index_count(), 1, 0, 0, 0);
}

void PbrPass::end(VkCommandBuffer cb) {
    vkCmdEndRenderPass(cb);
}

} // namespace cryogen::render
