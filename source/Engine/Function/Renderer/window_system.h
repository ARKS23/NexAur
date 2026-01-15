//
// Created by ASUS on 2026/1/14.
//

#pragma once

#include <array>

#include "GLFW/glfw3.h"

#include "Core/Base.h"

namespace NexAur
{
    struct WindowSpecification {
        int width{1920};
        int height{1080};
        const char* title{"NexAur"};
        bool fullscreen{false};
    };


    class NEXAUR_API WindowSystem {
    public:
        WindowSystem() = default;
        ~WindowSystem();

    public:
        void init(const WindowSpecification& specification);    // 初始化窗口
        void pollEvents();  // 获取输入
        bool shouldClose() const;
        void setTitle(const char* title);
        GLFWwindow* getNativeWindow() const;    // 得到原生GLFW窗口
        std::array<int, 2> getWindowSize() const;   // 返回宽高

        // 测试
        void update();

    private:
        GLFWwindow* m_window{nullptr};
        int m_width{1920};
        int m_height{1080};

        bool is_focus_mode{false};
    };
} // namespace NexAur
