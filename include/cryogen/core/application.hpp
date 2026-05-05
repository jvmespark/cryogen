#pragma once
//
// cryogen/core/application.hpp
// Top-level application loop with fixed-step physics + variable render rate.
//
// Subclasses override the lifecycle hooks `on_start`, `on_fixed_step`, and
// `on_frame`. `Application::run` returns when `request_exit()` is called or
// the window is closed.
//

#include <memory>
#include <string>

#include "cryogen/core/timer.hpp"
#include "cryogen/core/types.hpp"
#include "cryogen/core/window.hpp"

namespace cryogen::core {

struct ApplicationConfig {
    WindowConfig window;
    bool         create_window = true;     // server runs headless
    f32          fixed_dt      = 1.f / 60.f;
    f32          max_frame_dt  = 0.25f;     // clamp to avoid spiral-of-death
    std::string  asset_root    = "assets";
};

class Application {
public:
    explicit Application(ApplicationConfig cfg);
    virtual ~Application();

    int run();
    void request_exit() { exit_requested_ = true; }

    // Subclass hooks ------------------------------------------------------
    virtual void on_start()                {}
    virtual void on_fixed_step(f32 /*dt*/) {}      // physics tick rate
    virtual void on_frame(f32 /*dt*/)      {}      // pre-render
    virtual void on_render(f32 /*dt*/)     {}      // GPU command record
    virtual void on_shutdown()             {}

    // Accessors -----------------------------------------------------------
    Window*       window()                { return window_.get(); }
    const Window* window() const          { return window_.get(); }
    const ApplicationConfig& config() const { return cfg_; }
    f64 elapsed() const                   { return wall_.elapsed(); }

private:
    ApplicationConfig          cfg_;
    std::unique_ptr<Window>    window_;
    Timer                      wall_;
    bool                       exit_requested_ = false;
};

} // namespace cryogen::core
