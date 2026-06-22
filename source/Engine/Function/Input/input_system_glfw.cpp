#include "input_system_glfw.h"

#include <array>

namespace NexAur {
    namespace {
        constexpr std::array kTrackedKeys = {
            KeyCode::Space,
            KeyCode::Apostrophe,
            KeyCode::Comma,
            KeyCode::Minus,
            KeyCode::Period,
            KeyCode::Slash,
            KeyCode::D0,
            KeyCode::D1,
            KeyCode::D2,
            KeyCode::D3,
            KeyCode::D4,
            KeyCode::D5,
            KeyCode::D6,
            KeyCode::D7,
            KeyCode::D8,
            KeyCode::D9,
            KeyCode::Semicolon,
            KeyCode::Equal,
            KeyCode::A,
            KeyCode::B,
            KeyCode::C,
            KeyCode::D,
            KeyCode::E,
            KeyCode::F,
            KeyCode::G,
            KeyCode::H,
            KeyCode::I,
            KeyCode::J,
            KeyCode::K,
            KeyCode::L,
            KeyCode::M,
            KeyCode::N,
            KeyCode::O,
            KeyCode::P,
            KeyCode::Q,
            KeyCode::R,
            KeyCode::S,
            KeyCode::T,
            KeyCode::U,
            KeyCode::V,
            KeyCode::W,
            KeyCode::X,
            KeyCode::Y,
            KeyCode::Z,
            KeyCode::LeftBracket,
            KeyCode::Backslash,
            KeyCode::RightBracket,
            KeyCode::GraveAccent,
            KeyCode::World1,
            KeyCode::World2,
            KeyCode::Escape,
            KeyCode::Enter,
            KeyCode::Tab,
            KeyCode::Backspace,
            KeyCode::Insert,
            KeyCode::Delete,
            KeyCode::Right,
            KeyCode::Left,
            KeyCode::Down,
            KeyCode::Up,
            KeyCode::PageUp,
            KeyCode::PageDown,
            KeyCode::Home,
            KeyCode::End,
            KeyCode::CapsLock,
            KeyCode::ScrollLock,
            KeyCode::NumLock,
            KeyCode::PrintScreen,
            KeyCode::Pause,
            KeyCode::F1,
            KeyCode::F2,
            KeyCode::F3,
            KeyCode::F4,
            KeyCode::F5,
            KeyCode::F6,
            KeyCode::F7,
            KeyCode::F8,
            KeyCode::F9,
            KeyCode::F10,
            KeyCode::F11,
            KeyCode::F12,
            KeyCode::F13,
            KeyCode::F14,
            KeyCode::F15,
            KeyCode::F16,
            KeyCode::F17,
            KeyCode::F18,
            KeyCode::F19,
            KeyCode::F20,
            KeyCode::F21,
            KeyCode::F22,
            KeyCode::F23,
            KeyCode::F24,
            KeyCode::F25,
            KeyCode::KP0,
            KeyCode::KP1,
            KeyCode::KP2,
            KeyCode::KP3,
            KeyCode::KP4,
            KeyCode::KP5,
            KeyCode::KP6,
            KeyCode::KP7,
            KeyCode::KP8,
            KeyCode::KP9,
            KeyCode::KPDecimal,
            KeyCode::KPDivide,
            KeyCode::KPMultiply,
            KeyCode::KPSubtract,
            KeyCode::KPAdd,
            KeyCode::KPEnter,
            KeyCode::KPEqual,
            KeyCode::LeftShift,
            KeyCode::LeftControl,
            KeyCode::LeftAlt,
            KeyCode::LeftSuper,
            KeyCode::RightShift,
            KeyCode::RightControl,
            KeyCode::RightAlt,
            KeyCode::RightSuper,
            KeyCode::Menu
        };

        constexpr std::array kTrackedMouseButtons = {
            MouseCode::Button0,
            MouseCode::Button1,
            MouseCode::Button2,
            MouseCode::Button3,
            MouseCode::Button4,
            MouseCode::Button5,
            MouseCode::Button6,
            MouseCode::Button7
        };
    } // namespace

    InputSystemGLFW::InputSystemGLFW(GLFWwindow* window)
        : m_window(window) {}

    void InputSystemGLFW::setWindow(GLFWwindow* window) {
        m_window = window;
    }

    void InputSystemGLFW::update() {
        m_state.reset();

        GLFWwindow* window = getGLFWWindow();
        if (!window) {
            return;
        }

        for (KeyCode key_code : kTrackedKeys) {
            const int state = glfwGetKey(window, static_cast<int32_t>(key_code));
            m_state.setKeyPressed(key_code, state == GLFW_PRESS || state == GLFW_REPEAT);
        }

        for (MouseCode mouse_code : kTrackedMouseButtons) {
            const int state = glfwGetMouseButton(window, static_cast<int32_t>(mouse_code));
            m_state.setMouseButtonPressed(mouse_code, state == GLFW_PRESS);
        }

        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window, &x, &y);
        m_state.mouse_x = static_cast<float>(x);
        m_state.mouse_y = static_cast<float>(y);
    }

    bool InputSystemGLFW::isKeyPressed(KeyCode key_code) {
        GLFWwindow* window = getGLFWWindow();
        if (window) {
            const int state = glfwGetKey(window, static_cast<int32_t>(key_code));
            m_state.setKeyPressed(key_code, state == GLFW_PRESS || state == GLFW_REPEAT);
        }

        return m_state.isKeyPressed(key_code);
    }

    bool InputSystemGLFW::isMouseButtonPress(MouseCode mouse_code) {
        GLFWwindow* window = getGLFWWindow();
        if (window) {
            const int state = glfwGetMouseButton(window, static_cast<int32_t>(mouse_code));
            m_state.setMouseButtonPressed(mouse_code, state == GLFW_PRESS);
        }

        return m_state.isMouseButtonPressed(mouse_code);
    }

    std::pair<float, float> InputSystemGLFW::getMousePosition() {
        GLFWwindow* window = getGLFWWindow();
        if (window) {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(window, &x, &y);
            m_state.mouse_x = static_cast<float>(x);
            m_state.mouse_y = static_cast<float>(y);
        }

        return m_state.getMousePosition();
    }

    float InputSystemGLFW::getMousePositionX() {
        auto [x, y] = getMousePosition();
        (void)y;
        return x;
    }

    float InputSystemGLFW::getMousePositionY() {
        auto [x, y] = getMousePosition();
        (void)x;
        return y;
    }

    GLFWwindow* InputSystemGLFW::getGLFWWindow() {
        return m_window;
    }
} //namespace NexAur
