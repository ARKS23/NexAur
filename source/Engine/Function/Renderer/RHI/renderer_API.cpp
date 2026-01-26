#include "pch.h"
#include "renderer_API.h"
#include "Function/Renderer/Platform/OpenGL/opengl_renderer_api.h"

namespace NexAur {
    std::shared_ptr<RendererAPI> RendererAPI::m_API = nullptr;

    std::shared_ptr<RendererAPI> RendererAPI::getAPI() {
        if (!m_API) {
            m_API = std::make_shared<OpenGLRendererAPI>();
        }
        return m_API;
    }
} // namespace NexAur
