#pragma once
#include "glm/glm.hpp"

#include "Core/Base.h"
#include "vertex_array.h"

namespace NexAur{
    class RendererAPI {
    public:
        virtual ~RendererAPI() = default;

        virtual void init() = 0;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void setClearColor(const glm::vec4& color) = 0;
        virtual void clear() = 0;

        // 核心绘制指令
		virtual void drawIndex(const std::shared_ptr<VertexArray>& VertexArray, uint32_t indexCount = 0) = 0;	// 用索引来绘制，默认传入0表示绘制整个EBO
		virtual void drawLines(const std::shared_ptr<VertexArray>& VertexArray, uint32_t vertexCount) = 0;		// 调试渲染，绘制线框
		virtual void drawTriangles(const std::shared_ptr<VertexArray>& VertexArray, uint32_t vertexCount) = 0;

        virtual void setLineWidth(float width) = 0; // 设置线框大小
    };
} // namespace NexAur
