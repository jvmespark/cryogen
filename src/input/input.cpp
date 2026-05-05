#include "cryogen/input/input.hpp"
#include "cryogen/core/window.hpp"

#include <SDL2/SDL.h>

namespace cryogen::input {
namespace {

Key sdl_to_key(SDL_Keycode sym) {
    switch (sym) {
        case SDLK_SPACE:     return Key::Space;
        case SDLK_RETURN:    return Key::Return;
        case SDLK_ESCAPE:    return Key::Escape;
        case SDLK_TAB:       return Key::Tab;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_LEFT:      return Key::Left;
        case SDLK_RIGHT:     return Key::Right;
        case SDLK_UP:        return Key::Up;
        case SDLK_DOWN:      return Key::Down;
        case SDLK_LSHIFT:    return Key::LeftShift;
        case SDLK_LCTRL:     return Key::LeftCtrl;
        case SDLK_LALT:      return Key::LeftAlt;
        case SDLK_RSHIFT:    return Key::RightShift;
        case SDLK_RCTRL:     return Key::RightCtrl;
        case SDLK_RALT:      return Key::RightAlt;
        default: break;
    }
    if (sym >= SDLK_a && sym <= SDLK_z) {
        return static_cast<Key>(static_cast<u16>(Key::A) + (sym - SDLK_a));
    }
    if (sym >= SDLK_0 && sym <= SDLK_9) {
        return static_cast<Key>(static_cast<u16>(Key::Num0) + (sym - SDLK_0));
    }
    if (sym >= SDLK_F1 && sym <= SDLK_F12) {
        return static_cast<Key>(static_cast<u16>(Key::F1) + (sym - SDLK_F1));
    }
    return Key::Unknown;
}

} // namespace

Input::Input(core::Window& window) {
    window.on_event([this](const SDL_Event& ev) {
        switch (ev.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                const Key k = sdl_to_key(ev.key.keysym.sym);
                if (k != Key::Unknown) {
                    keys_.set(static_cast<usize>(k), ev.type == SDL_KEYDOWN);
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                MouseButton b = MouseButton::Left;
                if (ev.button.button == SDL_BUTTON_LEFT)   b = MouseButton::Left;
                if (ev.button.button == SDL_BUTTON_RIGHT)  b = MouseButton::Right;
                if (ev.button.button == SDL_BUTTON_MIDDLE) b = MouseButton::Middle;
                mb_[static_cast<usize>(b)] = (ev.type == SDL_MOUSEBUTTONDOWN);
                break;
            }
            case SDL_MOUSEMOTION:
                pos_  = { static_cast<f32>(ev.motion.x), static_cast<f32>(ev.motion.y) };
                delta_= delta_ + math::vec2{ static_cast<f32>(ev.motion.xrel),
                                             static_cast<f32>(ev.motion.yrel) };
                break;
            case SDL_MOUSEWHEEL:
                wheel_ += static_cast<f32>(ev.wheel.y);
                break;
            default: break;
        }
    });
}

void Input::end_frame() {
    prev_keys_ = keys_;
    prev_mb_   = mb_;
    delta_     = {0,0};
    wheel_     = 0.f;
}

void Input::set_relative_mouse(bool on) {
    SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
}

} // namespace cryogen::input
