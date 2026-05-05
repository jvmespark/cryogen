//
// cryogen/editor/editor.cpp
//
#include "cryogen/editor/editor.hpp"

#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include "cryogen/assets/asset_manager.hpp"
#include "cryogen/core/log.hpp"
#include "cryogen/core/window.hpp"
#include "cryogen/input/input.hpp"
#include "cryogen/render/pbr_pass.hpp"
#include "cryogen/render/swapchain.hpp"
#include "cryogen/render/vulkan_context.hpp"
#include "cryogen/scene/components.hpp"
#include "cryogen/scene/scene.hpp"

namespace cryogen::editor {

namespace {
void check_vk(VkResult r) {
    if (r != VK_SUCCESS) CRYOGEN_ERROR("editor", "ImGui Vulkan VkResult=", static_cast<int>(r));
}
}

Editor::Editor(core::Window& window, render::VulkanContext& ctx,
               render::Swapchain& swap, render::PbrPass& pbr)
    : window_(window), ctx_(ctx), swap_(swap), pbr_(pbr) {

    // Create a roomy descriptor pool for ImGui.
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER,                 32},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  64},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,           32},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          32},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          32},
    };
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pi.maxSets       = 256;
    pi.poolSizeCount = static_cast<u32>(sizeof(sizes) / sizeof(sizes[0]));
    pi.pPoolSizes    = sizes;
    CRYOGEN_VK_CHECK(vkCreateDescriptorPool(ctx_.device(), &pi, nullptr, &imgui_pool_));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForVulkan(window.sdl_handle());
    ImGui_ImplVulkan_InitInfo info{};
    info.Instance       = ctx_.instance();
    info.PhysicalDevice = ctx_.physical_device();
    info.Device         = ctx_.device();
    info.QueueFamily    = ctx_.graphics_family();
    info.Queue          = ctx_.graphics_queue();
    info.DescriptorPool = imgui_pool_;
    info.MinImageCount  = 2;
    info.ImageCount     = swap_.image_count();
    info.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;
    info.CheckVkResultFn= check_vk;
    info.RenderPass     = pbr_.render_pass();   // imgui draws inside our pass
    ImGui_ImplVulkan_Init(&info);

    // Forward SDL events into ImGui.
    window.on_event([](const SDL_Event& e) { ImGui_ImplSDL2_ProcessEvent(&e); });
}

Editor::~Editor() {
    if (ctx_.device()) vkDeviceWaitIdle(ctx_.device());
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (imgui_pool_) vkDestroyDescriptorPool(ctx_.device(), imgui_pool_, nullptr);
}

void Editor::begin_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Editor::end_frame(VkCommandBuffer cb) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
}

// ----------------------------------------------------------------------
void Editor::hierarchy_panel(scene::Scene& scene, assets::AssetManager& am) {
    ImGui::Begin("Hierarchy");
    if (ImGui::Button("+ Cube"))   scene.spawn_dynamic_box(am, {0, 5, 0});
    ImGui::SameLine();
    if (ImGui::Button("+ Sphere")) scene.spawn_dynamic_ball(am, {0, 5, 0});
    ImGui::Separator();

    auto entities = scene.world().entities();
    for (auto e : entities) {
        const char* name = "<unnamed>";
        if (auto* nc = scene.world().try_get<scene::NameComponent>(e))
            name = nc->name.c_str();
        char label[64];
        std::snprintf(label, sizeof(label), "%s##%u", name, e.id);
        if (ImGui::Selectable(label, selected_ == e)) selected_ = e;
    }
    ImGui::End();
}

