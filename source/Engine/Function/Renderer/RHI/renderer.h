#pragma once
#include <glm/glm.hpp>

#include "Core/Base.h"
#include "material.h"
#include "vertex_array.h"

namespace NexAur {
    class Renderer {
    public:
        static void init();
        static void shutdown();
        static void onWindowResize(uint32_t width, uint32_t height);

        static void submit(const std::shared_ptr<Material>& material, const std::shared_ptr<VertexArray>& vertex_array, const glm::mat4& transform = glm::mat4(1.f));

        static void setCameraMatrix(const glm::mat4& viewProjectionMatrix); // 设置VP矩阵，用UBO
    };
} // namespace NexAur
