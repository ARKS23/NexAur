//
// Created by ARK on 2026/1/17.
//

#pragma once
#include "Core/Events/event.h"

namespace NexAur {
    // 键盘事件基类: 提取公共特征
    class NEXAUR_API KeyEvent : public Event {
    public:
        // 获取按键代码
        inline int GetKeyCode() const { return m_KeyCode; }
        // 宏复用：生成获取类别的函数,这里设置两个类别
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput);

    protected:
        // 构造函数设置为protected,不能创建通用的KeyEvent只能创建子类
        KeyEvent(int keycode) : m_KeyCode(keycode) {}

        int m_KeyCode;
    };


    // 按键按下事件
    class NEXAUR_API KeyPressedEvent : public KeyEvent {
    public:
        // 按住一个键不放系统会连续发送KeyPressed事件，repeatCount增加, 这里用了委托构造
        KeyPressedEvent(int keycode, int repaetCount) : KeyEvent(keycode), m_RepeatCount(repaetCount) {}

        inline int GetRepeatCount() const { return m_RepeatCount;}

        // 调试用
        std::string toString() const override {
            std::string str = "KeyPressedEvent:" + std::to_string(m_KeyCode) + "(" + std::to_string(m_RepeatCount) + " repeats)";
            return str;
        }

        // 宏复用: 生成GetStaticType, GetEventType, GetName
        EVENT_CLASS_TYPE(KeyPressed);

    private:
        int m_RepeatCount;
    };


    // 按键松开事件
    class NEXAUR_API KeyReleasedEvent : public KeyEvent {
    public:
        KeyReleasedEvent(int keycode) : KeyEvent(keycode) {}

        std::string toString() const override {
            std::string str = "KeyReleasedEvent: " + std::to_string(m_KeyCode);
            return str;
        }

        EVENT_CLASS_TYPE(KeyReleased);
    };


    // 字符输入事件: 关注生成的字符是什么，一般用于输入框的输入
    class NEXAUR_API KeyTypedEvent : public KeyEvent {
    public:
        KeyTypedEvent(int keycode) : KeyEvent(keycode) {}

        std::string toString() const override {
            std::string str = "KeyTypedEvent: " + std::to_string(m_KeyCode);
            return str;
        }

        EVENT_CLASS_TYPE(KeyTyped);
    };
}

