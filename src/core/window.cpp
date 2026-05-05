#include "cryogen/core/window.hpp"
#include "cryogen/core/log.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <stdexcept>

namespace cryogen::core {

Window::Window(const WindowConfig& cfg) {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
            throw std::runtime_error(std::string{"SDL_Init: "} + SDL_GetError());
        }
    }

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (cfg.resizable) flags |= SDL_WINDOW_RESIZABLE;

    window_ = SDL_CreateWindow(
        cfg.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg.width, cfg.height,
        flags);

    if (!window_) {
        throw std::runtime_error(std::string{"SDL_CreateWindow: "} + SDL_GetError());
    }

    width_  = cfg.width;
    height_ = cfg.height;

    CRYOGEN_INFO("window", "Created ", cfg.width, "x", cfg.height, " title='", cfg.title, "'");
}

Window::~Window() {
    if (window_) SDL_DestroyWindow(window_);
}

bool Window::poll() {
    resized_ = false;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        for (auto& cb : listeners_) cb(ev);

        switch (ev.type) {
            case SDL_QUIT:
                should_close_ = true;
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED ||
                    ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    width_   = ev.window.data1;
                    height_  = ev.window.data2;
                    resized_ = true;
                } else if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    minimized_ = true;
                } else if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                    minimized_ = false;
                } else if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                    should_close_ = true;
                }
                break;
            default: break;
        }
    }
    return !should_close_;
}

std::vector<const char*> Window::required_instance_extensions() const {
    unsigned count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window_, &count, nullptr)) {
        throw std::runtime_error(std::string{"SDL_Vulkan_GetInstanceExtensions: "} + SDL_GetError());
    }
    std::vector<const char*> exts(count);
    if (!SDL_Vulkan_GetInstanceExtensions(window_, &count, exts.data())) {
        throw std::runtime_error(std::string{"SDL_Vulkan_GetInstanceExtensions: "} + SDL_GetError());
    }
    return exts;
}

VkResult Window::create_vulkan_surface(VkInstance instance, VkSurfaceKHR* out_surface) const {
    if (!SDL_Vulkan_CreateSurface(window_, instance, out_surface)) {
        CRYOGEN_ERROR("window", "SDL_Vulkan_CreateSurface failed: ", SDL_GetError());
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

} // namespace cryogen::core
