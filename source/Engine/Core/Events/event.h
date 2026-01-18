//
// Created by ARK on 2026/1/17.
//

#pragma once
#include <string>
#include <functional>

#include "Core/Base.h"

namespace NexAur {
    // 目前事件系统设计是阻塞事件，立即响应模式

    // 各类事件类型
    enum class EventType {
		None = 0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		AppTick, AppUpdate, AppRender,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled, MouseScrolling
    };

    // 给事件类型分大类,这里用了BIT宏：设置整数的二进制第几位为1
    enum EvnetCategory {
        None = 0,
        EventCategoryApplication = BIT(0),
        EventCategoryInput = BIT(1),
        EventCategoryKeyboard = BIT(2),
        EventCategoryMouse = BIT(3),
        EventCategoryMouseButton = BIT(4)
    };

// 每一个具体的 Event 子类都需要实现这三个函数，定义宏快速编写
#define EVENT_CLASS_TYPE(type) static EventType getStaticType() { return EventType::##type; }\
                                virtual EventType getEventType() const override { return getStaticType(); }\
                                virtual const char* getName() const override { return #type; }

// 用于生成 getCategoryFlags 函数
#define EVENT_CLASS_CATEGORY(category) virtual int getCategoryFlags() const override { return category; }

    // 事件基类
    class NEXAUR_API Event {
    public:
        friend class EventDispatcher;

    public:
        virtual  ~Event() = default;

    public:
        virtual EventType getEventType() const = 0; // 获取具体的类型
        virtual const char* getName() const = 0;	// 获取事件名字，用于调试
        virtual int getCategoryFlags() const = 0;   // 获取分类标志位
        virtual std::string toString() const { return getName(); } // 打印名字

        inline bool isInCategory(EvnetCategory category) { return getCategoryFlags() & category; } // 判断当前事件是否属于某个分类

    public:
        bool handled = false; // 拦截标志位：如果为true则说明事件已经处理，不继续传给下层
    };

    // 事件分发器
    class NEXAUR_API EventDispatcher {
        template<typename T>
        using event_fn = std::function<bool(T&)>;   // 定义一个函数指针，返回值为bool，参数T&.  （解释:OnEvent函数都接收一个事件参数返回是否拦截）

    public:
        EventDispatcher(Event &event) : m_event(event) {}

    public:
        template<typename T>
        bool dispatch(event_fn<T> func) {   // 核心分发逻辑
            if (m_event.getEventType() == T::getStaticType()) { // m_event是实例，可以调用成员函数。 T是类型需要调用静态成员函数
                m_event.handled = func(*static_cast<T*>(&m_event));
                return true;
            }
            return false;
        }

    private:
        Event &m_event;
    };

} // namespace NexAur