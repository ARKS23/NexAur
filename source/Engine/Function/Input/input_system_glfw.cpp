//
// Created by ASUS on 2026/1/15.
//

#include "input_system_glfw.h"

#include "Function/Global/global_context.h"
#include "Function/Renderer/window_system.h"

namespace NexAur {
    bool InputSystemGLFW::isKeyPressed(KeyCode key_code) {
        GLFWwindow *window = getGLFWWindow();
        int state = glfwGetKey(window, static_cast<int32_t>(key_code)); // 查询按键状态
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool InputSystemGLFW::isMouseButtonPress(MouseCode mouse_code) {
        GLFWwindow *window = getGLFWWindow();
        int state = glfwGetMouseButton(window, static_cast<int32_t>(mouse_code));
        return state == GLFW_PRESS;
    }

    std::pair<float, float> InputSystemGLFW::getMousePosition() {
        GLFWwindow *window = getGLFWWindow();
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        return {static_cast<float>(x), static_cast<float>(y)};
    }

    float InputSystemGLFW::getMousePositionX() {
        auto [x, y] = getMousePosition();
        return x;
    }

    float InputSystemGLFW::getMousePositionY() {
        auto [x, y] = getMousePosition();
        return y;
    }

    GLFWwindow* InputSystemGLFW::getGLFWWindow() {
        return g_runtime_global_context.m_window_system->getNativeWindow();
    }
} //namespace NexAur