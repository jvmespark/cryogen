#pragma once
//
// cryogen/render/renderer.hpp
// Top-level rendering facade. Owns:
//   - VulkanContext         (instance, device, queues)
//   - Swapchain             (frame sync, present)
//   - PbrPass               (forward raster)
//   - AccelStructures (opt) (BLAS/TLAS for ray tracing)
//
// Each frame, the renderer:
//   1. acquires the next swapchain image
//   2. records a graphics command buffer (raster pbr pass over scene meshes)
//   3. (optionally) issues a ray-traced pass for a single accumulating buffer
//   4. submits & presents
//

#include <memory>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"
#include "cryogen/render/pbr_pass.hpp"
#include "cryogen/render/ray_tracing.hpp"
#include "cryogen/render/swapchain.hpp"
#include "cryogen/render/vulkan_context.hpp"

namespace cryogen::core   { class Window; }
namespace cryogen::scene  { class Scene; }
namespace cryogen::assets { class AssetManager; }

namespace cryogen::render {

struct RendererConfig {
    bool enable_ray_tracing = true;
    bool enable_validation  = true;
};

class Renderer {
public:
    Renderer(core::Window& window, RendererConfig cfg);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Drive the rendering of one full frame. The scene's world matrices must
    // already be up to date.
    void render_frame(scene::Scene& scene, assets::AssetManager& am);

    void on_window_resized(u32 w, u32 h);

    // Accessors for editor/imgui setup:
    VulkanContext& context() { return *ctx_; }
    Swapchain&     swapchain() { return *swap_; }
    PbrPass&       pbr_pass()  { return *pbr_; }
    bool           rt_supported() const { return ctx_->rt_enabled(); }

private:
    void build_or_refresh_blas(assets::AssetManager& am);

    RendererConfig                                   cfg_;
    std::unique_ptr<VulkanContext>                   ctx_;
    std::unique_ptr<Swapchain>                       swap_;
    std::unique_ptr<PbrPass>                         pbr_;
    std::unique_ptr<AccelStructures>                 as_;
    std::unordered_map<u32, BLAS>                    blas_cache_; // mesh_id -> BLAS
    VkCommandPool                                    cmd_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>                     cbs_;
};

} // namespace cryogen::render
