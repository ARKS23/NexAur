#pragma once
#include <glm/glm.hpp>

#include "uniform_buffer.h"
#include "vertex_array.h"
#include "Core/Base.h"
#include "material.h"

namespace NexAur {
    class Renderer {
    public:
        static void init();
        static void shutdown();
        static void onWindowResize(uint32_t width, uint32_t height);

        static void submit(const std::shared_ptr<Material>& material, const std::shared_ptr<VertexArray>& vertex_array, const glm::mat4& transform = glm::mat4(1.f));

        static void setCameraMatrix(const glm::mat4& viewProjectionMatrix); // 设置VP矩阵，用UBO

    private:
        struct RendererData {
            std::shared_ptr<UniformBuffer> camera_ubo; // 相机矩阵UBO (mat4)
        };
        static RendererData* s_renderer_data;
    };
} // namespace NexAur
