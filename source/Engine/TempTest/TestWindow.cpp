#include "pch.h"
#include "Core/Base.h"
#include "TempTest/TestWindow.h"

#include <GLFW/glfw3.h>
#include <random>

namespace NexAur {

    Window::Window(int width, int height, const char* title)
    {
        // 1. 初始化 GLFW
        if (!glfwInit())
        {
            NA_CORE_FATAL("Failed to initialize GLFW!");
            return;
        }

        // 2. 创建窗口
        m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!m_Window)
        {
            NA_CORE_FATAL("Failed to create GLFW window!");
            glfwTerminate();
            return;
        }

        // 3. 设置上下文 (Context) - 必须在 GLAD 初始化之前！
        glfwMakeContextCurrent(m_Window);

        // 4. 初始化 GLAD
        // gladLoadGLLoader 加载 OpenGL 指针，如果这步失败，任何 gl开头的函数都会崩溃
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        if (!status)
        {
            NA_CORE_FATAL("Failed to initialize GLAD!");
            return;
        }

        // 5. 打印显卡信息 (验证是否真的拿到了 OpenGL 显卡句柄)
        NA_CORE_INFO("OpenGL Info:");
        NA_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        NA_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        NA_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    void Window::OnUpdate()
    {
        // 简单的输入测试：按 ESC 关闭
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(m_Window, true);

        // 处理窗口事件 (如鼠标移动)
        glfwPollEvents();
        
        // 交换缓冲区 (这一步才会把画的东西显示到屏幕上)
        glfwSwapBuffers(m_Window);
        
        // 随机数花屏效果
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        glClearColor(dis(gen), dis(gen), dis(gen), 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT);
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }
}