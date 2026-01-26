#include "pch.h"
#include "opengl_graphics_context.h"

namespace NexAur {
    OpenGLGraphicsContext::OpenGLGraphicsContext(GLFWwindow* window_handle)
        : m_window_handle(window_handle) {
        NX_CORE_ASSERT(window_handle, "Window handle is null!");
    }

    void OpenGLGraphicsContext::init() {
        glfwMakeContextCurrent(m_window_handle);
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        if (!status) {
            NX_CORE_FATAL("Failed to initialize GLAD!");
            return;
        }

        NX_CORE_INFO("OpenGL Info:");
        NX_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        NX_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        NX_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
    }

    void OpenGLGraphicsContext::swapBuffers() {
        glfwSwapBuffers(m_window_handle);
    }
} // namespace NexAur
