#pragma once

#include "Core/Base.h"

struct GLFWwindow; // 前置声明，不想在头文件里暴露 GLFW 细节

namespace NexAur {

    class NEXAUR_API Window
    {
    public:
        Window(int width, int height, const char* title);
        ~Window();

        void OnUpdate(); // 每一帧调用它来交换缓冲区、处理事件
        bool ShouldClose() const;

    private:
        GLFWwindow* m_Window;
    };
}