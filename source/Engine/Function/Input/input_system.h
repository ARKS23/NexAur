//
// Created by ASUS on 2026/1/15.
//

#pragma once

#include <unordered_map>

#include "Core/Base.h"
#include "Function/Input/KeyCode/key_codes.h"
#include "Function/Input/KeyCode/mouse_codes.h"

namespace NexAur {
    class NEXAUR_API InputSystem {
    public:
        virtual  ~InputSystem() = default;

    public:
        virtual bool isKeyPressed(KeyCode key_code) = 0;    // 检查按键是否按下
        virtual bool isMouseButtonPress(MouseCode mouse_code) = 0;  // 检查鼠标按键是否按下
        virtual std::pair<float, float> getMousePosition() = 0; // 获取鼠标位置
        virtual float getMousePositionX() = 0;
        virtual float getMousePositionY() = 0;
    };
} // namespace NexAur