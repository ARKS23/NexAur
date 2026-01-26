#pragma once
#include "glm/glm.hpp"

#include "Core/Base.h"
#include "vertex_array.h"
#include "definitions.h"

namespace NexAur{
    class RendererAPI {
    public:
        virtual ~RendererAPI() = default;

        virtual void init() = 0;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void setClearColor(const glm::vec4& color) = 0;
        virtual void clear(ClearBufferFlag flags = ClearBufferFlag::ColorDepth) = 0;

        // 核心绘制指令
		virtual void drawIndex(const std::shared_ptr<VertexArray>& vertex_array, uint32_t index_count = 0) = 0;	// 用索引来绘制，默认传入0表示绘制整个EBO
		virtual void drawLines(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) = 0;		// 调试渲染，绘制线框
		virtual void drawTriangles(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) = 0;

        // 设置选项
        virtual void setLineWidth(float width) = 0; // 设置线框大小
        virtual void setDepthFunc(DepthFunc depth_func) = 0; // 设置深度比较函数
        virtual void setDepthMask(bool writable) = 0;   // 设置深度是否写入

        virtual void bindDefaultFramebuffer() = 0;  // 绑定默认帧缓冲区

        static std::shared_ptr<RendererAPI> getAPI();

    private:
        static std::shared_ptr<RendererAPI> m_API;
    };
} // namespace NexAur
