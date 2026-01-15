//
// Created by ASUS on 2026/1/15.
//

#pragma once

#include <GLFW/glfw3.h>

#include "Core/Base.h"
#include "Function/Input/input_system.h"

namespace NexAur {
    class NEXAUR_API InputSystemGLFW : public InputSystem {
    public:
        bool isKeyPressed(KeyCode key_code) override;
        bool isMouseButtonPress(MouseCode mouse_code) override;
        std::pair<float, float> getMousePosition() override;
        float getMousePositionX() override;
        float getMousePositionY() override;

    private:
        GLFWwindow* getGLFWWindow();    // 获取GLFW的原生指针
    };
} //namespace NexAur

