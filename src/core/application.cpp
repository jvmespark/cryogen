#include "cryogen/core/application.hpp"
#include "cryogen/core/log.hpp"

#include <algorithm>
#include <thread>

namespace cryogen::core {

Application::Application(ApplicationConfig cfg) : cfg_(std::move(cfg)) {
    if (cfg_.create_window) {
        window_ = std::make_unique<Window>(cfg_.window);
    }
}

Application::~Application() = default;

int Application::run() {
    on_start();

    Timer  frame_timer;
    f64    accumulator = 0.0;

    while (!exit_requested_) {
        if (window_ && !window_->poll()) {
            break;
        }
        if (window_ && window_->minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        f64 dt = frame_timer.tick();
        if (dt > cfg_.max_frame_dt) dt = cfg_.max_frame_dt;

        // Fixed-step simulation
        accumulator += dt;
        while (accumulator >= cfg_.fixed_dt) {
            on_fixed_step(cfg_.fixed_dt);
            accumulator -= cfg_.fixed_dt;
        }

        // Variable-rate frame work
        on_frame(static_cast<f32>(dt));
        on_render(static_cast<f32>(dt));
    }

    on_shutdown();
    CRYOGEN_INFO("app", "Exited cleanly after ", wall_.elapsed(), " s");
    return 0;
}

} // namespace cryogen::core
