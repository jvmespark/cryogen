// apps/editor/main.cpp
// Editor entry-point. Wires the engine, asset manager, scene, renderer, and
// editor UI together. The fixed-step is the engine's authoritative tick;
// the editor draws ImGui inline inside the PBR pass each frame.
//
#include <cstdio>
#include <memory>

#include "cryogen/core/application.hpp"
#include "cryogen/core/log.hpp"
#include "cryogen/assets/asset_manager.hpp"
#include "cryogen/editor/editor.hpp"
#include "cryogen/input/input.hpp"
#include "cryogen/render/renderer.hpp"
#include "cryogen/scene/scene.hpp"
#include "cryogen/scene/serializer.hpp"

using namespace cryogen;

class EditorApp final : public core::Application {
public:
    explicit EditorApp(core::ApplicationConfig cfg) : core::Application(std::move(cfg)) {}

    void on_start() override {
        render::RendererConfig rc{};
        renderer_ = std::make_unique<render::Renderer>(*window(), rc);
        input_    = std::make_unique<input::Input>(*window());
        editor_   = std::make_unique<editor::Editor>(*window(),
                                                     renderer_->context(),
                                                     renderer_->swapchain(),
                                                     renderer_->pbr_pass());
        assets_   = std::make_unique<assets::AssetManager>(renderer_->context(), config().asset_root);
        scene_    = std::make_unique<scene::Scene>();

        scene_->spawn_static_floor (*assets_);
        scene_->spawn_dynamic_box  (*assets_, math::vec3{ 0.f, 4.f, 0.f});
        scene_->spawn_dynamic_box  (*assets_, math::vec3{ 1.5f, 6.f, -0.3f});
        scene_->spawn_dynamic_ball (*assets_, math::vec3{-1.5f, 5.f, 0.5f}, 0.5f);
        scene_->camera().update_basis();
    }

    void on_fixed_step(f32 dt) override {
        scene_->physics().step(scene_->world(), dt);
    }

    void on_frame(f32 /*dt*/) override {
        input_->end_frame();
        scene_->update_world_transforms();
        editor_->begin_frame();
        editor_->draw_panels(*scene_, *assets_, *input_);
    }

    void on_render(f32 /*dt*/) override {
        renderer_->render_frame(*scene_, *assets_);
    }

    void on_shutdown() override {
        editor_.reset();
        renderer_.reset();
    }

private:
    std::unique_ptr<render::Renderer>        renderer_;
    std::unique_ptr<editor::Editor>          editor_;
    std::unique_ptr<scene::Scene>            scene_;
    std::unique_ptr<assets::AssetManager>    assets_;
    std::unique_ptr<input::Input>            input_;
};

int main(int /*argc*/, char** /*argv*/) {
    core::ApplicationConfig cfg;
    cfg.window.title  = "CryoGen Editor";
    cfg.window.width  = 1600;
    cfg.window.height = 900;
    cfg.create_window = true;

    EditorApp app(cfg);
    return app.run();
}