void Editor::inspector_panel(scene::Scene& scene) {
    ImGui::Begin("Inspector");
    if (!selected_.valid() || !scene.world().alive(selected_)) {
        ImGui::TextDisabled("Nothing selected");
        ImGui::End();
        return;
    }
    auto& world = scene.world();

    if (auto* nc = world.try_get<scene::NameComponent>(selected_)) {
        char buf[128]; std::snprintf(buf, sizeof(buf), "%s", nc->name.c_str());
        if (ImGui::InputText("Name", buf, sizeof(buf))) nc->name = buf;
    }

    if (auto* tc = world.try_get<scene::TransformComponent>(selected_)) {
        ImGui::SeparatorText("Transform");
        ImGui::DragFloat3("Position", &tc->local.position.x, 0.05f);
        // Edit rotation as Euler for ergonomics; convert back via from_euler.
        static float euler[3] = {0,0,0};
        if (ImGui::DragFloat3("Rotation (rad)", euler, 0.01f)) {
            tc->local.rotation = math::quat::from_euler({euler[0], euler[1], euler[2]});
        }
        ImGui::DragFloat3("Scale", &tc->local.scale.x, 0.05f);
    }

    if (auto* mc = world.try_get<scene::MeshComponent>(selected_)) {
        ImGui::SeparatorText("Mesh");
        ImGui::Text("mesh_id = 0x%08x", mc->mesh_id);
        ImGui::Text("material_id = 0x%08x", mc->material_id);
        ImGui::Checkbox("Visible", &mc->visible);
    }

    if (auto* rb = world.try_get<physics::RigidBodyComponent>(selected_)) {
        ImGui::SeparatorText("RigidBody");
        const char* types[] = {"Static", "Dynamic", "Kinematic"};
        int t = static_cast<int>(rb->type);
        if (ImGui::Combo("Type", &t, types, 3)) rb->type = static_cast<physics::BodyType>(t);
        ImGui::DragFloat ("inv_mass",        &rb->inv_mass,        0.01f, 0.f, 100.f);
        ImGui::DragFloat3("linear_velocity", &rb->linear_velocity.x, 0.1f);
        ImGui::DragFloat3("angular_velocity",&rb->angular_velocity.x,0.1f);
        ImGui::DragFloat ("linear_damping",  &rb->linear_damping,  0.001f, 0.f, 1.f);
        ImGui::DragFloat ("angular_damping", &rb->angular_damping, 0.001f, 0.f, 1.f);
    }

    if (auto* cc = world.try_get<physics::ColliderComponent>(selected_)) {
        ImGui::SeparatorText("Collider");
        const char* shapes[] = {"Sphere", "Box", "Plane"};
        int t = static_cast<int>(cc->type);
        if (ImGui::Combo("Shape", &t, shapes, 3)) cc->type = static_cast<physics::ShapeType>(t);
        if (cc->type == physics::ShapeType::Sphere) ImGui::DragFloat("radius", &cc->radius, 0.01f);
        if (cc->type == physics::ShapeType::Box)    ImGui::DragFloat3("half_extents", &cc->half_extents.x, 0.01f);
        ImGui::DragFloat("restitution", &cc->restitution, 0.01f, 0.f, 1.f);
        ImGui::DragFloat("friction",    &cc->friction,    0.01f, 0.f, 2.f);
    }

    ImGui::End();
}

void Editor::scene_overlay(scene::Scene& scene, input::Input& input) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(8, 28));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("Scene", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::Text("CryoGen %.1f FPS", io.Framerate);
    ImGui::Text("Entities: %zu", scene.world().entity_count());
    ImGui::Separator();
    ImGui::TextWrapped("RMB: rotate camera, WASD: move, Q/E: down/up");

    // Free-fly camera when right mouse button held.
    auto& cam = scene.camera();
    if (input.mouse_down(input::MouseButton::Right)) {
        const auto d = input.mouse_delta();
        cam.yaw   += d.x * 0.0035f;
        cam.pitch -= d.y * 0.0035f;
        if (cam.pitch >  1.5f) cam.pitch =  1.5f;
        if (cam.pitch < -1.5f) cam.pitch = -1.5f;
        cam.update_basis();

        const float spd = io.KeysDown[ImGuiKey_LeftShift] ? 6.f : 2.f;
        const auto rt = math::normalize(math::cross(cam.forward, cam.up));
        const float dt = io.DeltaTime;
        if (input.key_down(input::Key::W)) cam.position += cam.forward * spd * dt;
        if (input.key_down(input::Key::S)) cam.position -= cam.forward * spd * dt;
        if (input.key_down(input::Key::A)) cam.position -= rt * spd * dt;
        if (input.key_down(input::Key::D)) cam.position += rt * spd * dt;
        if (input.key_down(input::Key::Q)) cam.position.y -= spd * dt;
        if (input.key_down(input::Key::E)) cam.position.y += spd * dt;
    }
    ImGui::End();
}

void Editor::draw_panels(scene::Scene& scene, assets::AssetManager& am, input::Input& input) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Scene")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) { /* hooked from main loop */ }
            if (ImGui::MenuItem("Quit"))           { window_.request_close(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Hierarchy", nullptr, &show_hierarchy);
            ImGui::MenuItem("Inspector", nullptr, &show_inspector);
            ImGui::MenuItem("Scene Overlay", nullptr, &show_scene_overlay);
            ImGui::MenuItem("ImGui Demo", nullptr, &show_demo);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    if (show_hierarchy)     hierarchy_panel(scene, am);
    if (show_inspector)     inspector_panel(scene);
    if (show_scene_overlay) scene_overlay  (scene, input);
    if (show_demo)          ImGui::ShowDemoWindow(&show_demo);
}

} // namespace cryogen::editor
