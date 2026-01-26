#pragma once

#include "Core/Base.h"
#include "renderer_API.h"
#include "definitions.h"

namespace NexAur {
    class NEXAUR_API RendererCommand {
    public:
        static void init();
        static void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        static void setClearColor(const glm::vec4& color);
        static void clear(ClearBufferFlag flags = ClearBufferFlag::ColorDepth);

        static void drawIndexed(const std::shared_ptr<VertexArray>& vertex_array, uint32_t index_count = 0);
        static void drawLines(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count);
        static void drawTriangles(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count);

        static void setLineWidth(float width);
        static void setDepthFunc(DepthFunc func); // 设置深度比较偏序关系
        static void setDepthMask(bool writable); // 设置深度是否写入

    private:
        static std::shared_ptr<RendererAPI> m_renderer_API;
    };
} // namespace NexAur
