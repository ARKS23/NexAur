//
// Created by ASUS on 2026/1/14.
//

#include "pch.h"

#include <random>

#include "window_system.h"
#include "Core/Events/window_event.h"
#include "Core/Events/key_event.h"
#include "Core/Events/mouse_event.h"
#include "Function/Renderer/Platform/OpenGL/opengl_graphics_context.h"

namespace NexAur {
    WindowSystem::WindowSystem() = default;

    WindowSystem::~WindowSystem() {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void WindowSystem::init(const WindowSpecification& specification) {
        if (!glfwInit()) {
            NX_CORE_FATAL(__FUNCTION__, "Fail to init GLFW");
            return;
        }

        m_width = specification.width;
        m_height = specification.height;

        // OpenGL 4.6 核心模式 （后期拓展Vulkan时修改）
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(specification.width, specification.height, specification.title, nullptr, nullptr);
        if (!m_window) {
            NX_CORE_FATAL(__FUNCTION__, "Fail to create GLFW window");
            glfwTerminate();
            return;
        }

        // 创建图形上下文管理对象
        m_context = std::make_unique<OpenGLGraphicsContext>(m_window);
        m_context->init();
    }

    void WindowSystem::pollEvents() {
        glfwPollEvents();
    }

    bool WindowSystem::shouldClose() const {
        return glfwWindowShouldClose(m_window);
    }

    void WindowSystem::shutdown() {
        glfwSetWindowShouldClose(m_window, true);
    }

    void WindowSystem::setTitle(const char* title) {
        glfwSetWindowTitle(m_window, title);
    }

    GLFWwindow* WindowSystem::getNativeWindow() const {
        return m_window;
    }

    std::array<int, 2> WindowSystem::getWindowSize() const {
        return {m_width, m_height};
    }

    void WindowSystem::update() {
        // 交换缓冲区 (这一步才会把画的东西显示到屏幕上)
        m_context->swapBuffers();
    }

    void WindowSystem::bindEvents() {
        // 设置自定义指针槽，存储自定义数据，以便在回调中使用
        glfwSetWindowUserPointer(m_window, &m_data);

        // 设置窗口大小回调
        glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.width = width;
            data.height = height;

            // 创建并分发窗口调整事件
            WindowResizeEvent event(width, height);
            data.event_callback(event);
        });

        // 关闭窗口
        glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            WindowCloseEvent event;
            data.event_callback(event);
        });

        // 键盘按键
		glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			switch (action)
			{
				case GLFW_PRESS:{
					KeyPressedEvent event(key, 0); // 0是repeatCount
					data.event_callback(event);
					break;
				}
				case GLFW_RELEASE: {
					KeyReleasedEvent event(key);
					data.event_callback(event);
					break;
				}
				case GLFW_REPEAT: {
					KeyPressedEvent event(key, 1); // 0是repeatCount
					data.event_callback(event);
					break;
				}
			}
		});

        // 字符输入
		glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int keycode) {
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
			KeyTypedEvent event(keycode);
			data.event_callback(event);
		});

        // 鼠标点击
        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
            WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

            switch (action)
            {
                case GLFW_PRESS: {
                    MouseButtonPressedEvent event(button);
                    data.event_callback(event);
                    break;
                }
                case GLFW_RELEASE: {
                    MouseButtonReleasedEvent event(button);
                    data.event_callback(event);
                    break;
                }
            }
        });

        // 鼠标滚动
		glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			// 进行类型转换，事件系统设计的是float输入参数
			MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
			data.event_callback(event);
		});

		// 鼠标移动
		glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			MouseMovedEvent event(static_cast<float>(xPos), static_cast<float>(yPos));
			data.event_callback(event);
		});
    }
} // namespace NexAur