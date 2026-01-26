#include "pch.h"
#include "renderer_command.h"

namespace NexAur {
    std::shared_ptr<RendererAPI> RendererCommand::m_renderer_API = RendererAPI::getAPI();

    void RendererCommand::init() {
        m_renderer_API->init();
    }

    void RendererCommand::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        m_renderer_API->setViewport(x, y, width, height);
    }

    void RendererCommand::setClearColor(const glm::vec4& color) {
        m_renderer_API->setClearColor(color);
    }

    void RendererCommand::clear(ClearBufferFlag flags) {
        m_renderer_API->clear(flags);
    }

    void RendererCommand::drawIndexed(const std::shared_ptr<VertexArray>& vertex_array, uint32_t index_count) {
        m_renderer_API->drawIndex(vertex_array, index_count);
    }

    void RendererCommand::drawLines(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) {
        m_renderer_API->drawLines(vertex_array, vertex_count);
    }

    void RendererCommand::drawTriangles(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) {
        m_renderer_API->drawTriangles(vertex_array, vertex_count);
    }

    void RendererCommand::setLineWidth(float width) {
        m_renderer_API->setLineWidth(width);
    }

    void RendererCommand::setDepthFunc(DepthFunc func) {
        m_renderer_API->setDepthFunc(func);
    }

    void RendererCommand::setDepthMask(bool writable) {
        m_renderer_API->setDepthMask(writable);
    }

    void RendererCommand::bindDefaultFramebuffer() {
        m_renderer_API->bindDefaultFramebuffer();
    }

} // namespace NexAur