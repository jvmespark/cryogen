#pragma once
//
// cryogen/editor/editor.hpp
// ImGui-based level editor:
//   - Hierarchy panel (entity list, selection)
//   - Inspector panel (per-component widgets)
//   - Scene panel (camera controls, viewport overlay)
//   - Translate gizmo (immediate-mode)
//

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"
#include "cryogen/ecs/entity.hpp"

namespace cryogen::core   { class Window; }
namespace cryogen::scene  { class Scene; }
namespace cryogen::assets { class AssetManager; }
namespace cryogen::input  { class Input; }
namespace cryogen::render { class VulkanContext; class Swapchain; class PbrPass; }

namespace cryogen::editor {

class Editor {
public:
    Editor(core::Window& window, render::VulkanContext& ctx,
           render::Swapchain& swap, render::PbrPass& pbr);
    ~Editor();

    Editor(const Editor&)            = delete;
    Editor& operator=(const Editor&) = delete;

    // Drive every frame: ImGui::NewFrame, build panels, ImGui::Render.
    void begin_frame();
    void draw_panels(scene::Scene& scene, assets::AssetManager& am, input::Input& input);
    void end_frame(VkCommandBuffer cb);

    ecs::Entity selection() const { return selected_; }
    void        select(ecs::Entity e) { selected_ = e; }

    // Accessors for app-level menu toggles.
    bool show_hierarchy = true;
    bool show_inspector = true;
    bool show_scene_overlay = true;
    bool show_demo = false;

private:
    void hierarchy_panel(scene::Scene& scene, assets::AssetManager& am);
    void inspector_panel(scene::Scene& scene);
    void scene_overlay  (scene::Scene& scene, input::Input& input);

    core::Window&           window_;
    render::VulkanContext&  ctx_;
    render::Swapchain&      swap_;
    render::PbrPass&        pbr_;

    VkDescriptorPool        imgui_pool_ = VK_NULL_HANDLE;
    ecs::Entity             selected_{};
};

} // namespace cryogen::editor
