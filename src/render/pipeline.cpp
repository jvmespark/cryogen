//
// cryogen/render/pipeline.cpp
//
#include "cryogen/render/pipeline.hpp"

#include <array>
#include <cstring>

#include "cryogen/core/log.hpp"
#include "cryogen/render/vulkan_context.hpp"

namespace cryogen::render {

VkShaderModule load_shader_module(VulkanContext& ctx, const std::string& spv_path) {
    auto bytes = read_spv(spv_path);
    if (bytes.empty()) {
        CRYOGEN_ERROR("render", "Empty/missing SPIR-V file: ", spv_path);
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes.size();
    ci.pCode    = reinterpret_cast<const u32*>(bytes.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    CRYOGEN_VK_CHECK(vkCreateShaderModule(ctx.device(), &ci, nullptr, &mod));
    return mod;
}

VkPipeline build_graphics_pipeline(VulkanContext& ctx, const GraphicsPipelineConfig& cfg) {
    VkShaderModule vsm = load_shader_module(ctx, cfg.vert_spv);
    VkShaderModule fsm = load_shader_module(ctx, cfg.frag_spv);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vsm;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fsm;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &cfg.vbinding;
    vi.vertexAttributeDescriptionCount = static_cast<u32>(cfg.vattrs.size());
    vi.pVertexAttributeDescriptions    = cfg.vattrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{0.f, 0.f, (float)cfg.extent.width, (float)cfg.extent.height, 0.f, 1.f};
    VkRect2D   sc{{0,0}, cfg.extent};
    VkPipelineViewportStateCreateInfo vs{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vs.viewportCount = 1; vs.pViewports = &vp;
    vs.scissorCount  = 1; vs.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = cfg.cull_mode;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable  = cfg.depth_test  ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = cfg.depth_write ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable    = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dyn_states;

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vs;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout              = cfg.layout;
    pci.renderPass          = cfg.render_pass;
    pci.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    CRYOGEN_VK_CHECK(vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline));

    vkDestroyShaderModule(ctx.device(), vsm, nullptr);
    vkDestroyShaderModule(ctx.device(), fsm, nullptr);
    return pipeline;
}

// ---- Ray tracing pipeline -------------------------------------------------
static u32 align_up(u32 v, u32 a) { return (v + a - 1) & ~(a - 1); }

RayTracingPipeline build_ray_tracing_pipeline(VulkanContext& ctx, const RayTracingPipelineConfig& cfg) {
    RayTracingPipeline out{};
    if (!ctx.rt_enabled()) {
        CRYOGEN_WARN("render", "Ray tracing not enabled on this device; skipping RT pipeline build.");
        return out;
    }

    auto vkCreateRTP = (PFN_vkCreateRayTracingPipelinesKHR)
        vkGetDeviceProcAddr(ctx.device(), "vkCreateRayTracingPipelinesKHR");
    auto vkGetRTGroupHandles = (PFN_vkGetRayTracingShaderGroupHandlesKHR)
        vkGetDeviceProcAddr(ctx.device(), "vkGetRayTracingShaderGroupHandlesKHR");

    VkShaderModule rgen  = load_shader_module(ctx, cfg.rgen_spv);
    VkShaderModule rmiss = load_shader_module(ctx, cfg.rmiss_spv);
    VkShaderModule rchit = load_shader_module(ctx, cfg.rchit_spv);

    std::array<VkPipelineShaderStageCreateInfo, 3> stages{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[0].module = rgen;  stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[1].module = rmiss; stages[1].pName = "main";
    stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[2].module = rchit; stages[2].pName = "main";

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{};
    for (auto& g : groups) {
        g.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        g.generalShader      = VK_SHADER_UNUSED_KHR;
        g.closestHitShader   = VK_SHADER_UNUSED_KHR;
        g.anyHitShader       = VK_SHADER_UNUSED_KHR;
        g.intersectionShader = VK_SHADER_UNUSED_KHR;
    }
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader = 1;
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].closestHitShader = 2;

    VkRayTracingPipelineCreateInfoKHR rci{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    rci.stageCount                   = static_cast<u32>(stages.size());
    rci.pStages                      = stages.data();
    rci.groupCount                   = static_cast<u32>(groups.size());
    rci.pGroups                      = groups.data();
    rci.maxPipelineRayRecursionDepth = cfg.max_recursion;
    rci.layout                       = cfg.layout;

    CRYOGEN_VK_CHECK(vkCreateRTP(ctx.device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rci, nullptr, &out.pipeline));

    vkDestroyShaderModule(ctx.device(), rgen,  nullptr);
    vkDestroyShaderModule(ctx.device(), rmiss, nullptr);
    vkDestroyShaderModule(ctx.device(), rchit, nullptr);

    // Build SBT.
    const auto& props = ctx.rt_props();
    const u32 handle_size       = props.shaderGroupHandleSize;
    const u32 handle_align      = props.shaderGroupHandleAlignment;
    const u32 base_align        = props.shaderGroupBaseAlignment;
    const u32 handle_size_align = align_up(handle_size, handle_align);

    const u32 group_count = static_cast<u32>(groups.size());
    const VkDeviceSize sbt_size_per_group = align_up(handle_size_align, base_align);
    const VkDeviceSize sbt_size = sbt_size_per_group * group_count;

    std::vector<u8> handles(group_count * handle_size);
    CRYOGEN_VK_CHECK(vkGetRTGroupHandles(ctx.device(), out.pipeline, 0, group_count,
                                         handles.size(), handles.data()));

    auto sbt = ctx.create_buffer(
        sbt_size,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mapped = nullptr;
    vkMapMemory(ctx.device(), sbt.memory, 0, sbt_size, 0, &mapped);
    auto* dst = static_cast<u8*>(mapped);
    for (u32 g = 0; g < group_count; ++g) {
        std::memcpy(dst + g * sbt_size_per_group,
                    handles.data() + g * handle_size,
                    handle_size);
    }
    vkUnmapMemory(ctx.device(), sbt.memory);

    out.sbt_buffer = sbt.buffer;
    out.sbt_memory = sbt.memory;

    out.rgen_region.deviceAddress = sbt.device_address + 0 * sbt_size_per_group;
    out.rgen_region.stride        = sbt_size_per_group;
    out.rgen_region.size          = sbt_size_per_group;

    out.miss_region.deviceAddress = sbt.device_address + 1 * sbt_size_per_group;
    out.miss_region.stride        = sbt_size_per_group;
    out.miss_region.size          = sbt_size_per_group;

    out.hit_region.deviceAddress  = sbt.device_address + 2 * sbt_size_per_group;
    out.hit_region.stride         = sbt_size_per_group;
    out.hit_region.size           = sbt_size_per_group;

    out.call_region = {};

    return out;
}

void destroy_ray_tracing_pipeline(VulkanContext& ctx, RayTracingPipeline& rtp) {
    if (rtp.pipeline)   vkDestroyPipeline(ctx.device(), rtp.pipeline, nullptr);
    if (rtp.sbt_buffer) vkDestroyBuffer  (ctx.device(), rtp.sbt_buffer, nullptr);
    if (rtp.sbt_memory) vkFreeMemory     (ctx.device(), rtp.sbt_memory, nullptr);
    rtp = {};
}

} // namespace cryogen::render
