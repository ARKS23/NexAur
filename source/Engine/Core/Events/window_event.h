#pragma once
#include "Core/Events/event.h"

namespace NexAur {
    /* 窗口大小改变事件 */
    class NEXAUR_API WindowResizeEvent : public Event {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height) : m_Width(width), m_Height(height) {}
        inline unsigned int getWidth() const { return m_Width; }
        inline unsigned int getHeight() const { return m_Height; }

        std::string toString() const override {
            std::string str = "WindowResizeEvent: " + std::to_string(m_Width) + ", " + std::to_string(m_Height);
            return str;
        }

        EVENT_CLASS_TYPE(WindowResize);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);

    private:
        unsigned int m_Width, m_Height;
    };

    
    /* 窗口关闭事件 */
    class NEXAUR_API WindowCloseEvent : public Event {
    public:
        WindowCloseEvent() {}

        EVENT_CLASS_TYPE(WindowClose);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);
    };
}