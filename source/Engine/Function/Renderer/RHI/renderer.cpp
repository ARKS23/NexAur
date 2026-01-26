#include "pch.h"
#include "renderer.h"
#include "uniform_buffer.h"
#include "renderer_command.h"

namespace NexAur {
    Renderer::RendererData* Renderer::s_renderer_data = nullptr;

    void Renderer::init() {
        RendererCommand::init();

        s_renderer_data = new RendererData();
        s_renderer_data->camera_ubo = UniformBuffer::create(sizeof(glm::mat4), 0); // 创建绑定点0，设置view Projection Matrix的UBO
    }

    void Renderer::shutdown() {
        delete s_renderer_data;
        s_renderer_data = nullptr;
    }

    void Renderer::onWindowResize(uint32_t width, uint32_t height) {
        RendererCommand::setViewport(0, 0, width, height);
    }

    void Renderer::submit(const std::shared_ptr<Material>& material, const std::shared_ptr<VertexArray>& vertex_array, const glm::mat4& transform) {
        material->setMat4("u_Transform", transform);
        material->bind();

        vertex_array->bind();
        RendererCommand::drawIndexed(vertex_array);
    }

    void Renderer::setCameraMatrix(const glm::mat4& viewProjectionMatrix) {
        s_renderer_data->camera_ubo->setData(&viewProjectionMatrix, sizeof(glm::mat4));
    }
} // namespace NexAur