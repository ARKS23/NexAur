#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/graphics_context.h"

struct GLFWwindow;

namespace NexAur {
    class OpenGLGraphicsContext : public GraphicsContext {
    public:
        OpenGLGraphicsContext(GLFWwindow* window_handle);
        virtual void init() override;
        virtual void swapBuffers() override;

    private:
        GLFWwindow* m_window_handle;
    };
} // namespace NexAur