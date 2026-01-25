#include "pch.h"
#include "renderer_API.h"

namespace NexAur {
    std::shared_ptr<RendererAPI> RendererAPI::m_API = nullptr;

    std::shared_ptr<RendererAPI> RendererAPI::getAPI() {
        if (!m_API) {
            m_API = nullptr;
        }
        return m_API;
    }
} // namespace NexAur
