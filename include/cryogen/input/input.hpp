#pragma once
//
// cryogen/input/input.hpp
// SDL2-derived input snapshot: keyboard state, mouse state, and per-frame
// edge-triggered events. The Input system subscribes to SDL events through
// Window::on_event and updates an internal cache.
//

#include <array>
#include <bitset>

#include "cryogen/core/types.hpp"
#include "cryogen/math/vec.hpp"

namespace cryogen::core { class Window; }

namespace cryogen::input {

enum class Key : u16 {
    Unknown = 0,
    Space, Return, Escape, Tab, Backspace,
    Left, Right, Up, Down,
    LeftShift, LeftCtrl, LeftAlt, RightShift, RightCtrl, RightAlt,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count
};

enum class MouseButton : u8 { Left = 0, Right, Middle, Count };

class Input {
public:
    explicit Input(core::Window& window);

    // Tick once per frame *after* polling events.
    void end_frame();

    bool key_down(Key k) const     { return keys_[static_cast<usize>(k)]; }
    bool key_pressed(Key k) const  { return keys_[static_cast<usize>(k)] && !prev_keys_[static_cast<usize>(k)]; }
    bool key_released(Key k) const { return !keys_[static_cast<usize>(k)] && prev_keys_[static_cast<usize>(k)]; }

    bool mouse_down(MouseButton b) const     { return mb_[static_cast<usize>(b)]; }
    bool mouse_pressed(MouseButton b) const  { return mb_[static_cast<usize>(b)] && !prev_mb_[static_cast<usize>(b)]; }
    bool mouse_released(MouseButton b) const { return !mb_[static_cast<usize>(b)] && prev_mb_[static_cast<usize>(b)]; }

    math::vec2 mouse_pos()   const { return pos_; }
    math::vec2 mouse_delta() const { return delta_; }
    f32 mouse_wheel()        const { return wheel_; }

    void set_relative_mouse(bool on);

private:
    static constexpr usize kKeyCount = static_cast<usize>(Key::Count);
    static constexpr usize kBtnCount = static_cast<usize>(MouseButton::Count);

    std::bitset<kKeyCount>          keys_, prev_keys_;
    std::array<bool, kBtnCount>     mb_{}, prev_mb_{};
    math::vec2                      pos_{0,0};
    math::vec2                      delta_{0,0};
    f32                             wheel_ = 0.f;
};

} // namespace cryogen::input
