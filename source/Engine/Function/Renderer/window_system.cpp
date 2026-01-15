//
// Created by ASUS on 2026/1/14.
//

#include "pch.h"

#include <random>

#include "window_system.h"

namespace NexAur {
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

        glfwMakeContextCurrent(m_window);
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        if (!status) {
            NX_CORE_FATAL("Failed to initialize GLAD!");
            return;
        }

        // 5. 打印显卡信息 (验证是否真的拿到了 OpenGL 显卡句柄)
        NX_CORE_INFO("OpenGL Info:");
        NX_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        NX_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        NX_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
    }

    void WindowSystem::pollEvents() {
        glfwPollEvents();
    }

    bool WindowSystem::shouldClose() const {
        return glfwWindowShouldClose(m_window);
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
        // 简单的输入测试：按 ESC 关闭
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(m_window, true);

        // 交换缓冲区 (这一步才会把画的东西显示到屏幕上)
        glfwSwapBuffers(m_window);

        // 随机数花屏效果
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);

        glClearColor(dis(gen), dis(gen), dis(gen), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
} // namespace NexAur