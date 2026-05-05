// apps/runtime/main.cpp
// Standalone runtime / game-client. Connects to a server when given a host
// argument, otherwise runs offline (single-player sandbox).
//
//   ./cryogen-runtime                 # offline sandbox
//   ./cryogen-runtime 127.0.0.1       # connect to local server
//
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "cryogen/assets/asset_manager.hpp"
#include "cryogen/core/application.hpp"
#include "cryogen/core/log.hpp"
#include "cryogen/input/input.hpp"
#include "cryogen/net/client.hpp"
#include "cryogen/net/replication.hpp"
#include "cryogen/render/renderer.hpp"
#include "cryogen/scene/scene.hpp"

using namespace cryogen;

class RuntimeApp final : public core::Application {
public:
    RuntimeApp(core::ApplicationConfig cfg, std::string host)
        : core::Application(std::move(cfg)), host_(std::move(host)) {}

    void on_start() override {
        renderer_ = std::make_unique<render::Renderer>(*window(), render::RendererConfig{});
        input_    = std::make_unique<input::Input>(*window());
        assets_   = std::make_unique<assets::AssetManager>(renderer_->context(), config().asset_root);
        scene_    = std::make_unique<scene::Scene>();

        scene_->spawn_static_floor (*assets_);
        scene_->spawn_dynamic_box  (*assets_, math::vec3{0.f, 5.f, 0.f});
        scene_->spawn_dynamic_ball (*assets_, math::vec3{1.f, 6.f, 0.f});
        scene_->camera().update_basis();

        if (!host_.empty()) {
            net::ClientConfig cc;
            cc.host = host_;
            client_ = std::make_unique<net::Client>(cc);
            client_->connect();
            CRYOGEN_INFO("runtime", "Connecting to ", host_);
        }
    }

    void on_fixed_step(f32 dt) override {
        // Client prediction: simulate physics locally even when networked;
        // the server's authoritative snapshot will reconcile in on_frame.
        scene_->physics().step(scene_->world(), dt);
        input_seq_++;
    }

    void on_frame(f32 /*dt*/) override {
        input_->end_frame();

        if (client_) {
            // Apply latest server snapshot if any (last one wins for now;
            auto snaps = client_->drain_snapshots();
            if (!snaps.empty()) net::apply_snapshot(scene_->world(), snaps.back());

            // Send a synthetic input command derived from current keyboard.
            net::InputCommand cmd;
            cmd.sequence = input_seq_;
            cmd.dt       = config().fixed_dt;
            if (input_->key_down(input::Key::W)) cmd.move.z -= 1.f;
            if (input_->key_down(input::Key::S)) cmd.move.z += 1.f;
            if (input_->key_down(input::Key::A)) cmd.move.x -= 1.f;
            if (input_->key_down(input::Key::D)) cmd.move.x += 1.f;
            client_->send_input(cmd);
        }

        scene_->update_world_transforms();
    }

    void on_render(f32 /*dt*/) override {
        renderer_->render_frame(*scene_, *assets_);
    }

    void on_shutdown() override {
        if (client_) client_->disconnect();
        renderer_.reset();
    }

private:
    std::string                              host_;
    std::unique_ptr<render::Renderer>        renderer_;
    std::unique_ptr<scene::Scene>            scene_;
    std::unique_ptr<assets::AssetManager>    assets_;
    std::unique_ptr<input::Input>            input_;
    std::unique_ptr<net::Client>             client_;
    u32                                      input_seq_ = 0;
};

int main(int argc, char** argv) {
    std::string host;
    if (argc >= 2) host = argv[1];

    core::ApplicationConfig cfg;
    cfg.window.title  = host.empty() ? "CryoGen Runtime" : "CryoGen Runtime — connected";
    cfg.window.width  = 1280;
    cfg.window.height = 720;

    RuntimeApp app(cfg, host);
    return app.run();
}
