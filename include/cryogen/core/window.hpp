#pragma once
//
// cryogen/core/window.hpp
// SDL2-backed window with Vulkan surface acquisition.
//
// The class is intentionally agnostic of the renderer: it produces a
// VkSurfaceKHR through `create_vulkan_surface`, but otherwise exposes only
// platform-neutral state (size, focus, close request, raw event polling).
//

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

struct SDL_Window;
union SDL_Event;

namespace cryogen::core {

struct WindowConfig {
    std::string title  = "CryoGen";
    int         width  = 1280;
    int         height = 720;
    bool        resizable = true;
    bool        vsync     = true;
};

class Window {
public:
    explicit Window(const WindowConfig& cfg);
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    // Pump SDL events and dispatch to subscribers. Returns false if the
    // window has been closed.
    bool poll();

    // Subscribe to SDL events (for input system, ImGui SDL backend, etc.).
    using EventCallback = std::function<void(const SDL_Event&)>;
    void on_event(EventCallback cb) { listeners_.push_back(std::move(cb)); }

    // Vulkan integration.
    std::vector<const char*> required_instance_extensions() const;
    VkResult create_vulkan_surface(VkInstance instance, VkSurfaceKHR* out_surface) const;

    // Platform handles (for ImGui_ImplSDL2_InitForVulkan etc.).
    SDL_Window* sdl_handle() const { return window_; }

    // State queries.
    int  width()      const { return width_; }
    int  height()     const { return height_; }
    bool minimized()  const { return minimized_; }
    bool resized()    const { return resized_; }   // single-frame flag, cleared by poll()
    bool should_close() const { return should_close_; }

    void request_close() { should_close_ = true; }

private:
    SDL_Window*                 window_       = nullptr;
    int                         width_        = 0;
    int                         height_       = 0;
    bool                        minimized_    = false;
    bool                        resized_      = false;
    bool                        should_close_ = false;
    std::vector<EventCallback>  listeners_;
};

} // namespace cryogen::core
