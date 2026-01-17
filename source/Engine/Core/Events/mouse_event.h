//
// Created by ARK on 2026/1/17.
//

#pragma once
#include "Core/Events/event.h"

namespace NexAur {
    /* 鼠标移动事件 */
    class NEXAUR_API MouseMovedEvent : public Event {
    public:
        MouseMovedEvent(float x, float y) : m_MouseX(x), m_MouseY(y) {}

        inline float GetX() const { return m_MouseX; }
        inline float GetY() const { return m_MouseY; }

        std::string toString() const override {
            std::string str = "MouseMovedEvent: " + std::to_string(m_MouseX) + ", " + std::to_string(m_MouseY);
            return str;
        }

        EVENT_CLASS_TYPE(MouseMoved); // 小类
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput); // 大类

    private:
        float m_MouseX, m_MouseY;
    };


    /* 鼠标滚轮滚动事件 */
    class NEXAUR_API MouseScrolledEvent : public Event {
    public:
        MouseScrolledEvent(float xOffset, float yOffset) : m_XOffset(xOffset), m_YOffset(yOffset) {}
        inline float GetXOffset() const { return m_XOffset; }
        inline float GetYOffset() const { return m_YOffset; }

        std::string toString() const override {
            std::string str = "MouseScrolledEvent: " + std::to_string(m_XOffset) + ", " + std::to_string(m_YOffset);
            return str;
        }

        EVENT_CLASS_TYPE(MouseScrolled); // 小类
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput); //大类

    private:
        float m_XOffset, m_YOffset;
    };


    /* 鼠标按键事件基类(类似键盘按键事件基类) */
    class NEXAUR_API MouseButtonEvent : public Event {
    public:
        inline int GetMouseButton() const { return m_Button; }

        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryMouseButton | EventCategoryInput); // 这里设置公共的大类，小类可能不同，在子类中设置

    protected:
        MouseButtonEvent(int button) : m_Button(button) {} // 后续子类委托构造

        int m_Button;
    };


    // 4. 鼠标按下事件
    class NEXAUR_API MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(int button)
            : MouseButtonEvent(button) {}

        std::string toString() const override {
            std::string str = "MouseButtonPressedEvent: " + std::to_string(m_Button);
            return str;
        }

        EVENT_CLASS_TYPE(MouseButtonPressed);
    };


    // 5. 鼠标松开事件
    class NEXAUR_API MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(int button)
            : MouseButtonEvent(button) {}

        std::string toString() const override {
            std::string str = "MouseButtonReleasedEvent: " + std::to_string(m_Button);
            return str;
        }

        EVENT_CLASS_TYPE(MouseButtonReleased);
    };
};
