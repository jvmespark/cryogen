//
// cryogen/render/renderer.cpp
//
#include "cryogen/render/renderer.hpp"

#include "cryogen/assets/asset_manager.hpp"
#include "cryogen/core/log.hpp"
#include "cryogen/core/window.hpp"
#include "cryogen/scene/components.hpp"
#include "cryogen/scene/scene.hpp"

namespace cryogen::render {

Renderer::Renderer(core::Window& window, RendererConfig cfg) : cfg_(cfg) {
    DeviceFeaturesRequested req;
    req.ray_tracing = cfg_.enable_ray_tracing;
    ctx_ = std::make_unique<VulkanContext>(window, req);
    swap_ = std::make_unique<Swapchain>(*ctx_,
                                        static_cast<u32>(window.width()),
                                        static_cast<u32>(window.height()));
    pbr_  = std::make_unique<PbrPass>(*ctx_, *swap_);

    if (ctx_->rt_enabled()) as_ = std::make_unique<AccelStructures>(*ctx_);

    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = ctx_->graphics_family();
    CRYOGEN_VK_CHECK(vkCreateCommandPool(ctx_->device(), &pi, nullptr, &cmd_pool_));

    cbs_.resize(Swapchain::kFramesInFlight);
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool        = cmd_pool_;
    cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = static_cast<u32>(cbs_.size());
    CRYOGEN_VK_CHECK(vkAllocateCommandBuffers(ctx_->device(), &cai, cbs_.data()));
}

Renderer::~Renderer() {
    if (ctx_) vkDeviceWaitIdle(ctx_->device());

    if (as_) {
        for (auto& [id, b] : blas_cache_) as_->destroy_blas(b);
        blas_cache_.clear();
    }

    if (cmd_pool_) vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr);

    pbr_.reset();
    swap_.reset();
    as_.reset();
    ctx_.reset();
}

void Renderer::on_window_resized(u32 w, u32 h) {
    if (!ctx_) return;
    vkDeviceWaitIdle(ctx_->device());
    swap_->recreate(w, h);
    pbr_->on_swapchain_recreated(*swap_);
}

void Renderer::build_or_refresh_blas(assets::AssetManager& am) {
    if (!as_) return;
    for (const auto& [id, mesh] : am.meshes()) {
        if (blas_cache_.find(id) == blas_cache_.end()) {
            blas_cache_[id] = as_->build_blas(*mesh);
            CRYOGEN_INFO("render", "Built BLAS for mesh ", id);
        }
    }
}

void Renderer::render_frame(scene::Scene& scene, assets::AssetManager& am) {
    using cryogen::scene::TransformComponent;
    using cryogen::scene::MeshComponent;

    if (cfg_.enable_ray_tracing && ctx_->rt_enabled()) {
        build_or_refresh_blas(am);

        std::vector<TLASInstance> instances;
        scene.world().each<TransformComponent, MeshComponent>(
            [&](ecs::Entity, TransformComponent& tc, MeshComponent& mc) {
                if (!mc.visible) return;
                auto it = blas_cache_.find(mc.mesh_id);
                if (it == blas_cache_.end()) return;
                instances.push_back({ &it->second, tc.world, mc.mesh_id });
            });
        if (!instances.empty())
            as_->build_or_update_tlas(instances);
    }

    const u32 image_index = swap_->acquire_next_image();
    if (image_index == UINT32_MAX) return;
    const u32 frame = swap_->current_frame();

    VkCommandBuffer cb = cbs_[frame];
    vkResetCommandBuffer(cb, 0);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CRYOGEN_VK_CHECK(vkBeginCommandBuffer(cb, &bi));

    // Camera UBO -----------------------------------------------------------
    CameraUBO ubo{};
    auto& cam = scene.camera();
    cam.aspect = static_cast<float>(swap_->extent().width) /
                 static_cast<float>(swap_->extent().height);
    ubo.view = cam.view();
    ubo.proj = cam.projection();
    ubo.view_proj = ubo.proj * ubo.view;
    ubo.camera_pos = { cam.position.x, cam.position.y, cam.position.z, 0.f };
    const auto& sun = scene.sun();
    ubo.light_dir   = { sun.direction.x, sun.direction.y, sun.direction.z, 0.f };
    ubo.light_color = { sun.color.x, sun.color.y, sun.color.z, sun.intensity };

    pbr_->begin(cb, image_index, ubo);

    scene.world().each<TransformComponent, MeshComponent>(
        [&](ecs::Entity, TransformComponent& tc, MeshComponent& mc) {
            if (!mc.visible) return;
            const auto* mesh = am.mesh(mc.mesh_id);
            if (!mesh) return;
            auto mat = am.material(mc.material_id).value_or(Material{});
            pbr_->draw_mesh(cb, *mesh, tc.world, mat);
        });

    pbr_->end(cb);
    CRYOGEN_VK_CHECK(vkEndCommandBuffer(cb));

    swap_->submit_and_present(cb, image_index);
}

} // namespace cryogen::render
