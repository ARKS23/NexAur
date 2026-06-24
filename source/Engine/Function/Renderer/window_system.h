//
// Created by ASUS on 2026/1/14.
//

#pragma once

#include <array>
#include <functional>
#include <vector>

#include <GLFW/glfw3.h>

#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Function/Platform/window_graphics_api.h"

namespace NexAur
{
    struct WindowSpecification {
        int width{1920};
        int height{1080};
        const char* title{"NexAur"};
        bool fullscreen{false};
        bool enable_vsync{true};
        WindowGraphicsAPI graphics_api{WindowGraphicsAPI::Vulkan};
    };


    class NEXAUR_API WindowSystem {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

    public:
        WindowSystem();
        ~WindowSystem();

    public:
        void init(const WindowSpecification& specification);    // 初始化窗口
        void pollEvents();  // 获取输入
        bool shouldClose() const;
        void shutdown();
        void setTitle(const char* title);
        GLFWwindow* getNativeWindow() const;    // 得到原生GLFW窗口
        WindowGraphicsAPI getGraphicsAPI() const;
        std::vector<const char*> getRequiredVulkanInstanceExtensions() const;
        std::array<int, 2> getWindowSize() const;   // 返回宽高

        // 设置事件回调
        inline void setEventCallback(const EventCallbackFn& callback) { 
            m_data.event_callback = callback; 
            // bindEvents(); 该函数在Window_system子系统初始化后调用，bindEvents放这会覆盖ImGui的事件
        }

        // 测试
        void update();

    protected:
        void bindEvents(); // 绑定事件回调

    private:
        GLFWwindow* m_window{nullptr};
        WindowGraphicsAPI m_graphics_api{WindowGraphicsAPI::Vulkan};

        bool is_focus_mode{false};

        // 设计这个结构体是为了兼容GLFW的C风格代码
        struct WindowData {
            std::string title;
            int width, height;
            bool vsync;
            
            EventCallbackFn event_callback;
        };
        WindowData m_data;
    };
} // namespace NexAur
